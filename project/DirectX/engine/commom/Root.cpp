#include "Root.h"

void Root::InitalizeForObject()
{
	// --- RootParameterの拡張 ---
	// 既存: [0]:Material(b0), [1]:Transform(b0-VS), [2]:DescriptorTable(t0...), [3]:Light(b1), [4]:Camera(b2)
	// 追加: [5]:PointLight(b3) 

	descriptionSignature_.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// [0] Material (Pixel b0)
	Parameters_[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	Parameters_[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	Parameters_[0].Descriptor.ShaderRegister = 0;

	// [1] TransformationMatrix (Vertex b0)
	Parameters_[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	Parameters_[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	Parameters_[1].Descriptor.ShaderRegister = 0;

	// [2] DescriptorTable (Pixel t0:Texture, t1:ShadowMap)
	// 記述子レンジを2つ分に増やします
	descriptorRange_[0].BaseShaderRegister = 0; // t0 (通常のテクスチャ)
	descriptorRange_[0].NumDescriptors = 1;
	descriptorRange_[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange_[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	descriptorRange_[1].BaseShaderRegister = 1; // t1 (シャドウマップ)
	descriptorRange_[1].NumDescriptors = 1;
	descriptorRange_[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange_[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	Parameters_[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	Parameters_[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	Parameters_[2].DescriptorTable.pDescriptorRanges = descriptorRange_;
	Parameters_[2].DescriptorTable.NumDescriptorRanges = 2; // レンジを2つに設定

	// [3] DirectionalLight (Pixel b1)
	Parameters_[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	Parameters_[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	Parameters_[3].Descriptor.ShaderRegister = 1;

	// [4] Camera (Pixel b2)
	Parameters_[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	Parameters_[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	Parameters_[4].Descriptor.ShaderRegister = 2;

	// [5] PointLight (Pixel b3) ★追加
	Parameters_[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	Parameters_[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	Parameters_[5].Descriptor.ShaderRegister = 3;

	Parameters_[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	Parameters_[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // VS用
	Parameters_[6].Descriptor.ShaderRegister = 1; // register(b1)

	descriptionSignature_.pParameters = Parameters_;
	descriptionSignature_.NumParameters = 7; // パラメータ数を更新

	// --- StaticSamplerの拡張 ---

	// [0] 通常のサンプラー (s0)
	staticSamplers_[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers_[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers_[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers_[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers_[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers_[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers_[0].ShaderRegister = 0;
	staticSamplers_[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// [1] シャドウ用比較サンプラー (s1) ★追加
	staticSamplers_[1].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR; // 比較用フィルタ
	staticSamplers_[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER; // 範囲外は境界色
	staticSamplers_[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	staticSamplers_[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	staticSamplers_[1].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // 深度比較関数
	staticSamplers_[1].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE; // 範囲外を白（影なし）に
	staticSamplers_[1].ShaderRegister = 1; // register(s1)
	staticSamplers_[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	descriptionSignature_.pStaticSamplers = staticSamplers_;
	descriptionSignature_.NumStaticSamplers = 2; // サンプラー数を更新
	// シリアライズしてバイナリにする
	HRESULT hr = D3D12SerializeRootSignature(&descriptionSignature_,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob_, &errorBlob_);
	if (FAILED(hr)) {
		log.Log(reinterpret_cast<char*> (errorBlob_->GetBufferPointer()));
		assert(false);
	}
}

void Root::InitalizeForParticle()
{
	// RootSignature作成
	descriptionSignature_.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// --- 1. ディスクリプタレンジの設定 ---

	// テクスチャ用 (t0)
	descriptorRange_[0].BaseShaderRegister = 0;
	descriptorRange_[0].NumDescriptors = 1;
	descriptorRange_[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange_[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// インスタンシングバッファ用 (t1) ★ここをシェーダーの register(t1) に合わせる
	descriptorRangeForInstancing_[0].BaseShaderRegister = 1; // t1を指定
	descriptorRangeForInstancing_[0].NumDescriptors = 1;
	descriptorRangeForInstancing_[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeForInstancing_[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// --- 2. ルートパラメータの設定 ---

	// Parameters[0]: マテリアル (b0)
	Parameters_[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	Parameters_[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // VS/PS両方で使うならALL
	Parameters_[0].Descriptor.ShaderRegister = 0;

	// Parameters[1]: テクスチャ (t0)
	Parameters_[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	Parameters_[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	Parameters_[1].DescriptorTable.pDescriptorRanges = descriptorRange_; // t0用レンジ
	Parameters_[1].DescriptorTable.NumDescriptorRanges = 1;

	// Parameters[2]: インスタンシングバッファ (t1)
	Parameters_[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	Parameters_[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // VSで使用
	Parameters_[2].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing_; // t1用レンジ
	Parameters_[2].DescriptorTable.NumDescriptorRanges = 1;

	// Parameters[3]: 未使用またはライティング用 (b1) など
	Parameters_[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	Parameters_[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	Parameters_[3].Descriptor.ShaderRegister = 1;

	descriptionSignature_.pParameters = Parameters_;
	descriptionSignature_.NumParameters = 4; // 使用する数に合わせる

	// --- 3. サンプラーとシリアライズ ---
	// (サンプラー設定は元のままでOKです)
	staticSamplers_[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers_[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers_[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers_[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers_[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers_[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers_[0].ShaderRegister = 0;
	staticSamplers_[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	descriptionSignature_.pStaticSamplers = staticSamplers_;
	descriptionSignature_.NumStaticSamplers = 1;

	HRESULT hr = D3D12SerializeRootSignature(&descriptionSignature_,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob_, &errorBlob_);
	if (FAILED(hr)) {
		log.Log(reinterpret_cast<char*> (errorBlob_->GetBufferPointer()));
		assert(false);
	}
}

void Root::InitalizeForModelParticle()
{
	log.Initialize();
	descriptionSignature_.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// RootParameterの設定 (ModelParticleシェーダーに合わせる)
	// index 0: Material (b0, Pixel)
	Parameters_[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	Parameters_[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	Parameters_[0].Descriptor.ShaderRegister = 0;

	// index 1: DirectionalLight (b1, Pixel)
	Parameters_[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	Parameters_[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	Parameters_[1].Descriptor.ShaderRegister = 1;

	// index 2: Camera (b2, Pixel)
	Parameters_[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	Parameters_[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	Parameters_[2].Descriptor.ShaderRegister = 2;

	// index 3: StructuredBuffer (t1, Vertex) -> DescriptorTableとして定義
	descriptorRange_[0].BaseShaderRegister = 1; // t1
	descriptorRange_[0].NumDescriptors = 1;
	descriptorRange_[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange_[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	Parameters_[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	Parameters_[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // 頂点シェーダーで使用
	Parameters_[3].DescriptorTable.pDescriptorRanges = &descriptorRange_[0];
	Parameters_[3].DescriptorTable.NumDescriptorRanges = 1;

	// index 4: Texture (t0, Pixel) -> DescriptorTable
	descriptorRange_[1].BaseShaderRegister = 0; // t0
	descriptorRange_[1].NumDescriptors = 1;
	descriptorRange_[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange_[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	Parameters_[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	Parameters_[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	Parameters_[4].DescriptorTable.pDescriptorRanges = &descriptorRange_[1];
	Parameters_[4].DescriptorTable.NumDescriptorRanges = 1;

	descriptionSignature_.pParameters = Parameters_;
	descriptionSignature_.NumParameters = 5;

	// サンプラー設定 (既存のものを利用)
	staticSamplers_[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers_[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers_[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers_[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers_[0].ShaderRegister = 0;
	staticSamplers_[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	descriptionSignature_.pStaticSamplers = staticSamplers_;
	descriptionSignature_.NumStaticSamplers = 1;

	// シリアライズと生成
	HRESULT hr = D3D12SerializeRootSignature(&descriptionSignature_, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob_, &errorBlob_);
	if (FAILED(hr)) {
		log.Log(reinterpret_cast<char*>(errorBlob_->GetBufferPointer()));
		assert(false);
	}
}

void Root::InitializeForPostEffect()
{
	log.Initialize();

	descriptionSignature_.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// -------- RootParameter 0 : CBV (BloomParam)
	Parameters_[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	Parameters_[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	Parameters_[0].Descriptor.ShaderRegister = 0; // b0
	Parameters_[0].Descriptor.RegisterSpace = 0;

	// -------- RootParameter 1 : SRV DescriptorTable (SceneRT)
	descriptorRange_[0].BaseShaderRegister = 0; // t0
	descriptorRange_[0].NumDescriptors = 2;     // ★ 修正
	descriptorRange_[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange_[0].OffsetInDescriptorsFromTableStart =
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	Parameters_[1].ParameterType =
		D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	Parameters_[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	Parameters_[1].DescriptorTable.pDescriptorRanges = descriptorRange_;
	Parameters_[1].DescriptorTable.NumDescriptorRanges = 1;

	descriptionSignature_.pParameters = Parameters_;
	descriptionSignature_.NumParameters = 2;

	// -------- Static Sampler (s0)
	staticSamplers_[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers_[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers_[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers_[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers_[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers_[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers_[0].ShaderRegister = 0; // s0
	staticSamplers_[0].ShaderVisibility =
		D3D12_SHADER_VISIBILITY_PIXEL;

	descriptionSignature_.pStaticSamplers = staticSamplers_;
	descriptionSignature_.NumStaticSamplers = 1;

	// -------- Serialize
	HRESULT hr = D3D12SerializeRootSignature(
		&descriptionSignature_,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&signatureBlob_,
		&errorBlob_);

	if (FAILED(hr)) {
		log.Log(reinterpret_cast<char*>(errorBlob_->GetBufferPointer()));
		assert(false);
	}
}

void Root::InitalizeForShadow() {
	descriptionSignature_.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// パラメータは WVP (b0) だけでOK
	Parameters_[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	Parameters_[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	Parameters_[0].Descriptor.ShaderRegister = 0;

	descriptionSignature_.pParameters = Parameters_;
	descriptionSignature_.NumParameters = 1; // 1つだけ

	// サンプラーは不要
	descriptionSignature_.pStaticSamplers = nullptr;
	descriptionSignature_.NumStaticSamplers = 0;

	HRESULT hr = D3D12SerializeRootSignature(
		&descriptionSignature_,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&signatureBlob_,
		&errorBlob_
	);
}

void Root::Create(Microsoft::WRL::ComPtr<ID3D12Device>& device)
{

	// バイナリをもとに生成
	HRESULT hr = device->CreateRootSignature(0,
		signatureBlob_->GetBufferPointer(), signatureBlob_->GetBufferSize(),
		IID_PPV_ARGS(&signature_));
	assert(SUCCEEDED(hr));

}
