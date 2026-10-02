#pragma once
#include <algorithm>
#include <cmath>

namespace tankexp {
// A room is committed once, behind a fully opaque curtain. Gameplay and input
// remain stopped until the reveal finishes; long frames cannot skip that gate.
/// @brief 遠征の画面遷移の時間と表示状態を管理する。
class PresentationTransition {
public:
    /// @brief 画面遷移や動作を開始し、進行時間を初期化する。
    bool Begin()
    {
        if (active_)
            return false;
        active_ = true;
        committed_ = false;
        age_ = 0;
        return true;
    }
    /// @brief 経過時間に応じて現在の状態を進める。
    /// @param dt この処理で進める経過時間（秒）。
    bool Advance(float dt)
    {
        if (!active_ || !std::isfinite(dt) || dt <= 0)
            return false;
        age_ += (std::min)(dt, 0.10f);
        if (!committed_ && age_ >= 0.50f) {
            committed_ = true;
            return true;
        }
        if (age_ >= 1.05f)
            active_ = false;
        return false;
    }
    /// @brief 有効であるか判定する。
    bool IsActive() const
    {
        return active_;
    }
    /// @brief Committedであるか判定する。
    bool IsCommitted() const
    {
        return committed_;
    }
    /// @brief 開始からの経過時間を返す。
    float Age() const
    {
        return age_;
    }
    /// @brief 画面遷移の覆いの進行度を返す。
    float Cover() const
    {
        if (!active_)
            return 0;
        return age_ < 0.65f ? Smooth((age_ - 0.14f) / 0.30f) : 1.0f - Smooth((age_ - 0.65f) / 0.40f);
    }
    /// @brief 画面遷移の文字の透明度を返す。
    float LabelAlpha() const
    {
        return active_ ? (std::min)(Smooth(age_ / 0.12f), 1.0f - Smooth((age_ - 0.76f) / 0.29f)) : 0;
    }
    /// @brief 補間係数を滑らかにして返す。
    static float Smooth(float t)
    {
        t = (std::clamp)(t, 0.0f, 1.0f);
        return t * t * (3 - 2 * t);
    }

private:
    bool active_ = false, committed_ = false;
    float age_ = 0;
};
} // namespace tankexp
