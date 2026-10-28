#include "game/ui/session/SurvivorChoiceHud.h"
#include "SpriteCommon.h"
#include <string>

namespace gameplay {

namespace {
std::unique_ptr<myEngine::Sprite> MakePanel(const myEngine::Vector2& position, const myEngine::Vector2& size,
                                            const myEngine::Vector4& color)
{
    auto sprite = std::make_unique<myEngine::Sprite>();
    sprite->Initialize(myEngine::SpriteCommon::GetInstance(), "resources/white512x512.png");
    sprite->SetPosition(position);
    sprite->SetSize(size);
    sprite->SetColor(color);
    sprite->Update();
    return sprite;
}

std::unique_ptr<myEngine::TextLabel> MakeLabel(float size, const myEngine::Vector2& position, const myEngine::Vector4& color)
{
    myEngine::TextStyle style{};
    style.fontFamily = "Meiryo";
    style.fontSize = size;
    style.color = color;
    style.outlineThickness = 0;
    style.padding = 4;
    auto label = std::make_unique<myEngine::TextLabel>();
    label->Initialize(myEngine::SpriteCommon::GetInstance(), " ", style);
    label->SetPosition(position);
    return label;
}

const myEngine::Vector4 kCardColor{0.028f, 0.045f, 0.075f, 0.94f};
const myEngine::Vector4 kCardHoverColor{0.045f, 0.16f, 0.19f, 0.97f};
} // namespace

void SurvivorChoiceHud::Initialize()
{
    // ゲームは止めずにスローで動くので、暗幕は薄くして後ろの戦いが見えるようにする。
    dimmer_ = MakePanel({0.0f, 0.0f}, {1280.0f, 720.0f}, {0.004f, 0.008f, 0.018f, 0.55f});
    for (size_t i = 0; i < cards_.size(); ++i) {
        const float x = CardLeft(i);
        cards_[i] = MakePanel({x, kCardTop}, {kCardWidth, kCardHeight}, kCardColor);
        titles_[i] = MakeLabel(26.0f, {x + 18.0f, kCardTop + 18.0f}, {0.82f, 1.0f, 0.94f, 1.0f});
        bodies_[i] = MakeLabel(20.0f, {x + 18.0f, kCardTop + 86.0f}, {0.84f, 0.90f, 0.96f, 1.0f});
    }
    heading_ = MakeLabel(32.0f, {64.0f, 168.0f}, {0.80f, 1.0f, 0.96f, 1.0f});
    heading_->SetText("部品を選ぶ");
    footer_ = MakeLabel(19.0f, {64.0f, kCardTop + kCardHeight + 22.0f}, {0.77f, 0.87f, 0.94f, 1.0f});
    badgePanel_ = MakePanel({1280.0f - 250.0f, 720.0f - 70.0f}, {226.0f, 48.0f}, {0.02f, 0.06f, 0.07f, 0.88f});
    badge_ = MakeLabel(22.0f, {1280.0f - 238.0f, 720.0f - 64.0f}, {1.0f, 0.86f, 0.36f, 1.0f});
}

void SurvivorChoiceHud::SetOffers(const std::vector<survivor::PartOffer>& offers,
                                  const std::array<survivor::PartDefinition, survivor::kPartCount>& parts)
{
    offerCount_ = (std::min)(offers.size(), cards_.size());
    for (size_t i = 0; i < offerCount_; ++i) {
        const auto& offer = offers[i];
        std::string title = std::to_string(i + 1) + "  ";
        if (offer.part == survivor::PartId::Count) {
            title += "修理キット";
        } else {
            title += std::string(parts[static_cast<size_t>(offer.part)].name) + "  Lv" + std::to_string(offer.nextLevel);
        }
        titles_[i]->SetText(title);
        bodies_[i]->SetText(survivor::DescribeOffer(parts, offer));
    }
}

void SurvivorChoiceHud::SetPending(int pending, bool open)
{
    if (badge_) {
        badge_->SetText("選択 ×" + std::to_string(pending) + "   [Tab]");
    }
    if (footer_) {
        footer_->SetText(open ? "1〜3 キーかクリックで選ぶ　/　Tab で閉じる（あとで選べる）　/　残り " + std::to_string(pending)
                              : std::string(" "));
    }
}

int SurvivorChoiceHud::HitTest(const myEngine::Vector2& mouse) const
{
    for (size_t i = 0; i < offerCount_; ++i) {
        const float x = CardLeft(i);
        if (mouse.x >= x && mouse.x <= x + kCardWidth && mouse.y >= kCardTop && mouse.y <= kCardTop + kCardHeight) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void SurvivorChoiceHud::Update(const myEngine::Vector2& mouse)
{
    const int hovered = HitTest(mouse);
    if (hovered == hovered_) {
        return;
    }
    hovered_ = hovered;
    for (size_t i = 0; i < cards_.size(); ++i) {
        cards_[i]->SetColor(static_cast<int>(i) == hovered_ ? kCardHoverColor : kCardColor);
        cards_[i]->Update();
    }
}

void SurvivorChoiceHud::Draw(bool open, int pending)
{
    myEngine::SpriteCommon::GetInstance()->PreDraw(myEngine::kNormal);
    if (!open) {
        if (pending > 0 && badgePanel_ && badge_) {
            badgePanel_->Draw();
            badge_->Draw();
        }
        return;
    }
    dimmer_->Draw();
    heading_->Draw();
    for (size_t i = 0; i < offerCount_; ++i) {
        cards_[i]->Draw();
        titles_[i]->Draw();
        bodies_[i]->Draw();
    }
    footer_->Draw();
}

} // namespace gameplay
