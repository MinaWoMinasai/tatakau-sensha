#include "PSO.h"

void PSO::Initialize(DirectXCommon& dxCommon, const std::wstring& VSPath, const std::wstring& PSPath, int shaderType)
{
	shaderType_ = static_cast<ShaderType>(shaderType);

	switch (shaderType_)
	{
	case Object:
		root_.InitalizeForObject();
		break;
	case Particle:
		root_.InitalizeForParticle();
	}

	root_.Create(dxCommon.GetDevice());

	vertexShaderBlob_ = compileShader.Initialize(VSPath,
		L"vs_6_0", dxCommon.GetDxcUtils(), dxCommon.GetDxcCompiler(), dxCommon.GetIncludeHandler());
	assert(vertexShaderBlob_ != nullptr);

	pixelShaderBlob_ = compileShader.Initialize(PSPath,
		L"ps_6_0", dxCommon.GetDxcUtils(), dxCommon.GetDxcCompiler(), dxCommon.GetIncludeHandler());
	assert(pixelShaderBlob_ != nullptr);

	inputDesc_.Initialize();

	state_.Initialize();

}

void PSO::Graphics()
{

	graphicsDesc_.pRootSignature = root_.GetSignature().Get();// RootSignature
	graphicsDesc_.InputLayout = inputDesc_.GetLayout();// InputLayout
	graphicsDesc_.VS = { vertexShaderBlob_->GetBufferPointer(),
	vertexShaderBlob_->GetBufferSize() };// VertexShader
	graphicsDesc_.PS = { pixelShaderBlob_->GetBufferPointer(),
	pixelShaderBlob_->GetBufferSize() };// pixelShader
	graphicsDesc_.BlendState = state_.GetBlendDesc();// BlendState
	graphicsDesc_.RasterizerState = state_.GetRasterizerDesc();// RasterizerState
	// 書き込むRTVの情報
	graphicsDesc_.NumRenderTargets = 1;
	graphicsDesc_.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	// 利用するトロポジ(形状)のタイプ、三角形
	graphicsDesc_.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// どのように画面に色を打ち込むかの設定
	graphicsDesc_.SampleDesc.Count = 1;
	graphicsDesc_.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	// DepthStencilの設定
	graphicsDesc_.DepthStencilState = state_.GetDepthStencilDesc();
	graphicsDesc_.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

}

void PSO::GraphicsLine()
{

	graphicsDescLine_.pRootSignature = root_.GetSignature().Get();// RootSignature
	graphicsDescLine_.InputLayout = inputDesc_.GetLayout();// InputLayout
	graphicsDescLine_.VS = { vertexShaderBlob_->GetBufferPointer(),
	vertexShaderBlob_->GetBufferSize() };// VertexShader
	graphicsDescLine_.PS = { pixelShaderBlob_->GetBufferPointer(),
	pixelShaderBlob_->GetBufferSize() };// pixelShader
	graphicsDescLine_.BlendState = state_.GetBlendDesc();// BlendState
	graphicsDescLine_.RasterizerState = state_.GetRasterizerDesc();// RasterizerState
	// 書き込むRTVの情報
	graphicsDescLine_.NumRenderTargets = 1;
	graphicsDescLine_.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	// 利用するトロポジ(形状)のタイプ、線
	graphicsDescLine_.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	// どのように画面に色を打ち込むかの設定
	graphicsDescLine_.SampleDesc.Count = 1;
	graphicsDescLine_.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	// DepthStencilの設定
	graphicsDescLine_.DepthStencilState = state_.GetDepthStencilDesc();
	graphicsDescLine_.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

}

void PSO::Create(Microsoft::WRL::ComPtr<ID3D12Device>& device)
{

	// 実際に生成
	HRESULT hr = device->CreateGraphicsPipelineState(&graphicsDesc_,
		IID_PPV_ARGS(&graphicsState_));
	assert(SUCCEEDED(hr));

}

void PSO::CreateLine(Microsoft::WRL::ComPtr<ID3D12Device>& device)
{

	// 実際に生成
	HRESULT hr = device->CreateGraphicsPipelineState(&graphicsDescLine_,
		IID_PPV_ARGS(&graphicsStateLine_));
	assert(SUCCEEDED(hr));

}

void PSO::Release()
{

	root_.GetSignatureBlob()->Release();
	if (root_.GetErrorBlob()) {
		root_.GetErrorBlob()->Release();
	}
	pixelShaderBlob_->Release();
	vertexShaderBlob_->Release();

}
