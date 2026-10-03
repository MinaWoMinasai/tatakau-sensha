#pragma once
#include <algorithm>
#include <cmath>

namespace tankexp {
/// @brief 遠征遷移の経過秒と実行中/反映済み状態を保持し、表示係数を計算する。
/// @note 覆いが完全に不透明な時点で部屋の反映を一度だけ通知する。
/// 戦闘や入力の停止と反映処理の実行はGameScene側が担当し、停止は遷移の終了まで続く。
class PresentationTransition {
public:
    /// @brief 非実行中なら遷移を開始し、反映済み状態と経過秒を初期化する。
    /// @return 開始した場合true。既に実行中ならfalseで状態を保持する。
    bool Begin()
    {
        if (active_)
            return false;
        active_ = true;
        committed_ = false;
        age_ = 0;
        return true;
    }
    /// @brief 遷移の時計を進め、反映時点に初めて到達した回だけtrueを返す。
    /// @param dt 経過秒。正の有限値を1回最大0.10秒へ制限し、長いフレームでも反映と終了を分ける。
    /// @return 0.50秒以上になって反映済みへ変えた回だけtrue。遷移完了の通知ではない。
    /// @note falseでも時計は進み、1.05秒以上では実行中状態を解除する。非実行中、またはdtが非有限/0以下の場合は状態を変えない。
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
    /// @brief 遷移の開始から終了までの実行中状態を返す。
    bool IsActive() const
    {
        return active_;
    }
    /// @brief 反映時点を通知済みか返す。遷移終了後も、次の開始までtrueを保持する。
    bool IsCommitted() const
    {
        return committed_;
    }
    /// @brief 開始から内部時計に加算した秒を返す。Advanceの1回分の時間上限を含み、実時間そのものではない。
    float Age() const
    {
        return age_;
    }
    /// @brief 内部時計から画面の覆いの係数を計算する。非実行中なら0。
    float Cover() const
    {
        if (!active_)
            return 0;
        return age_ < 0.65f ? Smooth((age_ - 0.14f) / 0.30f) : 1.0f - Smooth((age_ - 0.65f) / 0.40f);
    }
    /// @brief 内部時計から遷移中の文字の透明度を計算する。非実行中なら0。
    float LabelAlpha() const
    {
        return active_ ? (std::min)(Smooth(age_ / 0.12f), 1.0f - Smooth((age_ - 0.76f) / 0.29f)) : 0;
    }
    /// @brief tを0～1へ制限し、両端で傾きが0になる補間係数を計算する。
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
