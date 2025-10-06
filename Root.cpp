#include "Root.h"

void Root::Iniitalize()
{

	log.Initialize();

	// RootSignature作成
	descriptionSignature_.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	descriptorRangeForInstancing_[0].BaseShaderRegister = 0; // 0から始まる
	descriptorRangeForInstancing_[0].NumDescriptors = 1; // 数は一つ
	descriptorRangeForInstancing_[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	descriptorRangeForInstancing_[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // offsetを自動計算
	// RootParameterの作成
	Parameters_[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	Parameters_[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	Parameters_[0].Descriptor.ShaderRegister = 0; // レジスタ番号0とバインド
	Parameters_[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // CBVを使う
	Parameters_[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	Parameters_[1].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing_;
	Parameters_[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing_);

	//Parameters_[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	//Parameters_[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	//Parameters_[1].Descriptor.ShaderRegister = 0; // レジスタ番号0とバインド

	descriptorRange_[0].BaseShaderRegister = 0; // 0から始まる
	descriptorRange_[0].NumDescriptors = 1; // 数は一つ
	descriptorRange_[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	descriptorRange_[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // offsetを自動計算
	Parameters_[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う
	Parameters_[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // Tableの中身の配列を指定
	Parameters_[2].DescriptorTable.pDescriptorRanges = descriptorRange_; // Tableの中身の配列を指定
	Parameters_[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange_); // Tableで利用する数
	descriptionSignature_.pParameters = Parameters_; // ルートパラメータ配列へのポインタ
	descriptionSignature_.NumParameters = _countof(Parameters_); // 配列の長さ
	Parameters_[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CSVを使う
	Parameters_[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	Parameters_[3].Descriptor.ShaderRegister = 1; // レジスタ番号1とバインド

	staticSamplers_[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // バイリニアフィルタ
	staticSamplers_[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 0~1の範囲外をリピート
	staticSamplers_[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers_[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers_[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較しない
	staticSamplers_[0].MaxLOD = D3D12_FLOAT32_MAX; // ありったけのMipmapを使う
	staticSamplers_[0].ShaderRegister = 0; // レジスタ番号0を使う
	staticSamplers_[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	descriptionSignature_.pStaticSamplers = staticSamplers_;
	descriptionSignature_.NumStaticSamplers = _countof(staticSamplers_);

	// シリアライズしてバイナリにする
	HRESULT hr = D3D12SerializeRootSignature(&descriptionSignature_,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob_, &errorBlob_);
	if (FAILED(hr)) {
		log.Log(reinterpret_cast<char*> (errorBlob_->GetBufferPointer()));
		assert(false);
	}
}

void Root::Create(Microsoft::WRL::ComPtr<ID3D12Device>& device)
{

	// バイナリをもとに生成
	HRESULT hr = device->CreateRootSignature(0,
		signatureBlob_->GetBufferPointer(), signatureBlob_->GetBufferSize(),
		IID_PPV_ARGS(&signature_));
	assert(SUCCEEDED(hr));

}

void Root::Compile(Command& command)
{
	vertexShaderBlob_ = compileShader.Initialize(L"Particle.VS.hlsl",
		L"vs_6_0", command.GetDxcUtils(), command.GetDxcCompiler(), command.GetIncludeHandler());
	assert(vertexShaderBlob_ != nullptr);

	pixelShaderBlob_ = compileShader.Initialize(L"Particle.PS.hlsl",
		L"ps_6_0", command.GetDxcUtils(), command.GetDxcCompiler(), command.GetIncludeHandler());
	assert(pixelShaderBlob_ != nullptr);

}
