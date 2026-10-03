#include "NeonSkinnedRenderer.h"

#include "DirectXCommon.h"
#include "InputDesc.h"
#include "SkinCluster.h"
#include "SrvManager.h"
#include "WinApp.h"
#include <algorithm>
#include <cmath>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace cg2 {

using Microsoft::WRL::ComPtr;

namespace {
constexpr UINT kTransformRootParameter = 0;
constexpr UINT kNeonRootParameter = 1;
constexpr UINT kPaletteRootParameter = 2;
constexpr UINT kSurfaceRootParameter = 3;
constexpr UINT kSubmeshRootParameter = 4;
constexpr UINT kFeatureMaskRootParameter = 5;
constexpr UINT8 kOutlineStencilMask = 0x80;

/// @brief 結果を確認する。
void CheckResult(HRESULT hr, const char* message) {
	if (FAILED(hr)) {
		throw std::runtime_error(message);
	}
}

std::string ResultMessage(const char* message, HRESULT hr) {
	std::ostringstream result;
	result << message << " (HRESULT 0x" << std::hex << static_cast<unsigned long>(hr) << ")";
	return result.str();
}

// ネイティブBarycentricsだけを使用する。判定不能・未対応なら既存のSM6.0描画を維持する。
bool QueryGeometryLinesSupport(ID3D12Device* device, std::string& status) {
	D3D12_FEATURE_DATA_SHADER_MODEL shaderModel{ D3D_SHADER_MODEL_6_1 };
	const HRESULT shaderResult = device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL,
		&shaderModel, sizeof(shaderModel));
	if (FAILED(shaderResult)) {
		status = ResultMessage("Unavailable: Shader Model 6.1 query failed", shaderResult);
		return false;
	}
	if (shaderModel.HighestShaderModel < D3D_SHADER_MODEL_6_1) {
		status = "Unavailable: Shader Model 6.1 is required.";
		return false;
	}
	D3D12_FEATURE_DATA_D3D12_OPTIONS3 options{};
	const HRESULT optionsResult = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS3,
		&options, sizeof(options));
	if (FAILED(optionsResult)) {
		status = ResultMessage("Unavailable: D3D12 OPTIONS3 query failed", optionsResult);
		return false;
	}
	if (!options.BarycentricsSupported) {
		status = "Unavailable: OPTIONS3.BarycentricsSupported is false.";
		return false;
	}
	status = "Supported: Shader Model 6.1 / native SV_Barycentrics (ps_6_1).";
	return true;
}
}

void NeonSkinnedRenderer::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, bool enableGeometryLinePipeline) {
	if (!dxCommon || !srvManager || !dxCommon->GetDevice()) {
		throw std::invalid_argument("NeonSkinnedRenderer requires an initialized DirectXCommon and SrvManager.");
	}
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	// 有効なnull Texture2D SRVは0を返す。G=0なので未指定のSubmeshは自動線を維持する。
	// 描画中に書き換えず、SrvManagerのHeapと共に保持する。
	nullFeatureMaskSrvIndex_ = srvManager_->Allocate();
	if (nullFeatureMaskSrvIndex_ == 0 || nullFeatureMaskSrvIndex_ >= SrvManager::kMaxSrvCount) {
		throw std::runtime_error("Failed to allocate the Neon feature-mask fallback SRV.");
	}
	D3D12_SHADER_RESOURCE_VIEW_DESC nullMaskDesc{};
	nullMaskDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	nullMaskDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	nullMaskDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	nullMaskDesc.Texture2D.MipLevels = 1;
	dxCommon_->GetDevice()->CreateShaderResourceView(nullptr, &nullMaskDesc,
		srvManager_->GetCPUDescriptorHandle(nullFeatureMaskSrvIndex_));
	geometryPipelineState_.Reset();
	doubleSidedGeometryPipelineState_.Reset();
	geometryLinesSupported_ = QueryGeometryLinesSupport(dxCommon_->GetDevice().Get(), geometryLinesStatus_);
	if (!enableGeometryLinePipeline) {
		geometryLinesSupported_ = false;
		geometryLinesStatus_ = "Disabled by caller: using the original Neon body pipeline.";
	}
	OutputDebugStringA(("Neon mesh geometry lines: " + geometryLinesStatus_ + "\n").c_str());
	CreatePipeline();
	drawConstantBuffers_.clear();
	nextDrawIndex_ = 0;
	submeshParams_.clear();
	featureMaskSrvIndices_.clear();
}

void NeonSkinnedRenderer::BeginFrame() {
	if (!dxCommon_) {
		throw std::logic_error("Initialize NeonSkinnedRenderer before BeginFrame.");
	}
	// 現エンジンはフレーム終端でFenceを待つ。将来Frames-in-flight化する場合も
	// 実行中のCBを再利用しないよう、この契約またはフレーム別の領域管理を維持する。
	const auto fence = dxCommon_->GetFence();
	if (fence && fence->GetCompletedValue() < dxCommon_->GetFenceValue()) {
		throw std::logic_error("Wait for the previous GPU submission before NeonSkinnedRenderer::BeginFrame.");
	}
	nextDrawIndex_ = 0;
}

void NeonSkinnedRenderer::SetParams(const NeonSkinnedParams& params) {
	params_ = params;
	params_.emissiveIntensity = (std::max)(params_.emissiveIntensity, 0.0f);
	params_.rimStrength = (std::max)(params_.rimStrength, 0.0f);
	params_.rimPower = (std::max)(params_.rimPower, 0.001f);
	params_.outlineWidthPixels = std::isfinite(params_.outlineWidthPixels)
		? (std::clamp)(params_.outlineWidthPixels, 0.0f, 8.0f) : 0.0f;
	params_.outlineEnabled = params_.outlineEnabled ? 1u : 0u;
	params_.internalLineEnabled = params_.internalLineEnabled ? 1u : 0u;
	params_.internalLineWidthPixels = std::isfinite(params_.internalLineWidthPixels)
		? (std::clamp)(params_.internalLineWidthPixels, 0.0f, 4.0f) : 0.0f;
	params_.internalLineIntensity = std::isfinite(params_.internalLineIntensity)
		? (std::max)(params_.internalLineIntensity, 0.0f) : 0.0f;
	params_.internalLineThreshold = std::isfinite(params_.internalLineThreshold)
		? (std::clamp)(params_.internalLineThreshold, 0.001f, 1.0f) : 1.0f;
	params_.geometryLineEnabled = params_.geometryLineEnabled ? 1u : 0u;
	params_.geometryLineWidthPixels = std::isfinite(params_.geometryLineWidthPixels)
		? (std::clamp)(params_.geometryLineWidthPixels, 0.0f, 8.0f) : 0.0f;
	params_.geometryLineIntensity = std::isfinite(params_.geometryLineIntensity)
		? (std::clamp)(params_.geometryLineIntensity, 0.0f, 100.0f) : 0.0f;
	const auto colorComponent = [](float value) {
		return std::isfinite(value) ? (std::clamp)(value, 0.0f, 100.0f) : 0.0f;
	};
	params_.geometryLineColor = { colorComponent(params_.geometryLineColor.x),
		colorComponent(params_.geometryLineColor.y), colorComponent(params_.geometryLineColor.z) };
	params_.bodyEmissionIntensity = std::isfinite(params_.bodyEmissionIntensity)
		? (std::clamp)(params_.bodyEmissionIntensity, 0.0f, 4.0f) : 0.0f;
	params_.bodyPadding = 0.0f;
	params_.featureMaskColor = { colorComponent(params_.featureMaskColor.x),
		colorComponent(params_.featureMaskColor.y), colorComponent(params_.featureMaskColor.z) };
	params_.featureMaskIntensity = std::isfinite(params_.featureMaskIntensity)
		? (std::clamp)(params_.featureMaskIntensity, 0.0f, 100.0f) : 0.0f;
	params_.featureMaskBlend = std::isfinite(params_.featureMaskBlend)
		? (std::clamp)(params_.featureMaskBlend, 0.0f, 1.0f) : 0.0f;
	params_.featureMaskDebugMode = params_.featureMaskDebugMode <= 2 ? params_.featureMaskDebugMode : 0;
	params_.featureMaskPadding[0] = params_.featureMaskPadding[1] = 0.0f;
}

void NeonSkinnedRenderer::SetSubmeshFeatureMasks(const std::vector<std::optional<uint32_t>>& srvIndices) {
	featureMaskSrvIndices_ = srvIndices;
	for (auto& index : featureMaskSrvIndices_) {
		if (index && (*index == 0 || *index >= SrvManager::kMaxSrvCount)) index.reset();
	}
}

void NeonSkinnedRenderer::SetSubmeshParams(const std::vector<NeonSkinnedSubmeshParams>& params) {
	submeshParams_ = params;
	for (auto& submesh : submeshParams_) {
		submesh.lineStrength = std::isfinite(submesh.lineStrength)
			? (std::clamp)(submesh.lineStrength, 0.0f, 2.0f) : 0.0f;
		submesh.alphaCutoff = std::isfinite(submesh.alphaCutoff)
			? (std::clamp)(submesh.alphaCutoff, 0.0f, 1.0f) : 0.0f;
		submesh.geometryLineStrength = std::isfinite(submesh.geometryLineStrength)
			? (std::clamp)(submesh.geometryLineStrength, 0.0f, 2.0f) : 0.0f;
		submesh.internalLineThresholdScale = std::isfinite(submesh.internalLineThresholdScale)
			? (std::clamp)(submesh.internalLineThresholdScale, 0.1f, 4.0f) : 1.0f;
	}
}

void NeonSkinnedRenderer::BindSubmeshSurface(const SkinnedModel& model, size_t index) {
	const auto& asset = model.GetAsset();
	const auto& submesh = model.GetSubmesh(index);
	const auto& materials = asset.modelData.materials;
	const auto& material = materials.empty() ? asset.modelData.material
		: materials[submesh.materialIndex < materials.size() ? submesh.materialIndex : 0];
	// SkinnedModel::Initializeで既に読み込まれたBaseColor SRVを共有する。
	auto commandList = dxCommon_->GetList();
	commandList->SetGraphicsRootDescriptorTable(kSurfaceRootParameter,
		srvManager_->GetGPUDescriptorHandle(material.textureIndex));
	const auto maskIndex = featureMaskSrvIndices_.empty() ? std::nullopt : featureMaskSrvIndices_[index];
	commandList->SetGraphicsRootDescriptorTable(kFeatureMaskRootParameter,
		srvManager_->GetGPUDescriptorHandle(maskIndex.value_or(nullFeatureMaskSrvIndex_)));
	const auto surface = submeshParams_.empty() ? NeonSkinnedSubmeshParams{} : submeshParams_[index];
	commandList->SetGraphicsRoot32BitConstants(kSubmeshRootParameter,
		sizeof(surface) / sizeof(uint32_t), &surface, 0);
}

NeonSkinnedRenderer::DrawConstantBuffer& NeonSkinnedRenderer::AcquireDrawConstantBuffer() {
	if (nextDrawIndex_ == drawConstantBuffers_.size()) {
		DrawConstantBuffer buffer;
		// Root CBVの256byte alignmentとGPUが読む領域を確保する。
		buffer.resource = dxCommon_->CreateBufferResource(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		if (!buffer.resource) {
			throw std::runtime_error("Failed to allocate NeonSkinnedRenderer constants.");
		}
		const D3D12_RANGE noCpuReads{ 0, 0 };
		CheckResult(buffer.resource->Map(0, &noCpuReads, reinterpret_cast<void**>(&buffer.mapped)),
			"Failed to map NeonSkinnedRenderer constants.");
		drawConstantBuffers_.push_back(std::move(buffer));
	}
	return drawConstantBuffers_[nextDrawIndex_++];
}

void NeonSkinnedRenderer::Draw(const SkinnedModel& model,
	D3D12_GPU_VIRTUAL_ADDRESS transformationCbv, const Vector3& cameraWorldPosition) {
	Draw(model, transformationCbv, cameraWorldPosition,
		{ static_cast<float>(WinApp::kClientWidth), static_cast<float>(WinApp::kClientHeight) });
}

void NeonSkinnedRenderer::ClearOutlineStencil() {
	auto commandList = dxCommon_->GetList();
	commandList->SetPipelineState(stencilClearPipelineState_.Get());
	commandList->OMSetStencilRef(0);
	commandList->DrawInstanced(3, 1, 0, 0);
}

void NeonSkinnedRenderer::Draw(const SkinnedModel& model,
	D3D12_GPU_VIRTUAL_ADDRESS transformationCbv, const Vector3& cameraWorldPosition, const Vector2& viewportSize) {
	if (!rootSignature_ || !pipelineState_ || !doubleSidedPipelineState_ || !outlinePipelineState_ ||
		!doubleSidedOutlinePipelineState_ || !stencilClearPipelineState_) {
		throw std::logic_error("Initialize NeonSkinnedRenderer before Draw.");
	}
	if (transformationCbv == 0 || transformationCbv % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT != 0) {
		throw std::invalid_argument("NeonSkinnedRenderer requires a valid aligned transform CBV.");
	}
	if (model.GetSubmeshCount() == 0) {
		return;
	}
	if (!submeshParams_.empty() && submeshParams_.size() != model.GetSubmeshCount()) {
		throw std::invalid_argument("Neon surface settings must match the model submesh count.");
	}
	if (!featureMaskSrvIndices_.empty() && featureMaskSrvIndices_.size() != model.GetSubmeshCount()) {
		throw std::invalid_argument("Neon feature-mask bindings must match the model submesh count.");
	}
	if (!dxCommon_->HasCurrentDSV() || !std::isfinite(viewportSize.x) || !std::isfinite(viewportSize.y) ||
		viewportSize.x <= 0.0f || viewportSize.y <= 0.0f) {
		throw std::invalid_argument("Neon outline requires the Scene D24S8 target and a valid viewport size.");
	}

	DrawConstantBuffer& constants = AcquireDrawConstantBuffer();
	*constants.mapped = { params_, cameraWorldPosition, 0.0f, viewportSize, {} };

	auto commandList = dxCommon_->GetList();
	srvManager_->PreDraw();
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(pipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->SetGraphicsRootConstantBufferView(kTransformRootParameter, transformationCbv);
	commandList->SetGraphicsRootConstantBufferView(kNeonRootParameter, constants.resource->GetGPUVirtualAddress());
	model.BindGeometry();
	model.BindSkinningPalette(kPaletteRootParameter);
	// Sceneの他のStencil bitやDepth/Colorに触れず、このモデルのマスクを準備する。
	ClearOutlineStencil();
	commandList->OMSetStencilRef(kOutlineStencilMask);
	for (size_t index = 0; index < model.GetSubmeshCount(); ++index) {
		BindSubmeshSurface(model, index);
		const bool geometry = geometryLinesSupported_ && params_.geometryLineEnabled &&
			params_.geometryLineWidthPixels > 0.0f && params_.geometryLineIntensity > 0.0f &&
			!submeshParams_.empty() && submeshParams_[index].geometryLineStrength > 0.0f;
		const bool doubleSided = model.GetSubmesh(index).doubleSided;
		commandList->SetPipelineState(geometry
			? (doubleSided ? doubleSidedGeometryPipelineState_.Get() : geometryPipelineState_.Get())
			: (doubleSided ? doubleSidedPipelineState_.Get() : pipelineState_.Get()));
		model.DrawSubmesh(index);
	}
	if (params_.outlineEnabled && params_.outlineWidthPixels > 0.0f && params_.emissiveIntensity > 0.0f) {
		for (size_t index = 0; index < model.GetSubmeshCount(); ++index) {
			BindSubmeshSurface(model, index);
			commandList->SetPipelineState(model.GetSubmesh(index).doubleSided
				? doubleSidedOutlinePipelineState_.Get() : outlinePipelineState_.Get());
			model.DrawSubmesh(index);
		}
	}
	ClearOutlineStencil(); // 次のモデル・後続パスへこのモデルのマスクを残さない。
}

void NeonSkinnedRenderer::CreatePipeline() {
	D3D12_DESCRIPTOR_RANGE paletteRange{};
	paletteRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	paletteRange.NumDescriptors = 1;
	paletteRange.BaseShaderRegister = 3;
	paletteRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE surfaceRange{};
	surfaceRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	surfaceRange.NumDescriptors = 1;
	surfaceRange.BaseShaderRegister = 0;
	surfaceRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	D3D12_DESCRIPTOR_RANGE maskRange{};
	maskRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	maskRange.NumDescriptors = 1;
	maskRange.BaseShaderRegister = 1;
	maskRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	D3D12_ROOT_PARAMETER rootParameters[6]{};
	rootParameters[kTransformRootParameter].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[kTransformRootParameter].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[kTransformRootParameter].Descriptor.ShaderRegister = 0;
	rootParameters[kNeonRootParameter].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[kNeonRootParameter].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[kNeonRootParameter].Descriptor.ShaderRegister = 1;
	rootParameters[kPaletteRootParameter].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[kPaletteRootParameter].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[kPaletteRootParameter].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[kPaletteRootParameter].DescriptorTable.pDescriptorRanges = &paletteRange;
	rootParameters[kSurfaceRootParameter].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[kSurfaceRootParameter].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[kSurfaceRootParameter].DescriptorTable = { 1, &surfaceRange };
	rootParameters[kSubmeshRootParameter].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameters[kSubmeshRootParameter].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[kSubmeshRootParameter].Constants.ShaderRegister = 2;
	rootParameters[kSubmeshRootParameter].Constants.Num32BitValues = sizeof(NeonSkinnedSubmeshParams) / sizeof(uint32_t);
	rootParameters[kFeatureMaskRootParameter].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[kFeatureMaskRootParameter].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[kFeatureMaskRootParameter].DescriptorTable = { 1, &maskRange };
	D3D12_STATIC_SAMPLER_DESC sampler{};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.MaxAnisotropy = 1;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rootDesc{};
	rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootDesc.NumParameters = static_cast<UINT>(std::size(rootParameters));
	rootDesc.pParameters = rootParameters;
	rootDesc.NumStaticSamplers = 1;
	rootDesc.pStaticSamplers = &sampler;
	ComPtr<ID3DBlob> signatureBlob;
	ComPtr<ID3DBlob> errorBlob;
	const HRESULT serializeResult = D3D12SerializeRootSignature(&rootDesc,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (errorBlob) {
		OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
	}
	CheckResult(serializeResult, "Failed to serialize NeonSkinnedRenderer root signature.");
	CheckResult(dxCommon_->GetDevice()->CreateRootSignature(0,
		signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_)),
		"Failed to create NeonSkinnedRenderer root signature.");

	ComPtr<IDxcBlob> vertexShader;
	ComPtr<IDxcBlob> pixelShader;
	ComPtr<IDxcBlob> outlineVertexShader;
	ComPtr<IDxcBlob> outlinePixelShader;
	ComPtr<IDxcBlob> stencilClearVertexShader;
	ComPtr<IDxcBlob> stencilClearPixelShader;
	vertexShader.Attach(dxCommon_->CompileShader(L"resources/shaders/NeonSkinned.VS.hlsl", L"vs_6_0"));
	pixelShader.Attach(dxCommon_->CompileShader(L"resources/shaders/NeonSkinned.PS.hlsl", L"ps_6_0"));
	outlineVertexShader.Attach(dxCommon_->CompileShader(L"resources/shaders/NeonSkinnedOutline.VS.hlsl", L"vs_6_0"));
	outlinePixelShader.Attach(dxCommon_->CompileShader(L"resources/shaders/NeonSkinnedOutline.PS.hlsl", L"ps_6_0"));
	stencilClearVertexShader.Attach(dxCommon_->CompileShader(L"resources/shaders/NeonSkinnedStencilClear.VS.hlsl", L"vs_6_0"));
	stencilClearPixelShader.Attach(dxCommon_->CompileShader(L"resources/shaders/NeonSkinnedStencilClear.PS.hlsl", L"ps_6_0"));
	if (!vertexShader || !pixelShader || !outlineVertexShader || !outlinePixelShader || !stencilClearVertexShader || !stencilClearPixelShader) {
		throw std::runtime_error("Failed to compile NeonSkinnedRenderer shaders.");
	}

	// InputDescの既存Skinning用レイアウトをそのまま共有する。
	InputDesc inputDesc;
	inputDesc.InitializeForSkinning();
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSignature_.Get();
	desc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
	desc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
	desc.InputLayout = inputDesc.GetLayout();
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	desc.SampleDesc.Count = 1;
	desc.NumRenderTargets = 3;
	desc.RTVFormats[0] = DirectXCommon::kSceneRenderTargetFormat;
	desc.RTVFormats[1] = DirectXCommon::kNormalBufferFormat;
	desc.RTVFormats[2] = DirectXCommon::kMaterialBufferFormat;
	desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	desc.RasterizerState.DepthClipEnable = TRUE;
	desc.DepthStencilState.DepthEnable = TRUE;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	desc.DepthStencilState.StencilEnable = TRUE;
	desc.DepthStencilState.StencilReadMask = kOutlineStencilMask;
	desc.DepthStencilState.StencilWriteMask = kOutlineStencilMask;
	desc.DepthStencilState.FrontFace = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP,
		D3D12_STENCIL_OP_REPLACE, D3D12_COMPARISON_FUNC_ALWAYS };
	desc.DepthStencilState.BackFace = desc.DepthStencilState.FrontFace;
	// 不透明Dark Body + HDR emission。加算ブレンドではなくShader内で合成する。
	desc.BlendState.IndependentBlendEnable = TRUE;
	for (UINT target = 0; target < desc.NumRenderTargets; ++target) {
		auto& blend = desc.BlendState.RenderTarget[target];
		blend.SrcBlend = D3D12_BLEND_ONE;
		blend.DestBlend = D3D12_BLEND_ZERO;
		blend.BlendOp = D3D12_BLEND_OP_ADD;
		blend.SrcBlendAlpha = D3D12_BLEND_ONE;
		blend.DestBlendAlpha = D3D12_BLEND_ZERO;
		blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		blend.LogicOp = D3D12_LOGIC_OP_NOOP;
		blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	}
	CheckResult(dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineState_)),
		"Failed to create NeonSkinnedRenderer back-cull PSO.");
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	CheckResult(dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&doubleSidedPipelineState_)),
		"Failed to create NeonSkinnedRenderer double-sided PSO.");
	if (geometryLinesSupported_) {
		ComPtr<IDxcBlob> geometryPixelShader;
		geometryPixelShader.Attach(dxCommon_->CompileShader(L"resources/shaders/NeonSkinnedGeometry.PS.hlsl", L"ps_6_1"));
		if (!geometryPixelShader) {
			throw std::runtime_error("Native barycentrics are supported, but NeonSkinnedGeometry.PS.hlsl (ps_6_1) compilation failed. Check DXC diagnostics.");
		}
		// VS/Input/Depth/Stencil/MRTはBodyと同一。SOLIDのPS内で発光を加える。
		desc.PS = { geometryPixelShader->GetBufferPointer(), geometryPixelShader->GetBufferSize() };
		desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&geometryPipelineState_));
		if (FAILED(hr)) throw std::runtime_error(ResultMessage("Supported device: geometry back-cull PSO creation failed", hr));
		desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&doubleSidedGeometryPipelineState_));
		if (FAILED(hr)) throw std::runtime_error(ResultMessage("Supported device: geometry double-sided PSO creation failed", hr));
	}

	// 膨張したHullを描き、モデル全体のStencilの外側だけを残す。
	// DoubleSidedの薄い髪等は両面を対象にする。Depthは読み取りのみ。
	desc.VS = { outlineVertexShader->GetBufferPointer(), outlineVertexShader->GetBufferSize() };
	desc.PS = { outlinePixelShader->GetBufferPointer(), outlinePixelShader->GetBufferSize() };
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.DepthStencilState.StencilWriteMask = 0;
	desc.DepthStencilState.FrontFace = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP,
		D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_NOT_EQUAL };
	desc.DepthStencilState.BackFace = desc.DepthStencilState.FrontFace;
	desc.BlendState.RenderTarget[1].RenderTargetWriteMask = 0;
	desc.BlendState.RenderTarget[2].RenderTargetWriteMask = 0;
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
	CheckResult(dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&outlinePipelineState_)),
		"Failed to create NeonSkinnedRenderer outline PSO.");
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	CheckResult(dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&doubleSidedOutlinePipelineState_)),
		"Failed to create NeonSkinnedRenderer double-sided outline PSO.");

	// Fullscreen triangleで予約bitのみを消去。他のStencil利用者を壊さない。
	desc.VS = { stencilClearVertexShader->GetBufferPointer(), stencilClearVertexShader->GetBufferSize() };
	desc.PS = { stencilClearPixelShader->GetBufferPointer(), stencilClearPixelShader->GetBufferSize() };
	desc.InputLayout = {};
	desc.DepthStencilState.DepthEnable = FALSE;
	desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	desc.DepthStencilState.StencilWriteMask = kOutlineStencilMask;
	desc.DepthStencilState.FrontFace = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP,
		D3D12_STENCIL_OP_REPLACE, D3D12_COMPARISON_FUNC_ALWAYS };
	desc.DepthStencilState.BackFace = desc.DepthStencilState.FrontFace;
	desc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0;
	CheckResult(dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&stencilClearPipelineState_)),
		"Failed to create NeonSkinnedRenderer stencil-clear PSO.");
}

} // namespace cg2
