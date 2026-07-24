#include "OceanRenderer.h"

#include <algorithm>
#include <cassert>
#include <cmath>

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

	if (parameters_.waveSource >= 0.5f || parameters_.fftDebugMode > 0.5f) {
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
		static_cast<float>((std::clamp)(debugMode, 0, 7));
	parameters_.fftDebugScale = (std::max)(debugDisplayScale, 0.001f);
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
	CreateFFTTexture(evolvedSpectrumDebug_, DXGI_FORMAT_R32G32B32A32_FLOAT);
	for (uint32_t index = 0; index < 2; ++index) {
		CreateFFTTexture(spectrumA_[index], DXGI_FORMAT_R32G32B32A32_FLOAT);
		CreateFFTTexture(spectrumB_[index], DXGI_FORMAT_R32G32B32A32_FLOAT);
		CreateFFTTexture(spectrumC_[index], DXGI_FORMAT_R32G32B32A32_FLOAT);
	}
	CreateFFTTexture(displacement_, DXGI_FORMAT_R16G16B16A16_FLOAT);
	CreateFFTTexture(slope_, DXGI_FORMAT_R16G16_FLOAT);
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
		!fftOutputDirty_) {
		return;
	}

	*fftParameterData_ = fftParameters_;
	auto commandList = Object3dCommon::GetInstance()->GetDxCommon()->GetList();
	Object3dCommon::GetInstance()->GetSrvManager()->PreDraw();
	constexpr uint32_t groupCount = kFFTSize / 16;
	const uint32_t zeroConstants[4] = {};

	if (fftInitialSpectrumDirty_) {
		Transition(initialSpectrum_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		BindComputePipeline(spectrumInitializePipeline_.Get());
		commandList->SetComputeRoot32BitConstants(1, 4, zeroConstants, 0);
		BindComputeUav(5, initialSpectrum_);
		commandList->Dispatch(groupCount, groupCount, 1);
		InsertUAVBarrier(initialSpectrum_);
		Transition(initialSpectrum_, kShaderReadState);
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
