#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "Sprite.h"
#include "TextLabel.h"

namespace gameplay {
/// @brief 無限の場の、文字を減らした HUD（計画書 p9 の H4）。自機の下に HP・経験値・Lv・Q、上の真ん中に残り時間とボスの印を出す。
/// @note 数字の画像は始めに1回だけ作って使い回す（TextLabel の SetText は文字列ごとに画像を作り、解放しないので使わない）。
class SurvivorCompactHud {
public:
    struct State {
        /// @brief 自機の画面の位置（論理座標 1280×720）。
        myEngine::Vector2 anchor{};
        float hpRatio = 1.0f;
        /// @brief 治すまで戻らない分（最大HPに対する割合）。バーの右端から灰色で出す。
        float woundRatio = 0.0f;
        float expRatio = 0.0f;
        int level = 1;
        /// @brief Q のゲージ（負なら出さない）。
        float ultimateRatio = -1.0f;
        /// @brief 残りの秒（負なら出さない）。
        float secondsLeft = -1.0f;
        /// @brief ボスの HP（負ならボスはいない）。いる間は残り時間の代わりに出す。
        float bossRatio = -1.0f;
        /// @brief 大きさの倍率と、自機からバーまでの縦の間（論理座標）。
        float scale = 1.0f;
        float offset = 52.0f;
    };

    void Initialize();
    /// @brief 板の位置と色を決める。更新の段階で呼ぶ。
    void Update(float dt, const State& state);
    void Draw();

private:
    static constexpr size_t kRectCount = 32;
    static constexpr size_t kGlyphCount = 16;
    // 板1枚の描画は軽くない（画像の検索が何度も入る）ので、輪の区切りは少なめにする。
    static constexpr int kRingSegments = 12;
    static constexpr size_t kGlyphKinds = 14; // 0〜9、「:」、「Lv」、「Q」、ボスの印

    /// @brief 板に最後に入れた値。同じなら更新を省く。
    struct Placed {
        myEngine::Vector2 position{};
        myEngine::Vector2 size{};
        myEngine::Vector4 color{};
        float rotation = 0.0f;
        size_t glyph = kGlyphKinds;
        bool valid = false;
    };
    void Place(myEngine::Sprite& sprite, Placed& placed, const myEngine::Vector2& position, const myEngine::Vector2& size,
               const myEngine::Vector4& color, float rotation, size_t glyph);
    void Glyph(size_t index, const myEngine::Vector2& center, float height, const myEngine::Vector4& color);

    void Rect(const myEngine::Vector2& center, const myEngine::Vector2& size, const myEngine::Vector4& color, float rotation = 0.0f);
    /// @brief 数字（0〜9 と「:」）を、幅をそろえて並べる。align は 0 で左、0.5 で真ん中、1 で右。戻り値は幅。
    float Digits(const std::string& text, const myEngine::Vector2& position, float height, float align, const myEngine::Vector4& color);

    std::array<std::unique_ptr<myEngine::TextLabel>, kGlyphKinds> glyphLabels_;
    std::array<std::string, kGlyphKinds> glyphPaths_{};
    std::array<myEngine::Vector2, kGlyphKinds> glyphSizes_{};
    std::vector<std::unique_ptr<myEngine::Sprite>> rects_;
    std::vector<std::unique_ptr<myEngine::Sprite>> glyphs_;
    std::vector<Placed> rectPlaced_;
    std::vector<Placed> glyphPlaced_;
    size_t usedRects_ = 0;
    size_t usedGlyphs_ = 0;
    float time_ = 0.0f;
    float shownExp_ = 0.0f;
    float gainFlash_ = 0.0f;
    float levelFlash_ = 0.0f;
    float lastExp_ = -1.0f;
    int lastLevel_ = -1;
};
} // namespace gameplay
