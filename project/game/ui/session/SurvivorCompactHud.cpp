#include "game/ui/session/SurvivorCompactHud.h"
#include "SpriteCommon.h"
#include "WinApp.h"
#include <algorithm>
#include <cmath>

namespace gameplay {

namespace {
constexpr size_t kColon = 10;
constexpr size_t kLv = 11;
constexpr size_t kQ = 12;
constexpr size_t kBossMark = 13;
constexpr float kGlyphFontSize = 30.0f;
constexpr float kGlyphPadding = 3.0f;
// モック（p9）の色：HP は赤、経験値は水色、Q は橙（満タンで金）、ボスは赤、文字は白に近い紫。
constexpr myEngine::Vector4 kInk{0.91f, 0.90f, 1.0f, 1.0f};
constexpr myEngine::Vector4 kHp{1.0f, 0.23f, 0.36f, 1.0f};
constexpr myEngine::Vector4 kExp{0.37f, 0.78f, 1.0f, 1.0f};
constexpr myEngine::Vector4 kUltimate{1.0f, 0.70f, 0.28f, 1.0f};
constexpr myEngine::Vector4 kGold{1.0f, 0.83f, 0.30f, 1.0f};
constexpr myEngine::Vector4 kWhite{1.0f, 1.0f, 1.0f, 1.0f};

myEngine::Vector4 Mix(const myEngine::Vector4& a, const myEngine::Vector4& b, float t)
{
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
}

myEngine::Vector4 Alpha(myEngine::Vector4 color, float alpha)
{
    color.w = alpha;
    return color;
}
} // namespace

void SurvivorCompactHud::Initialize()
{
    myEngine::TextStyle style{};
    style.fontFamily = "Bahnschrift";
    style.fontWeight = 700;
    style.fontSize = kGlyphFontSize;
    style.color = kWhite;
    style.outlineColor = {0.02f, 0.02f, 0.06f, 0.9f};
    style.outlineThickness = 2.0f;
    style.padding = kGlyphPadding;
    style.preserveOutline = true;
    for (size_t i = 0; i < glyphLabels_.size(); ++i) {
        const std::string text = i < 10 ? std::to_string(i) : i == kColon ? ":" : i == kLv ? "Lv" : i == kQ ? "Q" : "\u2B20";
        if (i == kBossMark) {
            // ボスの印：線だけの五角。
            style.fontFamily = "Segoe UI Symbol";
        }
        glyphLabels_[i] = std::make_unique<myEngine::TextLabel>();
        glyphLabels_[i]->Initialize(myEngine::SpriteCommon::GetInstance(), text, style);
        glyphPaths_[i] = glyphLabels_[i]->GetTexturePath();
        glyphSizes_[i] = glyphLabels_[i]->GetSprite()->GetSize();
    }
    auto make = [](std::vector<std::unique_ptr<myEngine::Sprite>>& list, size_t count, const std::string& path) {
        list.resize(count);
        for (auto& sprite : list) {
            sprite = std::make_unique<myEngine::Sprite>();
            sprite->Initialize(myEngine::SpriteCommon::GetInstance(), path);
            sprite->SetAnchorPoint({0.5f, 0.5f});
        }
    };
    make(rects_, kRectCount, "resources/white512x512.png");
    make(glyphs_, kGlyphCount, glyphPaths_[0]);
    rectPlaced_.assign(kRectCount, {});
    glyphPlaced_.assign(kGlyphCount, {});
}

void SurvivorCompactHud::Place(myEngine::Sprite& sprite, Placed& placed, const myEngine::Vector2& position, const myEngine::Vector2& size,
                               const myEngine::Vector4& color, float rotation, size_t glyph)
{
    if (placed.valid && placed.position.x == position.x && placed.position.y == position.y && placed.size.x == size.x &&
        placed.size.y == size.y && placed.color.x == color.x && placed.color.y == color.y && placed.color.z == color.z &&
        placed.color.w == color.w && placed.rotation == rotation && placed.glyph == glyph) {
        return;
    }
    // 画像の差し替えは大きさを画像に戻すので、大きさより先に行う。
    if (glyph < kGlyphKinds && placed.glyph != glyph) {
        sprite.SetTexture(glyphPaths_[glyph]);
    }
    sprite.SetPosition(position);
    sprite.SetSize(size);
    sprite.SetColor(color);
    sprite.SetRotation(rotation);
    sprite.Update();
    placed = {position, size, color, rotation, glyph, true};
}

void SurvivorCompactHud::Rect(const myEngine::Vector2& center, const myEngine::Vector2& size, const myEngine::Vector4& color, float rotation)
{
    if (usedRects_ >= rects_.size() || size.x <= 0.0f || size.y <= 0.0f) {
        return;
    }
    Place(*rects_[usedRects_], rectPlaced_[usedRects_], center, size, color, rotation, kGlyphKinds);
    ++usedRects_;
}

void SurvivorCompactHud::Glyph(size_t index, const myEngine::Vector2& center, float height, const myEngine::Vector4& color)
{
    if (usedGlyphs_ >= glyphs_.size()) {
        return;
    }
    const float scale = height / (std::max)(1.0f, glyphSizes_[index].y);
    Place(*glyphs_[usedGlyphs_], glyphPlaced_[usedGlyphs_], center, {glyphSizes_[index].x * scale, height}, color, 0.0f, index);
    ++usedGlyphs_;
}

float SurvivorCompactHud::Digits(const std::string& text, const myEngine::Vector2& position, float height, float align,
                                 const myEngine::Vector4& color)
{
    const float scale = height / (std::max)(1.0f, glyphSizes_[0].y);
    float cell = 0.0f;
    for (size_t i = 0; i < 10; ++i) {
        cell = (std::max)(cell, glyphSizes_[i].x);
    }
    // 数字は幅をそろえて、減っていく時間が左右に揺れないようにする。縁取りと余白の分は詰める。
    auto advance = [&](char c) {
        return ((c == ':' ? glyphSizes_[kColon].x : cell) - kGlyphPadding * 2.0f - 3.0f) * scale;
    };
    float width = 0.0f;
    for (char c : text) {
        width += advance(c);
    }
    float x = position.x - width * align;
    for (char c : text) {
        const float step = advance(c);
        const size_t index = c == ':' ? kColon : static_cast<size_t>(c - '0');
        Glyph(index, {x + step * 0.5f, position.y}, glyphSizes_[index].y * scale, color);
        x += step;
    }
    return width;
}

void SurvivorCompactHud::Update(float dt, const State& state)
{
    usedRects_ = 0;
    usedGlyphs_ = 0;
    if (rects_.empty()) {
        return;
    }
    time_ += dt;
    const float expRatio = (std::clamp)(state.expRatio, 0.0f, 1.0f);
    if (lastLevel_ >= 0 && state.level > lastLevel_) {
        levelFlash_ = 0.6f;
        shownExp_ = 0.0f;
    } else if (lastLevel_ >= 0 && expRatio > lastExp_ + 0.0001f) {
        gainFlash_ = 0.18f;
    }
    lastLevel_ = state.level;
    lastExp_ = expRatio;
    gainFlash_ = (std::max)(0.0f, gainFlash_ - dt);
    levelFlash_ = (std::max)(0.0f, levelFlash_ - dt);
    shownExp_ += (expRatio - shownExp_) * (1.0f - std::exp(-dt * 14.0f));
    const float levelGlow = (std::min)(1.0f, levelFlash_ / 0.4f);

    // 横のゲージ1本：暗い溝、光る間だけのにじみ、塗り。
    auto gauge = [&](float left, float y, float width, float height, float ratio, const myEngine::Vector4& color, float glow) {
        Rect({left + width * 0.5f, y}, {width, height}, {color.x * 0.16f, color.y * 0.16f, color.z * 0.16f, 0.9f});
        const float filled = width * (std::clamp)(ratio, 0.0f, 1.0f);
        if (glow > 0.01f) {
            Rect({left + filled * 0.5f, y}, {filled, height + 5.0f * state.scale}, Alpha(color, 0.35f * glow));
        }
        Rect({left + filled * 0.5f, y}, {filled, height}, Mix(color, kWhite, 0.65f * glow));
    };

    // 自機の下：HP・経験値のゲージ、左に Lv、右に Q の円。
    const float s = state.scale;
    const float width = 100.0f * s, hpHeight = 5.0f * s, expHeight = 4.5f * s;
    // 画素に合わせて、細い線がぼけないように、また止まっている間は板の更新が要らないようにする。
    const myEngine::Vector2 anchor{std::round(state.anchor.x), std::round(state.anchor.y)};
    const float left = anchor.x - width * 0.5f;
    const float hpY = anchor.y + state.offset;
    const float expY = hpY + hpHeight * 0.5f + 3.0f * s + expHeight * 0.5f;
    const float middleY = (hpY + expY) * 0.5f;
    Rect({anchor.x, middleY}, {width + 6.0f * s, expY - hpY + hpHeight + 6.0f * s}, {0.0f, 0.0f, 0.02f, 0.55f});
    const float danger = state.hpRatio < 0.3f ? 0.5f + 0.5f * std::sin(time_ * 10.0f) : 0.0f;
    gauge(left, hpY, width, hpHeight, state.hpRatio, kHp, danger * 0.6f);
    const float wound = width * (std::clamp)(state.woundRatio, 0.0f, 1.0f);
    Rect({left + width - wound * 0.5f, hpY}, {wound, hpHeight}, {0.5f, 0.5f, 0.56f, 0.9f});
    gauge(left, expY, width, expHeight, shownExp_, kExp, (std::max)(gainFlash_ / 0.18f * 0.6f, levelGlow));

    const float labelHeight = 32.0f * s;
    const myEngine::Vector4 levelColor = Mix(kInk, kExp, levelGlow);
    const float labelRight = left - 7.0f * s;
    const float digitWidth = Digits(std::to_string((std::clamp)(state.level, 0, 999)), {labelRight, middleY}, labelHeight, 1.0f, levelColor);
    const float lvWidth = glyphSizes_[kLv].x * labelHeight * 0.8f / (std::max)(1.0f, glyphSizes_[kLv].y);
    Glyph(kLv, {labelRight - digitWidth - lvWidth * 0.5f, middleY + 1.0f * s}, labelHeight * 0.8f, Alpha(levelColor, 0.9f));

    if (state.ultimateRatio >= 0.0f) {
        const bool full = state.ultimateRatio >= 1.0f;
        const float pulse = full ? 0.5f + 0.5f * std::sin(time_ * 7.0f) : 0.0f;
        const myEngine::Vector2 center{left + width + 23.0f * s, middleY};
        const float radius = (13.0f + 1.2f * pulse) * s;
        const myEngine::Vector4 lit = full ? Mix(kGold, kWhite, 0.45f * pulse) : kUltimate;
        for (int i = 0; i < kRingSegments; ++i) {
            const float part = (static_cast<float>(i) + 0.5f) / static_cast<float>(kRingSegments);
            const float angle = -1.5707964f + 6.2831853f * part;
            const bool on = part <= state.ultimateRatio;
            Rect({center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius}, {6.2f * s, (on ? 3.2f : 2.2f) * s},
                 on ? lit : myEngine::Vector4{0.26f, 0.21f, 0.46f, 0.85f}, angle + 1.5707964f);
        }
        Glyph(kQ, center, 25.0f * s, full ? kGold : Alpha(kInk, 0.9f));
    }

    // 上の真ん中：残り時間の数字とボスの印。ボスがいる間は、印と HP のバー。
    const float topX = static_cast<float>(myEngine::WinApp::kClientWidth) * 0.5f;
    constexpr float kTopY = 40.0f;
    if (state.bossRatio >= 0.0f) {
        constexpr float kBossWidth = 300.0f;
        Glyph(kBossMark, {topX - kBossWidth * 0.5f - 18.0f, kTopY}, 36.0f, kHp);
        gauge(topX - kBossWidth * 0.5f, kTopY, kBossWidth, 7.0f, state.bossRatio, kHp, 0.15f);
    } else if (state.secondsLeft >= 0.0f) {
        const int total = static_cast<int>(state.secondsLeft);
        const std::string seconds = std::to_string(total % 60);
        const std::string text = std::to_string(total / 60) + ":" + (seconds.size() < 2 ? "0" : "") + seconds;
        const float timeWidth = Digits(text, {topX, kTopY}, 54.0f, 0.5f, kInk);
        Glyph(kBossMark, {topX + timeWidth * 0.5f + 18.0f, kTopY}, 36.0f, kHp);
    }
}

void SurvivorCompactHud::Draw()
{
    for (size_t i = 0; i < usedRects_; ++i) {
        rects_[i]->Draw();
    }
    for (size_t i = 0; i < usedGlyphs_; ++i) {
        glyphs_[i]->Draw();
    }
}

} // namespace gameplay
