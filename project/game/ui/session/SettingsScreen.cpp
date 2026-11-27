#include "game/ui/session/SettingsScreen.h"
#include "game/settings/UserSettings.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace gameplay {

namespace {
enum Row { kMusic, kEffects, kScreen, kShake, kCalm, kCurve, kAuto, kReplay, kRows };
constexpr std::array<const char*, kRows> kNames{"音楽", "効果音", "画面", "画面のゆれ", "点滅を弱める", "画面の曲がり", "自動で撃つ", "説明をもう一度"};
constexpr float kLeft = 76.8f, kTop = 140.8f, kStep = 42.6f, kRowHeight = 40.7f, kNameX = 125.5f, kValueX = 371.0f, kFont = 26.2f;

int* Level(int row)
{
    UserSettings& s = usersettings::Get();
    return row == kMusic ? &s.music : row == kEffects ? &s.effects : row == kShake ? &s.shake : row == kCurve ? &s.curve : nullptr;
}

bool* Flag(int row)
{
    UserSettings& s = usersettings::Get();
    return row == kScreen ? &s.fullscreen : row == kCalm ? &s.calm : row == kAuto ? &s.autoFire : nullptr;
}

const char* FlagText(int row)
{
    const bool on = *Flag(row);
    return row == kScreen ? (on ? "全画面" : "窓") : on ? "する" : "しない";
}
} // namespace

void SettingsScreen::Open(const MenuInput& input, bool confirmReplay)
{
    open_ = true;
    confirmReplay_ = confirmReplay;
    armed_ = false;
    selected_ = 0;
    age_ = 0.0f;
    shake_ = 0.0f;
    pad_ = input.pad;
}

void SettingsScreen::Apply(int row)
{
    moved_ = true;
    if (row == kScreen) {
        usersettings::ApplyWindow();
    } else if (row == kShake) {
        shake_ = 0.35f;
    }
    usersettings::Save();
}

void SettingsScreen::Adjust(int row, int direction, bool wrap)
{
    if (int* level = Level(row)) {
        const int before = *level;
        *level = wrap && *level >= 10 ? 0 : (std::clamp)(*level + direction, 0, 10);
        if (*level != before) {
            Apply(row);
        }
    } else if (bool* flag = Flag(row)) {
        *flag = !*flag;
        Apply(row);
    }
}

SettingsScreen::Result SettingsScreen::Update(float dt, const MenuInput& input)
{
    age_ += dt;
    shake_ = (std::max)(0.0f, shake_ - dt);
    moved_ = false;
    pad_ = input.pad;
    if (input.back && age_ > 0.05f) {
        if (armed_) {
            armed_ = false;
            return Result::None;
        }
        open_ = false;
        return Result::Closed;
    }
    int wanted = selected_;
    if (input.up) {
        wanted = (wanted + kRows - 1) % kRows;
    }
    if (input.down) {
        wanted = (wanted + 1) % kRows;
    }
    int hover = -1;
    for (int i = 0; i < kRows; ++i) {
        const float y = kTop + static_cast<float>(i) * kStep;
        if (input.mouse.x >= kLeft - 10.0f && input.mouse.x <= kLeft + 768.0f && input.mouse.y >= y && input.mouse.y < y + kRowHeight) {
            hover = i;
        }
    }
    if (hover >= 0 && (input.mouseMoved || input.click)) {
        wanted = hover;
    }
    if (wanted != selected_) {
        selected_ = wanted;
        armed_ = false;
        moved_ = true;
    }
    if (input.left || input.right) {
        Adjust(selected_, input.right ? 1 : -1, false);
    }
    const bool click = input.click && hover >= 0;
    if (age_ < 0.12f || !(input.confirm || click)) {
        return Result::None;
    }
    if (selected_ == kReplay) {
        if (confirmReplay_ && !armed_) {
            armed_ = true;
            return Result::None;
        }
        open_ = false;
        return Result::ReplayTutorial;
    }
    // 目盛りを押したら、その段にする。
    int* level = Level(selected_);
    if (click && level && input.mouse.x >= kValueX - 8.0f && input.mouse.x < kValueX + 10.0f * 30.1f) {
        *level = (std::clamp)(static_cast<int>((input.mouse.x - kValueX + 3.0f) / 30.1f) + 1, 1, 10);
        Apply(selected_);
        return Result::None;
    }
    Adjust(selected_, 1, true);
    return Result::None;
}

void SettingsScreen::Queue(NeonMenuKit& kit)
{
    kit.Darken(NeonMenuKit::Shade::Full);
    kit.Text("設定", 41.0f, kLeft, 94.0f, 0.0f, menucolor::kWhite);
    // ゆれを変えたら、行が少しゆれて強さが分かる。
    const float wave = shake_ > 0.0f ? std::sin(shake_ * 90.0f) * shake_ / 0.35f * usersettings::ShakeScale() * 7.0f : 0.0f;
    for (int i = 0; i < kRows; ++i) {
        const bool on = i == selected_;
        const float y = kTop + static_cast<float>(i) * kStep + kRowHeight * 0.5f;
        const float x = i == kShake ? wave : 0.0f;
        const bool warn = i == kReplay && armed_;
        if (on) {
            const auto& glow = warn ? menucolor::kRed : menucolor::kGreen;
            kit.Tube({kLeft + 2.7f, y}, {kLeft + 30.6f, y}, 5.4f, menucolor::Core(glow, 0.94f), glow, 1.0f, 28.0f);
        }
        kit.Text(warn ? "もう一度きめると、やりなおす" : kNames[static_cast<size_t>(i)], kFont, kNameX - 2.0f + x, y, 0.0f,
                 warn ? menucolor::kRed : on ? menucolor::kWhite : menucolor::kItem);
        if (const int* level = Level(i)) {
            kit.Meter(kValueX + x, y, *level, on);
        } else if (Flag(i)) {
            kit.Text(FlagText(i), kFont, kValueX - 2.0f, y, 0.0f, {0.812f, 0.847f, 0.855f, 1.0f});
        }
    }
    kit.Hints({{{"W", "S"}, "L", "えらぶ"}, {{"A", "D"}, "L", "かえる"}, {{"Esc"}, "B", "もどる"}}, pad_);
}

std::string SettingsScreen::Describe() const
{
    const int* level = Level(selected_);
    return std::to_string(selected_) + "=" + (level ? std::to_string(*level) : Flag(selected_) ? (*Flag(selected_) ? "on" : "off") : armed_ ? "!" : "") +
           (pad_ ? "p" : "k");
}

} // namespace gameplay
