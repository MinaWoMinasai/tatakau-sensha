#pragma once
#include "Struct.h"

#include <d3d12.h>
#include <wrl.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace myEngine {

class DirectXCommon;
class DynamicTexture;
class SrvManager;

/// @brief UI の四角形（位置・大きさ）。論理座標（1280×720、左上が原点）。
struct UiRect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    bool Contains(const Vector2& point) const
    {
        return point.x >= x && point.x < x + width && point.y >= y && point.y < y + height;
    }
};

/// @brief UI の頂点（論理座標・UV・色）。色は線形の値（sRGB の色は UiColor で変換する）。
struct UiVertex {
    Vector2 position;
    Vector2 texcoord;
    Vector4 color;
};

/// @brief sRGB の色（0xRRGGBB）を、UI の頂点に渡す線形の色にする。
Vector4 UiColor(uint32_t rgb, float alpha = 1.0f);

/// @brief テクスチャ付きの四角形を1フレーム分ため、テクスチャごとにまとめて描く（ゲーム内 UI 用）。
/// 頂点ごとに色を持つ UI 用の 2D パイプライン（Ui.VS / Ui.PS、深度なし、アルファ合成）を持つ。
/// 四角形は Update の中で積み、Draw はシーンの DrawSprite の中で呼ぶ（スプライトと同じ描画先に重ねる）。
class UiBatch {
public:
    UiBatch();
    ~UiBatch();
    UiBatch(const UiBatch&) = delete;
    UiBatch& operator=(const UiBatch&) = delete;

    /// @brief パイプラインと、塗りつぶし用の白いテクスチャを作る。
    /// @return 作れたらtrue。
    bool Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

    /// @brief 積んだ四角形を消す（フレームの初め）。
    void Clear();

    /// @brief テクスチャの一部を貼った四角形を積む。
    void AddQuad(const UiRect& rect, const Vector2& uvMin, const Vector2& uvMax, const Vector4& color, D3D12_GPU_DESCRIPTOR_HANDLE texture);
    /// @brief 塗りつぶしの四角形を積む。
    void AddRect(const UiRect& rect, const Vector4& color);

    /// @brief 積んだ四角形を描く。DrawSprite の中（描画先を設定した後）で呼ぶ。終わったらスプライトの描画の設定に戻す。
    void Draw();

    /// @brief 積んだ四角形の数。
    size_t GetQuadCount() const
    {
        return vertices_.size() / 6;
    }

    /// @brief 積んだ描画命令の数（同じテクスチャが続く四角形は1つにまとまる）。
    size_t GetCommandCount() const
    {
        return commands_.size();
    }

    /// @brief 前の Draw で描画命令を出した回数（テクスチャが変わるたびに1回）。
    size_t GetLastDrawCallCount() const
    {
        return lastDrawCalls_;
    }

    /// @brief 積んだ頂点（テスト用。1つの四角形は 6 頂点）。
    const std::vector<UiVertex>& GetVertices() const
    {
        return vertices_;
    }

    /// @brief 画面の大きさ（論理座標）。既定は WinApp の論理解像度。
    void SetScreenSize(const Vector2& size)
    {
        screenSize_ = size;
    }

private:
    struct Command {
        D3D12_GPU_DESCRIPTOR_HANDLE texture;
        uint32_t firstVertex;
        uint32_t vertexCount;
    };

    bool CreatePipeline();
    /// @brief 頂点バッファが vertexCount 個入るようにする（足りなければ作り直す）。
    bool EnsureVertexCapacity(size_t vertexCount);

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    UiVertex* mappedVertices_ = nullptr;
    size_t vertexCapacity_ = 0;
    std::unique_ptr<DynamicTexture> whiteTexture_;
    std::vector<UiVertex> vertices_;
    std::vector<Command> commands_;
    Vector2 screenSize_;
    size_t lastDrawCalls_ = 0;
};

} // namespace myEngine
