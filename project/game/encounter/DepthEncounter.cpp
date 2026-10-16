#include "game/encounter/DepthEncounter.h"
#include "game/render/settings/ArenaCameraProfile.h"
#include "game/session/GameplaySystems.h"
#include "Enemy.h"
#include "Player.h"
#include "RuntimeProfiler.h"
#include <fstream>

namespace gameplay {
// ボス戦の進行方針、専用カメラ、シーンが所有する演出の統合。

bool DepthEncounter::LoadNeonDepthConfig(bool hotReload)
{
    neondepth::Config candidate;
    std::string error;
    try {
        const std::filesystem::path path("resources/configs/neonBossDepth.json");
        if (std::filesystem::exists(path)) {
            if (!std::filesystem::is_regular_file(path) || std::filesystem::file_size(path) > 64 * 1024) {
                throw std::runtime_error("Depth settings must be a regular file of at most 64 KiB.");
            }
            std::ifstream stream(path, std::ios::binary);
            if (!stream) {
                throw std::runtime_error("Could not open Depth settings.");
            }
            const auto document = nlohmann::json::parse(stream);
            if (!neondepth::ReadConfig(document, candidate, error)) {
                throw std::runtime_error(error);
            }
        }
        // カメラと有効化の選択は、新しい戦闘の開始時にだけ確定する。
        // 戦闘中に有効な編集をした場合は、攻撃の形状・タイミングと表示位置を次のインスタンス用に
        // 予約し、既に予告した計画はそのまま保つ。
        if (hotReload && IsNeonDepthEncounterActive()) {
            if (!world_.resources.enemy_->QueueNeonDepthTuning(candidate.combat) ||
                (world_.resources.neonBossVisual_ && !world_.resources.neonBossVisual_->SetDepthProfile(candidate.visual))) {
                throw std::runtime_error("Validated Depth edit could not be queued.");
            }
        }
        world_.resources.neonDepthPendingConfig_ = candidate;
        if (!hotReload || !IsNeonDepthEncounterActive()) {
            world_.resources.neonDepthConfig_ = candidate;
        }
        world_.resources.neonDepthConfigStatus_ =
            hotReload ? "Depth settings queued for the next attack / encounter." : "Depth settings ready.";
        return true;
    }
    catch (const std::exception& exception) {
        world_.resources.neonDepthConfigStatus_ = std::string(exception.what()).substr(0, 256);
        OutputDebugStringA((std::string("[NeonDepth] ") + world_.resources.neonDepthConfigStatus_ + "\n").c_str());
        return false; // 直前の有効なプロファイル（または初期値）を保つ。
    }
}

bool DepthEncounter::IsNeonDepthEncounterActive() const
{
    return !world_.demo.titleDemo_ && world_.resources.enemy_ && world_.resources.enemy_->IsNeonDepthEncounterEnabled() &&
           world_.resources.enemy_->IsRunEncounterEnabled() && world_.gameplayQueries->IsRunRivalActive() &&
           world_.resources.enemy_->GetNeonDepthSnapshot().phase != neondepth::Phase::Aborted;
}

void DepthEncounter::ConfigureNeonDepthEncounter([[maybe_unused]] bool completedFixtureRoomReset)
{
    if (!world_.resources.enemy_ || !world_.run.expeditionRun_ || world_.run.tankExpedition_.GetRoomKind() != tankexp::RoomKind::Boss) {
        return;
    }
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    const auto& session = GameplayScenarioSession::Get();
    if (world_.resources.neonBossAutoTest_ ||
        (session.IsActive() && (!gameplaytest::IsNeonDepthScenario(session.GetSettings().scenario) || !completedFixtureRoomReset))) {
        return;
    }
#endif
    world_.resources.neonDepthConfig_ = world_.resources.neonDepthPendingConfig_;
    if (!world_.resources.neonDepthConfig_.enabled) {
        return;
    }
    // 明示的に選ばれた古いDeveloper用フィクスチャのプロファイルは、
    // フィクスチャのアダプタが保持する。通常の配置や順番どおりのボス登場ではDepthを選ぶ。
    if (!world_.resources.neonDepthEffects_) {
        world_.resources.neonDepthEffects_ = std::make_unique<NeonDepthEffects>();
    }
    auto* dx = myEngine::Object3dCommon::GetInstance()->GetDxCommon();
    if (!dx->InitializeTrailSceneNoDepthPipeline() || !world_.resources.neonDepthEffects_->Initialize(dx)) {
        world_.resources.neonDepthConfigStatus_ = "Depth floor cues could not initialize; encounter retains the existing safe policy.";
        OutputDebugStringA((world_.resources.neonDepthConfigStatus_ + "\n").c_str());
        return;
    }
    world_.resources.enemy_->EnableNeonDepthEncounter(true, world_.resources.neonDepthConfig_.combat);
    if (!world_.resources.enemy_->IsNeonDepthEncounterEnabled()) {
        return;
    }
    if (world_.resources.neonBossVisual_) {
        world_.resources.neonBossVisual_->SetDepthProfile(world_.resources.neonDepthConfig_.visual);
    }
    if (world_.resources.neonDepthEffects_) {
        world_.resources.neonDepthEffects_->ResetEncounter();
    }
    world_.combatFlow->SetEventCallout("床の投影コアを攻撃 / Enterで登場をスキップ", 2.4f);
}

bool DepthEncounter::UpdateNeonDepthCamera(float baseDeltaTime)
{
    const bool wantScope = IsNeonDepthEncounterActive() && (world_.combat.combatFlow_.GetState() == GameFlowState::Playing ||
                                                            world_.combat.combatFlow_.GetState() == GameFlowState::BossDefeatSequence);
    if (!wantScope) {
        RestoreNeonDepthCamera();
        return false;
    }
    if (!world_.resources.neonDepthCameraScoped_) {
        world_.presentation.bossCinematic_ = {};
        world_.resources.neonDepthSavedCamera_ = {world_.resources.camera->GetTranslate(),        world_.resources.camera->GetRotate(),
                                                  world_.resources.camera->GetProjectionJitter(), world_.resources.camera->GetFovY(),
                                                  world_.resources.camera->GetAspectRatio(),      world_.resources.camera->GetNearClip(),
                                                  world_.resources.camera->GetFarClip()};
        world_.resources.neonDepthCameraScoped_ = true;
        world_.resources.neonDepthSavedDebugCamera_ = myEngine::Object3dCommon::GetInstance()->GetIsDebugCamera();
    }
    myEngine::Object3dCommon::GetInstance()->SetIsDebugCamera(false);
    neondepth::CameraFrame frame;
    auto profile = world_.resources.neonDepthConfig_.camera;
    profile.aspect = float(myEngine::WinApp::kClientWidth) / float(myEngine::WinApp::kClientHeight);
    const bool reducedMotion = world_.resources.neonBossVisual_ ? world_.resources.neonBossVisual_->GetDepthProfile().reducedMotion
                                                                : world_.resources.neonDepthConfig_.visual.reducedMotion;
    myEngine::Vector3 focus = UpdateBossCinematicCamera(baseDeltaTime, profile);
    if (world_.presentation.cameraShakeTimer_ > 0 && !reducedMotion) {
        const float dt = std::isfinite(baseDeltaTime) && baseDeltaTime > 0 ? (std::min)(baseDeltaTime, .1f) : 0;
        world_.presentation.cameraShakeTimer_ = (std::max)(0.0f, world_.presentation.cameraShakeTimer_ - dt);
        const float duration = (std::max)(world_.presentation.cameraShakeDuration_, .001f);
        const float ratio = (std::clamp)(world_.presentation.cameraShakeTimer_ / duration, 0.0f, 1.0f);
        const float amplitude = (std::min)(world_.presentation.cameraShakePower_, .45f) * ratio * ratio;
        const auto& snapshot = world_.resources.enemy_->GetNeonDepthSnapshot();
        // 解析的に上限を決めた揺れで、共有のゲームプレイ乱数を消費しない。揺らした最終行列は
        // 床へのマウス入力とすべてのアクターより先に設定する。
        const double eventPhase = double(snapshot.generation % 997) + double(snapshot.plan.instance % 991) * .71;
        const double age = double(duration - world_.presentation.cameraShakeTimer_);
        focus.x += amplitude * float(std::sin(age * 53 + eventPhase));
        focus.y += amplitude * float(std::sin(age * 67 + eventPhase * .37));
    } else if (reducedMotion) {
        world_.presentation.cameraShakeTimer_ = 0;
    }
    if (!neondepth::TryCamera(profile, focus, frame)) {
        RestoreNeonDepthCamera();
        return false;
    }
    world_.resources.neonDepthFloorViewCenter_ = focus;
    world_.resources.camera->SetRotate(frame.rotation);
    world_.resources.camera->SetTranslate(frame.position);
    world_.resources.camera->SetFovY(profile.fovY);
    world_.resources.camera->SetAspectRatio(profile.aspect);
    world_.resources.camera->SetNearClip(profile.nearClip);
    world_.resources.camera->SetFarClip(profile.farClip);
    world_.resources.camera->SetProjectionJitter({});
    if (world_.resources.player_) {
        world_.resources.player_->SetNeonDepthAimEnabled(true);
    }
    return true;
}

myEngine::Vector3 DepthEncounter::UpdateBossCinematicCamera(float baseDeltaTime, neondepth::CameraProfile& profile)
{
    auto& cine = world_.presentation.bossCinematic_;
    const float dt = std::isfinite(baseDeltaTime) && baseDeltaTime > 0 ? (std::min)(baseDeltaTime, .1f) : 0.0f;
    const auto& snapshot = world_.resources.enemy_->GetNeonDepthSnapshot();
    const bool defeat = world_.combat.combatFlow_.GetState() == GameFlowState::BossDefeatSequence;
    const bool intro = !defeat && snapshot.phase == neondepth::Phase::Intro;
    auto smooth = [](float t) {
        t = (std::clamp)(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    };
    auto mix = [](const myEngine::Vector3& a, const myEngine::Vector3& b, float t) {
        return a + (b - a) * t;
    };
    // 論理上の床の中心からずれている、見えている投影像（の足元）を画面に収める。
    myEngine::Vector3 boss = world_.resources.enemy_->GetWorldPosition();
    if (world_.resources.neonBossVisual_ && world_.resources.neonBossVisual_->HasResources() &&
        world_.resources.neonBossVisual_->IsVisible()) {
        const auto& world = world_.resources.neonBossVisual_->GetWorldMatrix();
        const myEngine::Vector3 feet{world.m[3][0], world.m[3][1], 0.0f};
        if (std::isfinite(feet.x) && std::isfinite(feet.y) && myEngine::Length(feet - myEngine::Vector3{boss.x, boss.y, 0.0f}) < 30.0f) {
            boss = feet;
        }
    }
    boss.z = 0.0f;
    myEngine::Vector3 player = world_.resources.player_ ? world_.resources.player_->GetWorldPosition() : boss;
    player.z = 0.0f;

    // 戦闘中: 通常のアリーナと同じカメラ距離とレンズでプレイヤーを追い、ボス側に少し寄せる。
    // わずかに傾けて、立ち上がった投影像の奥行きが読み取れるようにする。
    // 両方のアクターを画面内に収めるため、中間点を中心にし、広がりに必要な分だけ引く。
    // 投影像はカメラ側へ立っているので、上半身は透視で外側へ拡大される
    // （offset * D / (D - height)）。それが収まる最小の引き量を求める。
    myEngine::Vector3 target = mix(player, boss, 0.5f);
    constexpr float kActorMargin = 4.0f;
    const float halfHeight = arenacamera::HalfViewHeight();
    const float bodyHeight = world_.resources.neonBossVisual_ ? world_.resources.neonBossVisual_->GetDepthProfile().worldHeight : 9.0f;
    const float spread = (std::max)(std::abs(boss.x - player.x) * 0.5f / arenacamera::kAspect, std::abs(boss.y - player.y) * 0.5f);
    float combatDistance = 1.0f;
    for (; combatDistance < 2.2f; combatDistance += 0.05f) {
        const float depth = arenacamera::Distance() * combatDistance;
        const float magnified = spread * depth / (std::max)(depth - bodyHeight * 0.85f, 1.0f);
        if (magnified + kActorMargin <= halfHeight * combatDistance) {
            break;
        }
    }
    float minX = 0, minY = 0, maxX = 0, maxY = 0;
    if (world_.resources.stage_ && arenacamera::ArenaBounds(world_.resources.stage_->GetBlocks(), minX, minY, maxX, maxY)) {
        // 投影像は城壁の上に浮かぶことがあるので、そこでも枠を追従させる。
        minX = (std::min)(minX, boss.x - bodyHeight);
        maxX = (std::max)(maxX, boss.x + bodyHeight);
        minY = (std::min)(minY, boss.y - bodyHeight);
        maxY = (std::max)(maxY, boss.y + bodyHeight);
        const float viewHalf = halfHeight * combatDistance;
        target.x = arenacamera::ClampFocus(target.x, minX, maxX, viewHalf * arenacamera::kAspect);
        target.y = arenacamera::ClampFocus(target.y, minY, maxY, viewHalf);
    }
    float targetDistance = combatDistance, targetTilt = 14.0f, targetLetterbox = 0.0f;

    if (intro) {
        // 登場時: 形成中の投影像に近く低い位置から始め、ゆっくり引いていく。
        cine.introSeen = true;
        cine.introTime += dt;
        const float t = smooth(cine.introTime / 3.2f);
        // カメラ側へ立ち上がる投影像の中央を狙う。
        const float bodyHeight = world_.resources.neonBossVisual_ ? world_.resources.neonBossVisual_->GetDepthProfile().worldHeight : 7.0f;
        target = boss + myEngine::Vector3{0.0f, 0.0f, -bodyHeight * 0.55f};
        targetDistance = 0.55f + 0.35f * t;
        targetTilt = 30.0f - 8.0f * t;
        targetLetterbox = 1.0f;
        if (cine.titleMode != 1) {
            cine.titleMode = 1;
            cine.titleAge = 0.0f;
        }
    } else if (cine.introSeen && !cine.revealFired) {
        // 投影像が完成したら、戦闘用の構図へ戻る切り替えをはっきり見せる。
        cine.revealFired = true;
        world_.combatEffects->TriggerDeathPostPulse(cine.focus, 1.25f);
        world_.presentation.cameraShakeDuration_ = 0.55f;
        world_.presentation.cameraShakeTimer_ = world_.presentation.cameraShakeDuration_;
        world_.presentation.cameraShakePower_ = 0.40f;
    }

    if (defeat) {
        if (cine.defeatTime < 0.0f) {
            cine.defeatTime = 0.0f;
            cine.defeatFocus = boss;
            cine.burstsFired = 0;
            cine.finaleFired = false;
        }
        cine.defeatTime += dt;
        // 撃破時: 連続する爆発で崩れながら倒れる投影像へ寄っていく。
        const float a = smooth(cine.defeatTime / 0.9f);
        target.z = 0.0f;
        target = mix(target, cine.defeatFocus, a);
        targetDistance = combatDistance + (0.62f - combatDistance) * a;
        targetTilt = 14.0f + (30.0f - 14.0f) * a;
        targetLetterbox = 1.0f;
        constexpr float kBurstTimes[] = {0.05f, 0.28f, 0.50f, 0.68f, 0.86f, 1.02f};
        constexpr int kBursts = static_cast<int>(sizeof(kBurstTimes) / sizeof(kBurstTimes[0]));
        const myEngine::Vector4 primary{1.8f, 0.25f, 0.95f, 1.0f}, secondary{1.6f, 1.2f, 1.6f, 1.0f};
        while (cine.burstsFired < kBursts && cine.defeatTime >= kBurstTimes[cine.burstsFired]) {
            // 決定的なオフセットにより、リプレイが同一になりゲームプレイの乱数も消費しない。
            const float angle = 2.39996f * static_cast<float>(cine.burstsFired);
            const float radius = 1.2f + 0.45f * static_cast<float>(cine.burstsFired % 3);
            const myEngine::Vector3 at = cine.defeatFocus + myEngine::Vector3{std::cos(angle) * radius, std::sin(angle) * radius, -1.0f};
            myEngine::ParticleManager::GetInstance()->EmitNeonDeathEffect(at, primary, secondary, 0.85f);
            world_.combatEffects->TriggerDeathPostPulse(at, 0.55f);
            world_.presentation.cameraShakeDuration_ = 0.22f;
            world_.presentation.cameraShakeTimer_ = world_.presentation.cameraShakeDuration_;
            world_.presentation.cameraShakePower_ = 0.30f;
            ++cine.burstsFired;
        }
        if (!cine.finaleFired && cine.defeatTime >= 1.30f) {
            cine.finaleFired = true;
            myEngine::ParticleManager::GetInstance()->EmitNeonDeathEffect(cine.defeatFocus, primary, secondary, 2.4f);
            myEngine::ParticleManager::GetInstance()->EmitNeonDeathEffect(cine.defeatFocus, secondary, primary, 1.6f);
            world_.combatEffects->TriggerDeathPostPulse(cine.defeatFocus, 2.0f);
            world_.presentation.cameraShakeDuration_ = 0.75f;
            world_.presentation.cameraShakeTimer_ = world_.presentation.cameraShakeDuration_;
            world_.presentation.cameraShakePower_ = 0.45f;
            cine.titleMode = 2;
            cine.titleAge = 0.0f;
        }
        if (cine.defeatTime >= 1.8f) {
            // フィナーレの後はアリーナに余韻を持たせ、締めのタイトルに向けて少しだけ引く。
            const float settle = smooth((cine.defeatTime - 1.8f) / 1.6f);
            targetDistance += 0.22f * settle;
            targetTilt -= 6.0f * settle;
        }
    }

    if (!cine.cameraInitialized) {
        cine.cameraInitialized = true;
        cine.focus = target;
        cine.distanceScale = targetDistance;
        cine.tiltDegrees = targetTilt;
    }
    const bool cinematic = intro || defeat;
    const float follow = 1.0f - std::exp(-dt * (cinematic ? 3.0f : 6.0f));
    const float lens = 1.0f - std::exp(-dt * 2.4f);
    cine.focus = mix(cine.focus, target, follow);
    cine.distanceScale += (targetDistance - cine.distanceScale) * lens;
    cine.tiltDegrees += (targetTilt - cine.tiltDegrees) * lens;
    cine.letterbox += (targetLetterbox - cine.letterbox) * (1.0f - std::exp(-dt * 5.0f));
    cine.titleAge += dt;
    const float titleTarget = cine.titleMode == 1 ? (intro ? 1.0f : 0.0f) : cine.titleMode == 2 ? 1.0f : 0.0f;
    cine.titleAlpha += (titleTarget - cine.titleAlpha) * (1.0f - std::exp(-dt * 4.0f));

    auto& ui = world_.combat;
    if (cine.titleMode != 0 && ui.cinematicTitle_ && ui.cinematicSubtitle_ && ui.cinematicTitleMode_ != cine.titleMode) {
        ui.cinematicTitleMode_ = cine.titleMode;
        auto style = ui.cinematicTitle_->GetStyle();
        style.color = cine.titleMode == 1 ? myEngine::Vector4{1.0f, 0.62f, 0.92f, 1.0f} : myEngine::Vector4{0.72f, 1.0f, 0.95f, 1.0f};
        ui.cinematicTitle_->SetStyle(style);
        ui.cinematicTitle_->SetText(cine.titleMode == 1 ? "最終決戦" : "ボス撃破");
        ui.cinematicSubtitle_->SetText(cine.titleMode == 1 ? "最深部の投影体があらわれた  /  Enterでスキップ" : "最深部を制圧した");
    }
    profile.tiltDegrees = (std::clamp)(cine.tiltDegrees, 0.0f, 35.0f);
    profile.distance = arenacamera::Distance() * cine.distanceScale;
    profile.fovY = arenacamera::kFovY;
    return cine.focus;
}

void DepthEncounter::RestoreNeonDepthCamera()
{
    if (!world_.resources.neonDepthCameraScoped_ || !world_.resources.camera) {
        return;
    }
    world_.resources.camera->SetRotate(world_.resources.neonDepthSavedCamera_.rotation);
    world_.resources.camera->SetTranslate(world_.resources.neonDepthSavedCamera_.position);
    world_.resources.camera->SetFovY(world_.resources.neonDepthSavedCamera_.fovY);
    world_.resources.camera->SetAspectRatio(world_.resources.neonDepthSavedCamera_.aspect);
    world_.resources.camera->SetNearClip(world_.resources.neonDepthSavedCamera_.nearClip);
    world_.resources.camera->SetFarClip(world_.resources.neonDepthSavedCamera_.farClip);
    world_.resources.camera->SetProjectionJitter(world_.resources.neonDepthSavedCamera_.jitter);
    world_.resources.camera->Update();
    if (world_.resources.player_) {
        world_.resources.player_->SetNeonDepthAimEnabled(false);
    }
    myEngine::Object3dCommon::GetInstance()->SetIsDebugCamera(world_.resources.neonDepthSavedDebugCamera_);
    world_.resources.neonDepthCameraScoped_ = false;
}

bool DepthEncounter::UpdateNeonDepthIntro(float baseDeltaTime)
{
    world_.resources.neonDepthIntroDelta_ = 0;
    if (!IsNeonDepthEncounterActive() || world_.resources.enemy_->GetNeonDepthSnapshot().phase != neondepth::Phase::Intro) {
        return false;
    }
    const bool permitted = world_.combat.combatFlow_.GetState() == GameFlowState::Playing && world_.combat.phase_ == Phase::kMain &&
                           !world_.arenaRunController->IsTankRunMenuOpen() && !world_.resources.player_->IsChangeMode() &&
                           !world_.resources.player_->IsDead();
    if (permitted && (world_.resources.neonDepthSkipRequested_ || world_.resources.input_->IsKeyTriggered(DIK_RETURN) ||
                      world_.resources.input_->IsKeyTriggered(DIK_SPACE))) {
        world_.resources.neonDepthSkipRequested_ = false;
        world_.resources.enemy_->SkipNeonDepthIntro();
        world_.resources.enemy_->Update(0); // 変換を更新する。スキップ入力で最初の戦闘tickは進めない。
    } else {
        world_.resources.neonDepthIntroDelta_ = permitted ? baseDeltaTime : 0;
        world_.resources.enemy_->Update(world_.resources.neonDepthIntroDelta_);
    }
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    world_.resources.gameplayScenarioCombatDt_ = world_.resources.neonDepthIntroDelta_;
#endif
    // dtが0のPlayer::Updateでも攻撃は処理される。導入中のロックの間はどのアクターにも
    // 入力を渡さず、借りている描画用の変換だけを更新する。
    if (world_.resources.playerObject_) {
        world_.resources.playerObject_->Update();
    }
    world_.resources.collisionDebugRingManager_->Clear();
    return true;
}

void DepthEncounter::UpdateNeonDepthEffects()
{
    if (!world_.resources.neonDepthEffects_ || !world_.resources.enemy_ || !world_.resources.neonBossVisual_) {
        return;
    }
    myEngine::RuntimeProfiler::CpuScope effectScope("Neon Depth FX Build");
    neondepth::EffectsInput input;
    input.frameId = ++world_.resources.neonDepthFrameId_;
    input.snapshot = world_.resources.enemy_->GetNeonDepthSnapshot();
    input.core = world_.resources.enemy_->GetWorldPosition();
    input.coreRadius = world_.resources.enemy_->GetRadius();
    input.hitFeedback = world_.resources.enemy_->GetDamageFeedbackRatio();
    input.profile = world_.resources.neonBossVisual_->GetDepthProfile();
    input.formation = world_.resources.neonBossVisual_->GetDepthFormation();
    input.life = world_.resources.neonBossVisual_->GetDepthLife();
    input.dissolveProgress = world_.resources.neonBossVisual_->GetDissolveProgress();
    input.bodyVisualEnabled = world_.resources.neonBossVisualEnabled_;
    input.encounterActive = world_.resources.enemy_->IsNeonDepthEncounterEnabled() && world_.resources.enemy_->IsRunEncounterEnabled() &&
                            world_.gameplayQueries->IsRunRivalActive();
    input.sockets.chestValid = world_.resources.neonBossVisual_->GetDepthSocketWorld(neondepth::Socket::Chest, input.sockets.chest);
    input.sockets.leftHandValid =
        world_.resources.neonBossVisual_->GetDepthSocketWorld(neondepth::Socket::LeftHand, input.sockets.leftHand);
    input.sockets.rightHandValid =
        world_.resources.neonBossVisual_->GetDepthSocketWorld(neondepth::Socket::RightHand, input.sockets.rightHand);
    input.viewProjection = world_.resources.camera->GetViewProjectionMatrix();
    const auto world = myEngine::MakeAffineMatrix({1, 1, 1}, world_.resources.camera->GetRotate(), {});
    input.cameraRight = {world.m[0][0], world.m[0][1], world.m[0][2]};
    input.cameraUp = {world.m[1][0], world.m[1][1], world.m[1][2]};
    input.cameraForward = {world.m[2][0], world.m[2][1], world.m[2][2]};
    const bool terminal = input.snapshot.phase == neondepth::Phase::Defeated || input.snapshot.phase == neondepth::Phase::Aborted;
    if (terminal && world_.resources.neonBossVisual_->IsFinished()) {
        // このUpdateの前に、前回送信したフレームのフェンスは完了している。
        // ラッパーの記録用カウンターは残し、インスタンスバッファは1回だけ返す。
        if (world_.resources.neonDepthEffects_->HasResources()) {
            world_.resources.neonDepthEffects_->Release();
        }
    } else {
        world_.resources.neonDepthEffects_->Update(input);
    }
    // 観測用のカウンターはプロファイラの実際のフレーム番号を共有する。
    // リソースの確保はせず、戦闘や演出の時計にも影響しない。
    auto& profiler = myEngine::RuntimeProfiler::Get();
    const auto& model = world_.resources.neonBossVisual_->GetStats();
    const auto& effects = world_.resources.neonDepthEffects_->GetStats();
    profiler.SetCounter("Depth enabled", input.encounterActive);
    profiler.SetCounter("Depth phase", static_cast<unsigned>(input.snapshot.phase));
    profiler.SetCounter("Depth attack", static_cast<unsigned>(input.snapshot.plan.attack));
    profiler.SetCounter("Depth phase two", input.snapshot.plan.phaseTwo);
    profiler.SetCounter("Depth visual enabled", input.bodyVisualEnabled);
    profiler.SetCounter("Depth model creates", model.resourceCreates);
    profiler.SetCounter("Depth model releases", model.resourceReleases);
    profiler.SetCounter("Depth model updates", model.modelUpdates);
    profiler.SetCounter("Depth animation selections", model.animationSelections);
    profiler.SetCounter("Depth visual draws", model.draws);
    profiler.SetCounter("Depth model resources", world_.resources.neonBossVisual_->HasResources());
    profiler.SetCounter("Depth draw buffers", static_cast<double>(world_.resources.neonBossVisual_->GetDrawConstantBufferCount()));
    profiler.SetCounter("Depth effect creates", effects.resourceCreates);
    profiler.SetCounter("Depth effect releases", effects.resourceReleases);
    profiler.SetCounter("Depth effect resources", world_.resources.neonDepthEffects_->HasResources());
    profiler.SetCounter("Depth effect updates", static_cast<double>(effects.updates));
    profiler.SetCounter("Depth effect vertices", effects.vertices);
    profiler.SetCounter("Depth effect air vertices", effects.airVertices);
    profiler.SetCounter("Depth effect floor vertices", effects.floorVertices);
    profiler.SetCounter("Depth effect launch latches", effects.launchLatches);
    profiler.SetCounter("Depth effect invalid frames", static_cast<double>(effects.invalidFrames));
    profiler.SetCounter("Depth effect capacity rejects", static_cast<double>(effects.capacityRejected));
    profiler.SetCounter("Depth effect binding rejects", static_cast<double>(effects.depthBindingRejected));
    profiler.SetCounter("Depth effect duplicate updates", static_cast<double>(effects.duplicateUpdates));
    profiler.SetCounter("SRV descriptors", myEngine::Object3dCommon::GetInstance()->GetSrvManager()->GetAllocatedCount());
}

} // namespace gameplay
