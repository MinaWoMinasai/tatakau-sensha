#include "game/ui/session/SurvivorInventoryHud.h"
#include "SpriteCommon.h"
#include <algorithm>

namespace gameplay {
namespace {
// 1280×720 の画面での並び。左に装備の欄、右に袋の欄。
constexpr float kCardLeft = 64.0f;
constexpr float kCardWidth = 600.0f;
constexpr float kBagLeft = 700.0f;
constexpr float kBagWidth = 516.0f;
constexpr float kTop = 118.0f;
constexpr float kBagLineStep = 23.0f;
// 武器の札は名前と3行の値、シールドと補助は名前と1〜2行。
constexpr float kWeaponCardHeight = 108.0f;
constexpr float kSmallCardHeight = 84.0f;
constexpr float kCardGap = 8.0f;

float CardTop(size_t index)
{
    const float weapons = kTop + 28.0f;
    if (index < SurvivorInventoryHud::kWeaponCards) {
        return weapons + static_cast<float>(index) * (kWeaponCardHeight + kCardGap);
    }
    return weapons + static_cast<float>(SurvivorInventoryHud::kWeaponCards) * (kWeaponCardHeight + kCardGap) +
           static_cast<float>(index - SurvivorInventoryHud::kWeaponCards) * (kSmallCardHeight + kCardGap);
}

myEngine::TextStyle Style(float size, bool bold, const myEngine::Vector4& color)
{
    myEngine::TextStyle style{};
    style.fontFamily = "Meiryo";
    style.fontWeight = bold ? 700 : 400;
    style.fontSize = size;
    style.color = color;
    style.outlineThickness = 0;
    style.padding = 3.0f;
    return style;
}
} // namespace

void SurvivorInventoryHud::Initialize()
{
    auto panel = [](const myEngine::Vector2& position, const myEngine::Vector2& size, const myEngine::Vector4& color) {
        auto sprite = std::make_unique<myEngine::Sprite>();
        sprite->Initialize(myEngine::SpriteCommon::GetInstance(), "resources/white512x512.png");
        sprite->SetPosition(position);
        sprite->SetSize(size);
        sprite->SetColor(color);
        sprite->Update();
        return sprite;
    };
    auto label = [](const myEngine::Vector2& position, const std::string& text, float size, bool bold, const myEngine::Vector4& color) {
        auto result = std::make_unique<myEngine::TextLabel>();
        result->Initialize(myEngine::SpriteCommon::GetInstance(), text, Style(size, bold, color));
        result->SetPosition(position);
        return result;
    };
    backdrop_ = panel({0.0f, 0.0f}, {1280.0f, 720.0f}, {0.0f, 0.005f, 0.012f, 0.95f});
    bagPanel_ = panel({kBagLeft, kTop}, {kBagWidth, 562.0f}, {0.02f, 0.04f, 0.07f, 0.92f});
    for (size_t i = 0; i < cardPanels_.size(); ++i) {
        const float height = i < kWeaponCards ? kWeaponCardHeight : kSmallCardHeight;
        cardPanels_[i] = panel({kCardLeft, CardTop(i)}, {kCardWidth, height}, {0.02f, 0.04f, 0.07f, 0.92f});
        cardStripes_[i] = panel({kCardLeft, CardTop(i)}, {6.0f, height}, {1.0f, 1.0f, 1.0f, 0.9f});
        cardTitles_[i] = label({kCardLeft + 20.0f, CardTop(i) + 6.0f}, " ", 22.0f, true, {1.0f, 1.0f, 1.0f, 1.0f});
        cardDetails_[i] = label({kCardLeft + 20.0f, CardTop(i) + 38.0f}, " ", 17.0f, false, {0.80f, 0.88f, 0.96f, 1.0f});
    }
    title_ = label({kCardLeft, 40.0f}, "持ち物", 34.0f, true, {0.82f, 0.97f, 1.0f, 1.0f});
    help_ = label({kCardLeft, 84.0f}, "E で閉じる　／　開いている間は止まる　／　付け替え・整理・売るのは休憩部屋で", 18.0f, false,
                  {0.62f, 0.74f, 0.86f, 1.0f});
    equipHeader_ = label({kCardLeft, kTop - 2.0f}, "装備", 20.0f, true, {1.0f, 0.86f, 0.36f, 1.0f});
    bagHeader_ = label({kBagLeft + 18.0f, kTop + 8.0f}, "出撃中の袋（倒れると全部失う）", 20.0f, true, {1.0f, 0.86f, 0.36f, 1.0f});
    for (size_t i = 0; i < bagLines_.size(); ++i) {
        bagLines_[i] = label({kBagLeft + 22.0f, kTop + 44.0f + static_cast<float>(i) * kBagLineStep}, " ", 17.0f, false,
                             {0.85f, 0.92f, 1.0f, 1.0f});
    }
}

void SurvivorInventoryHud::SetLine(myEngine::TextLabel& label, const std::string& text, const myEngine::Vector4& color, float size, bool bold)
{
    label.SetStyle(Style(size, bold, color));
    label.SetText(text.empty() ? std::string(" ") : text);
}

void SurvivorInventoryHud::Show(const View& view)
{
    std::array<const Card*, kWeaponCards + 2> cards{&view.weapons[0], &view.weapons[1], &view.weapons[2], &view.shield, &view.support};
    for (size_t i = 0; i < cards.size(); ++i) {
        const Card& card = *cards[i];
        cardEmpty_[i] = card.empty;
        // 構えている武器は板を明るくし、左の帯を太くする。
        cardPanels_[i]->SetColor(card.active ? myEngine::Vector4{0.05f, 0.11f, 0.16f, 0.95f} : myEngine::Vector4{0.02f, 0.04f, 0.07f, 0.92f});
        cardPanels_[i]->Update();
        cardStripes_[i]->SetColor({card.color.x, card.color.y, card.color.z, card.empty ? 0.25f : 0.95f});
        cardStripes_[i]->SetSize({card.active ? 10.0f : 6.0f, i < kWeaponCards ? kWeaponCardHeight : kSmallCardHeight});
        cardStripes_[i]->Update();
        SetLine(*cardTitles_[i], card.empty ? (card.title.empty() ? std::string("（空き）") : card.title + "　（空き）") : card.title,
                card.empty ? myEngine::Vector4{0.5f, 0.56f, 0.64f, 1.0f} : card.color, 22.0f, true);
        SetLine(*cardDetails_[i], card.empty ? std::string() : card.detail, {0.80f, 0.88f, 0.96f, 1.0f}, 17.0f, false);
    }
    bagLineCount_ = (std::min)(view.bag.size(), bagLines_.size());
    for (size_t i = 0; i < bagLineCount_; ++i) {
        SetLine(*bagLines_[i], view.bag[i].text, view.bag[i].color, 17.0f, false);
    }
}

void SurvivorInventoryHud::Draw()
{
    backdrop_->Draw();
    bagPanel_->Draw();
    title_->Draw();
    help_->Draw();
    equipHeader_->Draw();
    bagHeader_->Draw();
    for (size_t i = 0; i < cardPanels_.size(); ++i) {
        cardPanels_[i]->Draw();
        cardStripes_[i]->Draw();
        cardTitles_[i]->Draw();
        if (!cardEmpty_[i]) {
            cardDetails_[i]->Draw();
        }
    }
    for (size_t i = 0; i < bagLineCount_; ++i) {
        bagLines_[i]->Draw();
    }
}
} // namespace gameplay
