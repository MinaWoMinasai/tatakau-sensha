#pragma once
#include <algorithm>

namespace tankexp {
// Pure progression: the scene supplies actual combat/pickup/dash events. Merely
// pressing an input, waiting, or acknowledging text cannot complete a dash task.
/// @brief 戦闘中の操作課題と次の説明へ進む条件を管理する。
class GuidedCombatTutorial {
public:
    enum class Stage {
        Briefing,
        ShootKill,
        Collect,
        Vitals,
        Dash,
        Upgrade,
        Complete
    };
    /// @brief 画面遷移や動作を開始し、進行時間を初期化する。
    void Begin(int requiredKills = 1, int requiredCredits = 4)
    {
        *this = GuidedCombatTutorial{};
        requiredKills_ = (std::max)(1, requiredKills);
        requiredCredits_ = (std::max)(1, requiredCredits);
    }
    /// @brief 強化専用を開始する。
    void BeginUpgradeOnly()
    {
        *this = GuidedCombatTutorial{};
        stage_ = Stage::Upgrade;
    }
    /// @brief ステージを返す。
    Stage GetStage() const
    {
        return stage_;
    }
    /// @brief 完了であるか判定する。
    bool IsComplete() const
    {
        return stage_ == Stage::Complete;
    }
    /// @brief 戦闘準備完了への消去であるか判定する。
    bool IsCombatReadyToClear() const
    {
        return stage_ == Stage::Upgrade || stage_ == Stage::Complete;
    }
    /// @brief FailedダッシュAttemptsを返す。
    int GetFailedDashAttempts() const
    {
        return failedDashes_;
    }
    /// @brief CompletedDashesを返す。
    int GetCompletedDashes() const
    {
        return completedDashes_;
    }
    /// @brief Killsを返す。
    int GetKills() const
    {
        return kills_;
    }
    /// @brief Collected通貨を返す。
    int GetCollectedCredits() const
    {
        return credits_;
    }
    /// @brief 発射済みが存在するか判定する。
    bool HasFired() const
    {
        return fired_;
    }
    /// @brief Purchase強化が行われたか判定する。
    bool DidPurchaseUpgrade() const
    {
        return purchased_;
    }
    /// @brief ダッシュAttempt有効であるか判定する。
    bool IsDashAttemptActive() const
    {
        return dashAttempt_;
    }
    /// @brief 現在の説明を確認済みにして次の段階へ進む。
    bool Acknowledge()
    {
        if (stage_ == Stage::Briefing) {
            stage_ = Stage::ShootKill;
            return true;
        }
        if (stage_ == Stage::Vitals) {
            stage_ = Stage::Dash;
            dashAttempt_ = false;
            return true;
        }
        return false;
    }
    /// @brief 射撃を記録する。
    void RecordShot()
    {
        if (stage_ != Stage::ShootKill)
            return;
        fired_ = true;
        AdvanceCombat();
    }
    /// @brief 敵撃破を記録する。
    void RecordEnemyDefeat()
    {
        if (stage_ != Stage::ShootKill)
            return;
        kills_ = (std::min)(requiredKills_, kills_ + 1);
        AdvanceCombat();
    }
    /// @brief 通貨Collectedを記録する。
    void RecordCurrencyCollected(int amount)
    {
        if ((stage_ != Stage::ShootKill && stage_ != Stage::Collect) || amount <= 0)
            return;
        credits_ += (std::min)(amount, requiredCredits_ - credits_);
        AdvanceCombat();
    }
    /// @brief ダメージを記録する。
    void RecordDamage()
    {
        if (stage_ == Stage::Dash) {
            damageObserved_ = true;
            if (dashAttempt_)
                dashDamaged_ = true;
        }
    }
    // Observe once per active gameplay frame, after damage/collision processing.
    // HP dips stay latched even if a healing pickup restores HP before dash end.
    /// @brief ダッシュの実行結果をチュートリアルの達成条件へ反映する。
    void ObserveDash(bool actualDashing, int hp)
    {
        if (stage_ != Stage::Dash)
            return;
        if (actualDashing && !dashAttempt_) {
            dashAttempt_ = true;
            dashDamaged_ = damageObserved_;
            dashStartHp_ = hp;
        }
        damageObserved_ = false;
        if (!dashAttempt_)
            return;
        if (hp < dashStartHp_ || hp <= 0)
            dashDamaged_ = true;
        if (actualDashing)
            return;
        dashAttempt_ = false;
        // Practice counts completed movements, never asks beginners to survive hits.
        if (++completedDashes_ >= 3)
            stage_ = Stage::Upgrade;
    }
    /// @brief 強化を解決する。
    bool ResolveUpgrade(bool purchased)
    {
        if (stage_ != Stage::Upgrade)
            return false;
        purchased_ = purchased;
        stage_ = Stage::Complete;
        return true;
    }

private:
    /// @brief 戦闘を進める。
    void AdvanceCombat()
    {
        if (stage_ == Stage::ShootKill && fired_ && kills_ >= requiredKills_)
            stage_ = Stage::Collect;
        if (stage_ == Stage::Collect && credits_ >= requiredCredits_)
            stage_ = Stage::Vitals;
    }
    Stage stage_ = Stage::Briefing;
    int requiredKills_ = 1, requiredCredits_ = 4, kills_ = 0, credits_ = 0, failedDashes_ = 0, dashStartHp_ = 0, completedDashes_ = 0;
    bool fired_ = false, dashAttempt_ = false, dashDamaged_ = false, damageObserved_ = false, purchased_ = false;
};
} // namespace tankexp
