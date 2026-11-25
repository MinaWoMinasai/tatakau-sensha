#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <d3d12.h>
#include <wrl.h>
#include "Sprite.h"
#include "TextRenderer.h"

namespace gameplay {
/// @brief 画面の板と文字を、数個の GPU の領域にまとめて描く（計画書 p9 の L1）。
/// @note Sprite は1枚ごとに領域を4つ持つので、板の多い画面を Sprite で作ると、出していない間も重くなる。
/// 更新の段階で Begin → Quad・Text → End と積み、描画の段階で Draw する。積んだ順に、layer の小さい物から描く。
class SurvivorQuadBatch {
public:
    /// @brief 描く層。光の層（PreviewGlow・IconGlow）は光を足して描く。
    enum Layer { Panel, PreviewGlow, PreviewTop, IconGlow, Text, kLayers };

    void Initialize(size_t capacity);
    bool IsInitialized() const
    {
        return capacity_ > 0;
    }
    /// @brief 画像を読み込んでおく（遊んでいる途中で止まらないように）。
    void Preload(const std::string& texture);
    void Begin();
    /// @brief 板を積む。position は中心、color はそのまま材質の色になる。
    /// uLeft・uRight は、画像の横のどこからどこまでを貼るか（0〜1）。
    void Quad(Layer layer, const std::string& texture, const myEngine::Vector2& position, const myEngine::Vector2& size, float rotation,
              const myEngine::Vector4& color, float uLeft = 0.0f, float uRight = 1.0f);
    /// @brief 文字を積む。anchorX は 0 で左、0.5 で真ん中、1 で右。縦は真ん中。scale は大きさの倍率。戻り値は幅。
    float Label(Layer layer, const std::string& text, const myEngine::TextStyle& style, float x, float y, float anchorX, const myEngine::Vector4& color,
                float scale = 1.0f);
    /// @brief 文字の大きさ（論理座標）。
    myEngine::Vector2 Measure(const std::string& text, const myEngine::TextStyle& style);
    void End();
    void Draw();

private:
    struct Texture {
        D3D12_GPU_DESCRIPTOR_HANDLE handle{};
        myEngine::Vector2 size{};
    };
    struct Entry {
        Layer layer = Panel;
        D3D12_GPU_DESCRIPTOR_HANDLE handle{};
        myEngine::Vector2 position{}, size{};
        float rotation = 0.0f;
        myEngine::Vector4 color{};
        float uLeft = 0.0f, uRight = 1.0f;
    };
    /// @brief 続けて描ける板の並び（同じ層・画像・色）。
    struct Run {
        Layer layer = Panel;
        D3D12_GPU_DESCRIPTOR_HANDLE handle{};
        uint32_t start = 0, count = 0, material = 0;
    };
    const Texture& Find(const std::string& path);

    size_t capacity_ = 0;
    std::unordered_map<std::string, Texture> textures_;
    std::unordered_map<std::string, std::string> labels_;
    std::vector<Entry> entries_;
    std::vector<Run> runs_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertices_, materials_, transform_;
    myEngine::VertexData* vertexData_ = nullptr;
    uint8_t* materialData_ = nullptr;
    D3D12_VERTEX_BUFFER_VIEW view_{};
};
} // namespace gameplay
