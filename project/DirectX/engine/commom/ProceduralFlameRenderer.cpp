#include "ProceduralFlameRenderer.h"

#include <algorithm>
#include <cassert>
#include <cmath>

using Microsoft::WRL::ComPtr;

namespace {
float Hash01(uint32_t value)
{
	value ^= value >> 16;
	value *= 0x7feb352du;
	value ^= value >> 15;
	value *= 0x846ca68bu;
	value ^= value >> 16;
	return static_cast<float>(value & 0x00ffffffu) / 16777216.0f;
}

float SmoothStep01(float value)
{
	value = std::clamp(value, 0.0f, 1.0f);
	return value * value * (3.0f - 2.0f * value);
}
} // namespace

void ProceduralFlameRenderer::Initialize(DirectXCommon* dxCommon)
{
	assert(dxCommon);
	dxCommon_ = dxCommon;
	CreatePipeline();

	vertexResource_ = dxCommon_->CreateBufferResource(sizeof(TrailVertex) * 6);
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeof(TrailVertex) * 6;
	vertexBufferView_.StrideInBytes = sizeof(TrailVertex);
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

	viewProjectionResource_ = dxCommon_->CreateBufferResource(sizeof(Matrix4x4));
	viewProjectionResource_->Map(0, nullptr, reinterpret_cast<void**>(&viewProjectionData_));
	*viewProjectionData_ = MakeIdentity4x4();

	static_assert(sizeof(GpuParameters) % 16 == 0);
	parameterResource_ = dxCommon_->CreateBufferResource(sizeof(GpuParameters));
	parameterResource_->Map(0, nullptr, reinterpret_cast<void**>(&parameterData_));
	ResetMetaballs();
	UploadParameters(1.0f);
}

void ProceduralFlameRenderer::SetParameters(const Parameters& parameters)
{
	parameters_ = parameters;
	parameters_.noiseScale = (std::max)(parameters_.noiseScale, 0.01f);
	parameters_.noiseSpeed = (std::max)(parameters_.noiseSpeed, 0.0f);
	parameters_.distortionStrength = std::clamp(parameters_.distortionStrength, 0.0f, 0.45f);
	parameters_.contourThreshold = std::clamp(parameters_.contourThreshold, 0.1f, 4.0f);
	parameters_.contourWidth = std::clamp(parameters_.contourWidth, 0.002f, 0.35f);
	parameters_.contourSoftness = std::clamp(parameters_.contourSoftness, 0.001f, 0.2f);
	parameters_.contourAaScale = std::clamp(parameters_.contourAaScale, 0.5f, 2.5f);
	parameters_.contourWidthModulation = std::clamp(
		parameters_.contourWidthModulation,
		0.0f,
		0.35f);
	parameters_.fieldGain = std::clamp(parameters_.fieldGain, 0.05f, 4.0f);
	parameters_.contourEmissiveIntensity =
		(std::max)(parameters_.contourEmissiveIntensity, 0.0f);
	parameters_.innerLineWidth = std::clamp(parameters_.innerLineWidth, 0.001f, 0.12f);
	parameters_.innerLineIntensity = std::clamp(parameters_.innerLineIntensity, 0.0f, 24.0f);
	parameters_.outerGlowWidth = std::clamp(parameters_.outerGlowWidth, 0.01f, 0.5f);
	parameters_.outerGlowIntensity = std::clamp(parameters_.outerGlowIntensity, 0.0f, 8.0f);
	parameters_.coreThreshold = std::clamp(parameters_.coreThreshold, 0.2f, 6.0f);
	parameters_.coreSoftness = std::clamp(parameters_.coreSoftness, 0.005f, 1.0f);
	parameters_.coreIntensity = std::clamp(parameters_.coreIntensity, 0.0f, 24.0f);
	parameters_.coreVerticalBias = std::clamp(parameters_.coreVerticalBias, 0.0f, 1.0f);
	parameters_.coreBreakup = std::clamp(parameters_.coreBreakup, 0.0f, 1.5f);
	parameters_.coreNoiseScale = std::clamp(parameters_.coreNoiseScale, 0.25f, 8.0f);
	parameters_.coreHotThreshold = std::clamp(parameters_.coreHotThreshold, 0.2f, 8.0f);
	parameters_.flowSpeed = std::clamp(parameters_.flowSpeed, 0.0f, 3.0f);
	parameters_.radiusScale = std::clamp(parameters_.radiusScale, 0.35f, 2.0f);
	parameters_.swayStrength = std::clamp(parameters_.swayStrength, 0.0f, 3.0f);
	parameters_.spawnSpread = std::clamp(parameters_.spawnSpread, 0.0f, 0.45f);
	parameters_.compactSupportScale = std::clamp(parameters_.compactSupportScale, 1.25f, 4.0f);
	parameters_.satelliteSeparation = std::clamp(parameters_.satelliteSeparation, 0.0f, 0.35f);
	parameters_.activeMetaballCount = std::clamp(
		parameters_.activeMetaballCount,
		6u,
		kMaxMetaballs);
	if (static_cast<uint32_t>(parameters_.displayMode) > static_cast<uint32_t>(DisplayMode::CoreHotMask)) {
		parameters_.displayMode = DisplayMode::OuterContour;
	}
}

void ProceduralFlameRenderer::Update(float deltaTime)
{
	const float scaledDeltaTime = (std::max)(deltaTime, 0.0f) * parameters_.flowSpeed;
	for (uint32_t index = 0; index < parameters_.activeMetaballCount; ++index) {
		auto& metaball = metaballs_[index];
		metaball.age += scaledDeltaTime;
		metaball.baseX += metaball.velocity.x * scaledDeltaTime;
		metaball.position.y += metaball.velocity.y * scaledDeltaTime;

		const float swayTime = metaball.age * (1.15f + metaball.seed * 1.35f);
		const float primarySway = std::sin(swayTime + metaball.phase);
		const float secondarySway = std::sin(swayTime * 0.47f + metaball.phase * 1.73f);
		const float life = std::clamp(metaball.age / metaball.lifetime, 0.0f, 1.0f);
		const float separation = metaball.isSatellite
			? SmoothStep01((life - 0.16f) / 0.58f) * parameters_.satelliteSeparation
			: 0.0f;
		metaball.position.x = metaball.baseX
			+ (primarySway * 0.72f + secondarySway * 0.28f)
			* metaball.lateralSway * parameters_.swayStrength
			+ metaball.separationDirection * separation;

		if (metaball.age >= metaball.lifetime || metaball.position.y - metaball.radius > 1.12f) {
			RespawnMetaball(index);
		}
	}
}

void ProceduralFlameRenderer::ResetMetaballs()
{
	uint32_t bodyOrdinal = 0;
	uint32_t satelliteOrdinal = 0;
	uint32_t bodyCount = 0;
	uint32_t satelliteCount = 0;
	for (uint32_t index = 0; index < kMaxMetaballs; ++index) {
		if (index % 3u == 2u) {
			++satelliteCount;
		} else {
			++bodyCount;
		}
	}
	for (uint32_t index = 0; index < kMaxMetaballs; ++index) {
		metaballs_[index].generation = 0;
		const bool satellite = index % 3u == 2u;
		const uint32_t ordinal = satellite ? satelliteOrdinal++ : bodyOrdinal++;
		const uint32_t roleCount = satellite ? satelliteCount : bodyCount;
		const float lifeFraction = (static_cast<float>(ordinal) + 0.35f) /
			static_cast<float>(roleCount);
		RespawnMetaball(index, lifeFraction);
	}
}

void ProceduralFlameRenderer::RespawnMetaball(uint32_t index, float initialLifeFraction)
{
	auto& metaball = metaballs_[index];
	const uint32_t generation = metaball.generation++;
	const uint32_t key = 0x9e3779b9u * (index + 1u) + generation * 0x85ebca6bu;
	const float sizeRandom = Hash01(key + 1u);
	const float speedRandom = Hash01(key + 2u);
	const float swayRandom = Hash01(key + 3u);
	const float offsetRandom = Hash01(key + 4u);
	metaball.seed = Hash01(key + 5u);
	metaball.phase = Hash01(key + 6u) * 6.28318530718f;

	// Two deterministic roles keep the lower field dense while guaranteeing
	// that a few smaller points can peel away above it. Positions remain fully
	// animated and seeded; the roles define tendencies, not a fixed silhouette.
	metaball.isSatellite = index % 3u == 2u;
	const bool satellite = metaball.isSatellite;
	metaball.lifetime = satellite
		? 3.0f + speedRandom * 1.2f
		: 4.8f + speedRandom * 1.5f;
	metaball.baseRadius = satellite
		? 0.048f + sizeRandom * 0.030f
		: 0.105f + sizeRandom * 0.050f;
	metaball.radius = metaball.baseRadius;
	metaball.lateralSway = satellite
		? 0.070f + swayRandom * 0.065f
		: 0.025f + swayRandom * 0.040f;
	metaball.velocity.x = (Hash01(key + 7u) - 0.5f) * (satellite ? 0.020f : 0.008f);
	const float verticalStart = satellite ? 0.40f : 0.13f;
	const float verticalTravel = satellite ? 0.64f : 0.50f;
	metaball.velocity.y = verticalTravel / metaball.lifetime;
	metaball.baseX = 0.5f + (offsetRandom - 0.5f) * parameters_.spawnSpread
		* (satellite ? 0.95f : 0.65f);
	metaball.separationDirection = Hash01(key + 8u) < 0.5f ? -1.0f : 1.0f;

	const float progress = std::clamp(initialLifeFraction, 0.0f, 0.98f);
	metaball.age = metaball.lifetime * progress;
	metaball.position.y = verticalStart + progress * verticalTravel;
	metaball.baseX += metaball.velocity.x * metaball.age;
	const float swayTime = metaball.age * (1.15f + metaball.seed * 1.35f);
	const float separation = satellite
		? SmoothStep01((progress - 0.16f) / 0.58f) * parameters_.satelliteSeparation
		: 0.0f;
	metaball.position.x = metaball.baseX + std::sin(swayTime + metaball.phase)
		* metaball.lateralSway * parameters_.swayStrength
		+ metaball.separationDirection * separation;
}

void ProceduralFlameRenderer::UploadParameters(float billboardAspect)
{
	if (!parameterData_) {
		return;
	}
	parameterData_->color = parameters_.color;
	parameterData_->coreCyanTint = parameters_.coreCyanTint;
	parameterData_->time = parameters_.time;
	parameterData_->noiseScale = parameters_.noiseScale;
	parameterData_->noiseSpeed = parameters_.noiseSpeed;
	parameterData_->distortionStrength = parameters_.distortionStrength;
	parameterData_->contourThreshold = parameters_.contourThreshold;
	parameterData_->contourWidth = parameters_.contourWidth;
	parameterData_->contourSoftness = parameters_.contourSoftness;
	parameterData_->contourEmissiveIntensity = parameters_.contourEmissiveIntensity;
	parameterData_->innerLineWidth = parameters_.innerLineWidth;
	parameterData_->innerLineIntensity = parameters_.innerLineIntensity;
	parameterData_->outerGlowWidth = parameters_.outerGlowWidth;
	parameterData_->outerGlowIntensity = parameters_.outerGlowIntensity;
	parameterData_->contourAaScale = parameters_.contourAaScale;
	parameterData_->contourWidthModulation = parameters_.contourWidthModulation;
	parameterData_->coreThreshold = parameters_.coreThreshold;
	parameterData_->coreSoftness = parameters_.coreSoftness;
	parameterData_->coreIntensity = parameters_.coreIntensity;
	parameterData_->coreVerticalBias = parameters_.coreVerticalBias;
	parameterData_->coreBreakup = parameters_.coreBreakup;
	parameterData_->coreNoiseScale = parameters_.coreNoiseScale;
	parameterData_->coreHotThreshold = parameters_.coreHotThreshold;
	parameterData_->fieldGain = parameters_.fieldGain;
	parameterData_->billboardAspect = (std::max)(billboardAspect, 0.001f);
	parameterData_->compactSupportScale = parameters_.compactSupportScale;
	parameterData_->displayMode = static_cast<uint32_t>(parameters_.displayMode);
	parameterData_->activeMetaballCount = parameters_.activeMetaballCount;

	for (uint32_t index = 0; index < kMaxMetaballs; ++index) {
		if (index >= parameters_.activeMetaballCount) {
			parameterData_->metaballs[index] = {};
			continue;
		}
		auto& metaball = metaballs_[index];
		const float life = std::clamp(metaball.age / metaball.lifetime, 0.0f, 1.0f);
		const float fadeIn = SmoothStep01(life / (metaball.isSatellite ? 0.10f : 0.07f));
		const float fadeOutStart = metaball.isSatellite ? 0.78f : 0.88f;
		const float fadeOut = 1.0f - SmoothStep01(
			(life - fadeOutStart) / (1.0f - fadeOutStart));
		const float upperShrinkStart = metaball.isSatellite ? 0.58f : 0.70f;
		const float upperShrink = SmoothStep01(
			(life - upperShrinkStart) / (1.0f - upperShrinkStart));
		const float pulse = 0.91f + 0.09f * std::sin(
			metaball.age * (2.2f + metaball.seed * 1.6f) + metaball.phase);
		metaball.radius = metaball.baseRadius * parameters_.radiusScale * pulse
			* (1.0f - upperShrink * (metaball.isSatellite ? 0.58f : 0.32f));
		parameterData_->metaballs[index] = {
			metaball.position.x,
			metaball.position.y,
			metaball.radius,
			fadeIn * fadeOut,
		};
	}
}

void ProceduralFlameRenderer::Draw(
	const Vector3& center,
	const Vector2& size,
	const Matrix4x4& cameraWorld,
	const Matrix4x4& viewProjection)
{
	if (!dxCommon_ || !pipelineState_ || size.x <= 0.0f || size.y <= 0.0f) {
		return;
	}

	const Vector3 right = Normalize({ cameraWorld.m[0][0], cameraWorld.m[0][1], cameraWorld.m[0][2] });
	const Vector3 up = Normalize({ cameraWorld.m[1][0], cameraWorld.m[1][1], cameraWorld.m[1][2] });
	const Vector3 bottomCenter = center;
	const Vector3 topCenter = center + up * size.y;
	const Vector3 halfRight = right * (size.x * 0.5f);

	const Vector3 bottomLeft = bottomCenter - halfRight;
	const Vector3 bottomRight = bottomCenter + halfRight;
	const Vector3 topLeft = topCenter - halfRight;
	const Vector3 topRight = topCenter + halfRight;
	const Vector4 white = { 1.0f, 1.0f, 1.0f, 1.0f };
	vertexData_[0] = { bottomLeft, white, { 0.0f, 1.0f } };
	vertexData_[1] = { topLeft, white, { 0.0f, 0.0f } };
	vertexData_[2] = { bottomRight, white, { 1.0f, 1.0f } };
	vertexData_[3] = { topLeft, white, { 0.0f, 0.0f } };
	vertexData_[4] = { topRight, white, { 1.0f, 0.0f } };
	vertexData_[5] = { bottomRight, white, { 1.0f, 1.0f } };
	*viewProjectionData_ = viewProjection;
	UploadParameters(size.x / size.y);

	auto commandList = dxCommon_->GetList();
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(pipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->SetGraphicsRootConstantBufferView(0, viewProjectionResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1, parameterResource_->GetGPUVirtualAddress());
	commandList->DrawInstanced(6, 1, 0, 0);
}

void ProceduralFlameRenderer::CreatePipeline()
{
	D3D12_ROOT_PARAMETER rootParameters[2]{};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[1].Descriptor.ShaderRegister = 0;

	D3D12_ROOT_SIGNATURE_DESC rootDesc{};
	rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootDesc.NumParameters = static_cast<UINT>(std::size(rootParameters));
	rootDesc.pParameters = rootParameters;

	ComPtr<ID3DBlob> signatureBlob;
	ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(
		&rootDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&signatureBlob,
		&errorBlob);
	assert(SUCCEEDED(hr));
	hr = dxCommon_->GetDevice()->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));

	IDxcBlob* vertexShader = dxCommon_->CompileShader(
		L"resources/shaders/ProceduralFlame.VS.hlsl", L"vs_6_0");
	IDxcBlob* pixelShader = dxCommon_->CompileShader(
		L"resources/shaders/ProceduralFlame.PS.hlsl", L"ps_6_0");
	assert(vertexShader && pixelShader);

	D3D12_INPUT_ELEMENT_DESC inputElements[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSignature_.Get();
	desc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
	desc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
	desc.InputLayout = { inputElements, static_cast<UINT>(std::size(inputElements)) };
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	desc.SampleDesc.Count = 1;
	desc.NumRenderTargets = 3;
	desc.RTVFormats[0] = DirectXCommon::kSceneRenderTargetFormat;
	desc.RTVFormats[1] = DirectXCommon::kNormalBufferFormat;
	desc.RTVFormats[2] = DirectXCommon::kMaterialBufferFormat;
	desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	desc.RasterizerState.DepthClipEnable = TRUE;
	desc.DepthStencilState.DepthEnable = TRUE;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	desc.DepthStencilState.StencilEnable = FALSE;
	desc.BlendState.IndependentBlendEnable = TRUE;

	auto& colorBlend = desc.BlendState.RenderTarget[0];
	colorBlend.BlendEnable = TRUE;
	colorBlend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	colorBlend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	colorBlend.BlendOp = D3D12_BLEND_OP_ADD;
	colorBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
	colorBlend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	colorBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	colorBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	for (UINT target = 1; target < 3; ++target) {
		desc.BlendState.RenderTarget[target].RenderTargetWriteMask = 0;
	}

	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineState_));
	assert(SUCCEEDED(hr));
	vertexShader->Release();
	pixelShader->Release();
}
