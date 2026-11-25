#include "game/ui/session/SurvivorLevelUpScreen.h"
#include <algorithm>
#include <cmath>

namespace gameplay {

namespace {
using Color = myEngine::Vector4;
// モック（p9）の色。
constexpr Color kInk{0.91f, 0.90f, 1.0f, 1.0f};
constexpr Color kDim{0.54f, 0.51f, 0.72f, 1.0f};
constexpr Color kText{0.81f, 0.79f, 0.94f, 1.0f};
constexpr Color kGrid{0.11f, 0.09f, 0.25f, 1.0f};
constexpr Color kPerk{0.37f, 0.78f, 1.0f, 1.0f};
constexpr Color kGold{1.0f, 0.83f, 0.30f, 1.0f};
constexpr float kWidth = 1280.0f;
constexpr float kHeight = 720.0f;
const std::string kIconFolder = "resources/endless_ui/";
const std::string kWhite = "resources/white512x512.png";

myEngine::TextStyle Style(float size, bool bold, const char* family)
{
    myEngine::TextStyle style{};
    style.fontFamily = family;
    style.fontWeight = bold ? 700 : 500;
    style.fontSize = size;
    style.color = {1.0f, 1.0f, 1.0f, 1.0f};
    style.outlineThickness = 0.0f;
    style.padding = 2.0f;
    return style;
}

// 画面の色（モックの値）を、光の量で計算する描き方に合わせる。そのまま渡すと、暗い色ほど白っぽくなる。
Color Linear(const Color& color)
{
    auto convert = [](float v) {
        return std::pow((std::max)(0.0f, v), 2.2f);
    };
    return {convert(color.x), convert(color.y), convert(color.z), color.w};
}
Color Alpha(Color color, float alpha)
{
    color.w = alpha;
    return color;
}
Color Dark(const Color& color, float k)
{
    return {color.x * k, color.y * k, color.z * k, 1.0f};
}
} // namespace

const std::vector<std::string>& SurvivorLevelUpScreen::IconNames()
{
    static const std::vector<std::string> kIcons{
        "sub_orbit",     "sub_turret",  "sub_mine",     "sub_mortar",   "sub_drone",         "sub_flame",      "sub_pulse",  "perk_lifesteal",
        "perk_killCooldown", "perk_armor", "perk_speed", "perk_magnet", "perk_ultCharge",    "perk_crit",      "perk_area",  "perk_expGain",
        "perk_guard",    "perk_revive", "perk_healDrop", "perk_default", "skill_steadyAim",  "skill_ammoBelt", "skill_battleSense", "main",
        "ultimate",      "ability",     "filler_heal",  "filler_maxHp", "filler_power"};
    return kIcons;
}

void SurvivorLevelUpScreen::ShowPreview(const std::string& key, int tier, bool awaken, float seconds)
{
    preview_.Reset(key, tier, awaken);
    for (float t = 0.0f; t < seconds; t += 1.0f / 60.0f) {
        preview_.Update(1.0f / 60.0f);
    }
}

void SurvivorLevelUpScreen::Initialize()
{
    if (batch_.IsInitialized()) {
        return;
    }
    batch_.Initialize(700);
    batch_.Preload(kWhite);
    // アイコンは始めに全部読み込む（遊んでいる途中の読み込みで止まらないように）。
    for (const std::string& icon : IconNames()) {
        batch_.Preload(kIconFolder + icon + ".png");
    }
    preview_.Preload(batch_);
}

SurvivorLevelUpScreen::Rect SurvivorLevelUpScreen::CardRect(size_t index) const
{
    if (view_.inventory) {
        // 2列に並べる。
        constexpr float kCardWidth = 292.0f, kCardHeight = 66.0f, kGap = 10.0f;
        const float x = 656.0f + static_cast<float>(index % 2) * (kCardWidth + kGap);
        return {x, 84.0f + static_cast<float>(index / 2) * (kCardHeight + kGap), kCardWidth, kCardHeight};
    }
    constexpr float kCardHeight = 150.0f, kGap = 18.0f;
    const float count = static_cast<float>((std::max)(size_t{1}, view_.cards.size()));
    const float top = (kHeight - count * kCardHeight - (count - 1.0f) * kGap) * 0.5f;
    return {656.0f, top + static_cast<float>(index) * (kCardHeight + kGap), 588.0f, kCardHeight};
}

void SurvivorLevelUpScreen::Open(const View& view)
{
    // 途中で設定を入れたときは、ここで作る。
    Initialize();
    view_ = view;
    if (view_.cards.size() > kMaxCards) {
        view_.cards.resize(kMaxCards);
    }
    age_ = 0.0f;
    selected_ = 0;
    lift_.fill(0.0f);
    Select(0);
}

void SurvivorLevelUpScreen::Select(size_t index)
{
    if (view_.cards.empty()) {
        preview_.Reset("", 1, false);
        return;
    }
    selected_ = (std::min)(index, view_.cards.size() - 1);
    const Card& card = view_.cards[selected_];
    preview_.Reset(card.icon, card.tier, card.awaken);
}

int SurvivorLevelUpScreen::Update(float dt, const Input& input)
{
    age_ += dt;
    time_ += dt;
    const int count = static_cast<int>(view_.cards.size());
    int picked = -1;
    if (count > 0) {
        size_t wanted = selected_;
        if (input.move != 0) {
            wanted = static_cast<size_t>((static_cast<int>(selected_) + input.move % count + count) % count);
        }
        int hover = -1;
        for (int i = 0; i < count; ++i) {
            if (CardRect(static_cast<size_t>(i)).Contains(input.mouse)) {
                hover = i;
            }
        }
        // マウスを動かしたときだけ、合わせた札を選ぶ（キーで選んだ札が、置いたままのマウスに戻されないように）。
        if (hover >= 0 && (input.mouseMoved || input.click)) {
            wanted = static_cast<size_t>(hover);
        }
        if (wanted != selected_) {
            Select(wanted);
        }
        if (!view_.inventory && !IsLocked()) {
            if (input.number >= 0 && input.number < count) {
                picked = input.number;
            } else if (input.click && hover >= 0) {
                picked = hover;
            } else if (input.confirm) {
                picked = static_cast<int>(selected_);
            }
        }
    }
    preview_.Update(dt);
    Layout(dt);
    return picked;
}

void SurvivorLevelUpScreen::Panel(const Rect& rect, const Color& color)
{
    batch_.Quad(SurvivorQuadBatch::Panel, kWhite, {rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f}, {rect.width, rect.height}, 0.0f, Linear(color));
}

void SurvivorLevelUpScreen::Glow(const std::string& icon, float x, float y, float size, const Color& color)
{
    const auto& names = IconNames();
    const bool known = std::find(names.begin(), names.end(), icon) != names.end();
    batch_.Quad(SurvivorQuadBatch::IconGlow, kIconFolder + (known ? icon : std::string("perk_default")) + ".png", {x, y}, {size, size}, 0.0f, Linear(color));
}

float SurvivorLevelUpScreen::Text(const std::string& text, const myEngine::TextStyle& style, float x, float y, float anchorX, const Color& color)
{
    return batch_.Label(SurvivorQuadBatch::Text, text, style, x, y, anchorX, Linear(color));
}

void SurvivorLevelUpScreen::Layout(float dt)
{
    batch_.Begin();
    const bool inventory = view_.inventory;
    const auto title = Style(58.0f, true, "Bahnschrift"), subtitle = Style(22.0f, true, "Bahnschrift"), pending = Style(26.0f, true, "Bahnschrift"),
               name = Style(inventory ? 19.0f : 27.0f, true, "Meiryo"), level = Style(inventory ? 15.0f : 20.0f, true, "Bahnschrift"),
               badge = Style(15.0f, true, "Meiryo"), effect = Style(20.0f, false, "Meiryo"), key = Style(15.0f, true, "Bahnschrift"),
               detail = Style(22.0f, true, "Meiryo"), group = Style(14.0f, false, "Meiryo");
    const float open = (std::min)(1.0f, age_ / 0.18f);
    // 地：ほぼ黒にして、プレビューの光を際立たせる。左にうすい格子。
    Panel({0.0f, 0.0f, kWidth, kHeight}, {0.02f, 0.016f, 0.043f, open});
    for (int i = 0; i < 10; ++i) {
        Panel({20.0f + 64.0f * static_cast<float>(i), 90.0f, 1.0f, 510.0f}, Alpha(kGrid, 0.8f * open));
    }
    for (int i = 0; i < 9; ++i) {
        Panel({20.0f, 90.0f + 64.0f * static_cast<float>(i), 610.0f, 1.0f}, Alpha(kGrid, 0.8f * open));
    }

    // 左上：Lv と「LEVEL UP」。
    const float pop = inventory ? 0.0f : (std::max)(0.0f, 0.35f - age_) / 0.35f;
    const float titleWidth = Text("Lv " + std::to_string(view_.level), title, 40.0f, 52.0f, 0.0f, {kInk.x + pop * 0.1f, kInk.y + pop * 0.1f, 1.0f, open});
    const float subtitleWidth =
        Text(inventory ? "持ち物" : "LEVEL UP", subtitle, 40.0f + titleWidth + 18.0f, 62.0f, 0.0f, Alpha(inventory ? kDim : kPerk, open));
    if (view_.pending > 1) {
        Text("×" + std::to_string(view_.pending), pending, 40.0f + titleWidth + 18.0f + subtitleWidth + 16.0f, 61.0f, 0.0f, Alpha(kGold, open));
    }

    // 右：札。
    for (size_t i = 0; i < view_.cards.size(); ++i) {
        const Card& card = view_.cards[i];
        const bool selected = i == selected_;
        lift_[i] += ((selected ? 1.0f : 0.0f) - lift_[i]) * (1.0f - std::exp(-dt * 18.0f));
        Rect rect = CardRect(i);
        // 開くときは右から順に滑り込む。選んでいる札は左へ少し出る。
        const float enter = (std::clamp)((age_ - 0.04f * static_cast<float>((std::min)(i, size_t{6}))) / 0.18f, 0.0f, 1.0f);
        const float slide = (1.0f - enter) * (1.0f - enter) * 70.0f;
        rect.x += slide - lift_[i] * (view_.inventory ? 4.0f : 14.0f);
        const Color color = card.awaken && !view_.inventory ? kGold : card.color;
        const float alpha = enter;
        const float tint = 0.05f + 0.09f * lift_[i];
        Panel(rect, {0.045f + color.x * tint, 0.04f + color.y * tint, 0.10f + color.z * tint, 0.97f * alpha});
        const float edge = 0.30f + 0.70f * lift_[i];
        const float line = selected ? 2.0f : 1.0f;
        const Color border = Alpha(Dark(color, edge), alpha);
        Panel({rect.x, rect.y, rect.width, line}, border);
        Panel({rect.x, rect.y + rect.height - line, rect.width, line}, border);
        Panel({rect.x + rect.width - line, rect.y, line, rect.height}, border);
        Panel({rect.x, rect.y, view_.inventory ? 4.0f : 7.0f, rect.height}, Alpha(color, alpha));
        const float middle = rect.y + rect.height * 0.5f;
        if (view_.inventory) {
            Glow(card.icon, rect.x + 38.0f, middle, 50.0f, Dark(color, alpha * (0.7f + 0.3f * lift_[i])));
            Text(card.name, name, rect.x + 72.0f, middle - 11.0f, 0.0f, Alpha(kInk, alpha));
            Text(card.level, level, rect.x + 72.0f, middle + 15.0f, 0.0f, Alpha(color, alpha));
            continue;
        }
        const float size = 98.0f + 6.0f * lift_[i];
        Glow(card.icon, rect.x + 70.0f, middle, size, Dark(color, alpha * (0.75f + 0.25f * lift_[i])));
        const float textX = rect.x + 140.0f;
        const float nameWidth = Text(card.name, name, textX, rect.y + 48.0f, 0.0f, Alpha(kInk, alpha));
        const float levelX = textX + nameWidth + 14.0f;
        const float levelWidth = Text(card.level, level, levelX, rect.y + 50.0f, 0.0f, Alpha(color, alpha));
        if (!card.badge.empty()) {
            const float badgeWidth = batch_.Measure(card.badge, badge).x + 14.0f;
            const float badgeX = levelX + levelWidth + 12.0f;
            Panel({badgeX, rect.y + 38.0f, badgeWidth, 23.0f}, Alpha(color, alpha));
            Text(card.badge, badge, badgeX + badgeWidth * 0.5f, rect.y + 49.5f, 0.5f, {0.02f, 0.016f, 0.043f, alpha});
        }
        Text(card.effect, effect, textX, rect.y + 98.0f, 0.0f, Alpha(selected ? kInk : kText, alpha));
        // 右下：数字キー。
        Panel({rect.x + rect.width - 38.0f, rect.y + rect.height - 34.0f, 24.0f, 22.0f}, Alpha(kGrid, alpha));
        Text(std::to_string(i + 1), key, rect.x + rect.width - 26.0f, rect.y + rect.height - 23.0f, 0.5f, Alpha(kDim, alpha));
    }

    // 左下：今の持ち物（枠と Lv の点）。持ち物の画面では、選んでいる物の効果。
    preview_.Queue(batch_);
    if (inventory) {
        Text(view_.cards.empty() ? std::string("まだ何も持っていない") : view_.cards[selected_].effect, detail, 40.0f, 652.0f, 0.0f, Alpha(kInk, open));
        batch_.End();
        return;
    }
    float x = 40.0f;
    constexpr float kSlot = 40.0f, kSlotGap = 6.0f;
    for (const Group& owned : view_.groups) {
        if (owned.slots.empty()) {
            continue;
        }
        Text(owned.name, group, x, 622.0f, 0.0f, Alpha(kDim, open));
        for (const Slot& slot : owned.slots) {
            const Color color = slot.icon.empty() ? kGrid : slot.color;
            const Color border = Alpha(Dark(color, slot.icon.empty() ? 1.6f : 0.5f), open);
            Panel({x, 636.0f, kSlot, kSlot}, {0.04f, 0.03f, 0.08f, 0.9f * open});
            Panel({x, 636.0f, kSlot, 1.0f}, border);
            Panel({x, 636.0f + kSlot - 1.0f, kSlot, 1.0f}, border);
            Panel({x, 636.0f, 1.0f, kSlot}, border);
            Panel({x + kSlot - 1.0f, 636.0f, 1.0f, kSlot}, border);
            if (!slot.icon.empty()) {
                Glow(slot.icon, x + kSlot * 0.5f, 636.0f + kSlot * 0.5f, 36.0f, Dark(color, open));
                const int dots = (std::clamp)(slot.maxTier, 0, 7);
                const float dotGap = dots > 5 ? 6.0f : 7.5f;
                float dotX = x + kSlot * 0.5f - dotGap * static_cast<float>(dots - 1) * 0.5f;
                for (int n = 0; n < dots; ++n) {
                    Panel({dotX - 2.0f, 682.0f, 4.0f, 4.0f}, n < slot.tier ? Alpha(color, open) : Alpha(Dark(kGrid, 2.0f), open));
                    dotX += dotGap;
                }
            }
            x += kSlot + kSlotGap;
        }
        x += 12.0f;
    }
    batch_.End();
}

void SurvivorLevelUpScreen::Draw()
{
    batch_.Draw();
}

} // namespace gameplay
