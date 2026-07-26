#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <d3d12.h>
#include <wrl.h>

#include "Calculation.h"
#include "Root.h"

class Camera;
class DebugCamera;
class Model;

class OceanRenderer {
public:
	enum class Mode : uint32_t {
		Calm = 0,
		Naval = 1,
		ArcBlanc = 2,
	};

	enum class WaveSource : uint32_t {
		Procedural = 0,
		FFTSingleCascade = 1,
		FFTThreeCascades = 2,
	};

	enum class SpectrumModel : uint32_t {
		Phillips = 0,
		JonswapDonelanBanner = 1,
	};

	enum class CascadeBandMode : uint32_t {
		HardCutoff = 0,
		SmoothTransition = 1,
	};

	enum class MeshMode : uint32_t {
		FixedGrid = 0,
		ProjectedGrid = 1,
	};

	enum class ProjectedGridDebugMode : uint32_t {
		None = 0,
		Density = 1,
		GuardBand = 2,
		DisplacementFade = 3,
		NearFade = 4,
		Coverage = 5,
	};

	static constexpr uint32_t kFFTCascadeCount = 3;

	struct OceanCascadeSettings {
		bool enabled = true;
		float patchLength = 256.0f;
		float minimumWaveNumber = 0.0f;
		float maximumWaveNumber = 1.0f;
		float displacementContribution = 1.0f;
		float slopeContribution = 1.0f;
	};

	struct ProjectedGridSettings {
		uint32_t horizontalResolution = 256;
		float nearClamp = 3.0f;
		float farClamp = 1800.0f;
		float overscanX = 1.10f;
		float overscanTop = 1.05f;
		float overscanBottom = 1.15f;
		bool nearDisplacementFadeEnabled = true;
		float nearFadeWidth = 5.0f;
		float minimumSafeNearDistance = 3.0f;
		ProjectedGridDebugMode debugMode =
			ProjectedGridDebugMode::None;
		bool wireframe = false;
		bool outerWireframe = false;
	};

	struct OceanCascadeDiagnostics {
		bool valid = false;
		float sampleTime = 0.0f;
		float heightRms = 0.0f;
		float significantWaveHeight = 0.0f;
		float heightMinimum = 0.0f;
		float heightMaximum = 0.0f;
		float targetSpectrumVariance = 0.0f;
		float legacyCoefficientVariance = 0.0f;
		float h0PredictedVariance = 0.0f;
		float evolvedParsevalVariance = 0.0f;
		float ifftFloatVariance = 0.0f;
		float rgba16fVariance = 0.0f;
		float gaussianRealSquared = 0.0f;
		float gaussianImaginarySquared = 0.0f;
		float gaussianMagnitudeSquared = 0.0f;
		float h0MagnitudeSquared = 0.0f;
		float evolvedMagnitudeSquared = 0.0f;
		float hermitianSymmetryError = 0.0f;
		float ifftImaginaryResidual = 0.0f;
		float directionalNormalizationError = 0.0f;
		float expectedRms64 = 0.0f;
		float expectedRms128 = 0.0f;
		float expectedRms256 = 0.0f;
		float resolutionRelativeSpread = 0.0f;
		float jonswapAlpha = 0.0f;
		float peakAngularFrequency = 0.0f;
		float seedRmsMean = 0.0f;
		float seedRmsStandardDeviation = 0.0f;
		float seedRmsMinimum = 0.0f;
		float seedRmsMaximum = 0.0f;
		float seedMeanRelativeError = 0.0f;
		float seedMeanAbsoluteRelativeError = 0.0f;
		float seedMeanVariance = 0.0f;
		float rgba16fQuantizationError = 0.0f;
		Vector3 derivativeMinimum{};
		Vector3 derivativeMaximum{};
		Vector3 derivativeMean{};
		float jacobianMinimum = 1.0f;
		float jacobianMaximum = 1.0f;
		float jacobianMean = 1.0f;
		float breakingAreaRatio = 0.0f;
		float crossDerivativeSymmetryError = 0.0f;
		float derivativeFiniteDifferenceRelativeError = 0.0f;
		float derivativeQuantizationError = 0.0f;
		float gpuTimeMilliseconds = 0.0f;
		uint32_t invalidValueCount = 0;
		uint32_t gaussianSampleCount = 0;
		uint32_t selfConjugateBinCount = 0;
	};

	struct OceanFFTDiagnostics : OceanCascadeDiagnostics {
		bool threeCascades = false;
		std::array<OceanCascadeDiagnostics, kFFTCascadeCount> cascades{};
		float singleReferenceTargetVariance = 0.0f;
		float combinedTargetVariance = 0.0f;
		float combinedGpuVariance = 0.0f;
		float combinedHeightRms = 0.0f;
		float combinedSignificantWaveHeight = 0.0f;
		float combinedSeedMeanVariance = 0.0f;
		float combinedVarianceRelativeError = 0.0f;
		float energyPartitionError = 0.0f;
		float bandOverlapEnergy = 0.0f;
		float bandMissingEnergy = 0.0f;
		float totalGpuTimeMilliseconds = 0.0f;
		float oceanDrawGpuTimeMilliseconds = 0.0f;
		float totalGpuTimeIncludingDraw = 0.0f;
		float fftBaselineGpuTimeMilliseconds = 6.1f;
		float derivativeGpuIncreaseMilliseconds = 0.0f;
	};

	struct alignas(16) OceanParameters {
		Matrix4x4 viewProjection{};
		Matrix4x4 inverseViewProjection{};
		Vector4 tint = { 0.72f, 0.82f, 0.92f, 0.98f };
		Vector3 cameraPosition{};
		float time = 0.0f;
		Vector3 gridOrigin{};
		float baseHeight = -1.15f;
		Vector2 windDirection = { 0.18f, 0.98f };
		float windSpeed = 12.0f;
		float choppiness = 3.10f;
		Vector3 sunDirection = { -0.12f, -0.26f, -0.96f };
		float sunIntensity = 2.75f;
		Vector3 sunColor = { 1.0f, 0.97f, 0.90f };
		float sunSpecularStrength = 0.70f;
		float artisticSunLaneStrength = 1.0f;
		float atmosphereStrength = 1.0f;
		float farFlattenStrength = 1.0f;
		float debugMode = 0.0f;
		float mode = static_cast<float>(Mode::ArcBlanc);
		float diagnosticsEnabled = 1.0f;
		float sunPathEnabled = 1.0f;
		float atmosphereEnabled = 1.0f;
		float farFlattenEnabled = 1.0f;
		float proceduralCloudReflectionEnabled = 0.0f;
		Vector2 padding{};
		float waveSource = static_cast<float>(WaveSource::Procedural);
		float fftPatchLength = 256.0f;
		float fftDebugMode = 0.0f;
		float fftDebugScale = 1.0f;
		Vector4 cascadePatchLengths = { 256.0f, 16.0f, 4.0f, 256.0f };
		Vector4 cascadeDisplacementContributions = { 1.0f, 1.0f, 0.0f, 0.0f };
		Vector4 cascadeSlopeContributions = { 1.0f, 1.0f, 1.0f, 0.0f };
		Vector4 cascadeEnabled = { 1.0f, 1.0f, 1.0f, 0.0f };
		float cascadeDisplayMode = 0.0f;
		float fftDebugPatchLength = 256.0f;
		float fftDebugCascade = 0.0f;
		float cascadePadding = 0.0f;
		float meshMode = static_cast<float>(MeshMode::ProjectedGrid);
		float projectedNearClamp = 0.5f;
		float projectedFarClamp = 1800.0f;
		float projectedHorizonNdcY = 1.0f;
		Vector2 projectedGridResolution = { 256.0f, 144.0f };
		float projectedGridDebug = 0.0f;
		float projectedWireframe = 0.0f;
		Vector4 projectedOverscan = { 1.10f, 1.05f, 1.15f, 0.003f };
		Vector4 projectedDisplacementGuard = {
			1.0f, 5.0f, 3.0f, 0.0f
		};
		Vector4 breakingParameters = { 0.72f, 0.0f, 0.12f, 0.25f };
		Vector4 derivativeControls = { 1.0f, 0.0f, 0.0f, 0.0f };
	};

	struct alignas(16) OceanFFTParameters {
		uint32_t fftSize = 128;
		float patchLength = 256.0f;
		float time = 0.0f;
		float amplitude = 1.0f;
		Vector2 windDirection = { 0.18f, 0.98f };
		float windSpeed = 12.0f;
		float choppiness = 3.10f;
		uint32_t seed = 1337;
		uint32_t spectrumModel =
			static_cast<uint32_t>(SpectrumModel::Phillips);
		float fetch = 100000.0f;
		float gamma = 3.3f;
		float lowFrequencyDamping = 0.0f;
		float highFrequencyDamping = 0.01f;
		Vector2 swellDirection = { 0.60f, 0.80f };
		float swellAmount = 0.20f;
		float oppositeWaveSuppression = 0.85f;
		float minimumWaveNumber = 0.0f;
		float maximumWaveNumber = 1000000.0f;
		float bandTransitionWidth = 0.25f;
		uint32_t bandMode =
			static_cast<uint32_t>(CascadeBandMode::HardCutoff);
		uint32_t cascadeIndex = kFFTCascadeCount;
		Vector3 padding{};
	};

	void Initialize(Model* gridModel, uint32_t environmentSrvIndex);
	void Update(
		float time,
		Camera& camera,
		DebugCamera& debugCamera,
		bool useDebugCamera);
	void Draw();

	void SetMode(Mode mode);
	void SetWaveSource(WaveSource source);
	void SetTint(const Vector4& tint);
	void SetBaseHeight(float height);
	void SetWind(const Vector2& direction, float speed, float choppiness);
	void SetSun(
		float intensity,
		float sunSpecularStrength,
		float artisticSunLaneStrength);
	void SetDiagnostics(
		bool sunPathEnabled,
		bool atmosphereEnabled,
		bool farFlattenEnabled,
		bool proceduralCloudReflectionEnabled,
		int debugMode,
		float atmosphereStrength,
		float farFlattenStrength);
	void SetDerivativeSettings(
		int normalMode,
		bool breakingPreviewEnabled,
		float jacobianThreshold,
		float jacobianBias,
		float smoothWidth,
		float maskIntensity);
	void SetFFTSettings(
		float time,
		float amplitude,
		float patchLength,
		uint32_t seed,
		bool paused,
		int debugMode,
		float debugDisplayScale);
	void SetFFTSpectrumSettings(
		SpectrumModel model,
		float fetch,
		float gamma,
		float lowFrequencyDamping,
		float highFrequencyDamping,
		const Vector2& swellDirection,
		float swellAmount,
		float oppositeWaveSuppression);
	void SetFFTCascadeSettings(
		CascadeBandMode bandMode,
		float transitionWidth,
		const std::array<OceanCascadeSettings, kFFTCascadeCount>& settings,
		int displayMode,
		int debugCascadeIndex);
	void SetMeshSettings(
		MeshMode mode,
		const ProjectedGridSettings& settings);
	void RequestFFTDiagnostics();
	void SetEnvironmentSrvIndex(uint32_t environmentSrvIndex);

	const OceanParameters& GetParameters() const { return parameters_; }
	const OceanFFTDiagnostics& GetFFTDiagnostics() const { return fftDiagnostics_; }
	bool IsFFTDiagnosticsPending() const
	{
		return fftDiagnosticsRequested_ || fftDiagnosticsPending_;
	}

private:
	static constexpr uint32_t kFFTSize = 128;
	static constexpr uint32_t kFFTLog2 = 7;

	struct FFTTexture {
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		uint32_t srvIndex = 0;
		uint32_t uavIndex = 0;
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
		D3D12_RESOURCE_STATES state = static_cast<D3D12_RESOURCE_STATES>(
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	};

	struct FFTReadback {
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
		uint64_t totalBytes = 0;
	};

	struct OceanCascade {
		OceanFFTParameters parameters{};
		OceanCascadeSettings settings{};
		Microsoft::WRL::ComPtr<ID3D12Resource> parameterResource;
		OceanFFTParameters* parameterData = nullptr;
		FFTTexture initialSpectrum;
		FFTTexture spectrumDebug;
		FFTTexture evolvedSpectrumDebug;
		std::array<FFTTexture, 2> spectrumA;
		std::array<FFTTexture, 2> spectrumB;
		std::array<FFTTexture, 2> spectrumC;
		std::array<FFTTexture, 2> spectrumD;
		FFTTexture displacement;
		FFTTexture slope;
		FFTTexture derivative;
		FFTReadback displacementReadback;
		FFTReadback slopeReadback;
		FFTReadback derivativeReadback;
		FFTReadback initialSpectrumReadback;
		FFTReadback evolvedSpectrumReadback;
		FFTReadback finalSpectrumReadback;
		FFTReadback finalSpectrumCReadback;
		FFTReadback finalSpectrumDReadback;
		FFTReadback spectrumDebugReadback;
		std::vector<Vector4> diagnosticDerivativeSamples;
		OceanFFTParameters diagnosticsParameters{};
		OceanCascadeDiagnostics diagnostics{};
		uint32_t finalSpectrumIndex = 0;
		bool initialSpectrumDirty = true;
		bool outputDirty = true;
		bool hasOutput = false;
	};

	void UploadParameters();
	void InitializeFFT();
	void InitializeCascade(
		OceanCascade& cascade,
		const OceanCascadeSettings& settings,
		uint32_t cascadeIndex,
		uint32_t seed);
	void ApplyBaseParametersToCascades();
	void MarkAllInitialSpectraDirty();
	void MarkAllOutputsDirty();
	void CreateFFTTexture(FFTTexture& texture, DXGI_FORMAT format);
	void CreateFFTReadback(FFTReadback& readback, const FFTTexture& source);
	void CreateComputePipeline(
		const std::wstring& shaderPath,
		Microsoft::WRL::ComPtr<ID3D12PipelineState>& pipeline);
	void RunFFT();
	void RunCascadeFFT(OceanCascade& cascade);
	void QueueFFTDiagnosticsReadback(OceanCascade& cascade);
	void ResolveFFTDiagnostics();
	OceanCascadeDiagnostics ResolveCascadeDiagnostics(OceanCascade& cascade);
	void ResolveGpuTimings();
	void InitializeGpuTiming();
	void BeginCascadeGpuTiming(uint32_t queryIndex);
	void EndCascadeGpuTiming(uint32_t queryIndex);
	void QueueGpuTimingReadback(uint32_t queryCount);
	void CopyFFTTextureToReadback(
		FFTTexture& source,
		FFTReadback& destination);
	void BindComputePipeline(
		ID3D12PipelineState* pipeline,
		const OceanCascade& cascade);
	void Transition(
		FFTTexture& texture,
		D3D12_RESOURCE_STATES nextState);
	void InsertUAVBarrier(FFTTexture& texture);
	void BindComputeSrv(uint32_t rootIndex, const FFTTexture& texture);
	void BindComputeUav(uint32_t rootIndex, const FFTTexture& texture);
	void CreateProjectedGridMesh(uint32_t horizontalResolution);
	void DrawProjectedGrid();

	Model* gridModel_ = nullptr;
	uint32_t environmentSrvIndex_ = 0;
	OceanParameters parameters_{};
	Microsoft::WRL::ComPtr<ID3D12Resource> parameterResource_;
	OceanParameters* parameterData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> projectedGridVertexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> projectedGridIndexResource_;
	D3D12_VERTEX_BUFFER_VIEW projectedGridVertexBufferView_{};
	D3D12_INDEX_BUFFER_VIEW projectedGridIndexBufferView_{};
	uint32_t projectedGridIndexCount_ = 0;
	uint32_t projectedGridHorizontalResolution_ = 0;
	uint32_t projectedGridVerticalResolution_ = 0;

	Root fftComputeRoot_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> spectrumInitializePipeline_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> spectrumEvolvePipeline_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> fftPipeline_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> fftOutputPipeline_;
	OceanFFTParameters fftParameters_{};
	OceanCascade singleCascade_{};
	std::array<OceanCascade, kFFTCascadeCount> cascades_{};
	std::array<OceanCascadeSettings, kFFTCascadeCount> cascadeSettings_{};
	CascadeBandMode cascadeBandMode_ = CascadeBandMode::HardCutoff;
	float cascadeTransitionWidth_ = 0.25f;
	uint32_t cascadeDisplayMode_ = 0;
	uint32_t debugCascadeIndex_ = 0;
	OceanFFTDiagnostics fftDiagnostics_{};
	Microsoft::WRL::ComPtr<ID3D12QueryHeap> fftTimestampQueryHeap_;
	Microsoft::WRL::ComPtr<ID3D12Resource> fftTimestampReadback_;
	uint64_t fftTimestampFrequency_ = 0;
	uint32_t fftTimestampQueryCount_ = 0;
	std::array<float, kFFTCascadeCount> pendingGpuTimes_{};
	float pendingOceanDrawGpuTime_ = 0.0f;
	uint32_t fftTimingQueryCountBeforeDraw_ = 0;
	bool oceanDrawTimingPending_ = false;
	bool diagnosticsThreeCascades_ = false;
	bool fftPaused_ = false;
	bool fftDiagnosticsRequested_ = false;
	bool fftDiagnosticsPending_ = false;
};
