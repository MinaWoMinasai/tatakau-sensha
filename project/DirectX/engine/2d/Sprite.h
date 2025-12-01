#pragma once
#include "SpriteCommon.h"
#include "TextureManager.h"

class Sprite
{
public:

	// 初期化
	void Initialize(SpriteCommon* spriteCommon, std::string textureFilePath);

	void Update();

	void Draw();

	void SetSrvHandleGPU(D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU) { srvHandleGPU_ = srvHandleGPU; }

	/// <summary>
	/// テクスチャ変更
	/// </summary>
	/// <param name="textureFilePath"></param>
	void SetTexture(std::string textureFilePath);

	Vector2& GetPosition() { return position_; }
	void SetPosition(const Vector2& position) { position_ = position; }

	float& GetRotation() { return rotation_; }
	void SetRotation(float rotation) { rotation_ = rotation; }

	Vector4& GetColor() { return materialData->color; }
	void SetColor(const Vector4& color) { materialData->color = color; }

	Vector2& GetSize() { return size_; }
	void SetSize(const Vector2& size) { size_ = size; }

	Transform& GetUvTransform() { return uvTransform_; }
	void SetUvTransform(const Transform& uvTransform) { uvTransform_ = uvTransform; }

	Vector2& GetAnchorPoint() { return anchorPoint_; }
	void SetAnchorPoint(const Vector2& anchorPoint) { anchorPoint_ = anchorPoint; }

	bool& GetIsFlipX() { return isFlipX_; }
	void SetIsFlipX(bool isFlipX) { isFlipX_ = isFlipX; }

	bool& GetIsFlipY() { return isFlipY_; }
	void SetIsFlipY(bool isFlipY) { isFlipY_ = isFlipY; }

	Vector2& GetTextureLeftTop() { return textureLeftTop_; }
	void SetTextureLeftTop(const Vector2& textureLeftTop) { textureLeftTop_ = textureLeftTop; }

	Vector2& GetTextureSize() { return textureSize_; }
	void SetTextureSize(const Vector2& textureSize) { textureSize_ = textureSize; }

private:

	void AdjustTextureSize();

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

	Transform uvTransform_{
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f},
	};

	Transform transform{ {0.8f, 0.5f, 1.0f}, {0.0f, 0.0f, 0.0f }, {0.0f, 0.0f, 0.0f } };

	D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU_;

	Vector2 position_ = { 0.0f, 0.0f };
	float rotation_ = 0.0f;
	Vector2 size_ = { 640.0f, 360.0f };

	// テクスチャ番号
	uint32_t textureIndex = 0;

	Vector2 anchorPoint_ = { 0.0f, 0.0f };

	bool isFlipX_ = false;
	bool isFlipY_ = false;

	Vector2 textureLeftTop_ = { 0.0f, 0.0f };
	Vector2 textureSize_ = { 100.0f, 100.0f };

};

