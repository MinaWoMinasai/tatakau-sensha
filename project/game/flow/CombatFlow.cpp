#include "game/flow/CombatFlow.h"
#include "game/session/GameplaySystems.h"
#include "game/weapon/CombatTypes.h"
#include "RuntimeProfiler.h"
#include "StartupTrace.h"
#include "GameStartMode.h"
#include "game/ui/TankCombatNeonGeometry.h"
#include "game/effects/TankSpecialNeonGeometry.h"
#include "CollisionConfig.h"
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include "game/session/GameplayHelpers.h"

namespace gameplay {

using namespace detail;

void CombatFlow::UpdateGameplayEventEffects(float, bool)
{
    if (!world_.resources.player_ || !world_.resources.enemy_) {
        return;
    }
    if (world_.run.expeditionRun_ && world_.resources.player_->ConsumePrimaryAttackPerformedEvent())
        world_.run.tankExpeditionAudio_.Shot();

    // HPは各攻撃/衝突で適用済み。前回からの純減分を演出と累積表示へ使い、ここではHPを減らさない。
    const int playerHp = world_.resources.player_->GetHp();
    if (world_.combat.previousPlayerHp_ >= 0 && playerHp < world_.combat.previousPlayerHp_) {
        const int damage = world_.combat.previousPlayerHp_ - playerHp;
        world_.combat.damageTaken_ += damage;
        const cg2::Vector3 hitDelta = world_.resources.player_->GetWorldPosition() - world_.resources.enemy_->GetWorldPosition();
        cg2::Vector2 hitDirection{hitDelta.x, hitDelta.y};
        const float length = std::sqrt(hitDirection.x * hitDirection.x + hitDirection.y * hitDirection.y);
        if (length > 0.0001f) {
            hitDirection.x /= length;
            hitDirection.y /= length;
        }
        world_.combat.screenEffectDirector_.TriggerPlayerDamage(hitDirection);
        world_.presentation.cameraShakeDuration_ = world_.combat.screenEffectDirector_.GetConfig().cameraShakeDuration;
        world_.presentation.cameraShakeTimer_ = world_.presentation.cameraShakeDuration_;
        world_.presentation.cameraShakePower_ =
            (std::max)(world_.presentation.cameraShakePower_, world_.combat.screenEffectDirector_.GetConfig().cameraShakeStrength);
        SetEventCallout("ダメージ", 0.42f);
        if (world_.run.expeditionRun_)
            world_.run.tankExpeditionAudio_.ArmorBreak();
    }
    world_.combat.previousPlayerHp_ = playerHp;

    const float hpRatio = world_.resources.player_->GetMaxHp() > 0
                              ? static_cast<float>(playerHp) / static_cast<float>(world_.resources.player_->GetMaxHp())
                              : 0.0f;
    world_.combat.screenEffectDirector_.SetLowHpRatio(hpRatio);

    const bool dashing = world_.resources.player_->IsDashing();
    if (dashing && !world_.combat.previousDashing_) {
        world_.combat.screenEffectDirector_.TriggerDash(
            world_.gameplayQueries->WorldToScreenUv(world_.resources.player_->GetWorldPosition()));
        if (world_.run.expeditionRun_)
            world_.run.tankExpeditionAudio_.Dash();
    }
    world_.combat.previousDashing_ = dashing;

    if (world_.resources.player_->ConsumeEvolutionConfirmed()) {
        world_.combat.screenEffectDirector_.TriggerUpgradeConfirmed(
            world_.gameplayQueries->WorldToScreenUv(world_.resources.player_->GetWorldPosition()));
        if (world_.run.expeditionRun_)
            world_.run.tankExpeditionAudio_.Upgrade();
        SetEventCallout("EVOLUTION COMPLETE", 0.75f);
    }
    if (world_.resources.player_->ConsumeEvolutionCancelled()) {
        SetEventCallout("EVOLUTION CANCELLED", 0.45f);
    }

    if (!world_.run.expeditionRun_ && !world_.combat.bossEntryTriggered_ && world_.combat.phase_ == Phase::kMain &&
        world_.combat.playTime_ >= 1.25f) {
        world_.combat.bossEntryTriggered_ = true;
        world_.combat.screenEffectDirector_.TriggerBossEntry();
        SetEventCallout("ボス出現", 1.20f);
    }

    if (world_.run.expeditionRun_ && world_.gameplayQueries->IsRunRivalActive() &&
        world_.resources.enemy_->GetHp() < world_.combat.previousBossHp_)
        world_.run.tankExpeditionAudio_.Hit();
    world_.combat.previousBossHp_ = world_.resources.enemy_->GetHp();
    const auto outcome = SelectCombatDeathOutcome(
        {world_.run.expeditionRun_, world_.resources.player_->IsDead(), world_.combat.playerDeathHandled_,
         world_.gameplayQueries->IsRunRivalActive(), world_.resources.enemy_->IsDead(), world_.combat.bossDefeatHandled_,
         world_.run.expeditionRun_ && world_.run.tankExpedition_.GetRoomKind() == tankexp::RoomKind::Boss});
    if (outcome == CombatDeathOutcome::PlayerDeath)
        BeginGameOver();
    else if (outcome == CombatDeathOutcome::BossDefeat)
        BeginBossDefeatSequence();
}

void CombatFlow::BeginBossDefeatSequence()
{
    // 撃破演出の状態へ移り、次回Updateの戦闘分岐を止める。敵や弾の実体をここでは削除しない。
    if (world_.run.expeditionMapEnabled_)
        world_.run.expeditionCollectAll_ = true;
    if (world_.run.expeditionRun_)
        world_.run.tankExpedition_.CompleteRoom();
    if (world_.run.prototypeRun_) {
        world_.run.tankRun_.CompleteBoss();
        world_.arenaRunController->RefreshTankRunUi();
    }
    world_.combat.bossDefeatHandled_ = true;
    world_.combat.combatFlow_.BeginBossDefeat(world_.combat.screenEffectDirector_.GetConfig().dissolveSpeed,
                                              world_.combat.screenEffectDirector_.GetConfig().bossDefeatImpactDelay);
    if (world_.combat.flowBannerText_) {
        world_.combat.flowBannerText_->SetText("ボス撃破");
    }
    const cg2::Vector2 center = world_.gameplayQueries->WorldToScreenUv(world_.resources.enemy_->GetWorldPosition());
    world_.combat.screenEffectDirector_.TriggerBossDefeat(center);
    world_.presentation.cameraShakeDuration_ = world_.combat.screenEffectDirector_.GetConfig().cameraShakeDuration * 4.5f;
    world_.presentation.cameraShakeTimer_ = world_.presentation.cameraShakeDuration_;
    world_.presentation.cameraShakePower_ = world_.combat.screenEffectDirector_.GetConfig().cameraShakeStrength * 2.75f;
    SetEventCallout("ボス撃破", 1.35f);
}

void CombatFlow::BeginGameOver()
{
    if (world_.resources.enemy_ && world_.resources.enemy_->IsNeonDepthEncounterEnabled())
        world_.resources.enemy_->AbortNeonDepthEncounter();
    world_.depthEncounter->RestoreNeonDepthCamera();
    // 死亡済みの自機に対する進行・結果状態を設定する。戦闘の消去ではなく、以後の更新を止める入口。
    if (world_.run.expeditionMapEnabled_)
        world_.run.expeditionMapRun_.MarkDead();
    if (world_.run.expeditionRun_)
        world_.run.tankExpedition_.MarkDead();
    if (world_.run.prototypeRun_) {
        world_.run.tankRun_.MarkDead();
        world_.arenaRunController->RefreshTankRunUi();
    }
    world_.combat.playerDeathHandled_ = true;
    world_.combat.combatFlow_.BeginGameOver(world_.combat.screenEffectDirector_.GetConfig().gameOverDuration);
    if (world_.combat.flowBannerText_) {
        world_.combat.flowBannerText_->SetText("GAME OVER");
    }
    world_.combat.screenEffectDirector_.TriggerGameOver();
    world_.combatEffects->TriggerDeathPostPulse(world_.resources.player_->GetWorldPosition(), 1.10f);
    world_.presentation.cameraShakeDuration_ = 0.42f;
    world_.presentation.cameraShakeTimer_ = world_.presentation.cameraShakeDuration_;
    world_.presentation.cameraShakePower_ = 0.75f;
    SetEventCallout("戦闘不能", 0.85f);
    UpdateResultText();
}

void CombatFlow::UpdateGameFlow(float baseDeltaTime)
{
    const auto events =
        world_.combat.combatFlow_.Update(baseDeltaTime, world_.run.expeditionMapEnabled_ && !world_.run.expeditionCredits_.empty());
    if (events.bossImpact)
        world_.combatEffects->TriggerDeathPostPulse(world_.resources.enemy_->GetWorldPosition(), 1.55f);
    if (events.bossResultReady && (!world_.resources.enemy_->IsNeonDepthEncounterEnabled() || !world_.resources.neonBossVisual_ ||
                                   world_.resources.neonBossVisual_->IsFinished())) {
        if (world_.run.expeditionMapEnabled_) {
            world_.run.expeditionMapRun_.CompleteCombat();
            world_.run.expeditionCollectAll_ = false;
        }
        EnterResultState(true);
    }
    if (events.gameOverTextReady)
        UpdateResultText();
    if (events.waitForPresentation)
        return;

    if (world_.demo.titleDemo_)
        return;
    if (world_.combat.combatFlow_.GetState() != GameFlowState::StageClear &&
        world_.combat.combatFlow_.GetState() != GameFlowState::GameOver) {
        return;
    }
    if (world_.run.prototypeRun_) {
        if (world_.run.expeditionMapEnabled_ && (world_.run.tankExpeditionBalanceEditorOpen_ || world_.run.expeditionRoomEditorOpen_ ||
                                                 world_.run.expeditionMapEditorOpen_ || world_.run.expeditionContentEditorOpen_))
            return;
        const auto triggered = [this](int key) {
            return world_.resources.input_->IsKeyTriggered(static_cast<uint8_t>(key));
        };
        if (triggered(DIK_W) || triggered(DIK_S) || triggered(DIK_UP) || triggered(DIK_DOWN) || triggered(DIK_LEFT) || triggered(DIK_RIGHT))
            world_.combat.resultSelection_ = 1 - world_.combat.resultSelection_;
        bool confirm = triggered(DIK_RETURN) || triggered(DIK_SPACE);
        for (int i = 0; i < 2; ++i)
            if (triggered(DIK_1 + i)) {
                world_.combat.resultSelection_ = i;
                confirm = true;
            }
        const auto mouse = world_.resources.input_->GetMousePosition();
        const auto motion = world_.resources.input_->GetMouseState();
        for (int i = 0; i < 2; ++i) {
            const float x = 64.0f + static_cast<float>(i) * 388.0f;
            if (mouse.x < x || mouse.x > x + 368 || mouse.y < 280 || mouse.y > 560)
                continue;
            if (motion.lX || motion.lY)
                world_.combat.resultSelection_ = i;
            if (world_.resources.input_->IsTrigger(motion.rgbButtons[0], world_.resources.input_->GetPreMouseState().rgbButtons[0])) {
                world_.combat.resultSelection_ = i;
                confirm = true;
            }
        }
        if (confirm)
            ConfirmResultSelection();
        return;
    }

    const bool up =
        world_.resources.input_->IsTrigger(world_.resources.input_->GetKey()[DIK_W], world_.resources.input_->GetPreKey()[DIK_W]) ||
        world_.resources.input_->IsTrigger(world_.resources.input_->GetKey()[DIK_UP], world_.resources.input_->GetPreKey()[DIK_UP]);
    const bool down =
        world_.resources.input_->IsTrigger(world_.resources.input_->GetKey()[DIK_S], world_.resources.input_->GetPreKey()[DIK_S]) ||
        world_.resources.input_->IsTrigger(world_.resources.input_->GetKey()[DIK_DOWN], world_.resources.input_->GetPreKey()[DIK_DOWN]);
    if (up || down) {
        world_.combat.resultSelection_ = 1 - world_.combat.resultSelection_;
        UpdateResultText();
    }

    const bool confirm =
        world_.resources.input_->IsTrigger(world_.resources.input_->GetKey()[DIK_RETURN],
                                           world_.resources.input_->GetPreKey()[DIK_RETURN]) ||
        world_.resources.input_->IsTrigger(world_.resources.input_->GetKey()[DIK_SPACE], world_.resources.input_->GetPreKey()[DIK_SPACE]) ||
        world_.resources.input_->IsTrigger(world_.resources.input_->GetMouseState().rgbButtons[0],
                                           world_.resources.input_->GetPreMouseState().rgbButtons[0]);
    if (confirm) {
        ConfirmResultSelection();
    }
}

void CombatFlow::EnterResultState(bool stageClear)
{
    world_.combat.combatFlow_.EnterResult(stageClear);
    if (world_.combat.flowBannerText_) {
        world_.combat.flowBannerText_->SetText(world_.run.expeditionRun_ ? (stageClear ? "遠征クリア" : "遠征終了")
                                                                         : (stageClear ? "STAGE CLEAR" : "GAME OVER"));
    }
    world_.combat.resultSelection_ = 0;
    UpdateResultText();
}

void CombatFlow::ConfirmResultSelection()
{
    if (world_.combat.phase_ != Phase::kMain) {
        return;
    }
    if (world_.combat.resultSelection_ == 0) {
        world_.combat.nextSceneName_ = world_.run.expeditionRun_ ? "TANK_EXPEDITION" : world_.run.prototypeRun_ ? "TANK_RUN" : "GAME";
    } else {
        world_.combat.nextSceneName_ = "TITLE";
    }
    world_.combat.fade_->Start(Fade::Status::FadeOut, 0.65f);
    world_.combat.phase_ = Phase::kFadeOut;
}

void CombatFlow::UpdateResultText()
{
    if (!world_.combat.resultSummaryText_ || !world_.combat.resultMenuText_) {
        return;
    }
    const int totalSeconds = static_cast<int>(world_.combat.playTime_);
    const int minutes = totalSeconds / 60;
    const int seconds = totalSeconds % 60;

    std::ostringstream summary;
    summary << "RESULT\n\n"
            << "Clear Time       " << std::setfill('0') << std::setw(2) << minutes << ":" << std::setw(2) << seconds << "\n"
            << "Just Dodge       " << world_.combat.justDodgeCount_ << "\n"
            << "Damage Taken     " << world_.combat.damageTaken_ << "\n"
            << "Defeated Enemies " << world_.combat.defeatedEnemies_;
    world_.combat.resultSummaryText_->SetText(summary.str());

    std::ostringstream menu;
    menu << (world_.combat.resultSelection_ == 0 ? "> " : "  ") << "RETRY\n"
         << (world_.combat.resultSelection_ == 1 ? "> " : "  ") << "RETURN TO TITLE\n\n"
         << "W/S or Arrow Keys : Select   Enter/Click : Confirm";
    world_.combat.resultMenuText_->SetText(menu.str());
    if (world_.run.prototypeRun_ && world_.run.tankRunHeading_)
        world_.arenaRunController->RefreshTankRunUi();
}

void CombatFlow::SetEventCallout(const std::string& text, float duration)
{
    if (world_.combat.eventCalloutText_) {
        world_.combat.eventCalloutText_->SetText(text);
        auto style = world_.combat.eventCalloutText_->GetStyle();
        style.color = (text == "ダメージ" || text == "戦闘不能") ? cg2::Vector4{1, 0.23f, 0.16f, 1} : cg2::Vector4{0.68f, 1, 0.88f, 1};
        world_.combat.eventCalloutText_->SetStyle(style);
    }
    world_.combat.eventCalloutTimer_ = (std::max)(0.0f, duration);
}
} // namespace gameplay
