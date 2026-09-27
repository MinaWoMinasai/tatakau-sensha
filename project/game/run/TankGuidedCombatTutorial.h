#pragma once
#include <algorithm>

namespace tankexp {
// Pure progression: the scene supplies actual combat/pickup/dash events. Merely
// pressing an input, waiting, or acknowledging text cannot complete a dash task.
class GuidedCombatTutorial {
public:
    enum class Stage { Briefing, ShootKill, Collect, Vitals, Dash, Upgrade, Complete };
    void Begin(int requiredKills=1,int requiredCredits=4) {
        *this=GuidedCombatTutorial{};
        requiredKills_=(std::max)(1,requiredKills);
        requiredCredits_=(std::max)(1,requiredCredits);
    }
    void BeginUpgradeOnly() {
        *this=GuidedCombatTutorial{};
        stage_=Stage::Upgrade;
    }
    Stage GetStage() const {return stage_;}
    bool IsComplete() const {return stage_==Stage::Complete;}
    bool IsCombatReadyToClear() const {return stage_==Stage::Upgrade||stage_==Stage::Complete;}
    int GetFailedDashAttempts() const {return failedDashes_;}
    int GetCompletedDashes() const {return completedDashes_;}
    int GetKills() const {return kills_;}
    int GetCollectedCredits() const {return credits_;}
    bool HasFired() const {return fired_;}
    bool DidPurchaseUpgrade() const {return purchased_;}
    bool IsDashAttemptActive() const {return dashAttempt_;}
    bool Acknowledge() {
        if(stage_==Stage::Briefing) {stage_=Stage::ShootKill;return true;}
        if(stage_==Stage::Vitals) {stage_=Stage::Dash;dashAttempt_=false;return true;}
        return false;
    }
    void RecordShot() {
        if(stage_!=Stage::ShootKill) return;
        fired_=true;AdvanceCombat();
    }
    void RecordEnemyDefeat() {
        if(stage_!=Stage::ShootKill) return;
        kills_=(std::min)(requiredKills_,kills_+1);AdvanceCombat();
    }
    void RecordCurrencyCollected(int amount) {
        if((stage_!=Stage::ShootKill&&stage_!=Stage::Collect)||amount<=0) return;
        credits_+=(std::min)(amount,requiredCredits_-credits_);
        AdvanceCombat();
    }
    void RecordDamage() {
        if(stage_==Stage::Dash) {damageObserved_=true;if(dashAttempt_) dashDamaged_=true;}
    }
    // Observe once per active gameplay frame, after damage/collision processing.
    // HP dips stay latched even if a healing pickup restores HP before dash end.
    void ObserveDash(bool actualDashing,int hp) {
        if(stage_!=Stage::Dash) return;
        if(actualDashing&&!dashAttempt_) {
            dashAttempt_=true;dashDamaged_=damageObserved_;dashStartHp_=hp;
        }
        damageObserved_=false;
        if(!dashAttempt_) return;
        if(hp<dashStartHp_||hp<=0) dashDamaged_=true;
        if(actualDashing) return;
        dashAttempt_=false;
        // Practice counts completed movements, never asks beginners to survive hits.
        if(++completedDashes_>=3) stage_=Stage::Upgrade;
    }
    bool ResolveUpgrade(bool purchased) {
        if(stage_!=Stage::Upgrade) return false;
        purchased_=purchased;stage_=Stage::Complete;return true;
    }
private:
    void AdvanceCombat() {
        if(stage_==Stage::ShootKill&&fired_&&kills_>=requiredKills_) stage_=Stage::Collect;
        if(stage_==Stage::Collect&&credits_>=requiredCredits_) stage_=Stage::Vitals;
    }
    Stage stage_=Stage::Briefing;
    int requiredKills_=1,requiredCredits_=4,kills_=0,credits_=0,failedDashes_=0,dashStartHp_=0,completedDashes_=0;
    bool fired_=false,dashAttempt_=false,dashDamaged_=false,damageObserved_=false,purchased_=false;
};
}
