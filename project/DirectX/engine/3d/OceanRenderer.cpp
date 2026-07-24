#include "OceanRenderer.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <complex>
#include <cstdint>
#include <limits>
#include <vector>

#include <DirectXPackedVector.h>

#include "Camera.h"
#include "DebugCamera.h"
#include "DirectXCommon.h"
#include "Model.h"
#include "Object3dCommon.h"

namespace {
constexpr size_t AlignConstantBufferSize(size_t size)
{
	return (size + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1) &
		~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);
}

constexpr D3D12_RESOURCE_STATES kShaderReadState =
	static_cast<D3D12_RESOURCE_STATES>(
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

constexpr double kOceanPi = 3.14159265358979323846;
constexpr double kOceanGravity = 9.81;
constexpr double kPhillipsAmplitude = 0.0008;
constexpr uint32_t kDirectionIntegrationSamples = 64;
constexpr uint32_t kDirectionValidationSamples = 128;
static_assert(sizeof(OceanRenderer::OceanFFTParameters) == 80);

struct SpectrumEvaluation {
	double radial = 0.0;
	double directional = 0.0;
	double waveNumber = 0.0;
	double directionalNormalizationError = 0.0;
};

Vector2 NormalizeDirection(const Vector2& direction, const Vector2& fallback)
{
	const double lengthSquared =
		static_cast<double>(direction.x) * direction.x +
		static_cast<double>(direction.y) * direction.y;
	if (lengthSquared <= 0.000001) {
		return fallback;
	}
	const float inverseLength =
		static_cast<float>(1.0 / std::sqrt(lengthSquared));
	return { direction.x * inverseLength, direction.y * inverseLength };
}

double WrapAngle(double angle)
{
	return std::atan2(std::sin(angle), std::cos(angle));
}

double JonswapAlpha(const OceanRenderer::OceanFFTParameters& parameters)
{
	const double windSpeed = (std::max)(static_cast<double>(parameters.windSpeed), 0.1);
	const double fetch = (std::max)(static_cast<double>(parameters.fetch), 1.0);
	const double nondimensionalFetch =
		(std::max)(windSpeed * windSpeed / (fetch * kOceanGravity), 1.0e-8);
	return 0.076 * std::pow(nondimensionalFetch, 0.22);
}

double JonswapPeakAngularFrequency(
	const OceanRenderer::OceanFFTParameters& parameters)
{
	const double windSpeed = (std::max)(static_cast<double>(parameters.windSpeed), 0.1);
	const double fetch = (std::max)(static_cast<double>(parameters.fetch), 1.0);
	return 22.0 * std::pow(
		kOceanGravity * kOceanGravity / (windSpeed * fetch),
		1.0 / 3.0);
}

double JonswapRadialSpectrum(
	const OceanRenderer::OceanFFTParameters& parameters,
	double angularFrequency)
{
	const double omega = (std::max)(angularFrequency, 0.0001);
	const double omegaPeak =
		(std::max)(JonswapPeakAngularFrequency(parameters), 0.0001);
	const double sigma = omega <= omegaPeak ? 0.07 : 0.09;
	const double difference = omega - omegaPeak;
	const double peakExponent = std::exp(
		-(difference * difference) /
		(std::max)(2.0 * sigma * sigma * omegaPeak * omegaPeak, 0.000001));
	const double peakEnhancement = std::pow(
		(std::max)(static_cast<double>(parameters.gamma), 1.0),
		peakExponent);
	const double frequencyRatio = omegaPeak / omega;
	const double lowDamping = std::exp(
		-(std::max)(static_cast<double>(parameters.lowFrequencyDamping), 0.0) *
		std::pow(frequencyRatio, 4.0));
	const double highRatio = omega / omegaPeak;
	const double highDamping = std::exp(
		-(std::max)(static_cast<double>(parameters.highFrequencyDamping), 0.0) *
		highRatio * highRatio);
	const double spectrum =
		JonswapAlpha(parameters) * kOceanGravity * kOceanGravity /
		std::pow(omega, 5.0) *
		std::exp(-1.25 * std::pow(frequencyRatio, 4.0)) *
		peakEnhancement * lowDamping * highDamping;
	return std::isfinite(spectrum) ? (std::max)(spectrum, 0.0) : 0.0;
}

double DonelanBeta(double frequencyRatio)
{
	const double ratio = (std::max)(frequencyRatio, 0.0001);
	if (ratio < 0.95) {
		return 2.61 * std::pow(ratio, 1.3);
	}
	if (ratio < 1.6) {
		return 2.28 * std::pow(ratio, -1.3);
	}
	const double epsilon =
		0.8393 * std::exp(-0.567 * std::log(ratio * ratio)) - 0.4;
	return std::pow(10.0, epsilon);
}

double DonelanBanner(
	const OceanRenderer::OceanFFTParameters& parameters,
	double angularFrequency,
	double directionOffset)
{
	const double omegaPeak =
		(std::max)(JonswapPeakAngularFrequency(parameters), 0.0001);
	const double beta =
		(std::max)(DonelanBeta(angularFrequency / omegaPeak), 0.0001);
	const double wrappedOffset = WrapAngle(directionOffset);
	const double inverseCosh = 1.0 / std::cosh(
		(std::clamp)(beta * wrappedOffset, -20.0, 20.0));
	return 0.5 * beta /
		(std::max)(std::tanh(beta * kOceanPi), 0.0001) *
		inverseCosh * inverseCosh;
}

double OppositeWeight(
	const OceanRenderer::OceanFFTParameters& parameters,
	double directionOffset)
{
	const double forwardWeight = (std::clamp)(
		0.5 + 0.5 * std::cos(WrapAngle(directionOffset)),
		0.0,
		1.0);
	const double suppression = (std::clamp)(
		static_cast<double>(parameters.oppositeWaveSuppression),
		0.0,
		1.0);
	return (1.0 - suppression) +
		suppression * forwardWeight * forwardWeight;
}

double DirectionalRaw(
	const OceanRenderer::OceanFFTParameters& parameters,
	double angularFrequency,
	double absoluteAngle)
{
	const Vector2 windDirection = NormalizeDirection(
		parameters.windDirection,
		{ 1.0f, 0.0f });
	const Vector2 swellDirection = NormalizeDirection(
		parameters.swellDirection,
		windDirection);
	const double windOffset = WrapAngle(
		absoluteAngle - std::atan2(windDirection.y, windDirection.x));
	const double swellOffset = WrapAngle(
		absoluteAngle - std::atan2(swellDirection.y, swellDirection.x));
	const double frequencyRatio = angularFrequency /
		(std::max)(JonswapPeakAngularFrequency(parameters), 0.0001);
	const double swellAmount = (std::clamp)(
		static_cast<double>(parameters.swellAmount),
		0.0,
		1.0);
	const double swellExponent =
		16.0 * std::pow(std::tanh(1.0 / (std::max)(frequencyRatio, 0.0001)), 2.0) *
		swellAmount;
	const double swellShape = std::pow(
		(std::max)(std::abs(std::cos(0.5 * swellOffset)), 0.000001),
		2.0 * swellExponent);
	const double windLobe =
		DonelanBanner(parameters, angularFrequency, windOffset) *
		OppositeWeight(parameters, windOffset);
	const double swellLobe =
		DonelanBanner(parameters, angularFrequency, swellOffset) *
		swellShape *
		OppositeWeight(parameters, swellOffset);
	return (1.0 - swellAmount) * windLobe + swellAmount * swellLobe;
}

double DirectionalIntegral(
	const OceanRenderer::OceanFFTParameters& parameters,
	double angularFrequency,
	uint32_t sampleCount)
{
	const double angleStep = 2.0 * kOceanPi / sampleCount;
	double integral = 0.0;
	for (uint32_t index = 0; index < sampleCount; ++index) {
		const double angle =
			-kOceanPi + (static_cast<double>(index) + 0.5) * angleStep;
		integral += DirectionalRaw(parameters, angularFrequency, angle);
	}
	return (std::max)(integral * angleStep, 0.000001);
}

SpectrumEvaluation EvaluateSpectrum(
	const OceanRenderer::OceanFFTParameters& parameters,
	double waveX,
	double waveZ)
{
	SpectrumEvaluation result{};
	const double waveNumber = std::sqrt(waveX * waveX + waveZ * waveZ);
	if (waveNumber < 0.0001) {
		return result;
	}

	if (parameters.spectrumModel ==
		static_cast<uint32_t>(OceanRenderer::SpectrumModel::Phillips)) {
		const double directionX = waveX / waveNumber;
		const double directionZ = waveZ / waveNumber;
		const Vector2 windDirection = NormalizeDirection(
			parameters.windDirection,
			{ 1.0f, 0.0f });
		const double alignment =
			directionX * windDirection.x + directionZ * windDirection.y;
		result.directional = alignment * alignment *
			(alignment >= 0.0 ? 1.0 : 0.07);
		const double largestWave =
			parameters.windSpeed * parameters.windSpeed / kOceanGravity;
		const double k2 = waveNumber * waveNumber;
		const double dampingLength = largestWave * 0.001;
		result.radial =
			kPhillipsAmplitude * parameters.amplitude *
			std::exp(-1.0 / (std::max)(k2 * largestWave * largestWave, 0.000001)) *
			std::exp(-k2 * dampingLength * dampingLength) /
			(std::max)(k2 * k2, 0.000001);
		result.waveNumber = result.radial * result.directional;
		return result;
	}

	const double angularFrequency = std::sqrt(kOceanGravity * waveNumber);
	const double absoluteAngle = std::atan2(waveZ, waveX);
	const double directionIntegral = DirectionalIntegral(
		parameters,
		angularFrequency,
		kDirectionIntegrationSamples);
	result.directional =
		DirectionalRaw(parameters, angularFrequency, absoluteAngle) /
		directionIntegral;
	result.directionalNormalizationError = std::abs(
		DirectionalIntegral(
			parameters,
			angularFrequency,
			kDirectionValidationSamples) /
		directionIntegral - 1.0);
	result.radial = JonswapRadialSpectrum(parameters, angularFrequency);
	const double angularFrequencyDerivative =
		0.5 * kOceanGravity / (std::max)(angularFrequency, 0.0001);
	result.waveNumber =
		parameters.amplitude * result.radial * result.directional *
		angularFrequencyDerivative / waveNumber;
	if (!std::isfinite(result.waveNumber)) {
		result.waveNumber = 0.0;
	}
	return result;
}

float ExpectedHeightRms(
	const OceanRenderer::OceanFFTParameters& parameters,
	uint32_t resolution)
{
	const double deltaK =
		2.0 * kOceanPi / (std::max)(static_cast<double>(parameters.patchLength), 1.0);
	double variance = 0.0;
	for (uint32_t y = 0; y < resolution; ++y) {
		for (uint32_t x = 0; x < resolution; ++x) {
			const int32_t centeredX =
				static_cast<int32_t>(x) - static_cast<int32_t>(resolution / 2);
			const int32_t centeredY =
				static_cast<int32_t>(y) - static_cast<int32_t>(resolution / 2);
			const SpectrumEvaluation spectrum = EvaluateSpectrum(
				parameters,
				centeredX * deltaK,
				centeredY * deltaK);
			variance += spectrum.waveNumber * deltaK * deltaK;
		}
	}
	return static_cast<float>(std::sqrt((std::max)(variance, 0.0)));
}

uint32_t OceanHashReference(uint32_t value)
{
	value ^= value >> 16;
	value *= 0x7feb352du;
	value ^= value >> 15;
	value *= 0x846ca68bu;
	value ^= value >> 16;
	return value;
}

double OceanHash01Reference(
	uint32_t x,
	uint32_t y,
	uint32_t seed,
	uint32_t salt)
{
	const uint32_t value = OceanHashReference(
		x * 0x9e3779b9u ^
		y * 0x85ebca6bu ^
		seed ^
		salt);
	return (static_cast<double>(value) + 0.5) / 4294967296.0;
}

std::complex<double> OceanGaussianReference(
	uint32_t x,
	uint32_t y,
	uint32_t seed)
{
	const double u1 = (std::max)(
		OceanHash01Reference(x, y, seed, 0x68bc21ebu),
		0.000001);
	const double u2 =
		OceanHash01Reference(x, y, seed, 0x02e5be93u);
	const double radius = std::sqrt(-2.0 * std::log(u1));
	const double angle = 2.0 * kOceanPi * u2;
	return { radius * std::cos(angle), radius * std::sin(angle) };
}

struct SeedEnsembleResult {
	float mean = 0.0f;
	float standardDeviation = 0.0f;
	float minimum = 0.0f;
	float maximum = 0.0f;
	float meanRelativeError = 0.0f;
	float meanAbsoluteRelativeError = 0.0f;
	uint32_t invalidValueCount = 0;
};

SeedEnsembleResult EvaluateSeedEnsemble(
	const OceanRenderer::OceanFFTParameters& parameters,
	uint32_t seedCount)
{
	const uint32_t resolution = parameters.fftSize;
	const size_t coefficientCount =
		static_cast<size_t>(resolution) * resolution;
	const double deltaK =
		2.0 * kOceanPi / (std::max)(
			static_cast<double>(parameters.patchLength),
			1.0);
	std::vector<double> coefficientAmplitudes(coefficientCount);
	for (uint32_t y = 0; y < resolution; ++y) {
		for (uint32_t x = 0; x < resolution; ++x) {
			const int32_t centeredX =
				static_cast<int32_t>(x) - static_cast<int32_t>(resolution / 2);
			const int32_t centeredY =
				static_cast<int32_t>(y) - static_cast<int32_t>(resolution / 2);
			const SpectrumEvaluation spectrum = EvaluateSpectrum(
				parameters,
				centeredX * deltaK,
				centeredY * deltaK);
			coefficientAmplitudes[
				static_cast<size_t>(y) * resolution + x] =
				std::sqrt((std::max)(spectrum.waveNumber, 0.0) * 0.25) *
				deltaK;
		}
	}

	const double physicalExpectedRms =
		ExpectedHeightRms(parameters, resolution);
	std::vector<double> rmsValues;
	rmsValues.reserve(seedCount);
	std::vector<std::complex<double>> h0(coefficientCount);
	for (uint32_t seedIndex = 0; seedIndex < seedCount; ++seedIndex) {
		const uint32_t seed = parameters.seed + seedIndex;
		for (uint32_t y = 0; y < resolution; ++y) {
			for (uint32_t x = 0; x < resolution; ++x) {
				const size_t index =
					static_cast<size_t>(y) * resolution + x;
				h0[index] =
					OceanGaussianReference(x, y, seed) *
					coefficientAmplitudes[index];
				if (x == resolution / 2 && y == resolution / 2) {
					h0[index] = {};
				}
			}
		}

		double evolvedVariance = 0.0;
		for (uint32_t y = 0; y < resolution; ++y) {
			for (uint32_t x = 0; x < resolution; ++x) {
				const size_t index =
					static_cast<size_t>(y) * resolution + x;
				const uint32_t oppositeX = (resolution - x) % resolution;
				const uint32_t oppositeY = (resolution - y) % resolution;
				const size_t oppositeIndex =
					static_cast<size_t>(oppositeY) * resolution + oppositeX;
				const int32_t centeredX =
					static_cast<int32_t>(x) -
					static_cast<int32_t>(resolution / 2);
				const int32_t centeredY =
					static_cast<int32_t>(y) -
					static_cast<int32_t>(resolution / 2);
				const double waveNumber = deltaK * std::sqrt(
					static_cast<double>(centeredX) * centeredX +
					static_cast<double>(centeredY) * centeredY);
				const double phase =
					std::sqrt(kOceanGravity * waveNumber) *
					parameters.time;
				const std::complex<double> positivePhase = std::polar(1.0, phase);
				const std::complex<double> negativePhase =
					std::polar(1.0, -phase);
				const std::complex<double> evolved =
					h0[index] * positivePhase +
					std::conj(h0[oppositeIndex]) * negativePhase;
				evolvedVariance += std::norm(evolved);
			}
		}
		if (std::isfinite(evolvedVariance)) {
			rmsValues.push_back(std::sqrt((std::max)(evolvedVariance, 0.0)));
		}
	}

	SeedEnsembleResult result{};
	result.invalidValueCount = seedCount -
		static_cast<uint32_t>(rmsValues.size());
	if (rmsValues.empty()) {
		return result;
	}
	double sum = 0.0;
	double relativeErrorSum = 0.0;
	result.minimum = (std::numeric_limits<float>::max)();
	result.maximum = (std::numeric_limits<float>::lowest)();
	for (double rms : rmsValues) {
		sum += rms;
		result.minimum = (std::min)(result.minimum, static_cast<float>(rms));
		result.maximum = (std::max)(result.maximum, static_cast<float>(rms));
		if (physicalExpectedRms > 0.000001) {
			relativeErrorSum +=
				std::abs(rms - physicalExpectedRms) / physicalExpectedRms;
		}
	}
	const double mean = sum / rmsValues.size();
	double squaredDifferenceSum = 0.0;
	for (double rms : rmsValues) {
		const double difference = rms - mean;
		squaredDifferenceSum += difference * difference;
	}
	result.mean = static_cast<float>(mean);
	result.standardDeviation = static_cast<float>(
		std::sqrt(squaredDifferenceSum / rmsValues.size()));
	result.meanRelativeError =
		physicalExpectedRms > 0.000001
		? static_cast<float>(
			std::abs(mean - physicalExpectedRms) / physicalExpectedRms)
		: 0.0f;
	result.meanAbsoluteRelativeError = static_cast<float>(
		relativeErrorSum / rmsValues.size());
	return result;
}
}

void OceanRenderer::Initialize(Model* gridModel, uint32_t environmentSrvIndex)
{
	gridModel_ = gridModel;
	environmentSrvIndex_ = environmentSrvIndex;

	DirectXCommon* dxCommon = Object3dCommon::GetInstance()->GetDxCommon();
	parameterResource_ = dxCommon->CreateBufferResource(
		AlignConstantBufferSize(sizeof(OceanParameters)));
	parameterResource_->Map(0, nullptr, reinterpret_cast<void**>(&parameterData_));
	UploadParameters();
	InitializeFFT();
}

void OceanRenderer::Update(
	float time,
	Camera& camera,
	DebugCamera& debugCamera,
	bool useDebugCamera)
{
	ResolveFFTDiagnostics();
	parameters_.time = time;
	if (useDebugCamera) {
		parameters_.viewProjection = debugCamera.GetViewProjectionMatrix();
		parameters_.cameraPosition = debugCamera.GetEyePosition();
	} else {
		parameters_.viewProjection = camera.GetViewProjectionMatrix();
		parameters_.cameraPosition = camera.GetTranslate();
	}

	// Only the finite grid follows the camera. Ocean.VS evaluates every wave
	// from the resulting world position, so this movement cannot drag the pattern.
	parameters_.gridOrigin.x = parameters_.cameraPosition.x;
	parameters_.gridOrigin.y = parameters_.baseHeight;
	parameters_.gridOrigin.z = parameters_.cameraPosition.z;
	parameters_.sunDirection = Object3dCommon::GetInstance()->GetLightDir();
	UploadParameters();
}

void OceanRenderer::Draw()
{
	if (!gridModel_ || !parameterResource_) {
		return;
	}

	if (parameters_.waveSource >= 0.5f ||
		parameters_.fftDebugMode > 0.5f ||
		fftDiagnosticsRequested_) {
		RunFFT();
	}

	DirectXCommon* dxCommon = Object3dCommon::GetInstance()->GetDxCommon();
	auto commandList = dxCommon->GetList();
	auto& pso = dxCommon->GetPSOOceanForScene();
	commandList->SetGraphicsRootSignature(pso.root_.GetSignature().Get());
	commandList->SetPipelineState(pso.graphicsState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->SetGraphicsRootConstantBufferView(
		0,
		parameterResource_->GetGPUVirtualAddress());
	Object3dCommon::GetInstance()->GetSrvManager()->SetGraphicsRootDescriptorTable(
		1,
		environmentSrvIndex_);
	Object3dCommon::GetInstance()->GetSrvManager()->SetGraphicsRootDescriptorTable(
		2,
		displacement_.srvIndex);
	Object3dCommon::GetInstance()->GetSrvManager()->SetGraphicsRootDescriptorTable(
		3,
		slope_.srvIndex);
	Object3dCommon::GetInstance()->GetSrvManager()->SetGraphicsRootDescriptorTable(
		4,
		initialSpectrum_.srvIndex);
	Object3dCommon::GetInstance()->GetSrvManager()->SetGraphicsRootDescriptorTable(
		5,
		evolvedSpectrumDebug_.srvIndex);
	Object3dCommon::GetInstance()->GetSrvManager()->SetGraphicsRootDescriptorTable(
		6,
		spectrumDebug_.srvIndex);
	gridModel_->DrawOnlyMesh();
}

void OceanRenderer::SetMode(Mode mode)
{
	parameters_.mode = static_cast<float>(mode);
}

void OceanRenderer::SetWaveSource(WaveSource source)
{
	parameters_.waveSource = static_cast<float>(source);
}

void OceanRenderer::SetTint(const Vector4& tint)
{
	parameters_.tint = tint;
}

void OceanRenderer::SetBaseHeight(float height)
{
	parameters_.baseHeight = height;
}

void OceanRenderer::SetWind(
	const Vector2& direction,
	float speed,
	float choppiness)
{
	const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
	const Vector2 normalizedDirection =
		length > 0.0001f
		? Vector2{ direction.x / length, direction.y / length }
		: Vector2{ 0.18f, 0.98f };
	const float normalizedSpeed = (std::max)(speed, 0.0f);
	const float normalizedChoppiness = (std::max)(choppiness, 0.0f);
	if (std::abs(fftParameters_.windDirection.x - normalizedDirection.x) > 0.0001f ||
		std::abs(fftParameters_.windDirection.y - normalizedDirection.y) > 0.0001f ||
		std::abs(fftParameters_.windSpeed - normalizedSpeed) > 0.0001f) {
		fftInitialSpectrumDirty_ = true;
	}
	if (std::abs(fftParameters_.choppiness - normalizedChoppiness) > 0.0001f) {
		fftOutputDirty_ = true;
	}

	parameters_.windDirection = normalizedDirection;
	parameters_.windSpeed = normalizedSpeed;
	parameters_.choppiness = normalizedChoppiness;
	fftParameters_.windDirection = normalizedDirection;
	fftParameters_.windSpeed = normalizedSpeed;
	fftParameters_.choppiness = normalizedChoppiness;
}

void OceanRenderer::SetSun(
	float intensity,
	float sunSpecularStrength,
	float artisticSunLaneStrength)
{
	parameters_.sunIntensity = (std::max)(intensity, 0.0f);
	parameters_.sunSpecularStrength = (std::max)(sunSpecularStrength, 0.0f);
	parameters_.artisticSunLaneStrength = (std::max)(artisticSunLaneStrength, 0.0f);
}

void OceanRenderer::SetDiagnostics(
	bool sunPathEnabled,
	bool atmosphereEnabled,
	bool farFlattenEnabled,
	bool proceduralCloudReflectionEnabled,
	int debugMode,
	float atmosphereStrength,
	float farFlattenStrength)
{
	parameters_.diagnosticsEnabled = 1.0f;
	parameters_.sunPathEnabled = sunPathEnabled ? 1.0f : 0.0f;
	parameters_.atmosphereEnabled = atmosphereEnabled ? 1.0f : 0.0f;
	parameters_.farFlattenEnabled = farFlattenEnabled ? 1.0f : 0.0f;
	parameters_.proceduralCloudReflectionEnabled =
		proceduralCloudReflectionEnabled ? 1.0f : 0.0f;
	parameters_.debugMode = static_cast<float>((std::clamp)(debugMode, 0, 5));
	parameters_.atmosphereStrength = (std::max)(atmosphereStrength, 0.0f);
	parameters_.farFlattenStrength = (std::clamp)(farFlattenStrength, 0.0f, 1.0f);
}

void OceanRenderer::SetFFTSettings(
	float time,
	float amplitude,
	float patchLength,
	uint32_t seed,
	bool paused,
	int debugMode,
	float debugDisplayScale)
{
	const float clampedAmplitude = (std::max)(amplitude, 0.0f);
	const float clampedPatchLength = (std::clamp)(patchLength, 32.0f, 2048.0f);
	if (std::abs(fftParameters_.amplitude - clampedAmplitude) > 0.000001f ||
		std::abs(fftParameters_.patchLength - clampedPatchLength) > 0.0001f ||
		fftParameters_.seed != seed) {
		fftInitialSpectrumDirty_ = true;
	}
	if (std::abs(fftParameters_.time - time) > 0.0001f) {
		fftOutputDirty_ = true;
	}

	fftParameters_.time = time;
	fftParameters_.amplitude = clampedAmplitude;
	fftParameters_.patchLength = clampedPatchLength;
	fftParameters_.seed = seed;
	fftPaused_ = paused;
	parameters_.fftPatchLength = clampedPatchLength;
	parameters_.fftDebugMode =
		static_cast<float>((std::clamp)(debugMode, 0, 9));
	parameters_.fftDebugScale = (std::max)(debugDisplayScale, 0.001f);
}

void OceanRenderer::SetFFTSpectrumSettings(
	SpectrumModel model,
	float fetch,
	float gamma,
	float lowFrequencyDamping,
	float highFrequencyDamping,
	const Vector2& swellDirection,
	float swellAmount,
	float oppositeWaveSuppression)
{
	const uint32_t spectrumModel = static_cast<uint32_t>(model);
	const float clampedFetch = (std::clamp)(fetch, 1.0f, 10000000.0f);
	const float clampedGamma = (std::clamp)(gamma, 1.0f, 10.0f);
	const float clampedLowDamping =
		(std::clamp)(lowFrequencyDamping, 0.0f, 10.0f);
	const float clampedHighDamping =
		(std::clamp)(highFrequencyDamping, 0.0f, 10.0f);
	const Vector2 normalizedSwellDirection = NormalizeDirection(
		swellDirection,
		fftParameters_.windDirection);
	const float clampedSwellAmount = (std::clamp)(swellAmount, 0.0f, 1.0f);
	const float clampedOppositeSuppression =
		(std::clamp)(oppositeWaveSuppression, 0.0f, 1.0f);

	if (fftParameters_.spectrumModel != spectrumModel ||
		std::abs(fftParameters_.fetch - clampedFetch) > 0.01f ||
		std::abs(fftParameters_.gamma - clampedGamma) > 0.0001f ||
		std::abs(fftParameters_.lowFrequencyDamping - clampedLowDamping) >
			0.0001f ||
		std::abs(fftParameters_.highFrequencyDamping - clampedHighDamping) >
			0.0001f ||
		std::abs(fftParameters_.swellDirection.x - normalizedSwellDirection.x) >
			0.0001f ||
		std::abs(fftParameters_.swellDirection.y - normalizedSwellDirection.y) >
			0.0001f ||
		std::abs(fftParameters_.swellAmount - clampedSwellAmount) > 0.0001f ||
		std::abs(
			fftParameters_.oppositeWaveSuppression -
			clampedOppositeSuppression) > 0.0001f) {
		fftInitialSpectrumDirty_ = true;
		fftOutputDirty_ = true;
	}

	fftParameters_.spectrumModel = spectrumModel;
	fftParameters_.fetch = clampedFetch;
	fftParameters_.gamma = clampedGamma;
	fftParameters_.lowFrequencyDamping = clampedLowDamping;
	fftParameters_.highFrequencyDamping = clampedHighDamping;
	fftParameters_.swellDirection = normalizedSwellDirection;
	fftParameters_.swellAmount = clampedSwellAmount;
	fftParameters_.oppositeWaveSuppression = clampedOppositeSuppression;
}

void OceanRenderer::RequestFFTDiagnostics()
{
	if (!fftDiagnosticsPending_) {
		fftDiagnosticsRequested_ = true;
	}
}

void OceanRenderer::SetEnvironmentSrvIndex(uint32_t environmentSrvIndex)
{
	environmentSrvIndex_ = environmentSrvIndex;
}

void OceanRenderer::UploadParameters()
{
	if (parameterData_) {
		*parameterData_ = parameters_;
	}
}

void OceanRenderer::InitializeFFT()
{
	DirectXCommon* dxCommon = Object3dCommon::GetInstance()->GetDxCommon();
	fftParameterResource_ = dxCommon->CreateBufferResource(
		AlignConstantBufferSize(sizeof(OceanFFTParameters)));
	fftParameterResource_->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&fftParameterData_));
	*fftParameterData_ = fftParameters_;

	fftComputeRoot_.InitializeForOceanCompute();
	fftComputeRoot_.Create(dxCommon->GetDevice());
	CreateComputePipeline(
		L"resources/shaders/OceanSpectrumInitialize.CS.hlsl",
		spectrumInitializePipeline_);
	CreateComputePipeline(
		L"resources/shaders/OceanSpectrumEvolve.CS.hlsl",
		spectrumEvolvePipeline_);
	CreateComputePipeline(
		L"resources/shaders/OceanFFT.CS.hlsl",
		fftPipeline_);
	CreateComputePipeline(
		L"resources/shaders/OceanFFTOutput.CS.hlsl",
		fftOutputPipeline_);

	CreateFFTTexture(initialSpectrum_, DXGI_FORMAT_R32G32B32A32_FLOAT);
	CreateFFTTexture(spectrumDebug_, DXGI_FORMAT_R32G32B32A32_FLOAT);
	CreateFFTTexture(evolvedSpectrumDebug_, DXGI_FORMAT_R32G32B32A32_FLOAT);
	for (uint32_t index = 0; index < 2; ++index) {
		CreateFFTTexture(spectrumA_[index], DXGI_FORMAT_R32G32B32A32_FLOAT);
		CreateFFTTexture(spectrumB_[index], DXGI_FORMAT_R32G32B32A32_FLOAT);
		CreateFFTTexture(spectrumC_[index], DXGI_FORMAT_R32G32B32A32_FLOAT);
	}
	CreateFFTTexture(displacement_, DXGI_FORMAT_R16G16B16A16_FLOAT);
	CreateFFTTexture(slope_, DXGI_FORMAT_R16G16_FLOAT);
	CreateFFTReadback(displacementReadback_, displacement_);
	CreateFFTReadback(slopeReadback_, slope_);
	CreateFFTReadback(initialSpectrumReadback_, initialSpectrum_);
	CreateFFTReadback(evolvedSpectrumReadback_, evolvedSpectrumDebug_);
	CreateFFTReadback(finalSpectrumReadback_, spectrumA_[0]);
	CreateFFTReadback(spectrumDebugReadback_, spectrumDebug_);
}

void OceanRenderer::CreateFFTTexture(FFTTexture& texture, DXGI_FORMAT format)
{
	DirectXCommon* dxCommon = Object3dCommon::GetInstance()->GetDxCommon();
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC description{};
	description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	description.Width = kFFTSize;
	description.Height = kFFTSize;
	description.DepthOrArraySize = 1;
	description.MipLevels = 1;
	description.Format = format;
	description.SampleDesc.Count = 1;
	description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	description.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	HRESULT hr = dxCommon->GetDevice()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&description,
		kShaderReadState,
		nullptr,
		IID_PPV_ARGS(&texture.resource));
	assert(SUCCEEDED(hr));

	texture.format = format;
	texture.state = kShaderReadState;
	auto* srvManager = Object3dCommon::GetInstance()->GetSrvManager();
	texture.srvIndex = srvManager->Allocate();
	texture.uavIndex = srvManager->Allocate();
	srvManager->CreateSRVforTexture2D(
		texture.srvIndex,
		texture.resource.Get(),
		format,
		1);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDescription{};
	uavDescription.Format = format;
	uavDescription.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	dxCommon->GetDevice()->CreateUnorderedAccessView(
		texture.resource.Get(),
		nullptr,
		&uavDescription,
		srvManager->GetCPUDescriptorHandle(texture.uavIndex));
}

void OceanRenderer::CreateFFTReadback(
	FFTReadback& readback,
	const FFTTexture& source)
{
	DirectXCommon* dxCommon = Object3dCommon::GetInstance()->GetDxCommon();
	const D3D12_RESOURCE_DESC sourceDescription =
		source.resource->GetDesc();
	dxCommon->GetDevice()->GetCopyableFootprints(
		&sourceDescription,
		0,
		1,
		0,
		&readback.footprint,
		nullptr,
		nullptr,
		&readback.totalBytes);

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_READBACK;
	D3D12_RESOURCE_DESC bufferDescription{};
	bufferDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufferDescription.Width = readback.totalBytes;
	bufferDescription.Height = 1;
	bufferDescription.DepthOrArraySize = 1;
	bufferDescription.MipLevels = 1;
	bufferDescription.SampleDesc.Count = 1;
	bufferDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	const HRESULT hr = dxCommon->GetDevice()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&bufferDescription,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&readback.resource));
	assert(SUCCEEDED(hr));
}

void OceanRenderer::CreateComputePipeline(
	const std::wstring& shaderPath,
	Microsoft::WRL::ComPtr<ID3D12PipelineState>& pipeline)
{
	DirectXCommon* dxCommon = Object3dCommon::GetInstance()->GetDxCommon();
	IDxcBlob* shader = dxCommon->CompileShader(shaderPath, L"cs_6_0");
	assert(shader != nullptr);

	D3D12_COMPUTE_PIPELINE_STATE_DESC description{};
	description.pRootSignature = fftComputeRoot_.GetSignature().Get();
	description.CS = {
		shader->GetBufferPointer(),
		shader->GetBufferSize(),
	};
	HRESULT hr = dxCommon->GetDevice()->CreateComputePipelineState(
		&description,
		IID_PPV_ARGS(&pipeline));
	shader->Release();
	assert(SUCCEEDED(hr));
}

void OceanRenderer::RunFFT()
{
	if (fftPaused_ &&
		fftHasOutput_ &&
		!fftInitialSpectrumDirty_ &&
		!fftOutputDirty_ &&
		!fftDiagnosticsRequested_) {
		return;
	}

	*fftParameterData_ = fftParameters_;
	auto commandList = Object3dCommon::GetInstance()->GetDxCommon()->GetList();
	Object3dCommon::GetInstance()->GetSrvManager()->PreDraw();
	constexpr uint32_t groupCount = kFFTSize / 16;
	const uint32_t zeroConstants[4] = {};

	if (fftInitialSpectrumDirty_) {
		Transition(initialSpectrum_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		Transition(spectrumDebug_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		BindComputePipeline(spectrumInitializePipeline_.Get());
		commandList->SetComputeRoot32BitConstants(1, 4, zeroConstants, 0);
		BindComputeUav(5, initialSpectrum_);
		BindComputeUav(6, spectrumDebug_);
		commandList->Dispatch(groupCount, groupCount, 1);
		InsertUAVBarrier(initialSpectrum_);
		InsertUAVBarrier(spectrumDebug_);
		Transition(initialSpectrum_, kShaderReadState);
		Transition(spectrumDebug_, kShaderReadState);
		fftInitialSpectrumDirty_ = false;
	}

	Transition(spectrumA_[0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Transition(spectrumB_[0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Transition(spectrumC_[0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Transition(evolvedSpectrumDebug_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	BindComputePipeline(spectrumEvolvePipeline_.Get());
	commandList->SetComputeRoot32BitConstants(1, 4, zeroConstants, 0);
	BindComputeSrv(2, initialSpectrum_);
	BindComputeUav(5, spectrumA_[0]);
	BindComputeUav(6, spectrumB_[0]);
	BindComputeUav(7, spectrumC_[0]);
	BindComputeUav(8, evolvedSpectrumDebug_);
	commandList->Dispatch(groupCount, groupCount, 1);
	InsertUAVBarrier(spectrumA_[0]);
	InsertUAVBarrier(spectrumB_[0]);
	InsertUAVBarrier(spectrumC_[0]);
	InsertUAVBarrier(evolvedSpectrumDebug_);
	Transition(spectrumA_[0], kShaderReadState);
	Transition(spectrumB_[0], kShaderReadState);
	Transition(spectrumC_[0], kShaderReadState);
	Transition(evolvedSpectrumDebug_, kShaderReadState);

	uint32_t currentIndex = 0;
	for (uint32_t direction = 0; direction < 2; ++direction) {
		for (uint32_t stage = 0; stage < kFFTLog2; ++stage) {
			const uint32_t destinationIndex = 1u - currentIndex;
			Transition(
				spectrumA_[destinationIndex],
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			Transition(
				spectrumB_[destinationIndex],
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			Transition(
				spectrumC_[destinationIndex],
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			BindComputePipeline(fftPipeline_.Get());
			const uint32_t dispatchConstants[4] = {
				stage,
				direction,
				kFFTSize,
				0,
			};
			commandList->SetComputeRoot32BitConstants(
				1,
				4,
				dispatchConstants,
				0);
			BindComputeSrv(2, spectrumA_[currentIndex]);
			BindComputeSrv(3, spectrumB_[currentIndex]);
			BindComputeSrv(4, spectrumC_[currentIndex]);
			BindComputeUav(5, spectrumA_[destinationIndex]);
			BindComputeUav(6, spectrumB_[destinationIndex]);
			BindComputeUav(7, spectrumC_[destinationIndex]);
			commandList->Dispatch(groupCount, groupCount, 1);
			InsertUAVBarrier(spectrumA_[destinationIndex]);
			InsertUAVBarrier(spectrumB_[destinationIndex]);
			InsertUAVBarrier(spectrumC_[destinationIndex]);
			Transition(spectrumA_[destinationIndex], kShaderReadState);
			Transition(spectrumB_[destinationIndex], kShaderReadState);
			Transition(spectrumC_[destinationIndex], kShaderReadState);
			currentIndex = destinationIndex;
		}
	}
	finalSpectrumIndex_ = currentIndex;

	Transition(displacement_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Transition(slope_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	BindComputePipeline(fftOutputPipeline_.Get());
	commandList->SetComputeRoot32BitConstants(1, 4, zeroConstants, 0);
	BindComputeSrv(2, spectrumA_[finalSpectrumIndex_]);
	BindComputeSrv(3, spectrumB_[finalSpectrumIndex_]);
	BindComputeSrv(4, spectrumC_[finalSpectrumIndex_]);
	BindComputeUav(5, displacement_);
	BindComputeUav(6, slope_);
	commandList->Dispatch(groupCount, groupCount, 1);
	InsertUAVBarrier(displacement_);
	InsertUAVBarrier(slope_);
	Transition(displacement_, kShaderReadState);
	Transition(slope_, kShaderReadState);
	fftHasOutput_ = true;
	fftOutputDirty_ = false;
	if (fftDiagnosticsRequested_) {
		QueueFFTDiagnosticsReadback();
	}
}

void OceanRenderer::QueueFFTDiagnosticsReadback()
{
	CopyFFTTextureToReadback(displacement_, displacementReadback_);
	CopyFFTTextureToReadback(slope_, slopeReadback_);
	CopyFFTTextureToReadback(
		initialSpectrum_,
		initialSpectrumReadback_);
	CopyFFTTextureToReadback(
		evolvedSpectrumDebug_,
		evolvedSpectrumReadback_);
	CopyFFTTextureToReadback(
		spectrumA_[finalSpectrumIndex_],
		finalSpectrumReadback_);
	CopyFFTTextureToReadback(spectrumDebug_, spectrumDebugReadback_);
	diagnosticsParameters_ = fftParameters_;
	fftDiagnosticsRequested_ = false;
	fftDiagnosticsPending_ = true;
}

void OceanRenderer::ResolveFFTDiagnostics()
{
	if (!fftDiagnosticsPending_) {
		return;
	}

	const D3D12_RANGE displacementRange = {
		0,
		static_cast<SIZE_T>(displacementReadback_.totalBytes),
	};
	const D3D12_RANGE evolvedRange = {
		0,
		static_cast<SIZE_T>(evolvedSpectrumReadback_.totalBytes),
	};
	const D3D12_RANGE initialRange = {
		0,
		static_cast<SIZE_T>(initialSpectrumReadback_.totalBytes),
	};
	const D3D12_RANGE slopeRange = {
		0,
		static_cast<SIZE_T>(slopeReadback_.totalBytes),
	};
	const D3D12_RANGE finalRange = {
		0,
		static_cast<SIZE_T>(finalSpectrumReadback_.totalBytes),
	};
	const D3D12_RANGE debugRange = {
		0,
		static_cast<SIZE_T>(spectrumDebugReadback_.totalBytes),
	};
	void* displacementMapped = nullptr;
	void* evolvedMapped = nullptr;
	void* initialMapped = nullptr;
	void* slopeMapped = nullptr;
	void* finalMapped = nullptr;
	void* debugMapped = nullptr;
	HRESULT hr = displacementReadback_.resource->Map(
		0,
		&displacementRange,
		&displacementMapped);
	assert(SUCCEEDED(hr));
	hr = slopeReadback_.resource->Map(
		0,
		&slopeRange,
		&slopeMapped);
	assert(SUCCEEDED(hr));
	hr = initialSpectrumReadback_.resource->Map(
		0,
		&initialRange,
		&initialMapped);
	assert(SUCCEEDED(hr));
	hr = evolvedSpectrumReadback_.resource->Map(
		0,
		&evolvedRange,
		&evolvedMapped);
	assert(SUCCEEDED(hr));
	hr = finalSpectrumReadback_.resource->Map(
		0,
		&finalRange,
		&finalMapped);
	assert(SUCCEEDED(hr));
	hr = spectrumDebugReadback_.resource->Map(
		0,
		&debugRange,
		&debugMapped);
	assert(SUCCEEDED(hr));
	const auto* displacementData =
		static_cast<const uint8_t*>(displacementMapped);
	const auto* evolvedData = static_cast<const uint8_t*>(evolvedMapped);
	const auto* initialData = static_cast<const uint8_t*>(initialMapped);
	const auto* slopeData = static_cast<const uint8_t*>(slopeMapped);
	const auto* finalData = static_cast<const uint8_t*>(finalMapped);
	const auto* debugData = static_cast<const uint8_t*>(debugMapped);

	OceanFFTDiagnostics diagnostics{};
	diagnostics.sampleTime = diagnosticsParameters_.time;
	diagnostics.heightMinimum = (std::numeric_limits<float>::max)();
	diagnostics.heightMaximum = (std::numeric_limits<float>::lowest)();
	double heightSquaredSum = 0.0;
	double ifftFloatSquaredSum = 0.0;
	double imaginarySquaredSum = 0.0;
	double targetVarianceSum = 0.0;
	double h0MagnitudeSquaredSum = 0.0;
	double evolvedMagnitudeSquaredSum = 0.0;
	double gaussianRealSquaredSum = 0.0;
	double gaussianImaginarySquaredSum = 0.0;
	double hermitianErrorSquaredSum = 0.0;
	double hermitianMagnitudeSquaredSum = 0.0;
	float maximumDirectionalError = 0.0f;
	uint32_t validHeightCount = 0;
	uint32_t validFloatHeightCount = 0;
	uint32_t validH0Count = 0;
	uint32_t validEvolvedCount = 0;
	uint32_t validImaginaryCount = 0;
	uint32_t validHermitianCount = 0;

	auto readFloat4 = [](
		const uint8_t* data,
		const FFTReadback& readback,
		uint32_t x,
		uint32_t y) {
		const uint8_t* row =
			data + static_cast<size_t>(y) * readback.footprint.Footprint.RowPitch;
		return reinterpret_cast<const float*>(
			row + static_cast<size_t>(x) * sizeof(float) * 4);
	};

	const float inverseTransformScale =
		1.0f / static_cast<float>(kFFTSize * kFFTSize);
	const double deltaK =
		2.0 * kOceanPi /
		(std::max)(static_cast<double>(diagnosticsParameters_.patchLength), 1.0);
	for (uint32_t y = 0; y < kFFTSize; ++y) {
		const uint8_t* displacementRow =
			displacementData +
			static_cast<size_t>(y) *
			displacementReadback_.footprint.Footprint.RowPitch;
		const uint8_t* slopeRow =
			slopeData +
			static_cast<size_t>(y) *
			slopeReadback_.footprint.Footprint.RowPitch;
		for (uint32_t x = 0; x < kFFTSize; ++x) {
			const auto* displacementPixel =
				reinterpret_cast<const uint16_t*>(
					displacementRow + static_cast<size_t>(x) * sizeof(uint16_t) * 4);
			float displacementValues[4]{};
			for (uint32_t component = 0; component < 4; ++component) {
				displacementValues[component] =
					DirectX::PackedVector::XMConvertHalfToFloat(
						displacementPixel[component]);
				if (!std::isfinite(displacementValues[component])) {
					++diagnostics.invalidValueCount;
				}
			}
			const auto* slopePixel = reinterpret_cast<const uint16_t*>(
				slopeRow + static_cast<size_t>(x) * sizeof(uint16_t) * 2);
			for (uint32_t component = 0; component < 2; ++component) {
				const float slopeValue =
					DirectX::PackedVector::XMConvertHalfToFloat(
						slopePixel[component]);
				if (!std::isfinite(slopeValue)) {
					++diagnostics.invalidValueCount;
				}
			}
			const float height = displacementValues[1];
			if (std::isfinite(height)) {
				heightSquaredSum += static_cast<double>(height) * height;
				diagnostics.heightMinimum =
					(std::min)(diagnostics.heightMinimum, height);
				diagnostics.heightMaximum =
					(std::max)(diagnostics.heightMaximum, height);
				++validHeightCount;
			}

			const float* debugPixel = readFloat4(
				debugData,
				spectrumDebugReadback_,
				x,
				y);
			for (uint32_t component = 0; component < 4; ++component) {
				if (!std::isfinite(debugPixel[component])) {
					++diagnostics.invalidValueCount;
				}
			}
			if (std::isfinite(debugPixel[2])) {
				maximumDirectionalError =
					(std::max)(maximumDirectionalError, std::abs(debugPixel[2]));
			}
			const double waveNumberSpectrum =
				std::isfinite(debugPixel[3])
				? (std::max)(static_cast<double>(debugPixel[3]), 0.0)
				: 0.0;
			targetVarianceSum +=
				waveNumberSpectrum * deltaK * deltaK;

			const float* initialPixel = readFloat4(
				initialData,
				initialSpectrumReadback_,
				x,
				y);
			for (uint32_t component = 0; component < 4; ++component) {
				if (!std::isfinite(initialPixel[component])) {
					++diagnostics.invalidValueCount;
				}
			}
			const double h0Real =
				static_cast<double>(initialPixel[0]) * inverseTransformScale;
			const double h0Imaginary =
				static_cast<double>(initialPixel[1]) * inverseTransformScale;
			if (std::isfinite(h0Real) && std::isfinite(h0Imaginary)) {
				const double h0MagnitudeSquared =
					h0Real * h0Real + h0Imaginary * h0Imaginary;
				h0MagnitudeSquaredSum += h0MagnitudeSquared;
				++validH0Count;
				const double gaussianScale =
					std::sqrt(waveNumberSpectrum * 0.25) * deltaK;
				if (gaussianScale > 1.0e-20) {
					const double gaussianReal = h0Real / gaussianScale;
					const double gaussianImaginary =
						h0Imaginary / gaussianScale;
					gaussianRealSquaredSum += gaussianReal * gaussianReal;
					gaussianImaginarySquaredSum +=
						gaussianImaginary * gaussianImaginary;
					++diagnostics.gaussianSampleCount;
				}
			}

			const float* finalPixel = readFloat4(
				finalData,
				finalSpectrumReadback_,
				x,
				y);
			const float imaginaryHeight =
				finalPixel[1] * inverseTransformScale;
			const float floatHeight =
				finalPixel[0] * inverseTransformScale;
			for (uint32_t component = 0; component < 4; ++component) {
				if (!std::isfinite(finalPixel[component])) {
					++diagnostics.invalidValueCount;
				}
			}
			if (std::isfinite(imaginaryHeight)) {
				imaginarySquaredSum +=
					static_cast<double>(imaginaryHeight) * imaginaryHeight;
				++validImaginaryCount;
			}
			if (std::isfinite(floatHeight)) {
				ifftFloatSquaredSum +=
					static_cast<double>(floatHeight) * floatHeight;
				++validFloatHeightCount;
			}

			const float* evolvedPixel = readFloat4(
				evolvedData,
				evolvedSpectrumReadback_,
				x,
				y);
			const uint32_t oppositeX = (kFFTSize - x) % kFFTSize;
			const uint32_t oppositeY = (kFFTSize - y) % kFFTSize;
			if (oppositeX == x && oppositeY == y) {
				++diagnostics.selfConjugateBinCount;
			}
			const float* oppositePixel = readFloat4(
				evolvedData,
				evolvedSpectrumReadback_,
				oppositeX,
				oppositeY);
			for (uint32_t component = 0; component < 4; ++component) {
				if (!std::isfinite(evolvedPixel[component])) {
					++diagnostics.invalidValueCount;
				}
			}
			if (std::isfinite(evolvedPixel[0]) &&
				std::isfinite(evolvedPixel[1]) &&
				std::isfinite(oppositePixel[0]) &&
				std::isfinite(oppositePixel[1])) {
				const double evolvedReal =
					static_cast<double>(evolvedPixel[0]) *
					inverseTransformScale;
				const double evolvedImaginary =
					static_cast<double>(evolvedPixel[1]) *
					inverseTransformScale;
				evolvedMagnitudeSquaredSum +=
					evolvedReal * evolvedReal +
					evolvedImaginary * evolvedImaginary;
				++validEvolvedCount;
				const double realError =
					static_cast<double>(evolvedPixel[0]) - oppositePixel[0];
				const double imaginaryError =
					static_cast<double>(evolvedPixel[1]) + oppositePixel[1];
				hermitianErrorSquaredSum +=
					realError * realError + imaginaryError * imaginaryError;
				hermitianMagnitudeSquaredSum +=
					static_cast<double>(evolvedPixel[0]) * evolvedPixel[0] +
					static_cast<double>(evolvedPixel[1]) * evolvedPixel[1];
				++validHermitianCount;
			}
		}
	}

	if (validHeightCount > 0) {
		diagnostics.heightRms = static_cast<float>(
			std::sqrt(heightSquaredSum / validHeightCount));
	} else {
		diagnostics.heightMinimum = 0.0f;
		diagnostics.heightMaximum = 0.0f;
	}
	diagnostics.significantWaveHeight = 4.0f * diagnostics.heightRms;
	diagnostics.targetSpectrumVariance =
		static_cast<float>(targetVarianceSum);
	diagnostics.legacyCoefficientVariance =
		2.0f * diagnostics.targetSpectrumVariance;
	diagnostics.h0PredictedVariance =
		static_cast<float>(2.0 * h0MagnitudeSquaredSum);
	diagnostics.evolvedParsevalVariance =
		static_cast<float>(evolvedMagnitudeSquaredSum);
	diagnostics.ifftFloatVariance =
		validFloatHeightCount > 0
		? static_cast<float>(ifftFloatSquaredSum / validFloatHeightCount)
		: 0.0f;
	diagnostics.rgba16fVariance =
		validHeightCount > 0
		? static_cast<float>(heightSquaredSum / validHeightCount)
		: 0.0f;
	diagnostics.gaussianRealSquared =
		diagnostics.gaussianSampleCount > 0
		? static_cast<float>(
			gaussianRealSquaredSum / diagnostics.gaussianSampleCount)
		: 0.0f;
	diagnostics.gaussianImaginarySquared =
		diagnostics.gaussianSampleCount > 0
		? static_cast<float>(
			gaussianImaginarySquaredSum / diagnostics.gaussianSampleCount)
		: 0.0f;
	diagnostics.gaussianMagnitudeSquared =
		diagnostics.gaussianRealSquared +
		diagnostics.gaussianImaginarySquared;
	diagnostics.h0MagnitudeSquared =
		validH0Count > 0
		? static_cast<float>(h0MagnitudeSquaredSum / validH0Count)
		: 0.0f;
	diagnostics.evolvedMagnitudeSquared =
		validEvolvedCount > 0
		? static_cast<float>(
			evolvedMagnitudeSquaredSum / validEvolvedCount)
		: 0.0f;
	if (validImaginaryCount > 0) {
		diagnostics.ifftImaginaryResidual = static_cast<float>(
			std::sqrt(imaginarySquaredSum / validImaginaryCount));
	}
	if (validHermitianCount > 0) {
		diagnostics.hermitianSymmetryError = static_cast<float>(
			std::sqrt(
				hermitianErrorSquaredSum /
				(std::max)(hermitianMagnitudeSquaredSum, 1.0e-30)));
	}
	diagnostics.directionalNormalizationError = maximumDirectionalError;
	diagnostics.expectedRms64 =
		ExpectedHeightRms(diagnosticsParameters_, 64);
	diagnostics.expectedRms128 =
		ExpectedHeightRms(diagnosticsParameters_, 128);
	diagnostics.expectedRms256 =
		ExpectedHeightRms(diagnosticsParameters_, 256);
	const float minimumExpected = (std::min)(
		diagnostics.expectedRms64,
		(std::min)(diagnostics.expectedRms128, diagnostics.expectedRms256));
	const float maximumExpected = (std::max)(
		diagnostics.expectedRms64,
		(std::max)(diagnostics.expectedRms128, diagnostics.expectedRms256));
	diagnostics.resolutionRelativeSpread =
		maximumExpected > 0.000001f
		? (maximumExpected - minimumExpected) / maximumExpected
		: 0.0f;
	diagnostics.jonswapAlpha =
		static_cast<float>(JonswapAlpha(diagnosticsParameters_));
	diagnostics.peakAngularFrequency = static_cast<float>(
		JonswapPeakAngularFrequency(diagnosticsParameters_));
	const SeedEnsembleResult seedEnsemble =
		EvaluateSeedEnsemble(diagnosticsParameters_, 32);
	diagnostics.seedRmsMean = seedEnsemble.mean;
	diagnostics.seedRmsStandardDeviation =
		seedEnsemble.standardDeviation;
	diagnostics.seedRmsMinimum = seedEnsemble.minimum;
	diagnostics.seedRmsMaximum = seedEnsemble.maximum;
	diagnostics.seedMeanRelativeError =
		seedEnsemble.meanRelativeError;
	diagnostics.seedMeanAbsoluteRelativeError =
		seedEnsemble.meanAbsoluteRelativeError;
	diagnostics.invalidValueCount +=
		seedEnsemble.invalidValueCount;
	diagnostics.valid = true;
	fftDiagnostics_ = diagnostics;

	const D3D12_RANGE emptyWriteRange = { 0, 0 };
	displacementReadback_.resource->Unmap(0, &emptyWriteRange);
	slopeReadback_.resource->Unmap(0, &emptyWriteRange);
	initialSpectrumReadback_.resource->Unmap(0, &emptyWriteRange);
	evolvedSpectrumReadback_.resource->Unmap(0, &emptyWriteRange);
	finalSpectrumReadback_.resource->Unmap(0, &emptyWriteRange);
	spectrumDebugReadback_.resource->Unmap(0, &emptyWriteRange);
	fftDiagnosticsPending_ = false;
}

void OceanRenderer::CopyFFTTextureToReadback(
	FFTTexture& source,
	FFTReadback& destination)
{
	Transition(source, D3D12_RESOURCE_STATE_COPY_SOURCE);
	D3D12_TEXTURE_COPY_LOCATION sourceLocation{};
	sourceLocation.pResource = source.resource.Get();
	sourceLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	sourceLocation.SubresourceIndex = 0;
	D3D12_TEXTURE_COPY_LOCATION destinationLocation{};
	destinationLocation.pResource = destination.resource.Get();
	destinationLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	destinationLocation.PlacedFootprint = destination.footprint;
	Object3dCommon::GetInstance()->GetDxCommon()->GetList()->CopyTextureRegion(
		&destinationLocation,
		0,
		0,
		0,
		&sourceLocation,
		nullptr);
	Transition(source, kShaderReadState);
}

void OceanRenderer::BindComputePipeline(ID3D12PipelineState* pipeline)
{
	auto commandList = Object3dCommon::GetInstance()->GetDxCommon()->GetList();
	commandList->SetComputeRootSignature(fftComputeRoot_.GetSignature().Get());
	commandList->SetPipelineState(pipeline);
	commandList->SetComputeRootConstantBufferView(
		0,
		fftParameterResource_->GetGPUVirtualAddress());
}

void OceanRenderer::Transition(
	FFTTexture& texture,
	D3D12_RESOURCE_STATES nextState)
{
	if (texture.state == nextState) {
		return;
	}

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = texture.resource.Get();
	barrier.Transition.StateBefore = texture.state;
	barrier.Transition.StateAfter = nextState;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	Object3dCommon::GetInstance()->GetDxCommon()->GetList()->ResourceBarrier(
		1,
		&barrier);
	texture.state = nextState;
}

void OceanRenderer::InsertUAVBarrier(FFTTexture& texture)
{
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	barrier.UAV.pResource = texture.resource.Get();
	Object3dCommon::GetInstance()->GetDxCommon()->GetList()->ResourceBarrier(
		1,
		&barrier);
}

void OceanRenderer::BindComputeSrv(
	uint32_t rootIndex,
	const FFTTexture& texture)
{
	auto* srvManager = Object3dCommon::GetInstance()->GetSrvManager();
	Object3dCommon::GetInstance()->GetDxCommon()->GetList()->
		SetComputeRootDescriptorTable(
			rootIndex,
			srvManager->GetGPUDescriptorHandle(texture.srvIndex));
}

void OceanRenderer::BindComputeUav(
	uint32_t rootIndex,
	const FFTTexture& texture)
{
	auto* srvManager = Object3dCommon::GetInstance()->GetSrvManager();
	Object3dCommon::GetInstance()->GetDxCommon()->GetList()->
		SetComputeRootDescriptorTable(
			rootIndex,
			srvManager->GetGPUDescriptorHandle(texture.uavIndex));
}
