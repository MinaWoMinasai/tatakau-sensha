#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include "TankRunDirector.h"

namespace tankexp {
// Automated gameplay probes normally must not alter the player's local history.
// Submission validation opts in only inside its isolated staged game directory.
inline constexpr bool ShouldSaveTutorialCompletion(bool demo,bool validation,bool isolatedSubmission) {
    return !demo&&(!validation||isolatedSubmission);
}
enum class TutorialStep { Move, Shoot, Dash, ClearRoom, Route, Upgrade, Complete, Hidden };
struct TutorialValidationState {
    bool enabled=false;
    float elapsed=0,stepAge=0;
    int lastStep=-1;
    unsigned observedSteps=0;
};

// Input events are latched: a quick player cannot lose a kill or menu selection
// while the previous step's success message is being shown.
class ExpeditionTutorial {
public:
    void Reset(bool previouslyCompleted, bool suppressed = false) {
        *this = ExpeditionTutorial{};
        maySkip_ = previouslyCompleted;
        if (suppressed) step_ = TutorialStep::Hidden;
    }
    void AddMovement(float distance) {
        if (step_ == TutorialStep::Move && std::isfinite(distance) && distance > 0 && distance < 4)
            movement_ += distance;
    }
    void RecordKill() { killed_ = true; }
    void RecordDash() { dashed_ = true; }
    void RecordRoomClear() { roomCleared_ = true; }
    void RecordRoute() { routeChosen_ = true; }
    void RecordUpgrade() { upgraded_ = true; }
    bool Skip() {
        if (!maySkip_) return false;
        step_ = TutorialStep::Hidden;
        return true;
    }
    void Update(float dt) {
        if (!std::isfinite(dt) || dt <= 0 || step_ == TutorialStep::Hidden) return;
        if (step_ == TutorialStep::Complete) {
            timer_ -= dt;
            if (timer_ <= 0) step_ = TutorialStep::Hidden;
            return;
        }
        if (success_) {
            timer_ -= dt;
            if (timer_ > 0) return;
            success_ = false;
            step_ = static_cast<TutorialStep>(static_cast<int>(step_) + 1);
            if (step_ == TutorialStep::Complete) { completed_ = true; timer_ = 2.8f; return; }
        }
        const std::array<bool, 6> achieved = { movement_ >= 5, killed_, dashed_, roomCleared_, routeChosen_, upgraded_ };
        if (achieved[static_cast<size_t>(step_)]) { success_ = true; timer_ = 0.65f; }
    }
    TutorialStep GetStep() const { return step_; }
    bool IsSuccess() const { return success_; }
    bool IsVisible() const { return step_ != TutorialStep::Hidden; }
    bool IsComplete() const { return completed_; }
    bool CanSkip() const { return maySkip_ && IsVisible(); }
    // Keep an emptied first room playable until the movement/dash lessons are done.
    bool CanLeaveFirstRoom() const { return step_ >= TutorialStep::ClearRoom; }
    float GetMoveDistance() const { return movement_; }
private:
    TutorialStep step_ = TutorialStep::Move;
    float movement_ = 0, timer_ = 0;
    bool killed_ = false, dashed_ = false, roomCleared_ = false, routeChosen_ = false, upgraded_ = false;
    bool success_ = false, completed_ = false, maySkip_ = false;
};

// Keep only two ordinary upgrades on the battlefield; rare upgrades remain visible.
inline std::array<bool, tankrun::CardCount> CompactBuildCards(const tankrun::CardCounts& cards) {
    std::array<bool, tankrun::CardCount> visible{};
    int ordinary = 0;
    for (size_t i = 0; i < cards.size(); ++i) {
        if (cards[i] <= 0) continue;
        const bool rare = tankrun::IsRare(static_cast<tankrun::CardId>(i));
        visible[i] = rare || ordinary < 2;
        if (!rare && visible[i]) ++ordinary;
    }
    return visible;
}
} // namespace tankexp
