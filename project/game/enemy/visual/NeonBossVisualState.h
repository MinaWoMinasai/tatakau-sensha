#pragma once
#include "NeonDepthLifecycle.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

// Presentation-only values. No Enemy/Combat/controller can be mutated through this API.
enum class NeonBossAction { Idle, Reposition, Telegraph, Attack, Dash, Reload };
enum class NeonBossMotion { Idle, Attack, HoldPose };
enum class NeonBossVisualLife { Dormant, Alive, Dying, Finished };

struct NeonBossAnimationMapping {
    NeonBossMotion motion = NeonBossMotion::Idle;
    float speed = 1.0f;
    bool loop = true;
    bool holdAttackPose = false;
};

// AvatarSample_B has no embedded animation. Reuse the repository's two generated
// clips. Missing walk/dash/reload/damage/death clips use these explicit fallbacks.
inline NeonBossAnimationMapping MapNeonBossAnimation(NeonBossAction action, bool alive) {
    if (!alive) return {NeonBossMotion::HoldPose, 0.0f, false, true};
    switch (action) {
    case NeonBossAction::Reposition: return {NeonBossMotion::Idle, 1.35f, true, false};
    case NeonBossAction::Telegraph: return {NeonBossMotion::Attack, 0.8f, false, false};
    case NeonBossAction::Attack: return {NeonBossMotion::Attack, 1.6f, false, false};
    case NeonBossAction::Dash: return {NeonBossMotion::Attack, 1.0f, false, true};
    case NeonBossAction::Reload: return {NeonBossMotion::Idle, 0.7f, true, false};
    case NeonBossAction::Idle: return {};
    }
    return {};
}

inline float NeonBossHpRatio(int hp, int maxHp) {
    if (maxHp <= 0) return 0.0f;
    return (std::clamp)(static_cast<float>(hp) / static_cast<float>(maxHp), 0.0f, 1.0f);
}

// Negative means preserve current clip time. Rival Volley -> DashWarning uses
// the same generated Attack clip, so entry must restore its pull pose if the
// previous attack already passed it. Telegraph -> Attack resumes without seeking.
inline float NeonBossEntryPoseTime(NeonBossAction action, bool actionChanged, float currentTime) {
    if (!actionChanged) return -1.0f;
    if (action == NeonBossAction::Dash) return 0.82f;
    if (action == NeonBossAction::Telegraph && currentTime > 0.50f) return 0.50f;
    return -1.0f;
}

class NeonBossVisualState {
public:
    static constexpr float kDeathHoldSeconds = 0.12f;
    static constexpr float kDissolveSeconds = 1.2f;

    void Reset() { *this = NeonBossVisualState{}; }

    void Update(bool encounterActive, bool alive, int hp, int maxHp, NeonBossAction action, float deltaTime) {
        actionChanged_ = animationChanged_ = deathStarted_ = releaseRequested_ = false;
        if (life_ == NeonBossVisualLife::Finished) return;
        if (life_ == NeonBossVisualLife::Dormant && !encounterActive) return;
        if (life_ == NeonBossVisualLife::Dormant) life_ = NeonBossVisualLife::Alive;
        if (life_ == NeonBossVisualLife::Alive) {
            if (!alive || hp <= 0) {
                life_ = NeonBossVisualLife::Dying;
                deathStarted_ = true;
                mapping_ = MapNeonBossAnimation(action_, false);
                // Preserve the last alive color and pose throughout dissolution.
                return;
            }
            if (!encounterActive) return;
            hpRatio_ = NeonBossHpRatio(hp, maxHp);
            const auto next = MapNeonBossAnimation(action, true);
            actionChanged_ = !hasAction_ || action != action_;
            animationChanged_ = !hasAction_ || next.motion != mapping_.motion;
            if (animationChanged_) ++animationSelectionCount_;
            mapping_ = next;
            action_ = action;
            hasAction_ = true;
            return;
        }
        if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) return;
        deathElapsed_ = (std::min)(kDeathHoldSeconds + kDissolveSeconds, deathElapsed_ + deltaTime);
        dissolveProgress_ = (std::clamp)((deathElapsed_ - kDeathHoldSeconds) / kDissolveSeconds, 0.0f, 1.0f);
        if (dissolveProgress_ >= 1.0f) {
            life_ = NeonBossVisualLife::Finished;
            releaseRequested_ = true;
        }
    }

    // Independent Depth lifecycle. The legacy Update and constants above retain
    // their exact timing and mapping contract for existing callers and tests.
    void UpdateDepth(const neondepth::LifecycleInput& input,
        const neondepth::LifecycleTiming& timing, float permittedDelta) {
        actionChanged_ = animationChanged_ = false;
        depth_.Update(input, timing, permittedDelta);
        const auto life = depth_.GetLife();
        life_ = life == neondepth::Life::Dormant ? NeonBossVisualLife::Dormant :
            life == neondepth::Life::Finished ? NeonBossVisualLife::Finished :
            (life == neondepth::Life::Collapse || life == neondepth::Life::Dissolve) ?
                NeonBossVisualLife::Dying : NeonBossVisualLife::Alive;
        hpRatio_ = depth_.GetHpRatio();
        dissolveProgress_ = depth_.GetDissolveProgress();
        deathStarted_ = depth_.DidDeathStart();
        releaseRequested_ = depth_.ShouldReleaseResources();
    }
    const neondepth::Lifecycle& GetDepthLifecycle() const { return depth_; }

    NeonBossVisualLife GetLife() const { return life_; }
    NeonBossAction GetAction() const { return action_; }
    const NeonBossAnimationMapping& GetMapping() const { return mapping_; }
    float GetHpRatio() const { return hpRatio_; }
    float GetDissolveProgress() const { return dissolveProgress_; }
    bool DidActionChange() const { return actionChanged_; }
    bool DidAnimationChange() const { return animationChanged_; }
    bool DidDeathStart() const { return deathStarted_; }
    bool ShouldReleaseResources() const { return releaseRequested_; }
    unsigned GetAnimationSelectionCount() const { return animationSelectionCount_; }

private:
    neondepth::Lifecycle depth_{};
    NeonBossVisualLife life_ = NeonBossVisualLife::Dormant;
    NeonBossAction action_ = NeonBossAction::Idle;
    NeonBossAnimationMapping mapping_{};
    float hpRatio_ = 1.0f, deathElapsed_ = 0.0f, dissolveProgress_ = 0.0f;
    unsigned animationSelectionCount_ = 0;
    bool hasAction_ = false, actionChanged_ = false, animationChanged_ = false;
    bool deathStarted_ = false, releaseRequested_ = false;
};
