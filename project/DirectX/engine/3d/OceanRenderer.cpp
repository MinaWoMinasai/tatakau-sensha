#include "OceanRenderer.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
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
constexpr float kLargeMediumBoundary =
	static_cast<float>(12.0 * kOceanPi / 16.0);
constexpr float kMediumSmallBoundary =
	static_cast<float>(12.0 * kOceanPi / 4.0);
constexpr float kSmallNyquist =
	static_cast<float>(1.4142135623730951 * kOceanPi * 128.0 / 4.0);
constexpr uint32_t kCascadeSeedStride = 1009;
constexpr float kProjectedGridAspect = 9.0f / 16.0f;
static_assert(sizeof(OceanRenderer::OceanFFTParameters) == 112);
static_assert(sizeof(OceanRenderer::OceanParameters) == 464);

struct SpectrumEvaluation {
	double radial = 0.0;
	double directional = 0.0;
	double bandWeight = 1.0;
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

double CascadeBandWeight(
	const OceanRenderer::OceanFFTParameters& parameters,
	double waveNumber)
{
	if (parameters.cascadeIndex >= OceanRenderer::kFFTCascadeCount) {
		return 1.0;
	}

	if (parameters.bandMode == static_cast<uint32_t>(
		OceanRenderer::CascadeBandMode::HardCutoff)) {
		if (parameters.cascadeIndex == 0) {
			return waveNumber < parameters.maximumWaveNumber ? 1.0 : 0.0;
		}
		if (parameters.cascadeIndex == 1) {
			return waveNumber >= parameters.minimumWaveNumber &&
				waveNumber < parameters.maximumWaveNumber
				? 1.0
				: 0.0;
		}
		const double radialNyquist =
			std::sqrt(2.0) * kOceanPi * parameters.fftSize /
			(std::max)(static_cast<double>(parameters.patchLength), 0.001);
		const bool usesFullNyquist =
			parameters.maximumWaveNumber >= radialNyquist - 0.0001;
		return waveNumber >= parameters.minimumWaveNumber &&
			(usesFullNyquist ||
				waveNumber < parameters.maximumWaveNumber)
			? 1.0
			: 0.0;
	}

	const double halfWidth =
		(std::max)(static_cast<double>(parameters.bandTransitionWidth), 0.0001) *
		0.5;
	auto smoothStep = [](double minimum, double maximum, double value) {
		const double t = (std::clamp)(
			(value - minimum) / (std::max)(maximum - minimum, 0.000001),
			0.0,
			1.0);
		return t * t * (3.0 - 2.0 * t);
	};
	const double lowerWeight = parameters.cascadeIndex == 0
		? 1.0
		: smoothStep(
			parameters.minimumWaveNumber - halfWidth,
			parameters.minimumWaveNumber + halfWidth,
			waveNumber);
	const double radialNyquist =
		std::sqrt(2.0) * kOceanPi * parameters.fftSize /
		(std::max)(static_cast<double>(parameters.patchLength), 0.001);
	const bool usesFullNyquist =
		parameters.cascadeIndex == 2 &&
		parameters.maximumWaveNumber >= radialNyquist - 0.0001;
	const double upperWeight = usesFullNyquist
		? 1.0
		: 1.0 - smoothStep(
			parameters.maximumWaveNumber - halfWidth,
			parameters.maximumWaveNumber + halfWidth,
			waveNumber);
	return (std::clamp)(lowerWeight * upperWeight, 0.0, 1.0);
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
		result.bandWeight = CascadeBandWeight(parameters, waveNumber);
		result.waveNumber *= result.bandWeight;
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
	result.bandWeight = CascadeBandWeight(parameters, waveNumber);
	result.waveNumber *= result.bandWeight;
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
	float meanVariance = 0.0f;
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
	double varianceSum = 0.0;
	double relativeErrorSum = 0.0;
	result.minimum = (std::numeric_limits<float>::max)();
	result.maximum = (std::numeric_limits<float>::lowest)();
	for (double rms : rmsValues) {
		sum += rms;
		varianceSum += rms * rms;
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
	result.meanVariance = static_cast<float>(
		varianceSum / rmsValues.size());
	return result;
}

struct BandPartitionMetrics {
	float overlapEnergy = 0.0f;
	float missingEnergy = 0.0f;
};

BandPartitionMetrics EvaluateBandPartition(
	const OceanRenderer::OceanFFTParameters& baseParameters,
	const std::array<
		OceanRenderer::OceanFFTParameters,
		OceanRenderer::kFFTCascadeCount>& cascadeParameters)
{
	constexpr uint32_t radialSamples = 192;
	constexpr uint32_t angularSamples = 64;
	const double minimumWaveNumber = 2.0 * kOceanPi / 256.0;
	const double maximumWaveNumber = kSmallNyquist;
	const double logMinimum = std::log(minimumWaveNumber);
	const double logMaximum = std::log(maximumWaveNumber);
	const double logStep =
		(logMaximum - logMinimum) / radialSamples;
	const double angleStep = 2.0 * kOceanPi / angularSamples;
	OceanRenderer::OceanFFTParameters unfiltered = baseParameters;
	unfiltered.cascadeIndex = OceanRenderer::kFFTCascadeCount;

	double overlapEnergy = 0.0;
	double missingEnergy = 0.0;
	for (uint32_t radialIndex = 0;
		radialIndex < radialSamples;
		++radialIndex) {
		const double waveNumber = std::exp(
			logMinimum + (static_cast<double>(radialIndex) + 0.5) * logStep);
		const double radialWidth = waveNumber * logStep;
		for (uint32_t angularIndex = 0;
			angularIndex < angularSamples;
			++angularIndex) {
			const double angle =
				-kOceanPi +
				(static_cast<double>(angularIndex) + 0.5) * angleStep;
			const SpectrumEvaluation spectrum = EvaluateSpectrum(
				unfiltered,
				waveNumber * std::cos(angle),
				waveNumber * std::sin(angle));
			double weightSum = 0.0;
			for (const auto& parameters : cascadeParameters) {
				weightSum += CascadeBandWeight(parameters, waveNumber);
			}
			const double energyElement =
				spectrum.waveNumber *
				waveNumber *
				radialWidth *
				angleStep;
			overlapEnergy +=
				(std::max)(weightSum - 1.0, 0.0) * energyElement;
			missingEnergy +=
				(std::max)(1.0 - weightSum, 0.0) * energyElement;
		}
	}

	return {
		static_cast<float>(overlapEnergy),
		static_cast<float>(missingEnergy),
	};
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
	CreateProjectedGridMesh(
		static_cast<uint32_t>(parameters_.projectedGridResolution.x));
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
	Matrix4x4 unjitteredViewProjection{};
	if (useDebugCamera) {
		parameters_.viewProjection = debugCamera.GetViewProjectionMatrix();
		unjitteredViewProjection =
			debugCamera.GetUnjitteredViewProjectionMatrix();
		parameters_.cameraPosition = debugCamera.GetEyePosition();
	} else {
		parameters_.viewProjection = camera.GetViewProjectionMatrix();
		unjitteredViewProjection =
			camera.GetUnjitteredViewProjectionMatrix();
		parameters_.cameraPosition = camera.GetTranslate();
	}
	parameters_.inverseViewProjection = Inverse(unjitteredViewProjection);

	// Only the finite grid follows the camera. Ocean.VS evaluates every wave
	// from the resulting world position, so this movement cannot drag the pattern.
	parameters_.gridOrigin.x = parameters_.cameraPosition.x;
	parameters_.gridOrigin.y = parameters_.baseHeight;
	parameters_.gridOrigin.z = parameters_.cameraPosition.z;
	Vector4 nearCenter = TransformMatrix(
		{ 0.0f, 0.0f, 0.0f, 1.0f },
		parameters_.inverseViewProjection);
	Vector4 farCenter = TransformMatrix(
		{ 0.0f, 0.0f, 1.0f, 1.0f },
		parameters_.inverseViewProjection);
	if (std::abs(nearCenter.w) > 1.0e-6f &&
		std::abs(farCenter.w) > 1.0e-6f) {
		nearCenter /= nearCenter.w;
		farCenter /= farCenter.w;
		const Vector2 horizontalForward = NormalizeDirection(
			{
				farCenter.x - nearCenter.x,
				farCenter.z - nearCenter.z,
			},
			{ 0.0f, 1.0f });
		const Vector4 horizonPoint = {
			parameters_.cameraPosition.x +
				horizontalForward.x * parameters_.projectedFarClamp,
			parameters_.baseHeight,
			parameters_.cameraPosition.z +
				horizontalForward.y * parameters_.projectedFarClamp,
			1.0f,
		};
		const Vector4 horizonClip = TransformMatrix(
			horizonPoint,
			unjitteredViewProjection);
		if (std::abs(horizonClip.w) > 1.0e-6f) {
			parameters_.projectedHorizonNdcY = (std::clamp)(
				horizonClip.y / horizonClip.w,
				-1.0f,
				1.0f);
		}
	}
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
	std::array<OceanCascade*, kFFTCascadeCount> renderCascades{};
	if (parameters_.waveSource >=
		static_cast<float>(WaveSource::FFTThreeCascades) - 0.5f) {
		for (uint32_t index = 0; index < kFFTCascadeCount; ++index) {
			renderCascades[index] = &cascades_[index];
		}
	} else {
		renderCascades.fill(&singleCascade_);
	}
	Object3dCommon::GetInstance()->GetSrvManager()->SetGraphicsRootDescriptorTable(
		2,
		renderCascades[0]->displacement.srvIndex);
	Object3dCommon::GetInstance()->GetSrvManager()->SetGraphicsRootDescriptorTable(
		3,
		renderCascades[0]->slope.srvIndex);
	Object3dCommon::GetInstance()->GetSrvManager()->SetGraphicsRootDescriptorTable(
		4,
		renderCascades[1]->displacement.srvIndex);
	Object3dCommon::GetInstance()->GetSrvManager()->SetGraphicsRootDescriptorTable(
		5,
		renderCascades[1]->slope.srvIndex);
	Object3dCommon::GetInstance()->GetSrvManager()->SetGraphicsRootDescriptorTable(
		6,
		renderCascades[2]->displacement.srvIndex);
	Object3dCommon::GetInstance()->GetSrvManager()->SetGraphicsRootDescriptorTable(
		7,
		renderCascades[2]->slope.srvIndex);
	const uint32_t selectedDebugCascade =
		parameters_.waveSource >=
			static_cast<float>(WaveSource::FFTThreeCascades) - 0.5f
		? (std::min)(debugCascadeIndex_, kFFTCascadeCount - 1)
		: 0;
	OceanCascade* debugCascade = renderCascades[selectedDebugCascade];
	Object3dCommon::GetInstance()->GetSrvManager()->SetGraphicsRootDescriptorTable(
		8,
		debugCascade->initialSpectrum.srvIndex);
	Object3dCommon::GetInstance()->GetSrvManager()->SetGraphicsRootDescriptorTable(
		9,
		debugCascade->evolvedSpectrumDebug.srvIndex);
	Object3dCommon::GetInstance()->GetSrvManager()->SetGraphicsRootDescriptorTable(
		10,
		debugCascade->spectrumDebug.srvIndex);
	Object3dCommon::GetInstance()->GetSrvManager()->SetGraphicsRootDescriptorTable(
		11,
		renderCascades[0]->derivative.srvIndex);
	Object3dCommon::GetInstance()->GetSrvManager()->SetGraphicsRootDescriptorTable(
		12,
		renderCascades[1]->derivative.srvIndex);
	Object3dCommon::GetInstance()->GetSrvManager()->SetGraphicsRootDescriptorTable(
		13,
		renderCascades[2]->derivative.srvIndex);
	const bool measureOceanDraw = oceanDrawTimingPending_;
	if (measureOceanDraw) {
		BeginCascadeGpuTiming(fftTimingQueryCountBeforeDraw_);
	}
	if (parameters_.meshMode >=
			static_cast<float>(MeshMode::ProjectedGrid) - 0.5f &&
		projectedGridIndexCount_ > 0) {
		DrawProjectedGrid();
	} else {
		gridModel_->DrawOnlyMesh();
	}
	if (measureOceanDraw) {
		EndCascadeGpuTiming(fftTimingQueryCountBeforeDraw_ + 1);
		QueueGpuTimingReadback(fftTimingQueryCountBeforeDraw_ + 2);
		oceanDrawTimingPending_ = false;
	}
}

void OceanRenderer::SetMode(Mode mode)
{
	parameters_.mode = static_cast<float>(mode);
}

void OceanRenderer::SetWaveSource(WaveSource source)
{
	parameters_.waveSource = static_cast<float>(source);
	ApplyBaseParametersToCascades();
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
		MarkAllInitialSpectraDirty();
	}
	if (std::abs(fftParameters_.choppiness - normalizedChoppiness) > 0.0001f) {
		MarkAllOutputsDirty();
	}

	parameters_.windDirection = normalizedDirection;
	parameters_.windSpeed = normalizedSpeed;
	parameters_.choppiness = normalizedChoppiness;
	fftParameters_.windDirection = normalizedDirection;
	fftParameters_.windSpeed = normalizedSpeed;
	fftParameters_.choppiness = normalizedChoppiness;
	ApplyBaseParametersToCascades();
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
		MarkAllInitialSpectraDirty();
	}
	if (std::abs(fftParameters_.time - time) > 0.0001f) {
		MarkAllOutputsDirty();
	}

	fftParameters_.time = time;
	fftParameters_.amplitude = clampedAmplitude;
	fftParameters_.patchLength = clampedPatchLength;
	fftParameters_.seed = seed;
	fftPaused_ = paused;
	parameters_.fftPatchLength = clampedPatchLength;
	parameters_.fftDebugMode =
		static_cast<float>((std::clamp)(debugMode, 0, 19));
	parameters_.fftDebugScale = (std::max)(debugDisplayScale, 0.001f);
	ApplyBaseParametersToCascades();
}

void OceanRenderer::SetDerivativeSettings(
	int normalMode,
	bool breakingPreviewEnabled,
	float jacobianThreshold,
	float jacobianBias,
	float smoothWidth,
	float maskIntensity)
{
	parameters_.breakingParameters = {
		(std::clamp)(jacobianThreshold, -2.0f, 2.0f),
		(std::clamp)(jacobianBias, -2.0f, 2.0f),
		(std::max)(smoothWidth, 0.0001f),
		(std::clamp)(maskIntensity, 0.0f, 2.0f),
	};
	parameters_.derivativeControls = {
		static_cast<float>((std::clamp)(normalMode, 0, 1)),
		breakingPreviewEnabled ? 1.0f : 0.0f,
		0.0f,
		0.0f,
	};
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
		MarkAllInitialSpectraDirty();
		MarkAllOutputsDirty();
	}

	fftParameters_.spectrumModel = spectrumModel;
	fftParameters_.fetch = clampedFetch;
	fftParameters_.gamma = clampedGamma;
	fftParameters_.lowFrequencyDamping = clampedLowDamping;
	fftParameters_.highFrequencyDamping = clampedHighDamping;
	fftParameters_.swellDirection = normalizedSwellDirection;
	fftParameters_.swellAmount = clampedSwellAmount;
	fftParameters_.oppositeWaveSuppression = clampedOppositeSuppression;
	ApplyBaseParametersToCascades();
}

void OceanRenderer::SetFFTCascadeSettings(
	CascadeBandMode bandMode,
	float transitionWidth,
	const std::array<OceanCascadeSettings, kFFTCascadeCount>& settings,
	int displayMode,
	int debugCascadeIndex)
{
	const float clampedTransitionWidth =
		(std::clamp)(transitionWidth, 0.001f, 8.0f);
	bool spectrumChanged =
		cascadeBandMode_ != bandMode ||
		std::abs(cascadeTransitionWidth_ - clampedTransitionWidth) > 0.0001f;
	cascadeBandMode_ = bandMode;
	cascadeTransitionWidth_ = clampedTransitionWidth;

	for (uint32_t index = 0; index < kFFTCascadeCount; ++index) {
		OceanCascadeSettings normalized = settings[index];
		normalized.patchLength =
			(std::clamp)(normalized.patchLength, 1.0f, 2048.0f);
		normalized.minimumWaveNumber =
			(std::max)(normalized.minimumWaveNumber, 0.0f);
		normalized.maximumWaveNumber = (std::max)(
			normalized.maximumWaveNumber,
			normalized.minimumWaveNumber + 0.0001f);
		normalized.displacementContribution =
			(std::clamp)(normalized.displacementContribution, 0.0f, 4.0f);
		normalized.slopeContribution =
			(std::clamp)(normalized.slopeContribution, 0.0f, 4.0f);
		const OceanCascadeSettings& previous = cascadeSettings_[index];
		if (std::abs(previous.patchLength - normalized.patchLength) > 0.0001f ||
			std::abs(
				previous.minimumWaveNumber -
				normalized.minimumWaveNumber) > 0.0001f ||
			std::abs(
				previous.maximumWaveNumber -
				normalized.maximumWaveNumber) > 0.0001f) {
			spectrumChanged = true;
		}
		cascadeSettings_[index] = normalized;
	}
	if (spectrumChanged) {
		for (auto& cascade : cascades_) {
			cascade.initialSpectrumDirty = true;
			cascade.outputDirty = true;
		}
	}

	cascadeDisplayMode_ =
		static_cast<uint32_t>((std::clamp)(displayMode, 0, 3));
	debugCascadeIndex_ =
		static_cast<uint32_t>((std::clamp)(debugCascadeIndex, 0, 2));
	ApplyBaseParametersToCascades();
}

void OceanRenderer::SetMeshSettings(
	MeshMode mode,
	const ProjectedGridSettings& settings)
{
	const uint32_t normalizedResolution =
		(std::clamp)(settings.horizontalResolution, 32u, 512u);
	if (normalizedResolution != projectedGridHorizontalResolution_) {
		CreateProjectedGridMesh(normalizedResolution);
	}
	parameters_.meshMode = static_cast<float>(mode);
	parameters_.projectedNearClamp = (std::max)(settings.nearClamp, 0.0f);
	parameters_.projectedFarClamp = (std::max)(
		settings.farClamp,
		parameters_.projectedNearClamp + 1.0f);
	parameters_.projectedGridDebug =
		static_cast<float>(settings.debugMode);
	parameters_.projectedWireframe = settings.wireframe ? 1.0f : 0.0f;
	parameters_.projectedOverscan = {
		(std::max)(settings.overscanX, 1.0f),
		(std::max)(settings.overscanTop, 1.0f),
		(std::max)(settings.overscanBottom, 1.0f),
		0.003f,
	};
	parameters_.projectedDisplacementGuard = {
		settings.nearDisplacementFadeEnabled ? 1.0f : 0.0f,
		(std::max)(settings.nearFadeWidth, 0.001f),
		(std::max)(settings.minimumSafeNearDistance, 0.0f),
		settings.outerWireframe ? 1.0f : 0.0f,
	};
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

	const OceanCascadeSettings singleSettings = {
		true,
		256.0f,
		0.0f,
		1000000.0f,
		1.0f,
		1.0f,
	};
	cascadeSettings_[0] = {
		true,
		256.0f,
		0.0f,
		kLargeMediumBoundary,
		1.0f,
		1.0f,
	};
	cascadeSettings_[1] = {
		true,
		16.0f,
		kLargeMediumBoundary,
		kMediumSmallBoundary,
		1.0f,
		1.0f,
	};
	cascadeSettings_[2] = {
		true,
		4.0f,
		kMediumSmallBoundary,
		kSmallNyquist,
		0.0f,
		1.0f,
	};
	InitializeCascade(
		singleCascade_,
		singleSettings,
		kFFTCascadeCount,
		fftParameters_.seed);
	for (uint32_t index = 0; index < kFFTCascadeCount; ++index) {
		InitializeCascade(
			cascades_[index],
			cascadeSettings_[index],
			index,
			fftParameters_.seed + index * kCascadeSeedStride);
	}
	ApplyBaseParametersToCascades();
	InitializeGpuTiming();
}

void OceanRenderer::InitializeCascade(
	OceanCascade& cascade,
	const OceanCascadeSettings& settings,
	uint32_t cascadeIndex,
	uint32_t seed)
{
	DirectXCommon* dxCommon = Object3dCommon::GetInstance()->GetDxCommon();
	cascade.settings = settings;
	cascade.parameters = fftParameters_;
	cascade.parameters.patchLength = settings.patchLength;
	cascade.parameters.minimumWaveNumber = settings.minimumWaveNumber;
	cascade.parameters.maximumWaveNumber = settings.maximumWaveNumber;
	cascade.parameters.bandTransitionWidth = cascadeTransitionWidth_;
	cascade.parameters.bandMode =
		static_cast<uint32_t>(cascadeBandMode_);
	cascade.parameters.cascadeIndex = cascadeIndex;
	cascade.parameters.seed = seed;
	cascade.parameterResource = dxCommon->CreateBufferResource(
		AlignConstantBufferSize(sizeof(OceanFFTParameters)));
	cascade.parameterResource->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&cascade.parameterData));
	*cascade.parameterData = cascade.parameters;

	CreateFFTTexture(
		cascade.initialSpectrum,
		DXGI_FORMAT_R32G32B32A32_FLOAT);
	CreateFFTTexture(
		cascade.spectrumDebug,
		DXGI_FORMAT_R32G32B32A32_FLOAT);
	CreateFFTTexture(
		cascade.evolvedSpectrumDebug,
		DXGI_FORMAT_R32G32B32A32_FLOAT);
	for (uint32_t index = 0; index < 2; ++index) {
		CreateFFTTexture(
			cascade.spectrumA[index],
			DXGI_FORMAT_R32G32B32A32_FLOAT);
		CreateFFTTexture(
			cascade.spectrumB[index],
			DXGI_FORMAT_R32G32B32A32_FLOAT);
		CreateFFTTexture(
			cascade.spectrumC[index],
			DXGI_FORMAT_R32G32B32A32_FLOAT);
		CreateFFTTexture(
			cascade.spectrumD[index],
			DXGI_FORMAT_R32G32B32A32_FLOAT);
	}
	CreateFFTTexture(
		cascade.displacement,
		DXGI_FORMAT_R16G16B16A16_FLOAT);
	CreateFFTTexture(cascade.slope, DXGI_FORMAT_R16G16_FLOAT);
	CreateFFTTexture(
		cascade.derivative,
		DXGI_FORMAT_R16G16B16A16_FLOAT);
	CreateFFTReadback(
		cascade.displacementReadback,
		cascade.displacement);
	CreateFFTReadback(cascade.slopeReadback, cascade.slope);
	CreateFFTReadback(cascade.derivativeReadback, cascade.derivative);
	CreateFFTReadback(
		cascade.initialSpectrumReadback,
		cascade.initialSpectrum);
	CreateFFTReadback(
		cascade.evolvedSpectrumReadback,
		cascade.evolvedSpectrumDebug);
	CreateFFTReadback(
		cascade.finalSpectrumReadback,
		cascade.spectrumA[0]);
	CreateFFTReadback(
		cascade.finalSpectrumCReadback,
		cascade.spectrumC[0]);
	CreateFFTReadback(
		cascade.finalSpectrumDReadback,
		cascade.spectrumD[0]);
	CreateFFTReadback(
		cascade.spectrumDebugReadback,
		cascade.spectrumDebug);
}

void OceanRenderer::ApplyBaseParametersToCascades()
{
	auto applyBase = [this](
		OceanCascade& cascade,
		const OceanCascadeSettings& settings,
		uint32_t cascadeIndex,
		uint32_t seed) {
		cascade.settings = settings;
		cascade.parameters.fftSize = fftParameters_.fftSize;
		cascade.parameters.time = fftParameters_.time;
		cascade.parameters.amplitude = fftParameters_.amplitude;
		cascade.parameters.windDirection = fftParameters_.windDirection;
		cascade.parameters.windSpeed = fftParameters_.windSpeed;
		cascade.parameters.choppiness = fftParameters_.choppiness;
		cascade.parameters.seed = seed;
		cascade.parameters.spectrumModel = fftParameters_.spectrumModel;
		cascade.parameters.fetch = fftParameters_.fetch;
		cascade.parameters.gamma = fftParameters_.gamma;
		cascade.parameters.lowFrequencyDamping =
			fftParameters_.lowFrequencyDamping;
		cascade.parameters.highFrequencyDamping =
			fftParameters_.highFrequencyDamping;
		cascade.parameters.swellDirection =
			fftParameters_.swellDirection;
		cascade.parameters.swellAmount = fftParameters_.swellAmount;
		cascade.parameters.oppositeWaveSuppression =
			fftParameters_.oppositeWaveSuppression;
		cascade.parameters.patchLength = settings.patchLength;
		cascade.parameters.minimumWaveNumber =
			settings.minimumWaveNumber;
		cascade.parameters.maximumWaveNumber =
			settings.maximumWaveNumber;
		cascade.parameters.bandTransitionWidth =
			cascadeTransitionWidth_;
		cascade.parameters.bandMode =
			static_cast<uint32_t>(cascadeBandMode_);
		cascade.parameters.cascadeIndex = cascadeIndex;
	};

	OceanCascadeSettings singleSettings = singleCascade_.settings;
	singleSettings.patchLength = fftParameters_.patchLength;
	applyBase(
		singleCascade_,
		singleSettings,
		kFFTCascadeCount,
		fftParameters_.seed);
	for (uint32_t index = 0; index < kFFTCascadeCount; ++index) {
		applyBase(
			cascades_[index],
			cascadeSettings_[index],
			index,
			fftParameters_.seed + index * kCascadeSeedStride);
	}

	const bool useThree =
		parameters_.waveSource >=
		static_cast<float>(WaveSource::FFTThreeCascades) - 0.5f;
	if (useThree) {
		parameters_.cascadePatchLengths = {
			cascadeSettings_[0].patchLength,
			cascadeSettings_[1].patchLength,
			cascadeSettings_[2].patchLength,
			0.0f,
		};
		parameters_.cascadeDisplacementContributions = {
			cascadeSettings_[0].displacementContribution,
			cascadeSettings_[1].displacementContribution,
			cascadeSettings_[2].displacementContribution,
			0.0f,
		};
		parameters_.cascadeSlopeContributions = {
			cascadeSettings_[0].slopeContribution,
			cascadeSettings_[1].slopeContribution,
			cascadeSettings_[2].slopeContribution,
			0.0f,
		};
		parameters_.cascadeEnabled = {
			cascadeSettings_[0].enabled ? 1.0f : 0.0f,
			cascadeSettings_[1].enabled ? 1.0f : 0.0f,
			cascadeSettings_[2].enabled ? 1.0f : 0.0f,
			0.0f,
		};
		parameters_.cascadeDisplayMode =
			static_cast<float>(cascadeDisplayMode_);
		parameters_.fftDebugCascade =
			static_cast<float>(debugCascadeIndex_);
		parameters_.fftDebugPatchLength =
			cascadeSettings_[debugCascadeIndex_].patchLength;
	} else {
		parameters_.cascadePatchLengths = {
			fftParameters_.patchLength,
			fftParameters_.patchLength,
			fftParameters_.patchLength,
			0.0f,
		};
		parameters_.cascadeDisplacementContributions = {
			1.0f,
			0.0f,
			0.0f,
			0.0f,
		};
		parameters_.cascadeSlopeContributions = {
			1.0f,
			0.0f,
			0.0f,
			0.0f,
		};
		parameters_.cascadeEnabled = { 1.0f, 0.0f, 0.0f, 0.0f };
		parameters_.cascadeDisplayMode = 0.0f;
		parameters_.fftDebugCascade = 0.0f;
		parameters_.fftDebugPatchLength = fftParameters_.patchLength;
	}
}

void OceanRenderer::MarkAllInitialSpectraDirty()
{
	singleCascade_.initialSpectrumDirty = true;
	singleCascade_.outputDirty = true;
	for (auto& cascade : cascades_) {
		cascade.initialSpectrumDirty = true;
		cascade.outputDirty = true;
	}
}

void OceanRenderer::MarkAllOutputsDirty()
{
	singleCascade_.outputDirty = true;
	for (auto& cascade : cascades_) {
		cascade.outputDirty = true;
	}
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

void OceanRenderer::InitializeGpuTiming()
{
	DirectXCommon* dxCommon = Object3dCommon::GetInstance()->GetDxCommon();
	D3D12_QUERY_HEAP_DESC queryDescription{};
	queryDescription.Count = kFFTCascadeCount * 2 + 2;
	queryDescription.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	HRESULT hr = dxCommon->GetDevice()->CreateQueryHeap(
		&queryDescription,
		IID_PPV_ARGS(&fftTimestampQueryHeap_));
	assert(SUCCEEDED(hr));

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_READBACK;
	D3D12_RESOURCE_DESC bufferDescription{};
	bufferDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufferDescription.Width =
		sizeof(uint64_t) * (kFFTCascadeCount * 2 + 2);
	bufferDescription.Height = 1;
	bufferDescription.DepthOrArraySize = 1;
	bufferDescription.MipLevels = 1;
	bufferDescription.SampleDesc.Count = 1;
	bufferDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	hr = dxCommon->GetDevice()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&bufferDescription,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&fftTimestampReadback_));
	assert(SUCCEEDED(hr));

	hr = dxCommon->GetQueue()->GetTimestampFrequency(
		&fftTimestampFrequency_);
	assert(SUCCEEDED(hr));
}

void OceanRenderer::BeginCascadeGpuTiming(uint32_t queryIndex)
{
	Object3dCommon::GetInstance()->GetDxCommon()->GetList()->EndQuery(
		fftTimestampQueryHeap_.Get(),
		D3D12_QUERY_TYPE_TIMESTAMP,
		queryIndex);
}

void OceanRenderer::EndCascadeGpuTiming(uint32_t queryIndex)
{
	Object3dCommon::GetInstance()->GetDxCommon()->GetList()->EndQuery(
		fftTimestampQueryHeap_.Get(),
		D3D12_QUERY_TYPE_TIMESTAMP,
		queryIndex);
}

void OceanRenderer::QueueGpuTimingReadback(uint32_t queryCount)
{
	fftTimestampQueryCount_ = queryCount;
	Object3dCommon::GetInstance()->GetDxCommon()->GetList()->
		ResolveQueryData(
			fftTimestampQueryHeap_.Get(),
			D3D12_QUERY_TYPE_TIMESTAMP,
			0,
			queryCount,
			fftTimestampReadback_.Get(),
			0);
}

void OceanRenderer::ResolveGpuTimings()
{
	pendingGpuTimes_.fill(0.0f);
	pendingOceanDrawGpuTime_ = 0.0f;
	if (fftTimestampQueryCount_ < 2 ||
		fftTimestampFrequency_ == 0) {
		return;
	}

	const D3D12_RANGE readRange = {
		0,
		static_cast<SIZE_T>(
			sizeof(uint64_t) * fftTimestampQueryCount_),
	};
	void* mappedData = nullptr;
	const HRESULT hr = fftTimestampReadback_->Map(
		0,
		&readRange,
		&mappedData);
	assert(SUCCEEDED(hr));
	const auto* timestamps =
		static_cast<const uint64_t*>(mappedData);
	const uint32_t timingCount = (std::min)(
		(fftTimestampQueryCount_ - 2) / 2,
		kFFTCascadeCount);
	for (uint32_t index = 0; index < timingCount; ++index) {
		const uint64_t start = timestamps[index * 2];
		const uint64_t end = timestamps[index * 2 + 1];
		if (end >= start) {
			pendingGpuTimes_[index] = static_cast<float>(
				static_cast<double>(end - start) * 1000.0 /
				static_cast<double>(fftTimestampFrequency_));
		}
		if (diagnosticsThreeCascades_ &&
			!cascadeSettings_[index].enabled) {
			pendingGpuTimes_[index] = 0.0f;
		}
	}
	const uint64_t drawStart = timestamps[timingCount * 2];
	const uint64_t drawEnd = timestamps[timingCount * 2 + 1];
	if (drawEnd >= drawStart) {
		pendingOceanDrawGpuTime_ = static_cast<float>(
			static_cast<double>(drawEnd - drawStart) * 1000.0 /
			static_cast<double>(fftTimestampFrequency_));
	}
	const D3D12_RANGE emptyWriteRange = { 0, 0 };
	fftTimestampReadback_->Unmap(0, &emptyWriteRange);
}

void OceanRenderer::RunFFT()
{
	const bool useThree =
		parameters_.waveSource >=
		static_cast<float>(WaveSource::FFTThreeCascades) - 0.5f;
	auto cascadeNeedsWork = [](const OceanCascade& cascade) {
		return !cascade.hasOutput ||
			cascade.initialSpectrumDirty ||
			cascade.outputDirty;
	};
	bool needsWork = useThree
		? false
		: cascadeNeedsWork(singleCascade_);
	if (useThree) {
		for (uint32_t index = 0; index < kFFTCascadeCount; ++index) {
			needsWork = needsWork ||
				!cascades_[index].hasOutput ||
				(cascadeSettings_[index].enabled &&
					cascadeNeedsWork(cascades_[index]));
		}
	}
	if (fftPaused_ && !needsWork && !fftDiagnosticsRequested_) {
		return;
	}

	diagnosticsThreeCascades_ = useThree;
	if (useThree) {
		for (uint32_t index = 0; index < kFFTCascadeCount; ++index) {
			if (fftDiagnosticsRequested_) {
				BeginCascadeGpuTiming(index * 2);
			}
			if (cascadeSettings_[index].enabled ||
				!cascades_[index].hasOutput) {
				RunCascadeFFT(cascades_[index]);
			}
			if (fftDiagnosticsRequested_) {
				EndCascadeGpuTiming(index * 2 + 1);
			}
		}
		if (fftDiagnosticsRequested_) {
			for (uint32_t index = 0; index < kFFTCascadeCount; ++index) {
				if (cascadeSettings_[index].enabled) {
					QueueFFTDiagnosticsReadback(cascades_[index]);
				}
			}
			fftTimingQueryCountBeforeDraw_ = kFFTCascadeCount * 2;
			oceanDrawTimingPending_ = true;
		}
	} else {
		if (fftDiagnosticsRequested_) {
			BeginCascadeGpuTiming(0);
		}
		RunCascadeFFT(singleCascade_);
		if (fftDiagnosticsRequested_) {
			EndCascadeGpuTiming(1);
			QueueFFTDiagnosticsReadback(singleCascade_);
			fftTimingQueryCountBeforeDraw_ = 2;
			oceanDrawTimingPending_ = true;
		}
	}

	if (fftDiagnosticsRequested_) {
		fftDiagnosticsRequested_ = false;
		fftDiagnosticsPending_ = true;
	}
}

void OceanRenderer::RunCascadeFFT(OceanCascade& cascade)
{
	*cascade.parameterData = cascade.parameters;
	auto commandList = Object3dCommon::GetInstance()->GetDxCommon()->GetList();
	Object3dCommon::GetInstance()->GetSrvManager()->PreDraw();
	constexpr uint32_t groupCount = kFFTSize / 16;
	const uint32_t zeroConstants[4] = {};

	if (cascade.initialSpectrumDirty) {
		Transition(
			cascade.initialSpectrum,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		Transition(
			cascade.spectrumDebug,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		BindComputePipeline(
			spectrumInitializePipeline_.Get(),
			cascade);
		commandList->SetComputeRoot32BitConstants(1, 4, zeroConstants, 0);
		BindComputeUav(6, cascade.initialSpectrum);
		BindComputeUav(7, cascade.spectrumDebug);
		commandList->Dispatch(groupCount, groupCount, 1);
		InsertUAVBarrier(cascade.initialSpectrum);
		InsertUAVBarrier(cascade.spectrumDebug);
		Transition(cascade.initialSpectrum, kShaderReadState);
		Transition(cascade.spectrumDebug, kShaderReadState);
		cascade.initialSpectrumDirty = false;
	}

	Transition(cascade.spectrumA[0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Transition(cascade.spectrumB[0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Transition(cascade.spectrumC[0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Transition(cascade.spectrumD[0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Transition(
		cascade.evolvedSpectrumDebug,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	BindComputePipeline(spectrumEvolvePipeline_.Get(), cascade);
	commandList->SetComputeRoot32BitConstants(1, 4, zeroConstants, 0);
	BindComputeSrv(2, cascade.initialSpectrum);
	BindComputeUav(6, cascade.spectrumA[0]);
	BindComputeUav(7, cascade.spectrumB[0]);
	BindComputeUav(8, cascade.spectrumC[0]);
	BindComputeUav(9, cascade.spectrumD[0]);
	BindComputeUav(10, cascade.evolvedSpectrumDebug);
	commandList->Dispatch(groupCount, groupCount, 1);
	InsertUAVBarrier(cascade.spectrumA[0]);
	InsertUAVBarrier(cascade.spectrumB[0]);
	InsertUAVBarrier(cascade.spectrumC[0]);
	InsertUAVBarrier(cascade.spectrumD[0]);
	InsertUAVBarrier(cascade.evolvedSpectrumDebug);
	Transition(cascade.spectrumA[0], kShaderReadState);
	Transition(cascade.spectrumB[0], kShaderReadState);
	Transition(cascade.spectrumC[0], kShaderReadState);
	Transition(cascade.spectrumD[0], kShaderReadState);
	Transition(cascade.evolvedSpectrumDebug, kShaderReadState);

	uint32_t currentIndex = 0;
	for (uint32_t direction = 0; direction < 2; ++direction) {
		for (uint32_t stage = 0; stage < kFFTLog2; ++stage) {
			const uint32_t destinationIndex = 1u - currentIndex;
			Transition(
				cascade.spectrumA[destinationIndex],
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			Transition(
				cascade.spectrumB[destinationIndex],
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			Transition(
				cascade.spectrumC[destinationIndex],
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			Transition(
				cascade.spectrumD[destinationIndex],
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			BindComputePipeline(fftPipeline_.Get(), cascade);
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
			BindComputeSrv(2, cascade.spectrumA[currentIndex]);
			BindComputeSrv(3, cascade.spectrumB[currentIndex]);
			BindComputeSrv(4, cascade.spectrumC[currentIndex]);
			BindComputeSrv(5, cascade.spectrumD[currentIndex]);
			BindComputeUav(6, cascade.spectrumA[destinationIndex]);
			BindComputeUav(7, cascade.spectrumB[destinationIndex]);
			BindComputeUav(8, cascade.spectrumC[destinationIndex]);
			BindComputeUav(9, cascade.spectrumD[destinationIndex]);
			commandList->Dispatch(groupCount, groupCount, 1);
			InsertUAVBarrier(cascade.spectrumA[destinationIndex]);
			InsertUAVBarrier(cascade.spectrumB[destinationIndex]);
			InsertUAVBarrier(cascade.spectrumC[destinationIndex]);
			InsertUAVBarrier(cascade.spectrumD[destinationIndex]);
			Transition(cascade.spectrumA[destinationIndex], kShaderReadState);
			Transition(cascade.spectrumB[destinationIndex], kShaderReadState);
			Transition(cascade.spectrumC[destinationIndex], kShaderReadState);
			Transition(cascade.spectrumD[destinationIndex], kShaderReadState);
			currentIndex = destinationIndex;
		}
	}
	cascade.finalSpectrumIndex = currentIndex;

	Transition(cascade.displacement, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Transition(cascade.slope, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Transition(cascade.derivative, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	BindComputePipeline(fftOutputPipeline_.Get(), cascade);
	commandList->SetComputeRoot32BitConstants(1, 4, zeroConstants, 0);
	BindComputeSrv(2, cascade.spectrumA[cascade.finalSpectrumIndex]);
	BindComputeSrv(3, cascade.spectrumB[cascade.finalSpectrumIndex]);
	BindComputeSrv(4, cascade.spectrumC[cascade.finalSpectrumIndex]);
	BindComputeSrv(5, cascade.spectrumD[cascade.finalSpectrumIndex]);
	BindComputeUav(6, cascade.displacement);
	BindComputeUav(7, cascade.slope);
	BindComputeUav(8, cascade.derivative);
	commandList->Dispatch(groupCount, groupCount, 1);
	InsertUAVBarrier(cascade.displacement);
	InsertUAVBarrier(cascade.slope);
	InsertUAVBarrier(cascade.derivative);
	Transition(cascade.displacement, kShaderReadState);
	Transition(cascade.slope, kShaderReadState);
	Transition(cascade.derivative, kShaderReadState);
	cascade.hasOutput = true;
	cascade.outputDirty = false;
}

void OceanRenderer::QueueFFTDiagnosticsReadback(OceanCascade& cascade)
{
	CopyFFTTextureToReadback(
		cascade.displacement,
		cascade.displacementReadback);
	CopyFFTTextureToReadback(cascade.slope, cascade.slopeReadback);
	CopyFFTTextureToReadback(cascade.derivative, cascade.derivativeReadback);
	CopyFFTTextureToReadback(
		cascade.initialSpectrum,
		cascade.initialSpectrumReadback);
	CopyFFTTextureToReadback(
		cascade.evolvedSpectrumDebug,
		cascade.evolvedSpectrumReadback);
	CopyFFTTextureToReadback(
		cascade.spectrumA[cascade.finalSpectrumIndex],
		cascade.finalSpectrumReadback);
	CopyFFTTextureToReadback(
		cascade.spectrumC[cascade.finalSpectrumIndex],
		cascade.finalSpectrumCReadback);
	CopyFFTTextureToReadback(
		cascade.spectrumD[cascade.finalSpectrumIndex],
		cascade.finalSpectrumDReadback);
	CopyFFTTextureToReadback(
		cascade.spectrumDebug,
		cascade.spectrumDebugReadback);
	cascade.diagnosticsParameters = cascade.parameters;
}

void OceanRenderer::ResolveFFTDiagnostics()
{
	if (!fftDiagnosticsPending_) {
		return;
	}

	ResolveGpuTimings();
	OceanFFTDiagnostics diagnostics{};
	diagnostics.threeCascades = diagnosticsThreeCascades_;
	if (!diagnosticsThreeCascades_) {
		OceanCascadeDiagnostics singleDiagnostics =
			ResolveCascadeDiagnostics(singleCascade_);
		singleDiagnostics.gpuTimeMilliseconds = pendingGpuTimes_[0];
		singleCascade_.diagnostics = singleDiagnostics;
		static_cast<OceanCascadeDiagnostics&>(diagnostics) =
			singleDiagnostics;
		diagnostics.cascades[0] = singleDiagnostics;
		diagnostics.singleReferenceTargetVariance =
			singleDiagnostics.targetSpectrumVariance;
		diagnostics.combinedTargetVariance =
			singleDiagnostics.targetSpectrumVariance;
		diagnostics.combinedGpuVariance =
			singleDiagnostics.rgba16fVariance;
		diagnostics.combinedHeightRms =
			singleDiagnostics.heightRms;
		diagnostics.combinedSignificantWaveHeight =
			singleDiagnostics.significantWaveHeight;
		diagnostics.combinedSeedMeanVariance =
			singleDiagnostics.seedMeanVariance;
		diagnostics.totalGpuTimeMilliseconds =
			singleDiagnostics.gpuTimeMilliseconds;
	} else {
		std::array<OceanFFTParameters, kFFTCascadeCount>
			diagnosticParameters{};
		for (uint32_t index = 0; index < kFFTCascadeCount; ++index) {
			if (cascadeSettings_[index].enabled) {
				OceanCascadeDiagnostics cascadeDiagnostics =
					ResolveCascadeDiagnostics(cascades_[index]);
				cascadeDiagnostics.gpuTimeMilliseconds =
					pendingGpuTimes_[index];
				cascades_[index].diagnostics = cascadeDiagnostics;
				diagnostics.cascades[index] = cascadeDiagnostics;
				diagnostics.combinedTargetVariance +=
					cascadeDiagnostics.targetSpectrumVariance;
				diagnostics.combinedGpuVariance +=
					cascadeDiagnostics.rgba16fVariance;
				diagnostics.combinedSeedMeanVariance +=
					cascadeDiagnostics.seedMeanVariance;
				diagnostics.totalGpuTimeMilliseconds +=
					cascadeDiagnostics.gpuTimeMilliseconds;
				diagnostics.invalidValueCount +=
					cascadeDiagnostics.invalidValueCount;
				diagnostics.hermitianSymmetryError = (std::max)(
					diagnostics.hermitianSymmetryError,
					cascadeDiagnostics.hermitianSymmetryError);
				diagnostics.ifftImaginaryResidual = (std::max)(
					diagnostics.ifftImaginaryResidual,
					cascadeDiagnostics.ifftImaginaryResidual);
				diagnostics.directionalNormalizationError = (std::max)(
					diagnostics.directionalNormalizationError,
					cascadeDiagnostics.directionalNormalizationError);
				diagnostics.crossDerivativeSymmetryError = (std::max)(
					diagnostics.crossDerivativeSymmetryError,
					cascadeDiagnostics.crossDerivativeSymmetryError);
				diagnostics.derivativeFiniteDifferenceRelativeError = (std::max)(
					diagnostics.derivativeFiniteDifferenceRelativeError,
					cascadeDiagnostics.derivativeFiniteDifferenceRelativeError);
				diagnostics.derivativeQuantizationError = (std::max)(
					diagnostics.derivativeQuantizationError,
					cascadeDiagnostics.derivativeQuantizationError);
			}
			diagnosticParameters[index] =
				cascades_[index].parameters;
		}

		auto sampleDerivative = [](
			const OceanCascade& cascade,
			float worldX,
			float worldZ) {
			const float patchLength =
				(std::max)(cascade.settings.patchLength, 0.001f);
			auto wrapUnit = [](float value) {
				return value - std::floor(value);
			};
			const float textureX =
				wrapUnit(worldX / patchLength) * kFFTSize - 0.5f;
			const float textureY =
				wrapUnit(worldZ / patchLength) * kFFTSize - 0.5f;
			const int32_t x0 = static_cast<int32_t>(std::floor(textureX));
			const int32_t y0 = static_cast<int32_t>(std::floor(textureY));
			const float blendX = textureX - std::floor(textureX);
			const float blendY = textureY - std::floor(textureY);
			auto wrapIndex = [](int32_t value) {
				const int32_t size = static_cast<int32_t>(kFFTSize);
				return static_cast<uint32_t>((value % size + size) % size);
			};
			auto sample = [&](int32_t x, int32_t y) -> const Vector4& {
				return cascade.diagnosticDerivativeSamples[
					static_cast<size_t>(wrapIndex(y)) * kFFTSize +
					wrapIndex(x)];
			};
			const Vector4& p00 = sample(x0, y0);
			const Vector4& p10 = sample(x0 + 1, y0);
			const Vector4& p01 = sample(x0, y0 + 1);
			const Vector4& p11 = sample(x0 + 1, y0 + 1);
			auto bilinear = [&](float v00, float v10, float v01, float v11) {
				const float row0 = v00 + (v10 - v00) * blendX;
				const float row1 = v01 + (v11 - v01) * blendX;
				return row0 + (row1 - row0) * blendY;
			};
			return Vector4{
				bilinear(p00.x, p10.x, p01.x, p11.x),
				bilinear(p00.y, p10.y, p01.y, p11.y),
				bilinear(p00.z, p10.z, p01.z, p11.z),
				bilinear(p00.w, p10.w, p01.w, p11.w),
			};
		};

		diagnostics.derivativeMinimum = {
			(std::numeric_limits<float>::max)(),
			(std::numeric_limits<float>::max)(),
			(std::numeric_limits<float>::max)(),
		};
		diagnostics.derivativeMaximum = {
			(std::numeric_limits<float>::lowest)(),
			(std::numeric_limits<float>::lowest)(),
			(std::numeric_limits<float>::lowest)(),
		};
		diagnostics.jacobianMinimum = (std::numeric_limits<float>::max)();
		diagnostics.jacobianMaximum = (std::numeric_limits<float>::lowest)();
		Vector3 combinedDerivativeSum{};
		double combinedJacobianSum = 0.0;
		uint32_t combinedBreakingCount = 0;
		constexpr uint32_t combinedDiagnosticSamples = kFFTSize * kFFTSize;
		for (uint32_t sampleIndex = 0;
			sampleIndex < combinedDiagnosticSamples;
			++sampleIndex) {
			const float worldX = static_cast<float>(
				std::fmod(
					(static_cast<double>(sampleIndex) + 0.5) *
					0.6180339887498948,
					1.0) * 256.0);
			const float worldZ = static_cast<float>(
				std::fmod(
					(static_cast<double>(sampleIndex) + 0.5) *
					0.7548776662466927,
					1.0) * 256.0);
			Vector4 combined{};
			for (uint32_t cascadeIndex = 0;
				cascadeIndex < kFFTCascadeCount;
				++cascadeIndex) {
				if (!cascadeSettings_[cascadeIndex].enabled) {
					continue;
				}
				const Vector4 derivative = sampleDerivative(
					cascades_[cascadeIndex],
					worldX,
					worldZ);
				const float contribution =
					cascadeSettings_[cascadeIndex].displacementContribution;
				combined.x += derivative.x * contribution;
				combined.y += derivative.y * contribution;
				combined.z += derivative.z * contribution;
				combined.w += derivative.w * contribution;
			}
			combined.x *= fftParameters_.choppiness;
			combined.y *= fftParameters_.choppiness;
			combined.z *= fftParameters_.choppiness;
			combined.w *= fftParameters_.choppiness;
			diagnostics.derivativeMinimum.x = (std::min)(
				diagnostics.derivativeMinimum.x,
				combined.x);
			diagnostics.derivativeMinimum.y = (std::min)(
				diagnostics.derivativeMinimum.y,
				combined.y);
			diagnostics.derivativeMinimum.z = (std::min)(
				diagnostics.derivativeMinimum.z,
				combined.z);
			diagnostics.derivativeMaximum.x = (std::max)(
				diagnostics.derivativeMaximum.x,
				combined.x);
			diagnostics.derivativeMaximum.y = (std::max)(
				diagnostics.derivativeMaximum.y,
				combined.y);
			diagnostics.derivativeMaximum.z = (std::max)(
				diagnostics.derivativeMaximum.z,
				combined.z);
			combinedDerivativeSum.x += combined.x;
			combinedDerivativeSum.y += combined.y;
			combinedDerivativeSum.z += combined.z;
			const float jacobian =
				(1.0f + combined.x) * (1.0f + combined.z) -
				combined.y * combined.w;
			if (std::isfinite(jacobian)) {
				diagnostics.jacobianMinimum = (std::min)(
					diagnostics.jacobianMinimum,
					jacobian);
				diagnostics.jacobianMaximum = (std::max)(
					diagnostics.jacobianMaximum,
					jacobian);
				combinedJacobianSum += jacobian;
				if (jacobian + parameters_.breakingParameters.y <
					parameters_.breakingParameters.x) {
					++combinedBreakingCount;
				}
			} else {
				++diagnostics.invalidValueCount;
			}
		}
		const float inverseCombinedCount =
			1.0f / static_cast<float>(combinedDiagnosticSamples);
		diagnostics.derivativeMean = {
			combinedDerivativeSum.x * inverseCombinedCount,
			combinedDerivativeSum.y * inverseCombinedCount,
			combinedDerivativeSum.z * inverseCombinedCount,
		};
		diagnostics.jacobianMean = static_cast<float>(
			combinedJacobianSum * inverseCombinedCount);
		diagnostics.breakingAreaRatio =
			combinedBreakingCount * inverseCombinedCount;

		OceanFFTParameters singleReference = fftParameters_;
		singleReference.cascadeIndex = kFFTCascadeCount;
		singleReference.minimumWaveNumber = 0.0f;
		singleReference.maximumWaveNumber = 1000000.0f;
		const float singleReferenceRms =
			ExpectedHeightRms(singleReference, kFFTSize);
		diagnostics.singleReferenceTargetVariance =
			singleReferenceRms * singleReferenceRms;
		diagnostics.combinedHeightRms = std::sqrt(
			(std::max)(diagnostics.combinedGpuVariance, 0.0f));
		diagnostics.combinedSignificantWaveHeight =
			4.0f * diagnostics.combinedHeightRms;
		diagnostics.combinedVarianceRelativeError =
			diagnostics.singleReferenceTargetVariance > 1.0e-12f
			? std::abs(
				diagnostics.combinedGpuVariance -
				diagnostics.singleReferenceTargetVariance) /
				diagnostics.singleReferenceTargetVariance
			: 0.0f;
		diagnostics.energyPartitionError =
			diagnostics.singleReferenceTargetVariance > 1.0e-12f
			? std::abs(
				diagnostics.combinedTargetVariance -
				diagnostics.singleReferenceTargetVariance) /
				diagnostics.singleReferenceTargetVariance
			: 0.0f;
		const BandPartitionMetrics partition =
			EvaluateBandPartition(singleReference, diagnosticParameters);
		diagnostics.bandOverlapEnergy = partition.overlapEnergy;
		diagnostics.bandMissingEnergy = partition.missingEnergy;

		diagnostics.valid = true;
		diagnostics.sampleTime = fftParameters_.time;
		diagnostics.heightRms = diagnostics.combinedHeightRms;
		diagnostics.significantWaveHeight =
			diagnostics.combinedSignificantWaveHeight;
		diagnostics.targetSpectrumVariance =
			diagnostics.combinedTargetVariance;
		diagnostics.rgba16fVariance =
			diagnostics.combinedGpuVariance;
		diagnostics.seedMeanVariance =
			diagnostics.combinedSeedMeanVariance;
		diagnostics.gpuTimeMilliseconds =
			diagnostics.totalGpuTimeMilliseconds;
	}
	diagnostics.oceanDrawGpuTimeMilliseconds = pendingOceanDrawGpuTime_;
	diagnostics.totalGpuTimeIncludingDraw =
		diagnostics.totalGpuTimeMilliseconds +
		diagnostics.oceanDrawGpuTimeMilliseconds;
	diagnostics.fftBaselineGpuTimeMilliseconds = 6.1f;
	diagnostics.derivativeGpuIncreaseMilliseconds =
		diagnostics.totalGpuTimeMilliseconds -
		diagnostics.fftBaselineGpuTimeMilliseconds;

	fftDiagnostics_ = diagnostics;
	fftDiagnosticsPending_ = false;
}

OceanRenderer::OceanCascadeDiagnostics
OceanRenderer::ResolveCascadeDiagnostics(OceanCascade& cascade)
{
	const D3D12_RANGE displacementRange = {
		0,
		static_cast<SIZE_T>(cascade.displacementReadback.totalBytes),
	};
	const D3D12_RANGE evolvedRange = {
		0,
		static_cast<SIZE_T>(cascade.evolvedSpectrumReadback.totalBytes),
	};
	const D3D12_RANGE initialRange = {
		0,
		static_cast<SIZE_T>(cascade.initialSpectrumReadback.totalBytes),
	};
	const D3D12_RANGE slopeRange = {
		0,
		static_cast<SIZE_T>(cascade.slopeReadback.totalBytes),
	};
	const D3D12_RANGE derivativeRange = {
		0,
		static_cast<SIZE_T>(cascade.derivativeReadback.totalBytes),
	};
	const D3D12_RANGE finalRange = {
		0,
		static_cast<SIZE_T>(cascade.finalSpectrumReadback.totalBytes),
	};
	const D3D12_RANGE finalCRange = {
		0,
		static_cast<SIZE_T>(cascade.finalSpectrumCReadback.totalBytes),
	};
	const D3D12_RANGE finalDRange = {
		0,
		static_cast<SIZE_T>(cascade.finalSpectrumDReadback.totalBytes),
	};
	const D3D12_RANGE debugRange = {
		0,
		static_cast<SIZE_T>(cascade.spectrumDebugReadback.totalBytes),
	};
	void* displacementMapped = nullptr;
	void* evolvedMapped = nullptr;
	void* initialMapped = nullptr;
	void* slopeMapped = nullptr;
	void* derivativeMapped = nullptr;
	void* finalMapped = nullptr;
	void* finalCMapped = nullptr;
	void* finalDMapped = nullptr;
	void* debugMapped = nullptr;
	HRESULT hr = cascade.displacementReadback.resource->Map(
		0,
		&displacementRange,
		&displacementMapped);
	assert(SUCCEEDED(hr));
	hr = cascade.slopeReadback.resource->Map(
		0,
		&slopeRange,
		&slopeMapped);
	assert(SUCCEEDED(hr));
	hr = cascade.derivativeReadback.resource->Map(
		0,
		&derivativeRange,
		&derivativeMapped);
	assert(SUCCEEDED(hr));
	hr = cascade.initialSpectrumReadback.resource->Map(
		0,
		&initialRange,
		&initialMapped);
	assert(SUCCEEDED(hr));
	hr = cascade.evolvedSpectrumReadback.resource->Map(
		0,
		&evolvedRange,
		&evolvedMapped);
	assert(SUCCEEDED(hr));
	hr = cascade.finalSpectrumReadback.resource->Map(
		0,
		&finalRange,
		&finalMapped);
	assert(SUCCEEDED(hr));
	hr = cascade.finalSpectrumCReadback.resource->Map(
		0,
		&finalCRange,
		&finalCMapped);
	assert(SUCCEEDED(hr));
	hr = cascade.finalSpectrumDReadback.resource->Map(
		0,
		&finalDRange,
		&finalDMapped);
	assert(SUCCEEDED(hr));
	hr = cascade.spectrumDebugReadback.resource->Map(
		0,
		&debugRange,
		&debugMapped);
	assert(SUCCEEDED(hr));
	const auto* displacementData =
		static_cast<const uint8_t*>(displacementMapped);
	const auto* evolvedData = static_cast<const uint8_t*>(evolvedMapped);
	const auto* initialData = static_cast<const uint8_t*>(initialMapped);
	const auto* slopeData = static_cast<const uint8_t*>(slopeMapped);
	const auto* derivativeData =
		static_cast<const uint8_t*>(derivativeMapped);
	const auto* finalData = static_cast<const uint8_t*>(finalMapped);
	const auto* finalCData = static_cast<const uint8_t*>(finalCMapped);
	const auto* finalDData = static_cast<const uint8_t*>(finalDMapped);
	const auto* debugData = static_cast<const uint8_t*>(debugMapped);

	OceanCascadeDiagnostics diagnostics{};
	diagnostics.sampleTime = cascade.diagnosticsParameters.time;
	diagnostics.heightMinimum = (std::numeric_limits<float>::max)();
	diagnostics.heightMaximum = (std::numeric_limits<float>::lowest)();
	diagnostics.derivativeMinimum = {
		(std::numeric_limits<float>::max)(),
		(std::numeric_limits<float>::max)(),
		(std::numeric_limits<float>::max)(),
	};
	diagnostics.derivativeMaximum = {
		(std::numeric_limits<float>::lowest)(),
		(std::numeric_limits<float>::lowest)(),
		(std::numeric_limits<float>::lowest)(),
	};
	diagnostics.jacobianMinimum = (std::numeric_limits<float>::max)();
	diagnostics.jacobianMaximum = (std::numeric_limits<float>::lowest)();
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
	Vector3 derivativeSum{};
	double derivativeQuantizationErrorSquaredSum = 0.0;
	double derivativeQuantizationSignalSquaredSum = 0.0;
	double crossDerivativeErrorSquaredSum = 0.0;
	double crossDerivativeSignalSquaredSum = 0.0;
	double jacobianSum = 0.0;
	uint32_t breakingSampleCount = 0;
	float maximumDirectionalError = 0.0f;
	uint32_t validHeightCount = 0;
	uint32_t validFloatHeightCount = 0;
	uint32_t validH0Count = 0;
	uint32_t validEvolvedCount = 0;
	uint32_t validImaginaryCount = 0;
	uint32_t validHermitianCount = 0;
	uint32_t validDerivativeCount = 0;
	const size_t spatialSampleCount =
		static_cast<size_t>(kFFTSize) * kFFTSize;
	cascade.diagnosticDerivativeSamples.assign(
		spatialSampleCount,
		Vector4{});
	std::vector<Vector4> displacementSamples(
		spatialSampleCount,
		Vector4{});

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
		(std::max)(
			static_cast<double>(cascade.diagnosticsParameters.patchLength),
			1.0);
	for (uint32_t y = 0; y < kFFTSize; ++y) {
		const uint8_t* displacementRow =
			displacementData +
			static_cast<size_t>(y) *
			cascade.displacementReadback.footprint.Footprint.RowPitch;
		const uint8_t* slopeRow =
			slopeData +
			static_cast<size_t>(y) *
			cascade.slopeReadback.footprint.Footprint.RowPitch;
		const uint8_t* derivativeRow =
			derivativeData +
			static_cast<size_t>(y) *
			cascade.derivativeReadback.footprint.Footprint.RowPitch;
		for (uint32_t x = 0; x < kFFTSize; ++x) {
			const size_t sampleIndex =
				static_cast<size_t>(y) * kFFTSize + x;
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
			const auto* derivativePixel =
				reinterpret_cast<const uint16_t*>(
					derivativeRow +
					static_cast<size_t>(x) * sizeof(uint16_t) * 4);
			float derivativeValues[4]{};
			bool derivativeFinite = true;
			for (uint32_t component = 0; component < 4; ++component) {
				derivativeValues[component] =
					DirectX::PackedVector::XMConvertHalfToFloat(
						derivativePixel[component]);
				if (!std::isfinite(derivativeValues[component])) {
					++diagnostics.invalidValueCount;
					derivativeFinite = false;
				}
			}
			cascade.diagnosticDerivativeSamples[sampleIndex] = {
				derivativeValues[0],
				derivativeValues[1],
				derivativeValues[2],
				derivativeValues[3],
			};
			displacementSamples[sampleIndex] = {
				displacementValues[0],
				displacementValues[1],
				displacementValues[2],
				displacementValues[3],
			};

			const float* finalCPixel = readFloat4(
				finalCData,
				cascade.finalSpectrumCReadback,
				x,
				y);
			const float* finalDPixel = readFloat4(
				finalDData,
				cascade.finalSpectrumDReadback,
				x,
				y);
			for (uint32_t component = 0; component < 4; ++component) {
				if (!std::isfinite(finalCPixel[component]) ||
					!std::isfinite(finalDPixel[component])) {
					++diagnostics.invalidValueCount;
				}
			}
			const float floatDerivatives[4] = {
				finalCPixel[2] * inverseTransformScale,
				finalDPixel[0] * inverseTransformScale,
				finalDPixel[2] * inverseTransformScale,
				finalDPixel[0] * inverseTransformScale,
			};
			if (derivativeFinite) {
				const float derivativeScale =
					cascade.diagnosticsParameters.choppiness *
					cascade.settings.displacementContribution;
				const float effectiveDerivative[4] = {
					derivativeValues[0] * derivativeScale,
					derivativeValues[1] * derivativeScale,
					derivativeValues[2] * derivativeScale,
					derivativeValues[3] * derivativeScale,
				};
				diagnostics.derivativeMinimum.x = (std::min)(
					diagnostics.derivativeMinimum.x,
					effectiveDerivative[0]);
				diagnostics.derivativeMinimum.y = (std::min)(
					diagnostics.derivativeMinimum.y,
					effectiveDerivative[1]);
				diagnostics.derivativeMinimum.z = (std::min)(
					diagnostics.derivativeMinimum.z,
					effectiveDerivative[2]);
				diagnostics.derivativeMaximum.x = (std::max)(
					diagnostics.derivativeMaximum.x,
					effectiveDerivative[0]);
				diagnostics.derivativeMaximum.y = (std::max)(
					diagnostics.derivativeMaximum.y,
					effectiveDerivative[1]);
				diagnostics.derivativeMaximum.z = (std::max)(
					diagnostics.derivativeMaximum.z,
					effectiveDerivative[2]);
				derivativeSum.x += effectiveDerivative[0];
				derivativeSum.y += effectiveDerivative[1];
				derivativeSum.z += effectiveDerivative[2];
				const float jacobian =
					(1.0f + effectiveDerivative[0]) *
					(1.0f + effectiveDerivative[2]) -
					effectiveDerivative[1] * effectiveDerivative[3];
				if (std::isfinite(jacobian)) {
					diagnostics.jacobianMinimum = (std::min)(
						diagnostics.jacobianMinimum,
						jacobian);
					diagnostics.jacobianMaximum = (std::max)(
						diagnostics.jacobianMaximum,
						jacobian);
					jacobianSum += jacobian;
					if (jacobian + parameters_.breakingParameters.y <
						parameters_.breakingParameters.x) {
						++breakingSampleCount;
					}
				} else {
					++diagnostics.invalidValueCount;
				}
				for (uint32_t component = 0; component < 4; ++component) {
					if (std::isfinite(floatDerivatives[component])) {
						const double difference =
							static_cast<double>(derivativeValues[component]) -
							floatDerivatives[component];
						derivativeQuantizationErrorSquaredSum +=
							difference * difference;
						derivativeQuantizationSignalSquaredSum +=
							static_cast<double>(floatDerivatives[component]) *
							floatDerivatives[component];
					}
				}
				const double crossDifference =
					static_cast<double>(derivativeValues[1]) -
					derivativeValues[3];
				crossDerivativeErrorSquaredSum +=
					crossDifference * crossDifference;
				crossDerivativeSignalSquaredSum +=
					static_cast<double>(derivativeValues[1]) *
					derivativeValues[1] +
					static_cast<double>(derivativeValues[3]) *
					derivativeValues[3];
				++validDerivativeCount;
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
				cascade.spectrumDebugReadback,
				x,
				y);
			for (uint32_t component = 0; component < 4; ++component) {
				if (!std::isfinite(debugPixel[component])) {
					++diagnostics.invalidValueCount;
				}
			}
			const int32_t centeredX =
				static_cast<int32_t>(x) - static_cast<int32_t>(kFFTSize / 2);
			const int32_t centeredY =
				static_cast<int32_t>(y) - static_cast<int32_t>(kFFTSize / 2);
			const SpectrumEvaluation cpuSpectrum = EvaluateSpectrum(
				cascade.diagnosticsParameters,
				centeredX * deltaK,
				centeredY * deltaK);
			maximumDirectionalError = (std::max)(
				maximumDirectionalError,
				static_cast<float>(
					cpuSpectrum.directionalNormalizationError));
			const double waveNumberSpectrum =
				std::isfinite(debugPixel[3])
				? (std::max)(static_cast<double>(debugPixel[3]), 0.0)
				: 0.0;
			targetVarianceSum +=
				waveNumberSpectrum * deltaK * deltaK;

			const float* initialPixel = readFloat4(
				initialData,
				cascade.initialSpectrumReadback,
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
				cascade.finalSpectrumReadback,
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
				cascade.evolvedSpectrumReadback,
				x,
				y);
			const uint32_t oppositeX = (kFFTSize - x) % kFFTSize;
			const uint32_t oppositeY = (kFFTSize - y) % kFFTSize;
			if (oppositeX == x && oppositeY == y) {
				++diagnostics.selfConjugateBinCount;
			}
			const float* oppositePixel = readFloat4(
				evolvedData,
				cascade.evolvedSpectrumReadback,
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

	if (validDerivativeCount > 0) {
		const float inverseCount = 1.0f / validDerivativeCount;
		diagnostics.derivativeMean = {
			derivativeSum.x * inverseCount,
			derivativeSum.y * inverseCount,
			derivativeSum.z * inverseCount,
		};
		diagnostics.jacobianMean =
			static_cast<float>(jacobianSum * inverseCount);
		diagnostics.breakingAreaRatio =
			static_cast<float>(breakingSampleCount) * inverseCount;
	} else {
		diagnostics.derivativeMinimum = {};
		diagnostics.derivativeMaximum = {};
		diagnostics.jacobianMinimum = 1.0f;
		diagnostics.jacobianMaximum = 1.0f;
	}
	diagnostics.crossDerivativeSymmetryError = static_cast<float>(std::sqrt(
		crossDerivativeErrorSquaredSum /
		(std::max)(crossDerivativeSignalSquaredSum, 1.0e-30)));
	diagnostics.derivativeQuantizationError = static_cast<float>(std::sqrt(
		derivativeQuantizationErrorSquaredSum /
		(std::max)(derivativeQuantizationSignalSquaredSum, 1.0e-30)));

	const float gridSpacing =
		(std::max)(cascade.diagnosticsParameters.patchLength, 0.001f) /
		static_cast<float>(kFFTSize);
	const float inverseCentralDifference = 0.5f / gridSpacing;
	const float finiteDifferenceContribution =
		cascade.settings.displacementContribution;
	const float derivativeScale =
		cascade.diagnosticsParameters.choppiness *
		cascade.settings.displacementContribution;
	double finiteDifferenceErrorSquaredSum = 0.0;
	double finiteDifferenceSignalSquaredSum = 0.0;
	for (uint32_t y = 0; y < kFFTSize; ++y) {
		const uint32_t previousY = (y + kFFTSize - 1) % kFFTSize;
		const uint32_t nextY = (y + 1) % kFFTSize;
		for (uint32_t x = 0; x < kFFTSize; ++x) {
			const uint32_t previousX = (x + kFFTSize - 1) % kFFTSize;
			const uint32_t nextX = (x + 1) % kFFTSize;
			auto sample = [&](uint32_t sampleX, uint32_t sampleY) -> const Vector4& {
				return displacementSamples[
					static_cast<size_t>(sampleY) * kFFTSize + sampleX];
			};
			const Vector4& left = sample(previousX, y);
			const Vector4& right = sample(nextX, y);
			const Vector4& back = sample(x, previousY);
			const Vector4& front = sample(x, nextY);
			const float finiteDifference[4] = {
				(right.x - left.x) * inverseCentralDifference *
					finiteDifferenceContribution,
				(front.x - back.x) * inverseCentralDifference *
					finiteDifferenceContribution,
				(front.z - back.z) * inverseCentralDifference *
					finiteDifferenceContribution,
				(right.z - left.z) * inverseCentralDifference *
					finiteDifferenceContribution,
			};
			const Vector4& derivative =
				cascade.diagnosticDerivativeSamples[
					static_cast<size_t>(y) * kFFTSize + x];
			const float analytical[4] = {
				derivative.x * derivativeScale,
				derivative.y * derivativeScale,
				derivative.z * derivativeScale,
				derivative.w * derivativeScale,
			};
			for (uint32_t component = 0; component < 4; ++component) {
				if (std::isfinite(finiteDifference[component]) &&
					std::isfinite(analytical[component])) {
					const double difference =
						static_cast<double>(finiteDifference[component]) -
						analytical[component];
					finiteDifferenceErrorSquaredSum += difference * difference;
					finiteDifferenceSignalSquaredSum +=
						static_cast<double>(analytical[component]) *
						analytical[component];
				} else {
					++diagnostics.invalidValueCount;
				}
			}
		}
	}
	diagnostics.derivativeFiniteDifferenceRelativeError =
		static_cast<float>(std::sqrt(
			finiteDifferenceErrorSquaredSum /
			(std::max)(finiteDifferenceSignalSquaredSum, 1.0e-30)));

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
		ExpectedHeightRms(cascade.diagnosticsParameters, 64);
	diagnostics.expectedRms128 =
		ExpectedHeightRms(cascade.diagnosticsParameters, 128);
	diagnostics.expectedRms256 =
		ExpectedHeightRms(cascade.diagnosticsParameters, 256);
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
		static_cast<float>(JonswapAlpha(cascade.diagnosticsParameters));
	diagnostics.peakAngularFrequency = static_cast<float>(
		JonswapPeakAngularFrequency(cascade.diagnosticsParameters));
	const SeedEnsembleResult seedEnsemble =
		EvaluateSeedEnsemble(cascade.diagnosticsParameters, 32);
	diagnostics.seedRmsMean = seedEnsemble.mean;
	diagnostics.seedRmsStandardDeviation =
		seedEnsemble.standardDeviation;
	diagnostics.seedRmsMinimum = seedEnsemble.minimum;
	diagnostics.seedRmsMaximum = seedEnsemble.maximum;
	diagnostics.seedMeanRelativeError =
		seedEnsemble.meanRelativeError;
	diagnostics.seedMeanAbsoluteRelativeError =
		seedEnsemble.meanAbsoluteRelativeError;
	diagnostics.seedMeanVariance = seedEnsemble.meanVariance;
	diagnostics.rgba16fQuantizationError =
		diagnostics.ifftFloatVariance > 1.0e-12f
		? std::abs(
			diagnostics.rgba16fVariance -
			diagnostics.ifftFloatVariance) /
			diagnostics.ifftFloatVariance
		: 0.0f;
	diagnostics.invalidValueCount +=
		seedEnsemble.invalidValueCount;
	diagnostics.valid = true;
	cascade.diagnostics = diagnostics;

	const D3D12_RANGE emptyWriteRange = { 0, 0 };
	cascade.displacementReadback.resource->Unmap(0, &emptyWriteRange);
	cascade.slopeReadback.resource->Unmap(0, &emptyWriteRange);
	cascade.derivativeReadback.resource->Unmap(0, &emptyWriteRange);
	cascade.initialSpectrumReadback.resource->Unmap(0, &emptyWriteRange);
	cascade.evolvedSpectrumReadback.resource->Unmap(0, &emptyWriteRange);
	cascade.finalSpectrumReadback.resource->Unmap(0, &emptyWriteRange);
	cascade.finalSpectrumCReadback.resource->Unmap(0, &emptyWriteRange);
	cascade.finalSpectrumDReadback.resource->Unmap(0, &emptyWriteRange);
	cascade.spectrumDebugReadback.resource->Unmap(0, &emptyWriteRange);
	return diagnostics;
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

void OceanRenderer::BindComputePipeline(
	ID3D12PipelineState* pipeline,
	const OceanCascade& cascade)
{
	auto commandList = Object3dCommon::GetInstance()->GetDxCommon()->GetList();
	commandList->SetComputeRootSignature(fftComputeRoot_.GetSignature().Get());
	commandList->SetPipelineState(pipeline);
	commandList->SetComputeRootConstantBufferView(
		0,
		cascade.parameterResource->GetGPUVirtualAddress());
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

void OceanRenderer::CreateProjectedGridMesh(uint32_t horizontalResolution)
{
	horizontalResolution =
		(std::clamp)(horizontalResolution, 32u, 512u);
	const uint32_t verticalResolution = (std::max)(
		static_cast<uint32_t>(std::lround(
			static_cast<float>(horizontalResolution) *
			kProjectedGridAspect)),
		18u);
	if (horizontalResolution == projectedGridHorizontalResolution_ &&
		verticalResolution == projectedGridVerticalResolution_ &&
		projectedGridVertexResource_ &&
		projectedGridIndexResource_) {
		return;
	}

	std::vector<VertexData> vertices;
	std::vector<uint32_t> indices;
	vertices.reserve(
		static_cast<size_t>(horizontalResolution + 1u) *
		static_cast<size_t>(verticalResolution + 1u));
	indices.reserve(
		static_cast<size_t>(horizontalResolution) *
		static_cast<size_t>(verticalResolution) * 6u);

	for (uint32_t y = 0; y <= verticalResolution; ++y) {
		const float v =
			static_cast<float>(y) / static_cast<float>(verticalResolution);
		const float ndcY = -1.0f + 2.0f * v;
		for (uint32_t x = 0; x <= horizontalResolution; ++x) {
			const float u =
				static_cast<float>(x) /
				static_cast<float>(horizontalResolution);
			const float ndcX = -1.0f + 2.0f * u;
			vertices.push_back({
				{ ndcX, 0.0f, ndcY, 1.0f },
				{ u, v },
				{ 0.0f, 1.0f, 0.0f },
				{ 1.0f, 0.0f, 0.0f, 1.0f },
			});
		}
	}

	const uint32_t stride = horizontalResolution + 1u;
	for (uint32_t y = 0; y < verticalResolution; ++y) {
		for (uint32_t x = 0; x < horizontalResolution; ++x) {
			const uint32_t i0 = y * stride + x;
			const uint32_t i1 = i0 + 1u;
			const uint32_t i2 = i0 + stride;
			const uint32_t i3 = i2 + 1u;
			indices.push_back(i0);
			indices.push_back(i2);
			indices.push_back(i1);
			indices.push_back(i2);
			indices.push_back(i3);
			indices.push_back(i1);
		}
	}

	DirectXCommon* dxCommon = Object3dCommon::GetInstance()->GetDxCommon();
	projectedGridVertexResource_ = dxCommon->CreateBufferResource(
		sizeof(VertexData) * vertices.size());
	projectedGridIndexResource_ = dxCommon->CreateBufferResource(
		sizeof(uint32_t) * indices.size());

	void* mappedVertices = nullptr;
	projectedGridVertexResource_->Map(0, nullptr, &mappedVertices);
	std::memcpy(
		mappedVertices,
		vertices.data(),
		sizeof(VertexData) * vertices.size());
	projectedGridVertexResource_->Unmap(0, nullptr);

	void* mappedIndices = nullptr;
	projectedGridIndexResource_->Map(0, nullptr, &mappedIndices);
	std::memcpy(
		mappedIndices,
		indices.data(),
		sizeof(uint32_t) * indices.size());
	projectedGridIndexResource_->Unmap(0, nullptr);

	projectedGridVertexBufferView_.BufferLocation =
		projectedGridVertexResource_->GetGPUVirtualAddress();
	projectedGridVertexBufferView_.SizeInBytes =
		static_cast<UINT>(sizeof(VertexData) * vertices.size());
	projectedGridVertexBufferView_.StrideInBytes = sizeof(VertexData);
	projectedGridIndexBufferView_.BufferLocation =
		projectedGridIndexResource_->GetGPUVirtualAddress();
	projectedGridIndexBufferView_.SizeInBytes =
		static_cast<UINT>(sizeof(uint32_t) * indices.size());
	projectedGridIndexBufferView_.Format = DXGI_FORMAT_R32_UINT;
	projectedGridIndexCount_ = static_cast<uint32_t>(indices.size());
	projectedGridHorizontalResolution_ = horizontalResolution;
	projectedGridVerticalResolution_ = verticalResolution;
	parameters_.projectedGridResolution = {
		static_cast<float>(horizontalResolution),
		static_cast<float>(verticalResolution),
	};
}

void OceanRenderer::DrawProjectedGrid()
{
	auto commandList = Object3dCommon::GetInstance()->GetDxCommon()->GetList();
	commandList->IASetVertexBuffers(
		0,
		1,
		&projectedGridVertexBufferView_);
	commandList->IASetIndexBuffer(&projectedGridIndexBufferView_);
	commandList->DrawIndexedInstanced(
		projectedGridIndexCount_,
		1,
		0,
		0,
		0);
}
