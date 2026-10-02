#pragma once

#include <Windows.h>
#include <chrono>
#include <thread>

// A frame-rate ceiling, independent of monitor refresh and GPU presentation.
// Delayed frames start a fresh interval: no catch-up frames or 60--65 FPS gap.
namespace cg2 {

/// @brief 目標FPSに合わせて待機し、フレーム間隔と待機精度を計測する。
class FramePacer {
public:
    using Clock = std::chrono::steady_clock;

    /// @brief cg2::FramePacerの処理件数や所要時間を保持し、性能と動作の確認に使う。
    struct Stats {
        float frameMs = 0.0f;
        // Wall time before the limiter, including Present/fence waits.
        float preLimitMs = 0.0f;
        float waitMs = 0.0f;
        float overshootMs = 0.0f;
    };

    static constexpr float kTargetFps = 60.0f;
    static constexpr double kTargetFrameMs = 16.666667;

    /// @brief インスタンスの初期値と利用先を設定する。
    FramePacer() = default;
    /// @brief この型の終了処理を行う。所有している資源の寿命を終了させる。
    ~FramePacer()
    {
        CloseTimer();
    }
    FramePacer(const FramePacer&) = delete;
    FramePacer& operator=(const FramePacer&) = delete;

    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize()
    {
        CloseTimer();
        // Supported on Windows 10 1803+. Older systems retain a safe fallback.
        timer_ = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_MODIFY_STATE | SYNCHRONIZE);
        highResolution_ = timer_ != nullptr;
        if (!timer_) {
            timer_ = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_MODIFY_STATE | SYNCHRONIZE);
        }
        Reset();
    }

    /// @brief 状態と集計値を初期状態へ戻す。
    void Reset()
    {
        reference_ = Clock::now();
        stats_ = {};
    }

    /// @brief 有効を設定する。
    void SetEnabled(bool enabled)
    {
        if (enabled_ == enabled) {
            return;
        }
        enabled_ = enabled;
        Reset();
    }

    /// @brief 有効であるか判定する。
    bool IsEnabled() const
    {
        return enabled_;
    }
    /// @brief 高精度の待機タイマーを利用できているか判定する。
    bool IsHighResolutionTimer() const
    {
        return highResolution_;
    }
    /// @brief フレーム間隔・待機時間・待機超過の計測値をミリ秒で返す。
    const Stats& GetStats() const
    {
        return stats_;
    }

    /// @brief 目標時刻までのフレーム待機を行う。
    void Wait()
    {
        const auto waitStart = Clock::now();
        const auto deadline = reference_ + std::chrono::nanoseconds(16'666'667);
        if (enabled_ && waitStart < deadline) {
            // Block for almost all of the remainder. A bounded 0.5 ms tail
            // absorbs timer wake-up jitter without a frame-long busy wait.
            const auto tail = std::chrono::microseconds(500);
            auto now = waitStart;
            while (now < deadline) {
                const auto remaining = deadline - now;
                if (remaining > tail) {
                    const auto sleepDuration = remaining - tail;
                    using TimerTicks = std::chrono::duration<long long, std::ratio<1, 10'000'000>>;
                    LARGE_INTEGER dueTime{};
                    dueTime.QuadPart = -std::chrono::duration_cast<TimerTicks>(sleepDuration).count();
                    if (timer_ && dueTime.QuadPart < 0 && SetWaitableTimerEx(timer_, &dueTime, 0, nullptr, nullptr, nullptr, 0)) {
                        if (WaitForSingleObject(timer_, INFINITE) != WAIT_OBJECT_0) {
                            std::this_thread::sleep_until(deadline);
                        }
                    } else {
                        std::this_thread::sleep_until(deadline);
                    }
                } else {
                    YieldProcessor();
                }
                now = Clock::now();
            }
        }

        const auto finished = Clock::now();
        stats_.preLimitMs = Milliseconds(waitStart - reference_);
        stats_.waitMs = enabled_ ? Milliseconds(finished - waitStart) : 0.0f;
        stats_.frameMs = Milliseconds(finished - reference_);
        stats_.overshootMs = enabled_ && finished > deadline ? Milliseconds(finished - deadline) : 0.0f;
        reference_ = finished;
    }

private:
    /// @brief 計測した時間をミリ秒へ変換する。
    static float Milliseconds(Clock::duration duration)
    {
        return std::chrono::duration<float, std::milli>(duration).count();
    }

    /// @brief フレーム間隔の計測を終了する。
    void CloseTimer()
    {
        if (timer_) {
            CloseHandle(timer_);
            timer_ = nullptr;
        }
        highResolution_ = false;
    }

    HANDLE timer_ = nullptr;
    bool highResolution_ = false;
    bool enabled_ = true;
    Clock::time_point reference_ = Clock::now();
    Stats stats_{};
};

} // namespace cg2
