#pragma once
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "SrvManager.h"

namespace cg2 {

class Sprite
{
public:

	// 初期化
	void Initialize(SpriteCommon* spriteCommon, const std::string& textureFilePath);
	void Initialize(SpriteCommon* spriteCommon, uint32_t srvIndex, SrvManager* srvManager);

	void Update();

	void Draw();

	void SetSrvHandleGPU(D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU) { srvHandleGPU_ = srvHandleGPU; }

	/// <summary>
	/// テクスチャ変更
	/// </summary>
	/// <param name="textureFilePath"></param>
	void SetTexture(const std::string& textureFilePath);

	const Vector2& GetPosition() const { return position_; }
	void SetPosition(const Vector2& position) { position_ = position; }

	float GetRotation() const { return rotation_; }
	void SetRotation(float rotation) { rotation_ = rotation; }

	const Vector4& GetColor() const { return materialData->color; }
	void SetColor(const Vector4& color) { materialData->color = color; }

	void SetAlpha(const float alpha){ materialData->color.w = alpha; }

	const Vector2& GetSize() const { return size_; }
	void SetSize(const Vector2& size) { size_ = size; }

	const Transform& GetUvTransform() const { return uvTransform_; }
	void SetUvTransform(const Transform& uvTransform) { uvTransform_ = uvTransform; }

	const Vector2& GetAnchorPoint() const { return anchorPoint_; }
	void SetAnchorPoint(const Vector2& anchorPoint) { anchorPoint_ = anchorPoint; }

	bool GetIsFlipX() const { return isFlipX_; }
	void SetIsFlipX(bool isFlipX) { isFlipX_ = isFlipX; }

	bool GetIsFlipY() const { return isFlipY_; }
	void SetIsFlipY(bool isFlipY) { isFlipY_ = isFlipY; }

	const Vector2& GetTextureLeftTop() const { return textureLeftTop_; }
	void SetTextureLeftTop(const Vector2& textureLeftTop) { textureLeftTop_ = textureLeftTop; }

	const Vector2& GetTextureSize() const { return textureSize_; }
	void SetTextureSize(const Vector2& textureSize) { textureSize_ = textureSize; }

	void SetBlendMode(BlendMode blendMode);
	
	bool IsHovered(const Vector2& mousePos) const;

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

	std::string textureFilePath_;

	bool isRenderTexture_ = false;
	SrvManager* srvManager_ = nullptr;
};

} // namespace cg2
