#pragma once
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "SrvManager.h"

namespace cg2 {

/// @brief テクスチャを画面上の矩形として描画する。位置・色・UVをインスタンスごとに保持する。
class Sprite {
public:
    // 初期化
    void Initialize(SpriteCommon* spriteCommon, const std::string& textureFilePath);
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(SpriteCommon* spriteCommon, uint32_t srvIndex, SrvManager* srvManager);

    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    void Update();

    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    void Draw();

    /// @brief GPU側のSRVハンドルを設定する。
    void SetSrvHandleGPU(D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU)
    {
        srvHandleGPU_ = srvHandleGPU;
    }

    /// @brief テクスチャ変更
    void SetTexture(const std::string& textureFilePath);

    /// @brief 位置を返す。
    const Vector2& GetPosition() const
    {
        return position_;
    }
    /// @brief 位置を設定する。
    void SetPosition(const Vector2& position)
    {
        position_ = position;
    }

    /// @brief 回転角を返す。
    float GetRotation() const
    {
        return rotation_;
    }
    /// @brief 回転角を設定する。
    void SetRotation(float rotation)
    {
        rotation_ = rotation;
    }

    /// @brief 色を返す。
    const Vector4& GetColor() const
    {
        return materialData->color;
    }
    /// @brief 色を設定する。
    void SetColor(const Vector4& color)
    {
        materialData->color = color;
    }

    /// @brief 透明度を設定する。
    void SetAlpha(const float alpha)
    {
        materialData->color.w = alpha;
    }

    /// @brief サイズを返す。
    const Vector2& GetSize() const
    {
        return size_;
    }
    /// @brief サイズを設定する。
    void SetSize(const Vector2& size)
    {
        size_ = size;
    }

    /// @brief UV姿勢を返す。
    const Transform& GetUvTransform() const
    {
        return uvTransform_;
    }
    /// @brief UV姿勢を設定する。
    void SetUvTransform(const Transform& uvTransform)
    {
        uvTransform_ = uvTransform;
    }

    /// @brief スプライトの配置基準点を返す。
    const Vector2& GetAnchorPoint() const
    {
        return anchorPoint_;
    }
    /// @brief スプライトの配置基準点を設定する。
    void SetAnchorPoint(const Vector2& anchorPoint)
    {
        anchorPoint_ = anchorPoint;
    }

    /// @brief 画像の左右反転設定を返す。
    bool GetIsFlipX() const
    {
        return isFlipX_;
    }
    /// @brief 画像の左右反転設定を設定する。
    void SetIsFlipX(bool isFlipX)
    {
        isFlipX_ = isFlipX;
    }

    /// @brief 画像の上下反転設定を返す。
    bool GetIsFlipY() const
    {
        return isFlipY_;
    }
    /// @brief 画像の上下反転設定を設定する。
    void SetIsFlipY(bool isFlipY)
    {
        isFlipY_ = isFlipY;
    }

    /// @brief 画像の切り出し開始位置を返す。
    const Vector2& GetTextureLeftTop() const
    {
        return textureLeftTop_;
    }
    /// @brief 画像の切り出し開始位置を設定する。
    void SetTextureLeftTop(const Vector2& textureLeftTop)
    {
        textureLeftTop_ = textureLeftTop;
    }

    /// @brief 画像の切り出しサイズを返す。
    const Vector2& GetTextureSize() const
    {
        return textureSize_;
    }
    /// @brief 画像の切り出しサイズを設定する。
    void SetTextureSize(const Vector2& textureSize)
    {
        textureSize_ = textureSize;
    }

    /// @brief 合成方式を設定する。
    void SetBlendMode(BlendMode blendMode);

    /// @brief マウス重なりであるか判定する。
    bool IsHovered(const Vector2& mousePos) const;

private:
    /// @brief 画像の切り出しサイズを適切な状態へ調整する。
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

    Transform transform{{0.8f, 0.5f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};

    D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU_;

    Vector2 position_ = {0.0f, 0.0f};
    float rotation_ = 0.0f;
    Vector2 size_ = {640.0f, 360.0f};

    // テクスチャ番号
    uint32_t textureIndex = 0;

    Vector2 anchorPoint_ = {0.0f, 0.0f};

    bool isFlipX_ = false;
    bool isFlipY_ = false;

    Vector2 textureLeftTop_ = {0.0f, 0.0f};
    Vector2 textureSize_ = {100.0f, 100.0f};

    std::string textureFilePath_;

    bool isRenderTexture_ = false;
    SrvManager* srvManager_ = nullptr;
};

} // namespace cg2
