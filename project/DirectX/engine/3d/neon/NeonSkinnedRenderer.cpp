#include "NeonSkinnedRenderer.h"

#include "DirectXCommon.h"
#include "InputDesc.h"
#include "SkinCluster.h"
#include "SrvManager.h"
#include "WinApp.h"
#include <algorithm>
#include <cmath>
#include <iterator>
#include <stdexcept>
#include <utility>

using Microsoft::WRL::ComPtr;

namespace {
constexpr UINT kTransformRootParameter = 0;
constexpr UINT kNeonRootParameter = 1;
constexpr UINT kPaletteRootParameter = 2;
constexpr UINT8 kOutlineStencilMask = 0x80;

void CheckResult(HRESULT hr, const char* message) {
	if (FAILED(hr)) {
		throw std::runtime_error(message);
	}
}
}

void NeonSkinnedRenderer::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
	if (!dxCommon || !srvManager || !dxCommon->GetDevice()) {
		throw std::invalid_argument("NeonSkinnedRenderer requires an initialized DirectXCommon and SrvManager.");
	}
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	CreatePipeline();
	drawConstantBuffers_.clear();
	nextDrawIndex_ = 0;
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
		commandList->SetPipelineState(model.GetSubmesh(index).doubleSided
			? doubleSidedPipelineState_.Get() : pipelineState_.Get());
		model.DrawSubmesh(index);
	}
	if (params_.outlineEnabled && params_.outlineWidthPixels > 0.0f && params_.emissiveIntensity > 0.0f) {
		for (size_t index = 0; index < model.GetSubmeshCount(); ++index) {
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

	D3D12_ROOT_PARAMETER rootParameters[3]{};
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

	D3D12_ROOT_SIGNATURE_DESC rootDesc{};
	rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootDesc.NumParameters = static_cast<UINT>(std::size(rootParameters));
	rootDesc.pParameters = rootParameters;
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
	vertexShader.Attach(dxCommon_->CompileShader(L"resources/shaders/NeonSkinned.VS.hlsl", L"vs_6_0"));
	pixelShader.Attach(dxCommon_->CompileShader(L"resources/shaders/NeonSkinned.PS.hlsl", L"ps_6_0"));
	outlineVertexShader.Attach(dxCommon_->CompileShader(L"resources/shaders/NeonSkinnedOutline.VS.hlsl", L"vs_6_0"));
	outlinePixelShader.Attach(dxCommon_->CompileShader(L"resources/shaders/NeonSkinnedOutline.PS.hlsl", L"ps_6_0"));
	stencilClearVertexShader.Attach(dxCommon_->CompileShader(L"resources/shaders/NeonSkinnedStencilClear.VS.hlsl", L"vs_6_0"));
	if (!vertexShader || !pixelShader || !outlineVertexShader || !outlinePixelShader || !stencilClearVertexShader) {
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
