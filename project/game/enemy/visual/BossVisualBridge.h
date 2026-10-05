#pragma once
#include "game/enemy/actor/RivalBossCombat.h"
#include "game/enemy/actor/PrototypeBossCombat.h"
#include "game/enemy/visual/NeonBossVisualState.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

struct BossVisualBridgeInput {
    uint64_t encounterGeneration = 0;
    unsigned shotsFired = 0;
    std::array<float, 3> position{};
    int hp = 1, maxHp = 1;
    bool rivalEnabled = false, rivalPhaseTwo = false, prototypeEnabled = false;
    RivalBossCombat::Phase rivalPhase = RivalBossCombat::Phase::Reposition;
    PrototypeBossCombat::Phase prototypePhase = PrototypeBossCombat::Phase::Recovery;
};

struct BossVisualDecision {
    NeonBossAction action = NeonBossAction::Idle;
    bool phaseTwo = false;
    bool resetEncounter = false;
};

// A value-only bridge owns short presentation history, without Enemy, Scene,
// animation or renderer pointers. It cannot advance combat or modify its input.
class BossVisualBridge {
public:
    BossVisualDecision Update(const BossVisualBridgeInput& input, float deltaTime) {
        BossVisualDecision result;
        if (encounterGeneration_ != input.encounterGeneration) {
            encounterGeneration_ = input.encounterGeneration;
            previousShots_ = input.shotsFired;
            attackPresentationTimer_ = 0.0f;
            previousPosition_ = input.position;
            result.resetEncounter = true;
        }
        result.phaseTwo = input.rivalEnabled ? input.rivalPhaseTwo : input.hp <= input.maxHp / 2;
        if (input.rivalEnabled) {
            using P = RivalBossCombat::Phase;
            switch (input.rivalPhase) {
            case P::Reposition: result.action = NeonBossAction::Reposition; break;
            case P::Tracking: case P::Locked: case P::DashWarning: result.action = NeonBossAction::Telegraph; break;
            case P::Volley: result.action = NeonBossAction::Attack; break;
            case P::Dash: result.action = NeonBossAction::Dash; break;
            case P::Reload: result.action = NeonBossAction::Reload; break;
            }
        } else if (input.prototypeEnabled) {
            using P = PrototypeBossCombat::Phase;
            switch (input.prototypePhase) {
            case P::Recovery: result.action = NeonBossAction::Idle; break;
            case P::Telegraph: result.action = NeonBossAction::Telegraph; break;
            case P::Attack: result.action = NeonBossAction::Attack; break;
            }
        } else {
            const float x = input.position[0] - previousPosition_[0];
            const float y = input.position[1] - previousPosition_[1];
            const float z = input.position[2] - previousPosition_[2];
            if (std::sqrt(x*x + y*y + z*z) > 0.001f) result.action = NeonBossAction::Reposition;
        }
        // AimedSpread emits and returns to Recovery in one combat tick. This
        // hold affects only the visual; it never replaces a telegraph or Dash.
        if (input.shotsFired != previousShots_) attackPresentationTimer_ = 0.70f;
        else if (deltaTime > 0.0f)
            attackPresentationTimer_ = (std::max)(0.0f, attackPresentationTimer_ - deltaTime);
        if (!input.rivalEnabled && attackPresentationTimer_ > 0.0f &&
            (result.action == NeonBossAction::Idle || result.action == NeonBossAction::Reposition))
            result.action = NeonBossAction::Attack;
        previousShots_ = input.shotsFired;
        previousPosition_ = input.position;
        return result;
    }

private:
    uint64_t encounterGeneration_ = 0;
    unsigned previousShots_ = 0;
    float attackPresentationTimer_ = 0.0f;
    std::array<float, 3> previousPosition_{};
};
