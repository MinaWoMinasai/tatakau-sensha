#include "ProceduralFlameRenderer.h"

#include <algorithm>
#include <cassert>

using Microsoft::WRL::ComPtr;

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

	static_assert(sizeof(Parameters) % 16 == 0);
	parameterResource_ = dxCommon_->CreateBufferResource(sizeof(Parameters));
	parameterResource_->Map(0, nullptr, reinterpret_cast<void**>(&parameterData_));
	*parameterData_ = parameters_;
}

void ProceduralFlameRenderer::SetParameters(const Parameters& parameters)
{
	parameters_ = parameters;
	parameters_.noiseScale = (std::max)(parameters_.noiseScale, 0.01f);
	parameters_.noiseSpeed = (std::max)(parameters_.noiseSpeed, 0.0f);
	parameters_.distortionStrength = std::clamp(parameters_.distortionStrength, 0.0f, 0.45f);
	parameters_.flameWidth = std::clamp(parameters_.flameWidth, 0.05f, 1.5f);
	parameters_.flameHeight = std::clamp(parameters_.flameHeight, 0.05f, 1.0f);
	parameters_.edgeSoftness = std::clamp(parameters_.edgeSoftness, 0.001f, 0.15f);
	parameters_.threshold = std::clamp(parameters_.threshold, -0.2f, 0.2f);
	parameters_.emissiveIntensity = (std::max)(parameters_.emissiveIntensity, 0.0f);
	parameters_.debugMask = parameters_.debugMask > 0.5f ? 1.0f : 0.0f;
	parameters_.bodyRoundness = std::clamp(parameters_.bodyRoundness, 0.5f, 1.5f);
	parameters_.neckWidth = std::clamp(parameters_.neckWidth, 0.2f, 1.0f);
	parameters_.tongueStrength = std::clamp(parameters_.tongueStrength, 0.0f, 1.5f);
	if (parameterData_) {
		*parameterData_ = parameters_;
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
	*parameterData_ = parameters_;

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
