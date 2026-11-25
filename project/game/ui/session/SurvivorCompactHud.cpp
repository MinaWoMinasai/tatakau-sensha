#include "game/ui/session/SurvivorCompactHud.h"
#include "WinApp.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace gameplay {

namespace {
using Color = myEngine::Vector4;
} // namespace

void SurvivorCompactHud::Initialize()
{
    kit_.Initialize(220);
    // 数字の画像は始めに作っておく（遊んでいる途中で止まらないように）。
    for (int i = 0; i < 10; ++i) {
        kit_.MeasureText(std::to_string(i), 38.0f, true);
    }
    kit_.MeasureText(":", 38.0f, true);
}

void SurvivorCompactHud::Update(float dt, const State& state)
{
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

    kit_.Begin();
    // 横の管1本：暗い溝と、光る中身。
    auto tube = [&](float left, float y, float width, float line, float ratio, const Color& track, const Color& color, float glow, float white) {
        kit_.Tube({left, y}, {left + width, y}, line, track, track, 0.0f, 0.0f);
        const float filled = width * (std::clamp)(ratio, 0.0f, 1.0f);
        if (filled > 0.5f) {
            kit_.Tube({left, y}, {left + filled, y}, line, menucolor::Core(color, white), color, glow, 11.0f);
        }
    };

    // 自機の下：HP・経験値の管と、右に Q の輪（モックの大きさ。1ワールド単位＝32）。
    const float s = state.scale;
    const float width = 96.0f * s;
    const myEngine::Vector2 anchor{std::round(state.anchor.x), std::round(state.anchor.y)};
    const float left = anchor.x - width * 0.5f;
    const float hpY = anchor.y + state.offset + 4.0f * s;
    const float expY = hpY + 10.0f * s;
    const float danger = state.hpRatio < 0.3f ? 0.5f + 0.5f * std::sin(time_ * 10.0f) : 0.0f;
    tube(left, hpY, width, 3.2f * s, state.hpRatio, {0.235f, 0.118f, 0.157f, 1.0f}, menucolor::kRed, 0.55f + 0.6f * danger, 0.80f);
    const float wound = width * (std::clamp)(state.woundRatio, 0.0f, 1.0f);
    if (wound > 1.0f) {
        const Color grey{0.5f, 0.5f, 0.56f, 1.0f};
        kit_.Tube({left + width - wound, hpY}, {left + width, hpY}, 3.2f * s, grey, grey, 0.0f, 0.0f);
    }
    tube(left, expY, width, 2.6f * s, shownExp_, {0.098f, 0.176f, 0.235f, 1.0f}, menucolor::kCyan,
         0.45f + 0.8f * (std::max)(gainFlash_ / 0.18f * 0.6f, levelGlow), 0.80f + 0.2f * levelGlow);
    if (state.ultimateRatio >= 0.0f) {
        const bool full = state.ultimateRatio >= 1.0f;
        const myEngine::Vector2 center{left + width + 24.0f * s, (hpY + expY) * 0.5f};
        const float radius = 13.4f * s;
        const Color track{0.235f, 0.216f, 0.353f, 1.0f};
        kit_.Ring(center, radius, 2.2f * s, track, track, 0.0f, 0.0f);
        if (full) {
            kit_.Ring(center, radius, 2.6f * s, menucolor::Core(menucolor::kGold), menucolor::kGold, 1.0f + 0.5f * std::sin(time_ * 6.0f), 14.0f);
        } else if (state.ultimateRatio > 0.01f) {
            kit_.Ring(center, radius, 2.6f * s, menucolor::Core(menucolor::kGold, 0.75f), menucolor::kGold, 0.35f, 5.0f, 0.0f, state.ultimateRatio);
        }
    }

    // 上の真ん中：残り時間の数字とボスの印。ボスがいる間は、印と HP の管。
    const float topX = static_cast<float>(myEngine::WinApp::kClientWidth) * 0.5f;
    constexpr float kTopY = 46.0f;
    auto mark = [&](float x, float y) {
        std::array<myEngine::Vector2, 5> points{};
        for (size_t i = 0; i < points.size(); ++i) {
            const float angle = -1.5707964f + 6.2831853f * static_cast<float>(i) / 5.0f;
            points[i] = {x + std::cos(angle) * 14.0f, y + std::sin(angle) * 14.0f};
        }
        for (size_t i = 0; i < points.size(); ++i) {
            kit_.Tube(points[i], points[(i + 1) % points.size()], 2.2f, menucolor::Core(menucolor::kRed), menucolor::kRed, 0.6f, 10.0f);
        }
    };
    if (state.bossRatio >= 0.0f) {
        constexpr float kBossWidth = 320.0f;
        mark(topX - kBossWidth * 0.5f - 34.0f, kTopY);
        tube(topX - kBossWidth * 0.5f, kTopY, kBossWidth, 5.0f, state.bossRatio, {0.235f, 0.118f, 0.157f, 1.0f}, menucolor::kRed, 0.7f, 0.80f);
    } else if (state.secondsLeft >= 0.0f) {
        const int total = static_cast<int>(state.secondsLeft);
        const std::string seconds = std::to_string(total % 60);
        const std::string text = std::to_string(total / 60) + ":" + (seconds.size() < 2 ? "0" : "") + seconds;
        // 数字は1文字ずつ、幅をそろえて並べる（減っていく時間が左右に揺れず、文字の画像も増えない）。
        constexpr float kSize = 38.0f, kDigit = 21.0f, kColon = 12.0f;
        float all = 0.0f;
        for (char c : text) {
            all += c == ':' ? kColon : kDigit;
        }
        float x = topX - all * 0.5f;
        for (char c : text) {
            const float step = c == ':' ? kColon : kDigit;
            kit_.Text(std::string(1, c), kSize, x + step * 0.5f, kTopY, 0.5f, menucolor::kWhite, true);
            x += step;
        }
        mark(topX + all * 0.5f + 40.0f, kTopY - 1.0f);
    }
    kit_.End();
}

void SurvivorCompactHud::Draw()
{
    kit_.Draw();
}

} // namespace gameplay
