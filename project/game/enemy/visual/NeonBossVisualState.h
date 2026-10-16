#pragma once
#include "NeonDepthLifecycle.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

// 演出専用の値。このAPIを通してEnemy、戦闘、コントローラーを変更することはできない。
enum class NeonBossAction {
    Idle,
    Reposition,
    Telegraph,
    Attack,
    Dash,
    Reload
};
/// @brief ボスの見た目で再生する動き。
enum class NeonBossMotion {
    Idle,
    Attack,
    HoldPose
};
/// @brief ボスの見た目の生存段階。
enum class NeonBossVisualLife {
    Dormant,
    Alive,
    Dying,
    Finished
};

/// @brief 行動に対応するアニメーションの再生設定。
struct NeonBossAnimationMapping {
    NeonBossMotion motion = NeonBossMotion::Idle;
    float speed = 1.0f;
    bool loop = true;
    bool holdAttackPose = false;
};

// AvatarSample_Bには埋め込みアニメーションが無い。リポジトリで生成した2つのクリップを再利用する。
// 歩き・ダッシュ・リロード・被ダメージ・死亡のクリップが無い場合は、ここで明示した代替を使う。
inline NeonBossAnimationMapping MapNeonBossAnimation(NeonBossAction action, bool alive)
{
    if (!alive) {
        return {NeonBossMotion::HoldPose, 0.0f, false, true};
    }
    switch (action) {
    case NeonBossAction::Reposition:
        return {NeonBossMotion::Idle, 1.35f, true, false};
    case NeonBossAction::Telegraph:
        return {NeonBossMotion::Attack, 0.8f, false, false};
    case NeonBossAction::Attack:
        return {NeonBossMotion::Attack, 1.6f, false, false};
    case NeonBossAction::Dash:
        return {NeonBossMotion::Attack, 1.0f, false, true};
    case NeonBossAction::Reload:
        return {NeonBossMotion::Idle, 0.7f, true, false};
    case NeonBossAction::Idle:
        return {};
    }
    return {};
}

/// @brief HPの割合（0..1）を返す。最大HPが0以下なら0を返す。
inline float NeonBossHpRatio(int hp, int maxHp)
{
    if (maxHp <= 0) {
        return 0.0f;
    }
    return (std::clamp)(static_cast<float>(hp) / static_cast<float>(maxHp), 0.0f, 1.0f);
}

// 負の値なら現在のクリップ時刻を保つ。ライバルのVolley -> DashWarningは
// 同じ生成済みAttackクリップを使うため、前の攻撃が引き絞りポーズを過ぎていれば
// 開始時にそこへ戻す必要がある。Telegraph -> Attackはシークせずに続きから再生する。
inline float NeonBossEntryPoseTime(NeonBossAction action, bool actionChanged, float currentTime)
{
    if (!actionChanged) {
        return -1.0f;
    }
    if (action == NeonBossAction::Dash) {
        return 0.82f;
    }
    if (action == NeonBossAction::Telegraph && currentTime > 0.50f) {
        return 0.50f;
    }
    return -1.0f;
}

/// @brief ボスの見た目の生存段階と行動の変化を追跡する。
class NeonBossVisualState {
public:
    static constexpr float kDeathHoldSeconds = 0.12f;
    static constexpr float kDissolveSeconds = 1.2f;

    /// @brief 初期状態に戻す。
    void Reset()
    {
        *this = NeonBossVisualState{};
    }

    /// @brief 戦闘の状態から見た目の段階を更新し、変化があればフラグを立てる。
    void Update(bool encounterActive, bool alive, int hp, int maxHp, NeonBossAction action, float deltaTime)
    {
        actionChanged_ = animationChanged_ = deathStarted_ = releaseRequested_ = false;
        if (life_ == NeonBossVisualLife::Finished) {
            return;
        }
        if (life_ == NeonBossVisualLife::Dormant && !encounterActive) {
            return;
        }
        if (life_ == NeonBossVisualLife::Dormant) {
            life_ = NeonBossVisualLife::Alive;
        }
        if (life_ == NeonBossVisualLife::Alive) {
            if (!alive || hp <= 0) {
                life_ = NeonBossVisualLife::Dying;
                deathStarted_ = true;
                mapping_ = MapNeonBossAnimation(action_, false);
                // 消滅演出の間は、生きていた最後の色とポーズを保つ。
                return;
            }
            if (!encounterActive) {
                return;
            }
            hpRatio_ = NeonBossHpRatio(hp, maxHp);
            const auto next = MapNeonBossAnimation(action, true);
            actionChanged_ = !hasAction_ || action != action_;
            animationChanged_ = !hasAction_ || next.motion != mapping_.motion;
            if (animationChanged_) {
                ++animationSelectionCount_;
            }
            mapping_ = next;
            action_ = action;
            hasAction_ = true;
            return;
        }
        if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) {
            return;
        }
        deathElapsed_ = (std::min)(kDeathHoldSeconds + kDissolveSeconds, deathElapsed_ + deltaTime);
        dissolveProgress_ = (std::clamp)((deathElapsed_ - kDeathHoldSeconds) / kDissolveSeconds, 0.0f, 1.0f);
        if (dissolveProgress_ >= 1.0f) {
            life_ = NeonBossVisualLife::Finished;
            releaseRequested_ = true;
        }
    }

    // 独立したDepthのライフサイクル。上の旧来のUpdateと定数は、既存の呼び出し側と
    // テストのために、タイミングと対応関係の約束事をそのまま保つ。
    void UpdateDepth(const neondepth::LifecycleInput& input, const neondepth::LifecycleTiming& timing, float permittedDelta)
    {
        actionChanged_ = animationChanged_ = false;
        depth_.Update(input, timing, permittedDelta);
        const auto life = depth_.GetLife();
        life_ = life == neondepth::Life::Dormant                                           ? NeonBossVisualLife::Dormant
                : life == neondepth::Life::Finished                                        ? NeonBossVisualLife::Finished
                : (life == neondepth::Life::Collapse || life == neondepth::Life::Dissolve) ? NeonBossVisualLife::Dying
                                                                                           : NeonBossVisualLife::Alive;
        hpRatio_ = depth_.GetHpRatio();
        dissolveProgress_ = depth_.GetDissolveProgress();
        deathStarted_ = depth_.DidDeathStart();
        releaseRequested_ = depth_.ShouldReleaseResources();
    }

    const neondepth::Lifecycle& GetDepthLifecycle() const
    {
        return depth_;
    }

    NeonBossVisualLife GetLife() const
    {
        return life_;
    }

    NeonBossAction GetAction() const
    {
        return action_;
    }

    const NeonBossAnimationMapping& GetMapping() const
    {
        return mapping_;
    }

    float GetHpRatio() const
    {
        return hpRatio_;
    }

    float GetDissolveProgress() const
    {
        return dissolveProgress_;
    }

    /// @brief 直前のUpdateで行動が変わったかを返す。
    bool DidActionChange() const
    {
        return actionChanged_;
    }

    /// @brief 直前のUpdateで再生するアニメーションが変わったかを返す。
    bool DidAnimationChange() const
    {
        return animationChanged_;
    }

    /// @brief 直前のUpdateで死亡演出が始まったかを返す。
    bool DidDeathStart() const
    {
        return deathStarted_;
    }

    /// @brief 直前のUpdateでリソースの解放を要求したかを返す。
    bool ShouldReleaseResources() const
    {
        return releaseRequested_;
    }

    unsigned GetAnimationSelectionCount() const
    {
        return animationSelectionCount_;
    }

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
