#pragma once
// 演出専用のライフサイクル。既存のNeonBossVisualStateのテストが軽いインクルード構成を
// 保てるよう、このヘッダはD3D、モデル、数学、アクターに依存させない。
#include <algorithm>
#include <cmath>

namespace neondepth {
/// @brief Depthボスの生存段階。
enum class Life {
    Dormant,
    Intro,
    Active,
    Collapse,
    Dissolve,
    Finished
};

/// @brief 生存段階の更新に使う戦闘側の値。
struct LifecycleInput {
    bool encounterActive = false, alive = true, intro = false, aborted = false;
    int hp = 1, maxHp = 1;
    float introProgress = 0;
};

/// @brief 崩壊と消滅にかける時間（秒）。
struct LifecycleTiming {
    float collapseSeconds = .35f, dissolveSeconds = 1.2f;
};

/// @brief Depthボスの生存段階と、死亡時の演出の進行を管理する。
class Lifecycle {
public:
    /// @brief 初期状態に戻す。
    void Reset()
    {
        *this = Lifecycle{};
    }

    /// @brief 戦闘の状態から生存段階を更新する。
    void Update(const LifecycleInput& input, const LifecycleTiming& timing, float permittedDelta)
    {
        deathStarted_ = freezeRequested_ = releaseRequested_ = false;
        collapseDelta_ = 0;
        if (life_ == Life::Finished) {
            return;
        }
        if (life_ == Life::Dormant && !input.encounterActive) {
            return;
        }
        if (input.aborted) {
            life_ = Life::Finished;
            releaseRequested_ = true;
            return;
        }
        if (life_ == Life::Dormant) {
            life_ = input.intro ? Life::Intro : Life::Active;
        }
        if (life_ == Life::Intro || life_ == Life::Active) {
            if (!input.alive || input.hp <= 0) {
                // 死亡は取り消せない。後続の入力で誤ってHPや生存フラグが上がっても同じ。
                // 新しい命が始まるのは、戦闘を明示的にResetしたときだけ。
                timing_ = {Bounded(timing.collapseSeconds, .15f, .8f) ? timing.collapseSeconds : .35f,
                           Bounded(timing.dissolveSeconds, .4f, 2) ? timing.dissolveSeconds : 1.2f};
                totalSeconds_ = timing_.collapseSeconds + timing_.dissolveSeconds;
                life_ = Life::Collapse;
                deathStarted_ = true;
                return;
            }
            if (!input.encounterActive) {
                return;
            }
            hpRatio_ = input.maxHp > 0 ? (std::clamp)(float(input.hp) / float(input.maxHp), 0.0f, 1.0f) : 0;
            introProgress_ = Unit(input.introProgress);
            life_ = input.intro ? Life::Intro : Life::Active;
            return;
        }
        const float delta = std::isfinite(permittedDelta) && permittedDelta > 0 ? (std::min)(permittedDelta, .1f) : 0;
        const float previous = deathElapsed_;
        deathElapsed_ = (std::min)(totalSeconds_, deathElapsed_ + delta);
        collapseDelta_ = (std::min)(deathElapsed_, timing_.collapseSeconds) - (std::min)(previous, timing_.collapseSeconds);
        freezeRequested_ = previous < timing_.collapseSeconds && deathElapsed_ >= timing_.collapseSeconds;
        if (deathElapsed_ >= timing_.collapseSeconds) {
            life_ = Life::Dissolve;
        }
        if (deathElapsed_ >= totalSeconds_) {
            life_ = Life::Finished;
            releaseRequested_ = true;
        }
    }

    Life GetLife() const
    {
        return life_;
    }

    float GetHpRatio() const
    {
        return hpRatio_;
    }

    float GetIntroProgress() const
    {
        return introProgress_;
    }

    float GetCollapseProgress() const
    {
        return Unit(deathElapsed_ / timing_.collapseSeconds);
    }

    float GetDissolveProgress() const
    {
        return life_ == Life::Finished ? 1 : Unit((deathElapsed_ - timing_.collapseSeconds) / timing_.dissolveSeconds);
    }

    float GetCollapseDelta() const
    {
        return collapseDelta_;
    }

    const LifecycleTiming& GetLatchedTiming() const
    {
        return timing_;
    }

    /// @brief 直前のUpdateで死亡演出が始まったかを返す。
    bool DidDeathStart() const
    {
        return deathStarted_;
    }

    /// @brief 直前のUpdateでポーズの固定を要求したかを返す。
    bool ShouldFreezePose() const
    {
        return freezeRequested_;
    }

    /// @brief 直前のUpdateでリソースの解放を要求したかを返す。
    bool ShouldReleaseResources() const
    {
        return releaseRequested_;
    }

private:
    /// @brief 値が有限で、指定範囲内にあるかを返す。
    static bool Bounded(float v, float low, float high)
    {
        return std::isfinite(v) && v >= low && v <= high;
    }

    /// @brief 値を0..1に収める。非有限なら0を返す。
    static float Unit(float v)
    {
        return std::isfinite(v) ? (std::clamp)(v, 0.0f, 1.0f) : 0;
    }

    Life life_ = Life::Dormant;
    LifecycleTiming timing_{};
    float hpRatio_ = 1, introProgress_ = 0, deathElapsed_ = 0, collapseDelta_ = 0, totalSeconds_ = .35f + 1.2f;
    bool deathStarted_ = false, freezeRequested_ = false, releaseRequested_ = false;
};
} // namespace neondepth
