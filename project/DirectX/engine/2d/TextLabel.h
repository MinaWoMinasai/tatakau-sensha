#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include "Sprite.h"
#include "TextRenderer.h"

namespace cg2 {

/// @brief 表示文字列と描画用スプライトを保持し、文字の変更時にテクスチャを更新する。
class TextLabel {
public:
#if defined(USE_IMGUI) && !defined(NDEBUG)
    /// @brief cg2::TextLabelの処理件数や所要時間を保持し、性能と動作の確認に使う。
    struct ProfileStats {
        float setStyleCpuMs = 0.0f;
        float setTextCpuMs = 0.0f;
        float setTextRebuildCpuMs = 0.0f;
        float rebuildTextureCpuMs = 0.0f;
        float getOrCreateTextureCpuMs = 0.0f;
        float spriteSetTextureCpuMs = 0.0f;
        uint32_t cacheFileExistedCount = 0;
        uint32_t generatedPngCount = 0;
    };
    /// @brief 処理の計測値を初期状態へ戻す。
    static void ResetProfileStats();
    /// @brief 処理の計測値を返す。
    static const ProfileStats& GetProfileStats();
#endif

    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(SpriteCommon* spriteCommon, const std::string& text, const TextStyle& style = {});
    /// @brief からのJsonを初期化する。
    bool InitializeFromJson(SpriteCommon* spriteCommon, const std::string& configPath, const std::string& labelId);
    /// @brief 文字を設定する。
    void SetText(const std::string& text);
    /// @brief 外観を設定する。
    void SetStyle(const TextStyle& style);
    /// @brief 位置を設定する。
    void SetPosition(const Vector2& position);
    /// @brief スプライトの配置基準点を設定する。
    void SetAnchorPoint(const Vector2& anchorPoint);
    /// @brief 透明度を設定する。
    void SetAlpha(float alpha);
    /// @brief 用の描画を利用前に準備する。
    void PrepareForDraw();
    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    void Draw();

    /// @brief 文字を返す。
    const std::string& GetText() const
    {
        return text_;
    }
    /// @brief 外観を返す。
    const TextStyle& GetStyle() const
    {
        return style_;
    }
    /// @brief テクスチャパスを返す。
    const std::string& GetTexturePath() const
    {
        return texturePath_;
    }
    /// @brief スプライトを返す。
    Sprite* GetSprite() const
    {
        return sprite_.get();
    }

private:
    /// @brief テクスチャを再構築する。
    void RebuildTexture();
    /// @brief 同一外観であるか判定する。
    bool IsSameStyle(const TextStyle& style) const;

    SpriteCommon* spriteCommon_ = nullptr;
    std::unique_ptr<Sprite> sprite_;
    std::string text_;
    std::string texturePath_;
    TextStyle style_{};
    Vector2 position_ = {0.0f, 0.0f};
    Vector2 anchorPoint_ = {0.0f, 0.0f};
    float alpha_ = 1.0f;
    unsigned long long builtFontRevision_ = 0;
#if defined(USE_IMGUI) && !defined(NDEBUG)
    static ProfileStats profileStats_;
#endif
};

} // namespace cg2
