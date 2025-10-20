#include "Resource.h"

D3D12_VERTEX_BUFFER_VIEW Resource::CreateVBV(const ModelData& modelData, Texture texture, Microsoft::WRL::ComPtr<ID3D12Device>& device, Microsoft::WRL::ComPtr<ID3D12Resource>& vertexResource)
{

	// 頂点リソースを作る
	vertexResource = texture.CreateBufferResource(device, sizeof(VertexData) * modelData.vertices.size());

	// VBV
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	// リソースの先頭アドレスを使う
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	// 使用するリソースのサイズは頂点3つ分のサイズ
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
	// 1頂点あたりのサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	// 頂点リソースにデータを書き込む
	VertexData* vertexData = nullptr;
	// 書き込むためのアドレスを取得
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	// 頂点データをリソースにコピー
	std::memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData) * modelData.vertices.size());

	return vertexBufferView;

}

Microsoft::WRL::ComPtr<ID3D12Resource> Resource::CreateMaterial(Texture texture, Microsoft::WRL::ComPtr<ID3D12Device>& device)
{

	// マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource = texture.CreateBufferResource(device, sizeof(Material));
	// マテリアルにデータを書き込む
	Material* materialData = nullptr;
	// 書き込むためのアドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	// 赤を書き込む
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	// ライティングを有効にする
	materialData->enableLighting = false;
	materialData->uvTransform = MakeIdentity4x4();

	return materialResource;

}

void Resource::CreateWVP(Texture texture, Microsoft::WRL::ComPtr<ID3D12Device>& device, Microsoft::WRL::ComPtr<ID3D12Resource>& wvpResource, TransformationMatrix*& wvpData)
{

	// WVP用のリソースを作る。Matrix4x41つ分のサイズを用意する
	wvpResource = texture.CreateBufferResource(device, sizeof(TransformationMatrix));
	// 書き込むためのアドレスを取得
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	// 単位行列を書き込んでいく
	wvpData->WVP = MakeIdentity4x4();
	wvpData->World = MakeIdentity4x4();

}

Microsoft::WRL::ComPtr<ID3D12Resource> Resource::CreatedirectionalLight(Texture texture, Microsoft::WRL::ComPtr<ID3D12Device>& device)
{

	// 平行光源用のリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource = texture.CreateBufferResource(device, sizeof(DirectionalLight));
	// マテリアルにデータを書き込む
	DirectionalLight* directionalLightData = nullptr;
	// 書き込むためのアドレスを取得
	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));
	// デフォルト値
	directionalLightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData->direction = { 0.0f, -1.0f, 0.0f };
	directionalLightData->intensity = 1.0f;

	return directionalLightResource;
}
