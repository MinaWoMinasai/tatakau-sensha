#pragma once
#include "game/ui/session/NeonMenuKit.h"

namespace gameplay {
/// @brief 無限の場の、文字を減らした HUD（計画書 p9 の H4、p11 の R4）。自機の下に HP・経験値の管の線と Q の輪、上の真ん中に残り時間とボスの印を出す。
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
    NeonMenuKit kit_;
    float time_ = 0.0f;
    float shownExp_ = 0.0f;
    float gainFlash_ = 0.0f;
    float levelFlash_ = 0.0f;
    float lastExp_ = -1.0f;
    int lastLevel_ = -1;
};
} // namespace gameplay
