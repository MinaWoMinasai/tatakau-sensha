#pragma once

#include <chrono>
#include <cmath>

namespace myEngine {

/// @brief ゲームの更新1回で進める固定の時間（秒）。60FPSの1フレーム分。
inline constexpr float kFixedStepSeconds = 1.0f / 60.0f;
/// @brief 1秒あたりの固定ステップ数。1フレームあたりの量で書かれた値を秒あたりへ換算するときに使う。
inline constexpr float kFixedStepsPerSecond = 60.0f;

/// @brief 実時間を測り、描画1回ごとに回す固定ステップの回数を決める（アキュムレータ）。
/// @note 60FPS前後の小さな揺れは1ステップに丸め、60FPSで動いているときは今までどおり描画1回につき更新1回になるようにする。
class FixedTimestep {
public:
    using Clock = std::chrono::steady_clock;

    /// @brief 描画1回で回す更新の最大回数。これを超える遅れは切り捨て、ゲームを少し遅くして処理が追いつかなくなるのを防ぐ。
    /// @note 4回なら15FPSまでは実時間と同じ速さで進む。
    static constexpr int kMaxStepsPerFrame = 4;
    /// @brief 1フレームの経過時間の上限（秒）。ウィンドウのドラッグやブレークポイントで空いた時間を、まとめて進めないようにする。
    static constexpr double kMaxFrameSeconds = 0.25;
    /// @brief 経過時間がステップの整数倍からこの範囲（秒）にあれば、ちょうど整数倍とみなす。
    /// @note FPS上限の待機は0.1ms未満の誤差で戻るので、60FPS動作では毎回ちょうど1ステップになる。
    static constexpr double kSnapSeconds = 0.0005;

    /// @brief 測定とため込んだ時間を捨てる。次のAdvanceは前回の時刻を持たないので1ステップを返す。
    /// @note 一時停止・読み込み・シーン切り替えの後に呼び、待っていた時間をまとめて進めないようにする。
    void Reset()
    {
        hasPrevious_ = false;
        accumulatorSeconds_ = 0.0;
    }

    /// @brief 前回の呼び出しからの実時間をため、今回の描画までに回す固定ステップの回数を返す。0回もあり得る。
    int Advance(Clock::time_point now)
    {
        if (!hasPrevious_) {
            hasPrevious_ = true;
            previous_ = now;
            lastFrameSeconds_ = kStepSeconds;
            return 1;
        }
        double elapsed = std::chrono::duration<double>(now - previous_).count();
        previous_ = now;
        lastFrameSeconds_ = elapsed;
        if (elapsed > kMaxFrameSeconds) {
            droppedSeconds_ += elapsed - kMaxFrameSeconds;
            elapsed = kMaxFrameSeconds;
        }
        const double nearestSteps = std::round(elapsed / kStepSeconds);
        if (nearestSteps >= 1.0 && std::abs(elapsed - nearestSteps * kStepSeconds) < kSnapSeconds) {
            elapsed = nearestSteps * kStepSeconds;
        }
        accumulatorSeconds_ += elapsed;

        int steps = 0;
        while (steps < kMaxStepsPerFrame && accumulatorSeconds_ >= kStepSeconds - kEpsilonSeconds) {
            accumulatorSeconds_ -= kStepSeconds;
            ++steps;
        }
        if (accumulatorSeconds_ < 0.0) {
            accumulatorSeconds_ = 0.0;
        }
        if (accumulatorSeconds_ >= kStepSeconds) {
            // 上限まで回しても残った遅れは追い上げない。1ステップ未満の端数だけを次へ持ち越す。
            const double kept = std::fmod(accumulatorSeconds_, kStepSeconds);
            droppedSeconds_ += accumulatorSeconds_ - kept;
            accumulatorSeconds_ = kept;
        }
        return steps;
    }

    /// @brief 直前のAdvanceで測った実時間（秒）。上限で切る前の値。
    double GetLastFrameSeconds() const
    {
        return lastFrameSeconds_;
    }

    /// @brief 上限を超えたため進めずに捨てた時間（秒）の合計。
    double GetDroppedSeconds() const
    {
        return droppedSeconds_;
    }

private:
    static constexpr double kStepSeconds = 1.0 / 60.0;
    // ちょうど整数倍に丸めた時間を引き続けたときの、浮動小数点の誤差を吸収する。
    static constexpr double kEpsilonSeconds = 1e-9;

    Clock::time_point previous_{};
    bool hasPrevious_ = false;
    double accumulatorSeconds_ = 0.0;
    double lastFrameSeconds_ = 0.0;
    double droppedSeconds_ = 0.0;
};

} // namespace myEngine
