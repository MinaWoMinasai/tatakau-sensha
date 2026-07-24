#pragma once

#include <array>
#include <cstdint>
#include <string>

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
	};

	enum class SpectrumModel : uint32_t {
		Phillips = 0,
		JonswapDonelanBanner = 1,
	};

	struct OceanFFTDiagnostics {
		bool valid = false;
		float sampleTime = 0.0f;
		float heightRms = 0.0f;
		float significantWaveHeight = 0.0f;
		float heightMinimum = 0.0f;
		float heightMaximum = 0.0f;
		float hermitianSymmetryError = 0.0f;
		float ifftImaginaryResidual = 0.0f;
		float directionalNormalizationError = 0.0f;
		float expectedRms64 = 0.0f;
		float expectedRms128 = 0.0f;
		float expectedRms256 = 0.0f;
		float resolutionRelativeSpread = 0.0f;
		float jonswapAlpha = 0.0f;
		float peakAngularFrequency = 0.0f;
		uint32_t invalidValueCount = 0;
	};

	struct alignas(16) OceanParameters {
		Matrix4x4 viewProjection{};
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
		Vector2 padding{};
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

	void UploadParameters();
	void InitializeFFT();
	void CreateFFTTexture(FFTTexture& texture, DXGI_FORMAT format);
	void CreateFFTReadback(FFTReadback& readback, const FFTTexture& source);
	void CreateComputePipeline(
		const std::wstring& shaderPath,
		Microsoft::WRL::ComPtr<ID3D12PipelineState>& pipeline);
	void RunFFT();
	void QueueFFTDiagnosticsReadback();
	void ResolveFFTDiagnostics();
	void CopyFFTTextureToReadback(
		FFTTexture& source,
		FFTReadback& destination);
	void BindComputePipeline(ID3D12PipelineState* pipeline);
	void Transition(
		FFTTexture& texture,
		D3D12_RESOURCE_STATES nextState);
	void InsertUAVBarrier(FFTTexture& texture);
	void BindComputeSrv(uint32_t rootIndex, const FFTTexture& texture);
	void BindComputeUav(uint32_t rootIndex, const FFTTexture& texture);

	Model* gridModel_ = nullptr;
	uint32_t environmentSrvIndex_ = 0;
	OceanParameters parameters_{};
	Microsoft::WRL::ComPtr<ID3D12Resource> parameterResource_;
	OceanParameters* parameterData_ = nullptr;

	Root fftComputeRoot_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> spectrumInitializePipeline_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> spectrumEvolvePipeline_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> fftPipeline_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> fftOutputPipeline_;
	Microsoft::WRL::ComPtr<ID3D12Resource> fftParameterResource_;
	OceanFFTParameters* fftParameterData_ = nullptr;
	OceanFFTParameters fftParameters_{};
	FFTTexture initialSpectrum_;
	FFTTexture spectrumDebug_;
	FFTTexture evolvedSpectrumDebug_;
	std::array<FFTTexture, 2> spectrumA_;
	std::array<FFTTexture, 2> spectrumB_;
	std::array<FFTTexture, 2> spectrumC_;
	FFTTexture displacement_;
	FFTTexture slope_;
	FFTReadback displacementReadback_;
	FFTReadback slopeReadback_;
	FFTReadback evolvedSpectrumReadback_;
	FFTReadback finalSpectrumReadback_;
	FFTReadback spectrumDebugReadback_;
	OceanFFTParameters diagnosticsParameters_{};
	OceanFFTDiagnostics fftDiagnostics_{};
	uint32_t finalSpectrumIndex_ = 0;
	bool fftInitialSpectrumDirty_ = true;
	bool fftOutputDirty_ = true;
	bool fftHasOutput_ = false;
	bool fftPaused_ = false;
	bool fftDiagnosticsRequested_ = false;
	bool fftDiagnosticsPending_ = false;
};
