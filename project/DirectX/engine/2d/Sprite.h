#pragma once
#include "SpriteCommon.h"

class Sprite
{
public:

	// 初期化
	void Initialize(SpriteCommon* spriteCommon);

	void Update();

	void Draw();

	void SetSrvHandleGPU(D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU) { srvHandleGPU_ = srvHandleGPU; }

private:
	SpriteCommon* spriteCommon_ = nullptr;

	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource;
	// バッファリソース内のデータをさすポインタ
	VertexData* vertexData;
	uint32_t* indexData;
	Material* materialData;
	TransformationMatrix* transformationMatrixData;
	// バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW indexBufferView;

	Texture texture;

	Transform uvTransform{
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f},
	};

	Transform transform{ {0.8f, 0.5f, 1.0f}, {0.0f, 0.0f, 0.0f }, {0.0f, 0.0f, 0.0f } };

	D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU_;

};

