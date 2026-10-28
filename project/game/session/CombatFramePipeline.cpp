#include "game/session/CombatFramePipeline.h"
#include "FixedTimestep.h"
#include "game/render/settings/ArenaCameraProfile.h"
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

namespace {
/// @brief Bloom比較表示の説明文を返す。
const char* BloomViewCaption(int view)
{
    static const char* kCaptions[] = {
        "ブルーム：ON（5段のピラミッドを合成）",
        "ブルーム：OFF",
        "ブルーム：1段目だけ（1/2解像度・細いにじみ）",
        "ブルーム：2段目だけ（1/4解像度）",
        "ブルーム：3段目だけ（1/8解像度）",
        "ブルーム：4段目だけ（1/16解像度）",
        "ブルーム：5段目だけ（1/32解像度・広いにじみ）",
        "ブルーム：光の成分だけ（5段を合成した結果）",
    };
    return kCaptions[(std::clamp)(view, 0, 7)];
}
} // namespace

void CombatFramePipeline::Update()
{
    // 演出確認用: 実際のゲームでピラミッドBloomを比較する（[B]で表示切替、[Shift+B]で戻る、[V]で停止）。
    // 開発ビルドだけで使う。Releaseでプレイ中に押しても、画面が変わったりゲームが止まったりしないようにする。
    if (myEngine::kDeveloperTools && !world_.demo.titleDemo_ && world_.resources.input_) {
        auto& presentation = world_.presentation;
        const bool shift = world_.resources.input_->GetKey()[DIK_LSHIFT] || world_.resources.input_->GetKey()[DIK_RSHIFT];
        bool changed = false;
        if (world_.resources.input_->IsKeyTriggered(DIK_B)) {
            presentation.bloomView_ = (presentation.bloomView_ + (shift ? 7 : 1)) % 8;
            changed = true;
        }
        if (world_.resources.input_->IsKeyTriggered(DIK_V)) {
            presentation.bloomViewFreeze_ = !presentation.bloomViewFreeze_;
            changed = true;
        }
        if (changed) {
            presentation.bloomViewLabelTimer_ = 2.5f;
            if (world_.combat.bloomViewText_) {
                world_.combat.bloomViewText_->SetText(
                    std::string(BloomViewCaption(presentation.bloomView_)) +
                    (presentation.bloomViewFreeze_ ? "   ■ 一時停止中 [V]で再開" : "   [B]切替  [V]一時停止"));
            }
        }
        presentation.bloomViewLabelTimer_ = (std::max)(0.0f, presentation.bloomViewLabelTimer_ - myEngine::kFixedStepSeconds);
        if (presentation.bloomViewFreeze_) {
            return; // フレームを止める: 同じ状態のまま描画は続くので、同じ瞬間で表示を比べられる。
        }
    }

    // この更新は固定ステップ1回分（1/60秒）。実時間に合わせて何回回すかはApplicationが決める。
    // 検証では指定した固定時間でメニュー・演出も進める。
    float baseDeltaTime = myEngine::kFixedStepSeconds;
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    world_.resources.gameplayScenarioCombatDt_ = world_.resources.gameplayScenarioPresentationDt_ = 0.0f;
    if (GameplayScenarioSession::Get().IsActive()) {
        baseDeltaTime = GameplayScenarioSession::Get().GetSettings().fixedDeltaTime;
        if (world_.gameplayScenarioRunner->PrepareGameplayScenarioFrame()) {
            return;
        }
    }
    world_.presentation.developerGameCapture_.Resolve(*myEngine::Object3dCommon::GetInstance()->GetDxCommon());
    if (!world_.demo.titleDemo_ && world_.run.expeditionMapEnabled_ && world_.resources.input_->IsKeyTriggered(DIK_F7)) {
        world_.bossPresentation->RequestNeonBossDeveloperEncounter();
    }
    world_.bossPresentation->UpdateNeonBossDeveloperValidation();
    if (world_.resources.normalRouteReplayActive_ &&
        (world_.gameplayQueries->IsNeonShowcaseActive() || world_.presentation.developerBloomFreeze_ ||
         world_.resources.neonBossDeveloperFreeze_)) {
        world_.routeReplay->StopNormalRouteReplay("external-showcase-or-freeze");
    }
    if (world_.gameplayQueries->IsNeonShowcaseActive()) {
        // ゲームプレイ、追従カメラ、メニューの状態を止める。Showcaseのカメラとアニメーションは独立したまま。
#ifdef USE_IMGUI
        if (world_.resources.input_->IsKeyTriggered(DIK_F12)) {
            world_.presentation.showGameDebugConsole_ = !world_.presentation.showGameDebugConsole_;
        }
        world_.gameplayEditor->DrawGameplayDebugUi();
        world_.resources.neonSkinnedPreview_->DrawShowcaseWindow();
#endif
        world_.resources.neonSkinnedPreview_->Update(world_.gameplayQueries->IsNeonShowcaseActive() ? baseDeltaTime : 0.0f);
        return;
    }
    if ((world_.presentation.developerBloomFreeze_ || world_.resources.neonBossDeveloperFreeze_) && !world_.demo.titleDemo_) {
#ifdef USE_IMGUI
        if (world_.resources.input_->IsKeyTriggered(DIK_F12)) {
            world_.presentation.showGameDebugConsole_ = !world_.presentation.showGameDebugConsole_;
        }
        world_.gameplayEditor->DrawGameplayDebugUi();
#endif
        if (world_.resources.neonSkinnedPreview_) {
            world_.resources.neonSkinnedPreview_->Update(0.0f);
        }
        // 既存の比較モードではテンポラルのジッターを無効にする。戦闘を進めずに、
        // 同じ投影に対して両方のボス表現を更新する。
        // テンポラルのジッターを抑えるのはこの明示的な比較の方針だけで、
        // 連続記録が読み戻し待ちのためにこの分岐へ入ることはない。
        if (world_.gameplayQueries->GetDeveloperShowcaseState().comparisonFreeze) {
            world_.resources.camera->SetProjectionJitter({});
            world_.resources.debugCamera->SetProjectionJitter({});
        }
        if (world_.resources.enemyObject_) {
            world_.resources.enemyObject_->Update();
        }
        // 停止中もDrawは実行されるので、描画のフレームごとの定数バッファのスロットを再利用する。
        world_.bossPresentation->UpdateNeonBossVisual(0.0f);
        return; // ゲームプレイ、パーティクル、軌跡、追従カメラ、ワールド変換を保持したまま、Drawは実行する。
    }
#endif
    if (world_.demo.titleDemo_) {
        world_.titleDemoController->UpdateTitleDemo(baseDeltaTime);
    }
    if (world_.run.IsExpeditionRun() && !world_.demo.titleDemo_) {
        if (world_.run.expeditionMapEnabled_) {
            world_.expeditionAuthoring->UpdateExpeditionAuthoringHub();
        }
        world_.expeditionBalanceEditor->UpdateTankExpeditionBalanceEditor();
        world_.expeditionBalanceEditor->DrawTankExpeditionBalanceEditor();
        if (world_.run.expeditionMapEnabled_) {
            world_.expeditionMapController->UpdateExpeditionAuthoring();
        }
    }
    if (world_.run.UsesPrototypeRules()) {
        world_.arenaRunController->FinishTankRunCapture();
        if (world_.run.IsSurvivorRun()) {
            world_.survivorRunController->Update(baseDeltaTime);
        } else {
            world_.arenaRunController->UpdateTankRun(baseDeltaTime);
        }
    }
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    world_.routeReplay->PrepareNormalRouteReplay();
#endif
    if (world_.run.expeditionMapEnabled_) {
        world_.expeditionExperience->RefreshGuidedExpeditionUi();
    }
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
        if (world_.run.IsExpeditionRun()) {
            world_.run.tankExpeditionAudio_.Upgrade();
        } else {
            myEngine::Audio::GetInstance()->PlayAudioSE(L"bulletShoot", 0.25f);
        }
    }

    const float requestedTimeScale = world_.combat.screenEffectDirector_.GetTimeScaleMultiplier();
    if (requestedTimeScale < world_.combat.timeScale_) {
        world_.combat.timeScale_ = requestedTimeScale;
    } else {
        // 固定ステップ1回ごとに残りの12%ずつ戻す。ステップは実時間に合わせて回るので、戻る速さはFPSによらない。
        world_.combat.timeScale_ += (1.0f - world_.combat.timeScale_) * 0.12f;
    }

    // 戦闘へ渡す時間は基準時間に演出の時間倍率を掛け、命中時の短い停止中はさらに減速する。
    world_.combat.finalDeltaTime = baseDeltaTime * world_.combat.timeScale_;
    if (world_.run.expeditionImpactHold_ > 0) {
        world_.run.expeditionImpactHold_ = (std::max)(0.0f, world_.run.expeditionImpactHold_ - baseDeltaTime);
        world_.combat.finalDeltaTime *= 0.08f;
    }
    // 時間倍率が基準時間の95%を超え、命中時の停止も終わったら基準時間へ戻す。
    bool restoreBaseTime = world_.combat.finalDeltaTime * myEngine::kFixedStepsPerSecond > 0.95f;
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    if (GameplayScenarioSession::Get().IsActive()) {
        restoreBaseTime = gameplaytest::ScenarioShouldSnapTimeScale(world_.combat.finalDeltaTime, baseDeltaTime);
    }
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
    if (myEngine::kDeveloperTools && !world_.demo.titleDemo_ && !world_.run.expeditionMapEnabled_ &&
        world_.resources.input_->IsTrigger(world_.resources.input_->GetKey()[DIK_F5], world_.resources.input_->GetPreKey()[DIK_F5])) {
        world_.classConfigWatcher->ReloadPlayerClassConfig(false);
    }
    if (myEngine::kDeveloperTools) {
        world_.classConfigWatcher->UpdatePlayerClassConfigWatch(baseDeltaTime);
    }
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

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
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

    if (!world_.demo.titleDemo_) {
        world_.gameplayEditor->DrawGameplayDebugUi();
    }
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
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
    if (!world_.run.UsesPrototypeRules() &&
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
        if (world_.run.UsesPrototypeRules()) {
            world_.arenaRunController->RequestTankRunCapture("manual");
        } else {
            world_.levelRuntime->ReloadLevelData(true);
        }
    }
    if (!world_.demo.titleDemo_ &&
        world_.resources.input_->IsTrigger(world_.resources.input_->GetKey()[DIK_F11], world_.resources.input_->GetPreKey()[DIK_F11])) {
        world_.presentation.showLevelAiEditorPreview_ = !world_.presentation.showLevelAiEditorPreview_;
    }
    if (!world_.demo.titleDemo_ &&
        world_.resources.input_->IsTrigger(world_.resources.input_->GetKey()[DIK_F12], world_.resources.input_->GetPreKey()[DIK_F12])) {
        world_.presentation.showGameDebugConsole_ = !world_.presentation.showGameDebugConsole_;
    }
#endif // defined(USE_IMGUI) && !defined(NDEBUG)

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    // 通常の追従やボスのカメラが保存する前に、最後の比較用の視点を元に戻す。
    if (world_.presentation.developerStageTiltApplied_) {
        world_.resources.camera->SetTranslate(world_.presentation.developerStageBaseCamera_);
        world_.resources.camera->SetRotate(world_.presentation.developerStageBaseRotation_);
        world_.resources.player_->SetNeonDepthAimEnabled(false);
        world_.presentation.developerStageTiltApplied_ = false;
    }
#endif
    if (!world_.depthEncounter->UpdateNeonDepthCamera(baseDeltaTime)) {
        myEngine::Vector3 playerPos = world_.resources.player_->GetWorldPosition();
        const float kCameraZ = arenacamera::CameraZ();
        const float kMarginX = 18.0f;
        const float kMarginY = 11.0f;
        const float kMaxCameraX = MapChip::kBlockWidth * (MapChip::kNumBlockHorizontal - 1) - kMarginX;
        const float kMaxCameraY = MapChip::kBlockHeight * (MapChip::kNumBlockVirtical - 1) - kMarginY;
        // 壁に囲まれたアリーナを固定のモニター映像のように収める。床の表示範囲は外壁を
        // 少し越える程度までにし、高い城壁が画面を埋めないようにする。表示専用。
        float arenaMinX = 0.0f, arenaMinY = 0.0f, arenaMaxX = 0.0f, arenaMaxY = 0.0f;
        const bool framedArena = world_.run.UsesPrototypeRules() && world_.resources.stage_ &&
                                 arenacamera::ArenaBounds(world_.resources.stage_->GetBlocks(), arenaMinX, arenaMinY, arenaMaxX, arenaMaxY);
        auto clampTarget = [&](myEngine::Vector3 target) {
            if (framedArena) {
                const float halfHeight = arenacamera::HalfViewHeight();
                target.x = arenacamera::ClampFocus(target.x, arenaMinX, arenaMaxX, halfHeight * arenacamera::kAspect);
                target.y = arenacamera::ClampFocus(target.y, arenaMinY, arenaMaxY, halfHeight);
            } else {
                target.x = (std::clamp)(target.x, kMarginX, kMaxCameraX);
                target.y = (std::clamp)(target.y, kMarginY, kMaxCameraY);
            }
            return target;
        };
        myEngine::Vector3 targetCameraPos = clampTarget({playerPos.x, playerPos.y, kCameraZ});
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
        // キャプチャ用フィクスチャ専用: 通常と同じ見下ろしカメラで両方のアクターを映す。
        if ((world_.resources.neonBossAutoTest_ || GameplayScenarioSession::Get().IsActive()) &&
            world_.gameplayQueries->IsRunRivalActive()) {
            const auto center = (playerPos + world_.resources.enemy_->GetWorldPosition()) * 0.5f;
            targetCameraPos = clampTarget({center.x, center.y, kCameraZ});
        }
#endif
        myEngine::Vector3 currentCameraPos = world_.resources.camera->GetTranslate();
        myEngine::Vector3 nextCameraPos = currentCameraPos + (targetCameraPos - currentCameraPos) * 0.12f;
        if (world_.presentation.cameraShakeTimer_ > 0.0f) {
            world_.presentation.cameraShakeTimer_ -= baseDeltaTime;
            float t = (std::clamp)(world_.presentation.cameraShakeTimer_ / world_.presentation.cameraShakeDuration_, 0.0f, 1.0f);
            float power = world_.presentation.cameraShakePower_ * t * t;
            nextCameraPos.x += myEngine::Rand(-power, power);
            nextCameraPos.y += myEngine::Rand(-power, power);
        }
        world_.resources.camera->SetTranslate(nextCameraPos);
    }

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    if (world_.presentation.developerStageTilt_ && world_.run.UsesPrototypeRules() && !world_.demo.titleDemo_ &&
        !world_.resources.neonDepthCameraScoped_ && !myEngine::Object3dCommon::GetInstance()->GetIsDebugCamera()) {
        auto& presentation = world_.presentation;
        presentation.developerStageBaseCamera_ = world_.resources.camera->GetTranslate();
        presentation.developerStageBaseRotation_ = world_.resources.camera->GetRotate();
        const float angle = (std::clamp)(presentation.developerStageTiltDegrees_, 5.0f, 28.0f) * 3.14159265f / 180.0f;
        const float distance = std::abs(presentation.developerStageBaseCamera_.z);
        auto eye = presentation.developerStageBaseCamera_;
        eye.y += std::sin(angle) * distance;
        eye.z = -std::cos(angle) * distance;
        auto rotation = presentation.developerStageBaseRotation_;
        rotation.x = angle;
        world_.resources.camera->SetTranslate(eye);
        world_.resources.camera->SetRotate(rotation);
        world_.resources.player_->SetNeonDepthAimEnabled(true); // 既存のビューポートに合わせて拡縮したXYのレイと平面による照準。
        presentation.developerStageTiltApplied_ = true;
    }
#endif
    world_.resources.camera->Update();
    world_.resources.debugCamera->Update(world_.resources.input_->GetMouseState(), world_.resources.input_->GetKey(),
                                         world_.resources.input_->GetLeftStick());
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    if (world_.resources.neonSkinnedPreview_) {
        world_.resources.neonSkinnedPreview_->Update(baseDeltaTime);
    }
#endif
    if (world_.resources.skybox_) {
        world_.resources.skybox_->Update(world_.resources.camera.get(), world_.resources.debugCamera.get());
    }

    world_.combat.direction = myEngine::Normalize(world_.combat.direction);
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
            // プレイヤーが操作している間は、ゲームパッドのボタンで次フレームの戦闘（射撃/ダッシュ）を行う。
            if (!world_.demo.titleDemo_) {
                world_.resources.input_->RequestGamepadGameplayContext();
            }
        }
        world_.resources.stage_->Update();
        world_.levelRuntime->UpdateLevelItems();

        world_.resources.player_->SetDebugNoDamage(
            world_.presentation.debugPlayerNoDamage_ ||
            (world_.run.expeditionMapEnabled_ && world_.run.expeditionMapRun_.GetActiveNode() &&
             world_.run.expeditionMapRun_.GetActiveNode()->role == tankexp::NodeRole::TutorialCombat));
        // 誘導先は敵の位置の写し。これを設定してから自機・ドローンを更新し、弾の生成とレーザー・地雷・斬撃の予約を進める。
        if (world_.run.UsesPrototypeRules() && !world_.resources.player_->IsChangeMode()) {
            std::vector<myEngine::Vector3> targets;
            if ((world_.gameplayQueries->IsRunRivalActive() && !world_.resources.enemy_->IsDead())) {
                targets.push_back(world_.resources.enemy_->GetWorldPosition());
            }
            for (const auto* actor : world_.resources.enemyManager_->GetEnemyPtrs()) {
                if (actor && !actor->IsDead()) {
                    targets.push_back(actor->GetWorldPosition());
                }
            }
            world_.resources.player_->SetRunHomingTargets(targets);
        }
        {
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
            world_.resources.gameplayScenarioCombatDt_ = world_.combat.finalDeltaTime;
#endif
            myEngine::RuntimeProfiler::CpuScope scope("Player / Drones Update");
            world_.resources.player_->Update(world_.resources.camera.get(), *world_.resources.stage_, world_.resources.bulletManager_.get(),
                                             world_.combat.finalDeltaTime, baseDeltaTime);
        }
#if defined(USE_IMGUI) && !defined(NDEBUG)
        world_.presentation.upgradeHudAfterPlayerUpdate_ = world_.resources.player_->GetUpgradeHudDebugSnapshot();
#endif
        // 試作の規則を使うモード（UsesPrototypeRules）の進化画面は自機側で入力を受け付ける。閉じたフレームも戦闘の後半を再開しない。
        // この条件は試作の規則を使うモードに限り、通常モードは戦闘時間0で以下を呼ぶ。
        if (!world_.run.UsesPrototypeRules() || (!world_.resources.player_->IsChangeMode() && !evolutionUiWasOpenAtFrameStart)) {
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
                myEngine::RuntimeProfiler::CpuScope scope("Enemy AI Update");
                if (world_.gameplayQueries->IsRunRivalActive()) {
                    world_.resources.enemy_->Update(world_.combat.finalDeltaTime);
                }
                if (!world_.run.UsesPrototypeRules()) {
                    world_.gameplayEditor->UpdateLevelBossPhases();
                }
                world_.resources.enemyManager_->Update(*world_.resources.stage_, world_.combat.finalDeltaTime);
            }

            // 敵AIが発射した弾も含めて移動・壁衝突を処理する。この更新内で死亡弾の実体も削除する。
            {
                myEngine::RuntimeProfiler::CpuScope scope("Bullets / Trails / Wall Collision");
                world_.resources.bulletManager_->Update(*world_.resources.stage_, world_.combat.finalDeltaTime);
            }

            // 弾と敵の更新後に特殊戦闘を適用し、その後で通常のアクター接触を通知する。
            // 衝突通知中に予約された弾はCheckAllCollisionsの走査後に追加され、このフレームの移動には戻らない。
            if (!suppressTutorialCombat) {
                myEngine::RuntimeProfiler::CpuScope scope("Actor / Bullet Collision");
                world_.resources.player_->UpdateSpecialCombat(*world_.resources.stage_, world_.resources.bulletManager_.get(),
                                                              world_.gameplayQueries->IsRunRivalActive() ? world_.resources.enemy_.get()
                                                                                                         : nullptr,
                                                              world_.resources.enemyManager_.get(), world_.combat.finalDeltaTime);
                world_.resources.collisionManager_->CheckAllCollisions(
                    world_.resources.player_.get(), world_.gameplayQueries->IsRunRivalActive() ? world_.resources.enemy_.get() : nullptr,
                    world_.resources.bulletManager_.get(), world_.resources.enemyManager_.get());
                for (const auto& impact : world_.resources.player_->ConsumeDashImpactEvents()) {
                    world_.expeditionExperience->QueueExpeditionImpact(impact.origin, impact.direction, impact.powered);
                }
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
    // すべての攻撃と衝突の後で最終的なHPとフェーズを読む。死亡演出は独立して続く。
    const float bossPresentationDeltaTime =
        depthIntroLocked ? world_.resources.neonDepthIntroDelta_
                         : (world_.resources.enemy_->IsDead()
                                ? baseDeltaTime
                                : (world_.combat.combatFlow_.GetState() == GameFlowState::Playing &&
                                           !world_.arenaRunController->IsTankRunMenuOpen() && !world_.resources.player_->IsChangeMode()
                                       ? world_.combat.finalDeltaTime
                                       : 0.0f));
    world_.bossPresentation->UpdateNeonBossVisual(bossPresentationDeltaTime);
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    world_.resources.gameplayScenarioPresentationDt_ = bossPresentationDeltaTime;
#endif
    // 戦闘停止中も、残っている演出を進め、未消費の特殊戦闘イベントを受け取る。
    world_.playerAttackEffects->UpdateSpecialCombatPresentation(baseDeltaTime);
    // クリアしたボス部屋は開いているランメニューとして扱われる。フィナーレにはメニューの灰色の幕をかけない。
    world_.combat.screenEffectDirector_.SetUpgradeMenuOpen(
        (world_.resources.player_->IsChangeMode() || world_.arenaRunController->IsTankRunMenuOpen()) &&
        world_.combat.combatFlow_.GetState() != GameFlowState::BossDefeatSequence);
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
    if (!io.WantCaptureMouse && !world_.run.UsesPrototypeRules()) {
        // 左クリックしたらパーティクル追加
        if (world_.resources.input_->IsTrigger(world_.resources.input_->GetMouseState().rgbButtons[0],
                                               world_.resources.input_->GetPreMouseState().rgbButtons[0])) {
            myEngine::Matrix4x4 viewMatrix = myEngine::Object3dCommon::GetInstance()->GetIsDebugCamera()
                                                 ? world_.resources.debugCamera->GetViewMatrix()
                                                 : world_.resources.camera->GetViewMatrix();
            myEngine::Matrix4x4 projectionMatrix = myEngine::Object3dCommon::GetInstance()->GetIsDebugCamera()
                                                       ? world_.resources.debugCamera->GetProjectionMatrix()
                                                       : world_.resources.camera->GetProjectionMatrix();
            myEngine::Vector3 worldPos =
                myEngine::ScreenToWorldOnZ0(world_.resources.input_->GetMousePosition(), viewMatrix, projectionMatrix,
                                            myEngine::WinApp::kClientWidth, myEngine::WinApp::kClientHeight);
            myEngine::ParticleManager::GetInstance()->EmitHitEffect(worldPos);
        }
    }

    if (world_.presentation.showParticleEditor_) {
        myEngine::ParticleManager::GetInstance()->DrawImGuiEditor();
    }

#endif // USE_IMGUI

    world_.combatEffects->UpdateDeathPostPulse(baseDeltaTime);
    const float particleDeltaTime =
        world_.combat.combatFlow_.GetState() == GameFlowState::Playing && !world_.run.expeditionTransition_.IsActive()
            ? world_.combat.finalDeltaTime
            : baseDeltaTime;
    {
        myEngine::RuntimeProfiler::CpuScope scope("Particles Update");
        myEngine::ParticleManager::GetInstance()->Update(particleDeltaTime, world_.resources.camera.get(),
                                                         world_.resources.debugCamera.get());
    }
    for (const myEngine::ParticleManager::ScreenPulseEvent& event : myEngine::ParticleManager::GetInstance()->ConsumeScreenPulseEvents()) {
        world_.combatEffects->TriggerDeathPostPulse(event.position, event.strength);
    }
    // 死亡演出・通貨回収の終了条件を見て結果画面へ移る。戦闘の時間倍率には合わせない。
    world_.combatFlow->UpdateGameFlow(baseDeltaTime);

    switch (world_.combat.phase_) {
    case Phase::kFadeIn:
        world_.combat.fade_->Update();

        if (world_.combat.fade_->IsFinished()) {
            world_.combat.phase_ = Phase::kMain;
            if (world_.run.IsLegacyArenaRun() && !world_.combat.bossEntryTriggered_ &&
                !world_.gameplayQueries->IsTutorialCombatSuppressed()) {
                world_.combat.bossEntryTriggered_ = true;
                world_.combat.screenEffectDirector_.TriggerBossEntry();
                world_.combatFlow->SetEventCallout("ボス出現", 1.20f);
            }
        }
        break;
    case Phase::kMain:
        if (world_.combat.combatFlow_.GetState() == GameFlowState::Playing && !world_.run.UsesPrototypeRules() &&
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
    if (!world_.demo.titleDemo_) {
        world_.titleDemoController->VerifyTitleDemoTransition(baseDeltaTime);
    }
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    world_.gameplayScenarioRunner->RecordGameplayScenarioFrame();
    world_.routeReplay->RecordNormalRouteReplay();
#endif
}
} // namespace gameplay
