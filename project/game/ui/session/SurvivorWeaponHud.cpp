#include "game/ui/session/SurvivorWeaponHud.h"
#include "SpriteCommon.h"
#include <algorithm>
#include <cmath>

namespace gameplay {
namespace {
// 輪の中心と半径（1280×720 の画面の右下）。弾の数は輪の左に出す。
constexpr myEngine::Vector2 kRingCenter{1135.0f, 590.0f};
constexpr float kRingRadius = 74.0f;
constexpr float kCounterLeft = 880.0f;
constexpr float kCounterY = 640.0f;
constexpr float kDegToRad = 3.14159265f / 180.0f;
} // namespace

void SurvivorWeaponHud::Initialize()
{
    auto sprite = [](const char* path, const myEngine::Vector4& color) {
        auto result = std::make_unique<myEngine::Sprite>();
        result->Initialize(myEngine::SpriteCommon::GetInstance(), path);
        result->SetColor(color);
        return result;
    };
    for (size_t i = 0; i < ringDots_.size(); ++i) {
        const float angle = static_cast<float>(i) * 360.0f / static_cast<float>(ringDots_.size()) * kDegToRad;
        ringDots_[i] = sprite("resources/white512x512.png", {0.35f, 0.85f, 1.0f, 0.45f});
        ringDots_[i]->SetAnchorPoint({0.5f, 0.5f});
        ringDots_[i]->SetSize({3.0f, 3.0f});
        ringDots_[i]->SetPosition({kRingCenter.x + std::cos(angle) * kRingRadius, kRingCenter.y - std::sin(angle) * kRingRadius});
        ringDots_[i]->Update();
    }
    for (auto& bar : heatBars_) {
        bar = sprite("resources/white512x512.png", {1.0f, 0.55f, 0.2f, 0.9f});
        bar->SetAnchorPoint({0.0f, 0.5f});
    }
    // 構えている枠の後ろの光る板。
    marker_ = sprite("resources/white512x512.png", {0.35f, 1.0f, 0.85f, 0.22f});
    marker_->SetAnchorPoint({0.5f, 0.5f});
    marker_->SetSize({132.0f, 34.0f});
    marker_->SetPosition({kRingCenter.x - kRingRadius, kRingCenter.y});
    marker_->Update();
    progressTrack_ = sprite("resources/white512x512.png", {0.05f, 0.09f, 0.12f, 0.85f});
    progressTrack_->SetPosition({kCounterLeft, kCounterY + 24.0f});
    progressTrack_->SetSize({150.0f, 6.0f});
    progressTrack_->Update();
    progressFill_ = sprite("resources/white512x512.png", {1.0f, 0.86f, 0.36f, 1.0f});
    progressFill_->SetPosition({kCounterLeft, kCounterY + 24.0f});
    progressFill_->SetSize({0.0f, 6.0f});
    progressFill_->Update();

    myEngine::TextStyle style{};
    style.fontFamily = "Meiryo";
    style.fontWeight = 700;
    style.fontSize = 40.0f;
    style.color = {1.0f, 1.0f, 1.0f, 1.0f};
    style.outlineColor = {0.20f, 0.85f, 1.0f, 1.0f};
    style.outlineThickness = 3.0f;
    style.padding = 3.0f;
    style.preserveOutline = true;
    for (size_t i = 0; i < glyphLabels_.size(); ++i) {
        glyphLabels_[i] = std::make_unique<myEngine::TextLabel>();
        glyphLabels_[i]->Initialize(myEngine::SpriteCommon::GetInstance(), i < 10 ? std::to_string(i) : std::string("/"), style);
        glyphPaths_[i] = glyphLabels_[i]->GetTexturePath();
        glyphSizes_[i] = glyphLabels_[i]->GetSprite()->GetSize();
    }
    for (auto& number : numberSprites_) {
        number = std::make_unique<myEngine::Sprite>();
        number->Initialize(myEngine::SpriteCommon::GetInstance(), glyphPaths_[0]);
        number->SetAnchorPoint({0.0f, 0.5f});
    }

    auto label = [&](const char* text, float size, const myEngine::Vector4& color) {
        myEngine::TextStyle labelStyle{};
        labelStyle.fontFamily = "Meiryo";
        labelStyle.fontSize = size;
        labelStyle.color = color;
        labelStyle.outlineThickness = 2.0f;
        labelStyle.padding = 3.0f;
        auto result = std::make_unique<myEngine::TextLabel>();
        result->Initialize(myEngine::SpriteCommon::GetInstance(), text, labelStyle);
        result->SetPosition({kCounterLeft, kCounterY + 28.0f});
        return result;
    };
    reloadingLabel_ = label("リロード中", 20.0f, {1.0f, 0.86f, 0.36f, 1.0f});
    swappingLabel_ = label("切り替え中", 20.0f, {0.95f, 0.55f, 1.0f, 1.0f});
    emptyLabel_ = label("弾倉が空：R でリロード", 20.0f, {1.0f, 0.38f, 0.32f, 1.0f});
    noReserveLabel_ = label("予備の弾がない", 20.0f, {1.0f, 0.38f, 0.32f, 1.0f});
}

void SurvivorWeaponHud::SetSlots(const std::array<Slot, kSlots>& slots)
{
    slots_ = slots;
    myEngine::TextStyle style{};
    style.fontFamily = "Meiryo";
    style.fontWeight = 700;
    style.fontSize = 20.0f;
    style.outlineThickness = 2.0f;
    style.padding = 3.0f;
    for (size_t i = 0; i < kSlots; ++i) {
        style.color = slots[i].empty ? myEngine::Vector4{0.55f, 0.6f, 0.65f, 1.0f} : slots[i].color;
        style.fontSize = 20.0f;
        nameLabels_[i] = std::make_unique<myEngine::TextLabel>();
        nameLabels_[i]->Initialize(myEngine::SpriteCommon::GetInstance(),
                                   std::to_string(i + 1) + " " + (slots[i].empty ? std::string("（空き）") : slots[i].name), style);
        nameLabels_[i]->SetAnchorPoint({0.5f, 0.5f});
        style.fontSize = 18.0f;
        style.color = {0.70f, 0.90f, 1.0f, 1.0f};
        ammoLabels_[i] = std::make_unique<myEngine::TextLabel>();
        ammoLabels_[i]->Initialize(myEngine::SpriteCommon::GetInstance(), slots[i].empty ? std::string(" ") : slots[i].ammo, style);
        ammoLabels_[i]->SetPosition({kCounterLeft, kCounterY - 44.0f});
    }
}

size_t SurvivorWeaponHud::LayoutNumber(size_t firstSprite, const std::string& text, float left, float centerY, float scale,
                                       const myEngine::Vector4& color)
{
    size_t used = 0;
    float x = left;
    for (char c : text) {
        const size_t index = c == '/' ? 10 : static_cast<size_t>(c - '0');
        if (index > 10 || firstSprite + used >= numberSprites_.size()) {
            continue;
        }
        auto& sprite = numberSprites_[firstSprite + used];
        sprite->SetTexture(glyphPaths_[index]);
        sprite->SetSize({glyphSizes_[index].x * scale, glyphSizes_[index].y * scale});
        sprite->SetPosition({x, centerY});
        sprite->SetColor(color);
        sprite->Update();
        x += glyphSizes_[index].x * 0.80f * scale;
        ++used;
    }
    return used;
}

void SurvivorWeaponHud::Update(float dt, const State& state)
{
    state_ = state;
    // 構えている枠が左（180度）に来る角度。回る向きは近い方。
    const float target = 180.0f - static_cast<float>(state.active) * 120.0f;
    if (!angleReady_) {
        angle_ = target;
        angleReady_ = true;
    }
    float delta = std::fmod(target - angle_ + 540.0f, 360.0f) - 180.0f;
    angle_ += delta * (1.0f - std::exp(-dt * 14.0f));
    for (size_t i = 0; i < kSlots; ++i) {
        if (!nameLabels_[i]) {
            continue;
        }
        const float angle = (angle_ + static_cast<float>(i) * 120.0f) * kDegToRad;
        nameLabels_[i]->SetPosition({kRingCenter.x + std::cos(angle) * kRingRadius, kRingCenter.y - std::sin(angle) * kRingRadius});
        nameLabels_[i]->SetAlpha(i == state.active ? 1.0f : 0.45f);
        // 熱の棒：名前の下に、熱の割合の長さで。オーバーヒート中は赤。
        const myEngine::Vector2 at{kRingCenter.x + std::cos(angle) * kRingRadius, kRingCenter.y - std::sin(angle) * kRingRadius};
        const float heat = (std::clamp)(state.heat[i], 0.0f, 1.0f);
        heatBars_[i]->SetPosition({at.x - 30.0f, at.y + 16.0f});
        heatBars_[i]->SetSize({60.0f * heat, 3.0f});
        heatBars_[i]->SetColor(state.overheated[i] ? myEngine::Vector4{1.0f, 0.25f, 0.2f, 1.0f} : myEngine::Vector4{1.0f, 0.62f, 0.25f, 0.9f});
        heatBars_[i]->Update();
    }

    // 弾の数：弾倉 / 予備。エネルギー弾は予備だけを大きく出す。弾倉が空なら赤くする。
    const bool energy = state.magazineMax <= 0;
    const myEngine::Vector4 white{1.0f, 1.0f, 1.0f, 1.0f};
    const myEngine::Vector4 red{1.0f, 0.35f, 0.30f, 1.0f};
    usedNumberSprites_ = 0;
    if (energy) {
        usedNumberSprites_ += LayoutNumber(0, std::to_string((std::min)(state.reserve, 9999)), kCounterLeft, kCounterY, 0.9f,
                                           state.reserve > 0 ? white : red);
    } else {
        const float left = kCounterLeft;
        const size_t first = LayoutNumber(0, std::to_string((std::min)(state.magazine, 999)), left, kCounterY, 1.0f,
                                          state.magazine > 0 ? white : red);
        float x = left;
        for (size_t i = 0; i < first; ++i) {
            x += numberSprites_[i]->GetSize().x * 0.80f;
        }
        usedNumberSprites_ = first;
        usedNumberSprites_ += LayoutNumber(usedNumberSprites_, "/" + std::to_string((std::min)(state.reserve, 9999)), x + 4.0f,
                                           kCounterY + 7.0f, 0.55f, state.reserve > 0 ? myEngine::Vector4{0.75f, 0.9f, 1.0f, 1.0f} : red);
    }
    const float ratio = state.reloadRatio >= 0.0f ? state.reloadRatio : state.swapRatio;
    showProgress_ = ratio >= 0.0f;
    if (showProgress_) {
        progressFill_->SetColor(state.reloadRatio >= 0.0f ? myEngine::Vector4{1.0f, 0.86f, 0.36f, 1.0f}
                                                          : myEngine::Vector4{0.95f, 0.55f, 1.0f, 1.0f});
        progressFill_->SetSize({150.0f * (std::clamp)(ratio, 0.0f, 1.0f), 6.0f});
        progressFill_->Update();
    }
}

void SurvivorWeaponHud::Draw()
{
    if (!marker_) {
        return;
    }
    for (const auto& dot : ringDots_) {
        dot->Draw();
    }
    marker_->Draw();
    for (size_t i = 0; i < kSlots; ++i) {
        if (nameLabels_[i]) {
            nameLabels_[i]->Draw();
            if (!slots_[i].empty && state_.heat[i] > 0.01f) {
                heatBars_[i]->Draw();
            }
        }
    }
    if (state_.active < kSlots && ammoLabels_[state_.active] && !slots_[state_.active].empty) {
        ammoLabels_[state_.active]->Draw();
        for (size_t i = 0; i < usedNumberSprites_; ++i) {
            numberSprites_[i]->Draw();
        }
    }
    if (showProgress_) {
        progressTrack_->Draw();
        progressFill_->Draw();
    }
    // 状態の一言：切り替え中・リロード中・弾倉が空・予備が無い（この順に1つだけ）。
    const bool energy = state_.magazineMax <= 0;
    if (state_.swapRatio >= 0.0f) {
        swappingLabel_->Draw();
    } else if (state_.reloadRatio >= 0.0f) {
        reloadingLabel_->Draw();
    } else if (!energy && state_.magazine <= 0 && state_.reserve > 0) {
        emptyLabel_->Draw();
    } else if ((energy || state_.magazine <= 0) && state_.reserve <= 0) {
        noReserveLabel_->Draw();
    }
}
} // namespace gameplay
