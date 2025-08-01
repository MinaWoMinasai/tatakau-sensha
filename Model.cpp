#include "Model.h"

void Model::Initialize(Microsoft::WRL::ComPtr<ID3D12Device>& device, Descriptor descriptor)
{
	device_ = device;
	descriptor_ = descriptor;

}

void Model::Load(Command command, const std::string& filename, const std::string& textureName, const uint32_t& index)
{

	modelData_ = loadFile_.Obj("resources", filename);
	// リソース
	vertexBufferView = resource.CreateVBV(modelData_, texture, device_, vertexResource);

	// マテリアル用のリソースを作る
	materialResource = resource.CreateMaterial(texture, device_);

	// データを書き込む	
	resource.CreateWVP(texture, device_, wvpResource, wvpData);

	// 平行光源用のリソースを作る
	directionalLightResource = resource.CreatedirectionalLight(texture, device_);

	// Textureを読んで転送する
	DirectX::ScratchImage mipImages = texture.Load("resources/" + textureName);
	const DirectX::TexMetadata metadata = mipImages.GetMetadata();
	textureResource = texture.CreateResource(device_, metadata);
	intermediateResource = texture.UploadData(textureResource, mipImages, device_, command.GetList());

	// metaDataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	// DescriptorSizeを取得しておく
	const uint32_t descriptorSizeSRV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	const uint32_t descriptorSizeRTV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	const uint32_t descriptorSizeDSV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	descriptor_.GetCPUHandle(descriptor_.GetRtvHeap(), descriptorSizeRTV, 0);

	// SRVを作成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = descriptor_.GetCPUHandle(descriptor_.GetSrvHeap(), descriptorSizeSRV, index);
	textureSrvHandleGPU = descriptor_.GetGPUHandle(descriptor_.GetSrvHeap(), descriptorSizeSRV, index);

	// SRVの作成
	device_->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);
}

void Model::Update()
{

}

void Model::Draw(Renderer renderer, const Transform& transform, const Matrix4x4& cameraMatrix)
{

	Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	Matrix4x4 viewMatrix = cameraMatrix;
	Matrix4x4 projectionMatrix = MakePerspectiveForMatrix(0.45f, float(kClientWidth) / float(kClientHeight), 0.1f, 100.0f);
	Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
	wvpData->WVP = worldViewProjectionMatrix;
	wvpData->World = worldMatrix;

	renderer.DrawModel(
		vertexBufferView,                // VBV
		nullptr,                          // IBV（インデックスなしの例）
		materialResource->GetGPUVirtualAddress(),  // Material CBV
		wvpResource->GetGPUVirtualAddress(),       // WVP CBV
		textureSrvHandleGPU, // SRV
		directionalLightResource->GetGPUVirtualAddress(),            // Light CBV
		static_cast<UINT>(modelData_.vertices.size())
	);
}

void Model::DrawPro(Renderer renderer, const Transform& transform, const Matrix4x4& cameraMatrix, Microsoft::WRL::ComPtr<ID3D12Resource> materialResource, Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource)
{
	Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	Matrix4x4 viewMatrix = cameraMatrix;
	Matrix4x4 projectionMatrix = MakePerspectiveForMatrix(0.45f, float(kClientWidth) / float(kClientHeight), 0.1f, 100.0f);
	Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
	wvpData->WVP = worldViewProjectionMatrix;
	wvpData->World = worldMatrix;

	renderer.DrawModel(
		vertexBufferView,                // VBV
		nullptr,                          // IBV（インデックスなしの例）
		materialResource->GetGPUVirtualAddress(),  // Material CBV
		wvpResource->GetGPUVirtualAddress(),       // WVP CBV
		textureSrvHandleGPU, // SRV
		directionalLightResource->GetGPUVirtualAddress(),            // Light CBV
		static_cast<UINT>(modelData_.vertices.size())
	);
}

