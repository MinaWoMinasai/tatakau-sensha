#include "game/ui/session/SurvivorLoadoutHud.h"
#include "SpriteCommon.h"

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

const myEngine::Vector4 kRowColor{0.028f, 0.045f, 0.075f, 0.94f};
const myEngine::Vector4 kRowHoverColor{0.20f, 0.06f, 0.07f, 0.97f};
// 左下の積載の表示の1行の高さ（文字18の行送り）。
constexpr float kLoadoutLineHeight = 25.0f;
constexpr size_t kLoadoutPartsPerLine = 3;
} // namespace

void SurvivorLoadoutHud::Initialize()
{
    loadoutPanel_ = MakePanel({16.0f, 600.0f}, {470.0f, 100.0f}, {0.01f, 0.03f, 0.05f, 0.62f});
    loadoutText_ = MakeLabel(18.0f, {26.0f, 606.0f}, {0.80f, 0.93f, 1.0f, 1.0f});
    dimmer_ = MakePanel({0.0f, 0.0f}, {1280.0f, 720.0f}, {0.01f, 0.004f, 0.006f, 0.72f});
    heading_ = MakeLabel(26.0f, {64.0f, 120.0f}, {1.0f, 0.82f, 0.78f, 1.0f});
    footer_ = MakeLabel(19.0f, {64.0f, 614.0f}, {0.86f, 0.86f, 0.90f, 1.0f});
    footer_->SetText("数字キーかクリックで外す　/　Esc で3択へ戻る（積まない）　/　Tab で閉じる（あとで選べる）");
    for (size_t i = 0; i < kMaxRows; ++i) {
        const myEngine::Vector2 position = RowPosition(i);
        rows_[i] = MakePanel(position, {kRowWidth, kRowHeight}, kRowColor);
        rowTexts_[i] = MakeLabel(20.0f, {position.x + 14.0f, position.y + 8.0f}, {0.90f, 0.95f, 1.0f, 1.0f});
    }
}

myEngine::Vector2 SurvivorLoadoutHud::RowPosition(size_t row) const
{
    const size_t column = row / 7, line = row % 7;
    return {64.0f + static_cast<float>(column) * (kRowWidth + 24.0f), kRowsTop + static_cast<float>(line) * (kRowHeight + kRowGap)};
}

void SurvivorLoadoutHud::SetLoadout(const std::array<survivor::PartDefinition, survivor::kPartCount>& parts,
                                    const survivor::PartLevels& levels, int capacity)
{
    if (!loadoutText_) {
        return;
    }
    // 1行目に積載、2行目からは「名前 Lv」を3つずつ並べる。
    std::string text = "積載 " + std::to_string(survivor::LoadOf(parts, levels)) + " / " + std::to_string(capacity);
    size_t count = 0;
    for (size_t i = 0; i < survivor::kPartCount; ++i) {
        if (levels[i] <= 0) {
            continue;
        }
        text += count % kLoadoutPartsPerLine == 0 ? "\n" : "　";
        text += std::string(parts[i].name) + " " + std::to_string(levels[i]);
        ++count;
    }
    loadoutText_->SetText(text);
    const size_t lines = 1 + (count + kLoadoutPartsPerLine - 1) / kLoadoutPartsPerLine;
    loadoutHeight_ = static_cast<float>(lines) * kLoadoutLineHeight + 14.0f;
    const float top = 720.0f - 16.0f - loadoutHeight_;
    loadoutPanel_->SetPosition({16.0f, top});
    loadoutPanel_->SetSize({470.0f, loadoutHeight_});
    loadoutPanel_->Update();
    loadoutText_->SetPosition({26.0f, top + 6.0f});
}

void SurvivorLoadoutHud::SetRemoval(const std::array<survivor::PartDefinition, survivor::kPartCount>& parts,
                                    const survivor::PartLevels& levels, int capacity, survivor::PartId incoming, int refundPerLevel)
{
    rowParts_.clear();
    for (size_t i = 0; i < survivor::kPartCount && rowParts_.size() < kMaxRows; ++i) {
        if (levels[i] > 0) {
            rowParts_.push_back(static_cast<survivor::PartId>(i));
        }
    }
    const int load = survivor::LoadOf(parts, levels);
    const auto& coming = parts[static_cast<size_t>(incoming)];
    const int need = (std::max)(0, load + coming.weight - capacity);
    heading_->SetText("外す部品を選ぶ（積載 " + std::to_string(load) + " / " + std::to_string(capacity) + "）\n「" + std::string(coming.name) +
                      "」（重さ" + std::to_string(coming.weight) + "）を積むには、あと " + std::to_string(need) + " 空ける");
    for (size_t row = 0; row < rowParts_.size(); ++row) {
        const size_t index = static_cast<size_t>(rowParts_[row]);
        const std::string key = row < 9 ? std::to_string(row + 1) : row == 9 ? std::string("0") : std::string("－");
        rowTexts_[row]->SetText(key + "  " + parts[index].name + "  Lv" + std::to_string(levels[index]) + "　　重さ " +
                                std::to_string(parts[index].weight) + "　　欠片 " + std::to_string(levels[index] * refundPerLevel));
    }
    hovered_ = -2;
}

int SurvivorLoadoutHud::HitTest(const myEngine::Vector2& mouse) const
{
    for (size_t row = 0; row < rowParts_.size(); ++row) {
        const myEngine::Vector2 position = RowPosition(row);
        if (mouse.x >= position.x && mouse.x <= position.x + kRowWidth && mouse.y >= position.y && mouse.y <= position.y + kRowHeight) {
            return static_cast<int>(row);
        }
    }
    return -1;
}

void SurvivorLoadoutHud::Update(const myEngine::Vector2& mouse)
{
    const int hovered = HitTest(mouse);
    if (hovered == hovered_) {
        return;
    }
    hovered_ = hovered;
    for (size_t row = 0; row < kMaxRows; ++row) {
        rows_[row]->SetColor(static_cast<int>(row) == hovered_ ? kRowHoverColor : kRowColor);
        rows_[row]->Update();
    }
}

void SurvivorLoadoutHud::DrawLoadout()
{
    if (!loadoutPanel_ || !loadoutText_) {
        return;
    }
    myEngine::SpriteCommon::GetInstance()->PreDraw(myEngine::kNormal);
    loadoutPanel_->Draw();
    loadoutText_->Draw();
}

void SurvivorLoadoutHud::DrawRemoval()
{
    myEngine::SpriteCommon::GetInstance()->PreDraw(myEngine::kNormal);
    dimmer_->Draw();
    heading_->Draw();
    for (size_t row = 0; row < rowParts_.size(); ++row) {
        rows_[row]->Draw();
        rowTexts_[row]->Draw();
    }
    footer_->Draw();
}

} // namespace gameplay
