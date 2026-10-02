#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ObjectPostEffect.h"
#include "TextLabel.h"

/// @brief 文字に重ねるネオン発光の外観を指定する。
struct NeonTextEffectStyle {
    bool enabled = true;
    cg2::Vector4 glowColor{0.18f, 1.0f, 0.48f, 1.0f};
    float sourceBrightness = 2.2f;
    float threshold = 0.0f;
    float innerIntensity = 0.82f;
    float outerIntensity = 0.48f;
};

/// @brief 文字の描画結果に発光を加える演出を管理する。
class NeonTextEffect {
public:
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(cg2::DirectXCommon* dxCommon, cg2::SrvManager* srvManager, cg2::RtvManager* rtvManager = nullptr);
    /// @brief 外観を設定する。
    void SetStyle(const NeonTextEffectStyle& style);
    /// @brief ブルームを描画する。
    void DrawBloom(const std::vector<cg2::TextLabel*>& labels);
    /// @brief Sourcesを消去する。
    void ClearSources();

private:
    /// @brief 文字の発光抽出に使う位置・色・画像を保持する。
    struct GlowSource {
        std::unique_ptr<cg2::Sprite> sprite;
        std::string texturePath;
    };

    /// @brief 文字の状態と発光元の一覧を同期する。
    void SynchronizeSources(const std::vector<cg2::TextLabel*>& labels);
    /// @brief Layerを描画する。
    void DrawLayer(cg2::ObjectPostEffect& effect, const std::vector<cg2::TextLabel*>& labels);

    cg2::SpriteCommon* spriteCommon_ = nullptr;
    std::unique_ptr<cg2::ObjectPostEffect> innerEffect_;
    std::unique_ptr<cg2::ObjectPostEffect> outerEffect_;
    std::unordered_map<const cg2::TextLabel*, GlowSource> sources_;
    NeonTextEffectStyle style_{};
};
