#include "game/session/CombatFramePipeline.h"
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

void CombatFramePipeline::Update()
{

    // 通常の基準時間は1/60秒。検証では指定した固定時間でメニュー・演出も進める。
    float baseDeltaTime = 1.0f / 60.0f;
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    world_.resources.gameplayScenarioCombatDt_ = world_.resources.gameplayScenarioPresentationDt_ = 0.0f;
    if (GameplayScenarioSession::Get().IsActive()) {
        baseDeltaTime = GameplayScenarioSession::Get().GetSettings().fixedDeltaTime;
        if (world_.gameplayScenarioRunner->PrepareGameplayScenarioFrame())
            return;
    }
    world_.presentation.developerGameCapture_.Resolve(*cg2::Object3dCommon::GetInstance()->GetDxCommon());
    if (!world_.demo.titleDemo_ && world_.run.expeditionMapEnabled_ && world_.resources.input_->IsKeyTriggered(DIK_F7))
        world_.bossPresentation->RequestNeonBossDeveloperEncounter();
    world_.bossPresentation->UpdateNeonBossDeveloperValidation();
    if (world_.resources.normalRouteReplayActive_ &&
        (world_.gameplayQueries->IsNeonShowcaseActive() || world_.presentation.developerBloomFreeze_ ||
         world_.resources.neonBossDeveloperFreeze_))
        world_.routeReplay->StopNormalRouteReplay("external-showcase-or-freeze");
    if (world_.gameplayQueries->IsNeonShowcaseActive()) {
        // Freeze gameplay, follow-camera and menu state; Showcase camera/animation remain independent.
#ifdef USE_IMGUI
        if (world_.resources.input_->IsKeyTriggered(DIK_F12))
            world_.presentation.showGameDebugConsole_ = !world_.presentation.showGameDebugConsole_;
        world_.gameplayEditor->DrawGameplayDebugUi();
        world_.resources.neonSkinnedPreview_->DrawShowcaseWindow();
#endif
        world_.resources.neonSkinnedPreview_->Update(world_.gameplayQueries->IsNeonShowcaseActive() ? baseDeltaTime : 0.0f);
        return;
    }
    if ((world_.presentation.developerBloomFreeze_ || world_.resources.neonBossDeveloperFreeze_) && !world_.demo.titleDemo_) {
#ifdef USE_IMGUI
        if (world_.resources.input_->IsKeyTriggered(DIK_F12))
            world_.presentation.showGameDebugConsole_ = !world_.presentation.showGameDebugConsole_;
        world_.gameplayEditor->DrawGameplayDebugUi();
#endif
        if (world_.resources.neonSkinnedPreview_)
            world_.resources.neonSkinnedPreview_->Update(0.0f);
        // Existing comparison mode disables temporal jitter. Refresh both boss
        // representations against that same projection without advancing combat.
        // Only this explicit comparison policy suppresses temporal jitter;
        // continuous recording never enters this branch for readback waits.
        if (world_.gameplayQueries->GetDeveloperShowcaseState().comparisonFreeze) {
            world_.resources.camera->SetProjectionJitter({});
            world_.resources.debugCamera->SetProjectionJitter({});
        }
        if (world_.resources.enemyObject_)
            world_.resources.enemyObject_->Update();
        // Draw still runs while frozen; reuse the renderer's per-frame CB slots.
        world_.bossPresentation->UpdateNeonBossVisual(0.0f);
        return; // Retain gameplay, particles, trails, follow-camera and world transforms; Draw still runs.
    }
#endif
    if (world_.demo.titleDemo_)
        world_.titleDemoController->UpdateTitleDemo(baseDeltaTime);
    if (world_.run.expeditionRun_ && !world_.demo.titleDemo_) {
        if (world_.run.expeditionMapEnabled_)
            world_.expeditionAuthoring->UpdateExpeditionAuthoringHub();
        world_.expeditionBalanceEditor->UpdateTankExpeditionBalanceEditor();
        world_.expeditionBalanceEditor->DrawTankExpeditionBalanceEditor();
        if (world_.run.expeditionMapEnabled_)
            world_.expeditionMapController->UpdateExpeditionAuthoring();
    }
    if (world_.run.prototypeRun_) {
        world_.arenaRunController->FinishTankRunCapture();
        world_.arenaRunController->UpdateTankRun(baseDeltaTime);
    }
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    world_.routeReplay->PrepareNormalRouteReplay();
#endif
    if (world_.run.expeditionMapEnabled_)
        world_.expeditionExperience->RefreshGuidedExpeditionUi();
#if defined(USE_IMGUI) && !defined(NDEBUG)
    if (world_.resources.player_) {
        world_.presentation.upgradeHudAfterPlayerUpdate_ = world_.resources.player_->GetUpgradeHudDebugSnapshot();
        world_.presentation.upgradeHudAfterCollision_ = world_.presentation.upgradeHudAfterPlayerUpdate_;
    }
#endif
    const bool evolutionUiWasOpenAtFrameStart = world_.resources.player_ && world_.resources.player_->IsChangeMode();
    world_.combat.screenEffectDirector_.Update(baseDeltaTime);
    if (world_.combat.eventCalloutTimer_ > 0.0f) {
        world_.combat.eventCalloutTimer_ = (std::max)(0.0f, world_.combat.eventCalloutTimer_ - baseDeltaTime);
    }

    const bool justDodgeTriggered = world_.resources.player_->RequestSlow();
    if (justDodgeTriggered) {
        world_.combat.screenEffectDirector_.TriggerJustDodge(
            world_.gameplayQueries->WorldToScreenUv(world_.resources.player_->GetWorldPosition()));
        ++world_.combat.justDodgeCount_;
        world_.combatFlow->SetEventCallout("ジャスト回避", 0.70f);
        if (world_.run.expeditionRun_)
            world_.run.tankExpeditionAudio_.Upgrade();
        else
            cg2::Audio::GetInstance()->PlayAudioSE(L"bulletShoot", 0.25f);
    }

    const float requestedTimeScale = world_.combat.screenEffectDirector_.GetTimeScaleMultiplier();
    if (requestedTimeScale < world_.combat.timeScale_) {
        world_.combat.timeScale_ = requestedTimeScale;
    } else {
        world_.combat.timeScale_ += (1.0f - world_.combat.timeScale_) * 0.12f;
    }

    // 戦闘へ渡す時間は基準時間に演出の時間倍率を掛け、命中時の短い停止中はさらに減速する。
    world_.combat.finalDeltaTime = baseDeltaTime * world_.combat.timeScale_;
    if (world_.run.expeditionImpactHold_ > 0) {
        world_.run.expeditionImpactHold_ = (std::max)(0.0f, world_.run.expeditionImpactHold_ - baseDeltaTime);
        world_.combat.finalDeltaTime *= 0.08f;
    }
    // 時間倍率が基準時間の95%を超え、命中時の停止も終わったら基準時間へ戻す。
    bool restoreBaseTime = world_.combat.finalDeltaTime * 60.0f > 0.95f;
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    if (GameplayScenarioSession::Get().IsActive())
        restoreBaseTime = gameplaytest::ScenarioShouldSnapTimeScale(world_.combat.finalDeltaTime, baseDeltaTime);
#endif
    if (restoreBaseTime && world_.run.expeditionImpactHold_ <= 0) {
        world_.combat.finalDeltaTime = baseDeltaTime;
    }
    if (!world_.demo.titleDemo_ &&
        world_.resources.input_->IsTrigger(world_.resources.input_->GetKey()[DIK_H], world_.resources.input_->GetPreKey()[DIK_H])) {
        world_.combat.showControlGuide_ = !world_.combat.showControlGuide_;
        if (world_.combat.controlGuideText_) {
            world_.combat.controlGuideText_->SetText(
                world_.combat.showControlGuide_
                    ? "WASD 移動 / 左クリック 射撃 / 右クリック ダッシュ\nC 進化ツリー / ESC タイトルへ / H ヘルプ切替"
                    : "H:操作説明ON");
        }
    }
    if (cg2::kDeveloperTools && !world_.demo.titleDemo_ && !world_.run.expeditionMapEnabled_ &&
        world_.resources.input_->IsTrigger(world_.resources.input_->GetKey()[DIK_F5], world_.resources.input_->GetPreKey()[DIK_F5])) {
        world_.classConfigWatcher->ReloadPlayerClassConfig(false);
    }
    if (cg2::kDeveloperTools)
        world_.classConfigWatcher->UpdatePlayerClassConfigWatch(baseDeltaTime);
    world_.presentation.slowMotionPostActive_ = world_.combat.finalDeltaTime < baseDeltaTime * 0.98f;

    // メニュー・進化画面・結果表示中は戦闘用の時間を0にする。演出用の基準時間は保つ。
    if (world_.arenaRunController->IsTankRunMenuOpen() || world_.resources.player_->IsChangeMode() ||
        world_.combat.combatFlow_.GetState() == GameFlowState::StageClear ||
        world_.combat.combatFlow_.GetState() == GameFlowState::GameOver) {
        world_.combat.finalDeltaTime = 0.0f;
    }

    world_.validation.neonTriangleDemoRotation_ += world_.validation.neonTriangleDemoRotateSpeed_ * baseDeltaTime;
    world_.presentation.stageDamageBlockPulseTime_ += baseDeltaTime;
    ExpEnemy::SetShapeNeonRenderMode(world_.presentation.expEnemyNeonRenderMode_);

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    {
        const auto now = std::chrono::steady_clock::now();
        const float realDeltaTime = std::chrono::duration<float>(now - world_.combat.fpsLastSampleTime_).count();
        world_.combat.fpsLastSampleTime_ = now;
        world_.combat.fpsAccumulatedTime_ += realDeltaTime;
        ++world_.combat.fpsFrameCount_;
        if (world_.combat.fpsText_ && world_.combat.fpsAccumulatedTime_ >= 0.25f) {
            const float fps = static_cast<float>(world_.combat.fpsFrameCount_) / world_.combat.fpsAccumulatedTime_;
            char text[32]{};
            std::snprintf(text, sizeof(text), "FPS: %.0f", fps);
            world_.combat.fpsText_->SetText(text);
            world_.combat.fpsAccumulatedTime_ = 0.0f;
            world_.combat.fpsFrameCount_ = 0;
        }
    }
#endif // !defined(NDEBUG)

    if (world_.presentation.sceneFadeBlurTimer_ > 0.0f) {
        world_.presentation.sceneFadeBlurTimer_ -= baseDeltaTime;
        float t = (std::clamp)(world_.presentation.sceneFadeBlurTimer_ / world_.presentation.sceneFadeBlurDuration_, 0.0f, 1.0f);
        world_.presentation.sceneFadeBlurIntensity_ = t * t;
    } else {
        world_.presentation.sceneFadeBlurIntensity_ = 0.0f;
    }

#if defined(USE_IMGUI) && !defined(NDEBUG)

    if (!world_.demo.titleDemo_)
        world_.gameplayEditor->DrawGameplayDebugUi();
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    if (world_.gameplayQueries->IsNeonShowcaseActive()) {
        world_.resources.neonSkinnedPreview_->DrawShowcaseWindow();
        world_.resources.neonSkinnedPreview_->Update(world_.gameplayQueries->IsNeonShowcaseActive() ? baseDeltaTime : 0.0f);
        return;
    }
#endif
    if (!world_.demo.titleDemo_ && world_.presentation.showPlayerClassEditor_) {
        world_.resources.player_->DrawPlayerClassEditor();
    }

#endif // defined(USE_IMGUI) && !defined(NDEBUG)

#if defined(USE_IMGUI) && !defined(NDEBUG)
    if (!world_.demo.titleDemo_ && !world_.run.expeditionMapEnabled_ &&
        world_.resources.input_->IsTrigger(world_.resources.input_->GetKey()[DIK_F7], world_.resources.input_->GetPreKey()[DIK_F7])) {
        world_.presentation.showCollisionDebug_ = !world_.presentation.showCollisionDebug_;
    }
    if (!world_.run.prototypeRun_ &&
        world_.resources.input_->IsTrigger(world_.resources.input_->GetKey()[DIK_F6], world_.resources.input_->GetPreKey()[DIK_F6])) {
        world_.resources.player_->AddExp(200);
    }
#endif // defined(USE_IMGUI) && !defined(NDEBUG)

#if defined(USE_IMGUI) && !defined(NDEBUG)
    if (!world_.demo.titleDemo_ &&
        world_.resources.input_->IsTrigger(world_.resources.input_->GetKey()[DIK_F8], world_.resources.input_->GetPreKey()[DIK_F8])) {
        world_.presentation.showPostProfileOverlay_ = !world_.presentation.showPostProfileOverlay_;
    }
    if (!world_.demo.titleDemo_ &&
        world_.resources.input_->IsTrigger(world_.resources.input_->GetKey()[DIK_F9], world_.resources.input_->GetPreKey()[DIK_F9])) {
        world_.presentation.postProfileMode_ = (world_.presentation.postProfileMode_ + 1) % 8;
    }
    if (!world_.demo.titleDemo_ &&
        world_.resources.input_->IsTrigger(world_.resources.input_->GetKey()[DIK_F10], world_.resources.input_->GetPreKey()[DIK_F10])) {
        if (world_.run.prototypeRun_)
            world_.arenaRunController->RequestTankRunCapture("manual");
        else
            world_.levelRuntime->ReloadLevelData(true);
    }
    if (!world_.demo.titleDemo_ &&
        world_.resources.input_->IsTrigger(world_.resources.input_->GetKey()[DIK_F11], world_.resources.input_->GetPreKey()[DIK_F11])) {
        world_.presentation.showLevelAIDitorPreview_ = !world_.presentation.showLevelAIDitorPreview_;
    }
    if (!world_.demo.titleDemo_ &&
        world_.resources.input_->IsTrigger(world_.resources.input_->GetKey()[DIK_F12], world_.resources.input_->GetPreKey()[DIK_F12])) {
        world_.presentation.showGameDebugConsole_ = !world_.presentation.showGameDebugConsole_;
    }
#endif // defined(USE_IMGUI) && !defined(NDEBUG)

    if (!world_.depthEncounter->UpdateNeonDepthCamera(baseDeltaTime)) {
        cg2::Vector3 playerPos = world_.resources.player_->GetWorldPosition();
        const float kCameraZ = -55.0f;
        const float kMarginX = 18.0f;
        const float kMarginY = 11.0f;
        const float kMaxCameraX = MapChip::kBlockWidth * (MapChip::kNumBlockHorizontal - 1) - kMarginX;
        const float kMaxCameraY = MapChip::kBlockHeight * (MapChip::kNumBlockVirtical - 1) - kMarginY;
        cg2::Vector3 targetCameraPos = {(std::clamp)(playerPos.x, kMarginX, kMaxCameraX), (std::clamp)(playerPos.y, kMarginY, kMaxCameraY),
                                        kCameraZ};
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
        // Capture fixture only: show both actors with the same normal top-down camera.
        if ((world_.resources.neonBossAutoTest_ || GameplayScenarioSession::Get().IsActive()) &&
            world_.gameplayQueries->IsRunRivalActive()) {
            const auto center = (playerPos + world_.resources.enemy_->GetWorldPosition()) * 0.5f;
            targetCameraPos.x = (std::clamp)(center.x, kMarginX, kMaxCameraX);
            targetCameraPos.y = (std::clamp)(center.y, kMarginY, kMaxCameraY);
        }
#endif
        cg2::Vector3 currentCameraPos = world_.resources.camera->GetTranslate();
        cg2::Vector3 nextCameraPos = currentCameraPos + (targetCameraPos - currentCameraPos) * 0.12f;
        if (world_.presentation.cameraShakeTimer_ > 0.0f) {
            world_.presentation.cameraShakeTimer_ -= baseDeltaTime;
            float t = (std::clamp)(world_.presentation.cameraShakeTimer_ / world_.presentation.cameraShakeDuration_, 0.0f, 1.0f);
            float power = world_.presentation.cameraShakePower_ * t * t;
            nextCameraPos.x += cg2::Rand(-power, power);
            nextCameraPos.y += cg2::Rand(-power, power);
        }
        world_.resources.camera->SetTranslate(nextCameraPos);
    }

    world_.resources.camera->Update();
    world_.resources.debugCamera->Update(world_.resources.input_->GetMouseState(), world_.resources.input_->GetKey(),
                                         world_.resources.input_->GetLeftStick());
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    if (world_.resources.neonSkinnedPreview_)
        world_.resources.neonSkinnedPreview_->Update(baseDeltaTime);
#endif
    if (world_.resources.skybox_) {
        world_.resources.skybox_->Update(world_.resources.camera.get(), world_.resources.debugCamera.get());
    }

    world_.combat.direction = cg2::Normalize(world_.combat.direction);
    world_.resources.ball_->SetDirectionalLightDirection(world_.combat.direction);
    world_.resources.ball_->SetInsensity(world_.combat.insensity);
    world_.resources.ball_->SetShininess(world_.combat.shininess);

    world_.resources.ballObj_->Update();
    world_.resources.ball_->Update();
    world_.resources.groundObj_->Update();

    // 戦闘本体はプレイ中・フェード完了後・遠征メニューが閉じている場合にだけ進める。
    const bool depthIntroLocked = world_.depthEncounter->UpdateNeonDepthIntro(baseDeltaTime);
    if (!depthIntroLocked && world_.combat.combatFlow_.GetState() == GameFlowState::Playing && world_.combat.phase_ == Phase::kMain &&
        !world_.arenaRunController->IsTankRunMenuOpen()) {
        if (world_.combat.phase_ == Phase::kMain && !world_.resources.player_->IsChangeMode()) {
            world_.combat.playTime_ += baseDeltaTime;
        }
        world_.resources.stage_->Update();
        world_.levelRuntime->UpdateLevelItems();

        world_.resources.player_->SetDebugNoDamage(
            world_.presentation.debugPlayerNoDamage_ ||
            (world_.run.expeditionMapEnabled_ && world_.run.expeditionMapRun_.GetActiveNode() &&
             world_.run.expeditionMapRun_.GetActiveNode()->role == tankexp::NodeRole::TutorialCombat));
        // 誘導先は敵の位置の写し。これを設定してから自機・ドローンを更新し、弾の生成とレーザー・地雷・斬撃の予約を進める。
        if (world_.run.prototypeRun_ && !world_.resources.player_->IsChangeMode()) {
            std::vector<cg2::Vector3> targets;
            if ((world_.gameplayQueries->IsRunRivalActive() && !world_.resources.enemy_->IsDead()))
                targets.push_back(world_.resources.enemy_->GetWorldPosition());
            for (const auto* actor : world_.resources.enemyManager_->GetEnemyPtrs())
                if (actor && !actor->IsDead())
                    targets.push_back(actor->GetWorldPosition());
            world_.resources.player_->SetRunHomingTargets(targets);
        }
        {
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
            world_.resources.gameplayScenarioCombatDt_ = world_.combat.finalDeltaTime;
#endif
            cg2::RuntimeProfiler::CpuScope scope("Player / Drones Update");
            world_.resources.player_->Update(world_.resources.camera.get(), *world_.resources.stage_, world_.resources.bulletManager_.get(),
                                             world_.combat.finalDeltaTime, baseDeltaTime);
        }
#if defined(USE_IMGUI) && !defined(NDEBUG)
        world_.presentation.upgradeHudAfterPlayerUpdate_ = world_.resources.player_->GetUpgradeHudDebugSnapshot();
#endif
        // prototypeRun_が有効なモードの進化画面は自機側で入力を受け付ける。閉じたフレームも戦闘の後半を再開しない。
        // この条件はprototypeRun_に限り、通常モードは戦闘時間0で以下を呼ぶ。
        if (!world_.run.prototypeRun_ || (!world_.resources.player_->IsChangeMode() && !evolutionUiWasOpenAtFrameStart)) {
            world_.combatTutorial->UpdateTutorial(baseDeltaTime);
            // 自機が予約した攻撃をここで消費する。レーザーは受取時に命中を適用し、地雷・斬撃は状態を登録する。
            for (const Player::LaserShotEvent& event : world_.resources.player_->ConsumeLaserShotEvents()) {
                world_.playerAttackEffects->SpawnPlayerLaser(event);
            }
            for (const Player::MineDropEvent& event : world_.resources.player_->ConsumeMineDropEvents()) {
                world_.playerAttackEffects->SpawnPlayerMine(event);
            }
            for (const Player::MeleeSlashEvent& event : world_.resources.player_->ConsumeMeleeSlashEvents()) {
                world_.playerAttackEffects->SpawnPlayerMeleeSlash(event);
            }
            // レーザー表示と地雷は基準時間、斬撃の有効時間は減速を含む戦闘時間で進める。
            world_.playerAttackEffects->UpdatePlayerNeonAfterimages(baseDeltaTime);
            world_.playerAttackEffects->UpdatePlayerLasers(baseDeltaTime);
            world_.playerAttackEffects->UpdatePlayerMines(baseDeltaTime);
            world_.playerAttackEffects->UpdatePlayerMeleeSlashes(world_.combat.finalDeltaTime);
            world_.combatEffects->UpdateNeonTriangleParticles(baseDeltaTime);
            if (world_.resources.player_->IsDead() && !world_.presentation.playerDeathShakeStarted_) {
                world_.presentation.playerDeathShakeStarted_ = true;
            }

            // チュートリアルの抑制中は敵AI・特殊戦闘・アクター衝突を呼ばない。
            // 予約攻撃の受取と弾の更新はこの抑制条件の外で行う。
            const bool suppressTutorialCombat = world_.gameplayQueries->IsTutorialCombatSuppressed();
            if (!suppressTutorialCombat) {
                cg2::RuntimeProfiler::CpuScope scope("Enemy AI Update");
                if (world_.gameplayQueries->IsRunRivalActive())
                    world_.resources.enemy_->Update(world_.combat.finalDeltaTime);
                if (!world_.run.prototypeRun_)
                    world_.gameplayEditor->UpdateLevelBossPhases();
                world_.resources.enemyManager_->Update(*world_.resources.stage_, world_.combat.finalDeltaTime);
            }

            // 敵AIが発射した弾も含めて移動・壁衝突を処理する。この更新内で死亡弾の実体も削除する。
            {
                cg2::RuntimeProfiler::CpuScope scope("Bullets / Trails / Wall Collision");
                world_.resources.bulletManager_->Update(*world_.resources.stage_, world_.combat.finalDeltaTime);
            }

            // 弾と敵の更新後に特殊戦闘を適用し、その後で通常のアクター接触を通知する。
            // 衝突通知中に予約された弾はCheckAllCollisionsの走査後に追加され、このフレームの移動には戻らない。
            if (!suppressTutorialCombat) {
                cg2::RuntimeProfiler::CpuScope scope("Actor / Bullet Collision");
                world_.resources.player_->UpdateSpecialCombat(*world_.resources.stage_, world_.resources.bulletManager_.get(),
                                                              world_.gameplayQueries->IsRunRivalActive() ? world_.resources.enemy_.get()
                                                                                                         : nullptr,
                                                              world_.resources.enemyManager_.get(), world_.combat.finalDeltaTime);
                world_.resources.collisionManager_->CheckAllCollisions(
                    world_.resources.player_.get(), world_.gameplayQueries->IsRunRivalActive() ? world_.resources.enemy_.get() : nullptr,
                    world_.resources.bulletManager_.get(), world_.resources.enemyManager_.get());
                for (const auto& impact : world_.resources.player_->ConsumeDashImpactEvents())
                    world_.expeditionExperience->QueueExpeditionImpact(impact.origin, impact.direction, impact.powered);
            }
#if defined(USE_IMGUI) && !defined(NDEBUG)
            world_.presentation.upgradeHudAfterCollision_ = world_.resources.player_->GetUpgradeHudDebugSnapshot();
#endif
            if (world_.presentation.showCollisionDebug_ && !suppressTutorialCombat) {
                for (Collider* collider : world_.resources.collisionManager_->GetColliders()) {
                    if (!collider) {
                        continue;
                    }
                    if (!world_.presentation.showCollisionDebugBullets_ && IsBulletCollider(collider->GetCollisionAttribute())) {
                        continue;
                    }
                    EmitColliderDebugRings(*world_.resources.collisionDebugRingManager_, *collider);
                }
            } else {
                world_.resources.collisionDebugRingManager_->Clear();
            }
            world_.resources.collisionDebugRingManager_->Update(world_.combat.finalDeltaTime);
            // そのフレームの攻撃・衝突結果を受けてHP差分と死亡状態を調べ、死亡時は次回の戦闘を止める状態へ移る。
            if (!suppressTutorialCombat) {
                world_.combatFlow->UpdateGameplayEventEffects(baseDeltaTime, justDodgeTriggered);
            }
        }
    } else {
        // 戦闘本体を呼ばないフレームでも、撃破・死亡演出は基準時間で進める。
        world_.resources.collisionDebugRingManager_->Clear();
        if (world_.combat.combatFlow_.GetState() == GameFlowState::BossDefeatSequence && world_.resources.enemy_) {
            world_.resources.enemy_->UpdateDefeatPresentation(baseDeltaTime);
        }
        if (world_.combat.combatFlow_.GetState() == GameFlowState::GameOver && world_.resources.player_) {
            world_.resources.player_->UpdateDefeatPresentation(baseDeltaTime);
        }
    }
    // Read the final HP/phase after every attack and collision; death presentation continues independently.
    const float bossPresentationDeltaTime =
        depthIntroLocked ? world_.resources.neonDepthIntroDelta_
                         : (world_.resources.enemy_->IsDead()
                                ? baseDeltaTime
                                : (world_.combat.combatFlow_.GetState() == GameFlowState::Playing &&
                                           !world_.arenaRunController->IsTankRunMenuOpen() && !world_.resources.player_->IsChangeMode()
                                       ? world_.combat.finalDeltaTime
                                       : 0.0f));
    world_.bossPresentation->UpdateNeonBossVisual(bossPresentationDeltaTime);
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    world_.resources.gameplayScenarioPresentationDt_ = bossPresentationDeltaTime;
#endif
    // 戦闘停止中も、残っている演出を進め、未消費の特殊戦闘イベントを受け取る。
    world_.playerAttackEffects->UpdateSpecialCombatPresentation(baseDeltaTime);
    world_.combat.screenEffectDirector_.SetUpgradeMenuOpen(world_.resources.player_->IsChangeMode() ||
                                                           world_.arenaRunController->IsTankRunMenuOpen());
    world_.resources.playerPostEffect_->Update(world_.combat.finalDeltaTime);
    world_.resources.enemyPostEffect_->Update(world_.combat.finalDeltaTime);
    world_.resources.expEnemyPostEffect_->Update(world_.combat.finalDeltaTime);
    world_.resources.stagePostEffect_->Update(world_.combat.finalDeltaTime);
    world_.resources.neonGridPostEffect_->Update(world_.combat.finalDeltaTime);
    world_.resources.bulletTrailPostEffect_->Update(world_.combat.finalDeltaTime);
    world_.resources.particlePostEffect_->Update(world_.combat.finalDeltaTime);
    world_.resources.sharedObjectBloomPostEffect_->Update(world_.combat.finalDeltaTime);
    world_.performanceMonitor->UpdatePostProfileText();

#ifdef USE_IMGUI

    ImGuiIO& io = ImGui::GetIO();

    // アプリ側のクリック処理を行う前にチェック
    if (!io.WantCaptureMouse && !world_.run.prototypeRun_) {
        // 左クリックしたらパーティクル追加
        if (world_.resources.input_->IsTrigger(world_.resources.input_->GetMouseState().rgbButtons[0],
                                               world_.resources.input_->GetPreMouseState().rgbButtons[0])) {
            cg2::Matrix4x4 viewMatrix = cg2::Object3dCommon::GetInstance()->GetIsDebugCamera()
                                            ? world_.resources.debugCamera->GetViewMatrix()
                                            : world_.resources.camera->GetViewMatrix();
            cg2::Matrix4x4 projectionMatrix = cg2::Object3dCommon::GetInstance()->GetIsDebugCamera()
                                                  ? world_.resources.debugCamera->GetProjectionMatrix()
                                                  : world_.resources.camera->GetProjectionMatrix();
            cg2::Vector3 worldPos = cg2::ScreenToWorldOnZ0(world_.resources.input_->GetMousePosition(), viewMatrix, projectionMatrix,
                                                           cg2::WinApp::kClientWidth, cg2::WinApp::kClientHeight);
            cg2::ParticleManager::GetInstance()->EmitHitEffect(worldPos);
        }
    }

    if (world_.presentation.showParticleEditor_) {
        cg2::ParticleManager::GetInstance()->DrawImGuiEditor();
    }

#endif // USE_IMGUI

    world_.combatEffects->UpdateDeathPostPulse(baseDeltaTime);
    const float particleDeltaTime =
        world_.combat.combatFlow_.GetState() == GameFlowState::Playing && !world_.run.expeditionTransition_.IsActive()
            ? world_.combat.finalDeltaTime
            : baseDeltaTime;
    {
        cg2::RuntimeProfiler::CpuScope scope("Particles Update");
        cg2::ParticleManager::GetInstance()->Update(particleDeltaTime, world_.resources.camera.get(), world_.resources.debugCamera.get());
    }
    for (const cg2::ParticleManager::ScreenPulseEvent& event : cg2::ParticleManager::GetInstance()->ConsumeScreenPulseEvents()) {
        world_.combatEffects->TriggerDeathPostPulse(event.position, event.strength);
    }
    // 死亡演出・通貨回収の終了条件を見て結果画面へ移る。戦闘の時間倍率には合わせない。
    world_.combatFlow->UpdateGameFlow(baseDeltaTime);

    switch (world_.combat.phase_) {
    case Phase::kFadeIn:
        world_.combat.fade_->Update();

        if (world_.combat.fade_->IsFinished()) {
            world_.combat.phase_ = Phase::kMain;
            if (!world_.run.expeditionRun_ && !world_.combat.bossEntryTriggered_ && !world_.gameplayQueries->IsTutorialCombatSuppressed()) {
                world_.combat.bossEntryTriggered_ = true;
                world_.combat.screenEffectDirector_.TriggerBossEntry();
                world_.combatFlow->SetEventCallout("ボス出現", 1.20f);
            }
        }
        break;
    case Phase::kMain:
        if (world_.combat.combatFlow_.GetState() == GameFlowState::Playing && !world_.run.prototypeRun_ &&
            !evolutionUiWasOpenAtFrameStart &&
            world_.resources.input_->IsTrigger(world_.resources.input_->GetKey()[DIK_ESCAPE],
                                               world_.resources.input_->GetPreKey()[DIK_ESCAPE])) {
            world_.combat.nextSceneName_ = "TITLE";
            world_.combat.fade_->Start(Fade::Status::FadeOut, 1.0f);
            world_.combat.phase_ = Phase::kFadeOut;
        }
        break;
    case Phase::kFadeOut:
        world_.combat.fade_->Update();
        if (world_.combat.fade_->IsFinished()) {
            world_.combat.finished_ = true;
        }
        break;
    }

    if (world_.combat.shotGide) {
        world_.combat.shotGide->Update();
    }
    world_.combat.wasdGide->Update();
    world_.combat.dashGide->Update();
    world_.combat.toTitleGide->Update();
    if (!world_.demo.titleDemo_)
        world_.titleDemoController->VerifyTitleDemoTransition(baseDeltaTime);
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    world_.gameplayScenarioRunner->RecordGameplayScenarioFrame();
    world_.routeReplay->RecordNormalRouteReplay();
#endif
}
} // namespace gameplay
