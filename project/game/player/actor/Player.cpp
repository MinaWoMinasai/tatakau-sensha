#include "game/player/combat/PlayerSystems.h"
#include "game/player/ui/PlayerPresentation.h"
#include "game/weapon/CombatTypes.h"
#include "Player.h"
#include "game/player/PlayerMovement.h"
#include "game/enemy/visual/NeonDepthPlacement.h"
#include "StartupTrace.h"
#include "Stage.h"
#include "game/exp/ExpEnemy.h"
#include "game/enemy/actor/Enemy.h"
#include "game/exp/EnemyManager.h"
#include "Audio.h"
#include "game/ui/TankButtonUI.h"
#include "game/ui/NeonTextEffect.h"
#include "ObjectPostEffect.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numbers>
#include <nlohmann/json.hpp>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
/// @brief 係数を0〜1に制限し、2つのRGBA色を線形補間して返す。
myEngine::Vector4 LerpColor(const myEngine::Vector4& a, const myEngine::Vector4& b, float t)
{
    t = (std::clamp)(t, 0.0f, 1.0f);
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
}

/// @brief WASDから移動方向を読む。ゲームパッドを使っているときは、スティックの傾きで置き換える。
myEngine::Vector3 ReadMoveInput(myEngine::Input* input)
{
    myEngine::Vector3 direction{};
    if (input->IsPress(input->GetKey()[DIK_A])) {
        direction.x -= 1.0f;
    }
    if (input->IsPress(input->GetKey()[DIK_D])) {
        direction.x += 1.0f;
    }
    if (input->IsPress(input->GetKey()[DIK_W])) {
        direction.y += 1.0f;
    }
    if (input->IsPress(input->GetKey()[DIK_S])) {
        direction.y -= 1.0f;
    }
    if (input->IsGamepadActive()) {
        // アナログスティック: 方向と傾き（長さは移動の1ステップで制限する）。
        const myEngine::Vector2 move = input->GetGamepadMove();
        direction = {move.x, move.y, 0};
    }
    return direction;
}

} // namespace

Player::~Player() {}

bool Player::ConsumePrimaryAttackPerformedEvent()
{
    const bool performed = primaryAttackPerformedEvent_;
    primaryAttackPerformedEvent_ = false;
    return performed;
}

bool Player::ConsumeDashStartedEvent()
{
    const bool started = dashStartedEvent_;
    dashStartedEvent_ = false;
    return started;
}

myEngine::Sphere Player::GetSphere() const
{
    myEngine::Sphere s{};
    s.center = GetWorldPosition();

    // 半径は「横幅基準」が安定
    s.radius = kRadius;

    return s;
}

bool Player::RequestSlow()
{
    if (requestSlow_) {
        requestSlow_ = false;
        return true;
    }
    return false;
}

void Player::RotateToMouse(myEngine::Camera* viewProjection)
{
    if (demoInputEnabled_) {
        runAimWorld_ = demoAim_;
        const myEngine::Vector3 offset = demoAim_ - worldTransform_.translate;
        if (myEngine::Length(offset) > 0.001f) {
            dir_ = myEngine::Normalize(offset);
        }
        angle_ = std::atan2(dir_.y, dir_.x);
        worldTransform_.rotate.z = angle_;
        object_->SetRotate(worldTransform_.rotate);
        return;
    }
    if (input_ && input_->IsGamepadActive()) {
        // ツインスティックの照準: 右スティックで砲身の向きを決め、離すと最後の向きを保つ。
        // 置いたままのマウスで照準が引っ張られないよう、OSのカーソルは無視する。
        myEngine::Vector2 aim{};
        if (input_->TryGetGamepadAim(aim)) {
            dir_ = myEngine::Normalize(myEngine::Vector3{aim.x, aim.y, 0.0f});
        }
        runAimWorld_ = worldTransform_.translate + dir_ * 12.0f;
        angle_ = std::atan2(dir_.y, dir_.x);
        worldTransform_.rotate.z = angle_;
        object_->SetRotate(worldTransform_.rotate);
        return;
    }
    if (neonDepthAimEnabled_) {
        if (!viewProjection) {
            return;
        }
        auto* window = myEngine::WinApp::GetInstance();
        POINT mousePosition{};
        if (!window || !GetCursorPos(&mousePosition) || !ScreenToClient(window->GetHwnd(), &mousePosition)) {
            return;
        }
        constexpr myEngine::Vector2 viewport{1280.0f, 720.0f};
        myEngine::Vector2 logicalMouse{static_cast<float>(mousePosition.x), static_cast<float>(mousePosition.y)};
        window->ClientToLogical(logicalMouse.x, logicalMouse.y);
        myEngine::Vector2 pixel{};
        myEngine::Vector3 target{};
        if (!neondepth::TryClientToViewport(logicalMouse, viewport, viewport, pixel) ||
            !neondepth::TryScreenToFloorFromViewProjection(pixel, viewport, viewProjection->GetViewProjectionMatrix(), 0.0f, target)) {
            return;
        }
        const myEngine::Vector3 offset = target - worldTransform_.translate;
        const float length = myEngine::Length(offset);
        if (!neondepth::Finite(offset) || !std::isfinite(length) || length <= 0.001f) {
            return;
        }
        dir_ = myEngine::Normalize(offset);
        runAimWorld_ = target;
        angle_ = std::atan2(dir_.y, dir_.x);
        worldTransform_.rotate.z = angle_;
        object_->SetRotate(worldTransform_.rotate);
        return;
    }
    // --- 1. マウス座標取得 ---
    POINT mousePosition;
    GetCursorPos(&mousePosition);
    HWND hwnd = myEngine::WinApp::GetInstance()->GetHwnd();
    ScreenToClient(hwnd, &mousePosition);
    float logicalMouseX = static_cast<float>(mousePosition.x);
    float logicalMouseY = static_cast<float>(mousePosition.y);
    myEngine::WinApp::GetInstance()->ClientToLogical(logicalMouseX, logicalMouseY);

    // --- 2. 逆変換用の行列を準備 ---
    myEngine::Matrix4x4 matViewport =
        myEngine::MakeViewportMatrix(0, 0, myEngine::WinApp::kClientWidth, myEngine::WinApp::kClientHeight, 0, 1);
    myEngine::Matrix4x4 matVPV = viewProjection->GetViewMatrix() * viewProjection->GetProjectionMatrix() * matViewport;
    myEngine::Matrix4x4 matInverseVPV = myEngine::Inverse(matVPV);

    // --- 3. マウス座標をワールドに変換 ---
    myEngine::Vector3 posNear = myEngine::Vector3(logicalMouseX, logicalMouseY, 0);
    myEngine::Vector3 posFar = myEngine::Vector3(logicalMouseX, logicalMouseY, 1);

    posNear = myEngine::TransformMatrix(posNear, matInverseVPV);
    posFar = myEngine::TransformMatrix(posFar, matInverseVPV);

    // --- 4. レイと Z=0 平面の交差 ---
    myEngine::Vector3 mouseDirection = posFar - posNear;
    myEngine::Vector3 rayDir = myEngine::Normalize(mouseDirection);
    float t = -posNear.z / rayDir.z;
    myEngine::Vector3 target = posNear + rayDir * t;
    runAimWorld_ = target;

    // --- 5. プレイヤーの位置と方向ベクトル ---
    myEngine::Vector3 playerPos = worldTransform_.translate;
    myEngine::Vector3 targetPos = target - playerPos;
    dir_ = myEngine::Normalize(targetPos);

    // --- 6. 回転角度を算出 ---
    angle_ = atan2(dir_.y, dir_.x);
    worldTransform_.rotate.z = angle_;
    object_->SetRotate(worldTransform_.rotate);
}

void Player::Initialize(myEngine::Object3d* object, const myEngine::Vector3& position, bool arenaUi)
{
    myEngine::StartupTrace::Scope scope("Player.Initialize");
    wchar_t startupCacheFlag[8]{};
    const bool baseline = GetEnvironmentVariableW(L"MYENGINE_STARTUP_CACHE", startupCacheFlag, 8) > 0 && startupCacheFlag[0] == L'0';
    ui_->arenaUiEnabled_ = arenaUi || baseline;

    object_ = object;
    baseVehicleColor_ = object_->GetColor();

    worldTransform_ = myEngine::InitWorldTransform();
    worldTransform_.translate = position;
    object_->SetTransform(worldTransform_);
    object_->Update();
    if (!LoadPlayerClassConfigs()) {
        classCatalog_.ResetToDefaults();
        std::cerr << "[PlayerClass] Using built-in defaults." << std::endl;
    }
    InitializeBarrels();
    UpdateBarrelLayout();

    // シングルトンインスタンス
    input_ = myEngine::Input::GetInstance();

    // 衝突属性を設定
    SetCollisionAttribute(kCollisionAttributePlayer);
    // 衝突対象を自分の属性以外に設定
    SetCollisionMask(kCollisionAttributeEnemy | kCollisionAttributeExpEnemy | kCollisionAttributeEnemyBullet);
    SetDamage(static_cast<uint32_t>(stats_.bodyDamage));

    level_ = 1;
    exp_ = 0;
    nextLevelExp_ = GetNextLevelExp();
    hp_ = GetMaxHp();

    ui_->machineGunBtnSprite_ = std::make_unique<myEngine::Sprite>();
    ui_->machineGunBtnSprite_->Initialize(myEngine::SpriteCommon::GetInstance(), "resources/white512x512.png");
    ui_->machineGunBtnSprite_->SetPosition({ui_->btnPos_});
    ui_->machineGunBtnSprite_->SetSize({ui_->btnSize_});

    playerEvolution_->LoadEvolutionUiStyle();
    if (ui_->arenaUiEnabled_) {
        playerEvolution_->InitializeEncyclopedia();
        playerEvolution_->InitializeStaticEvolutionPrototype();
        playerEvolution_->InitializeEvolutionCircuitPrototype();
        playerHud_->InitializeUpgradeHud();
        playerHud_->LoadUpgradeHudConfig();
        playerHud_->ApplyUpgradeHudLayout();
        playerHud_->PrepareUpgradeHudSegmentBars();
        playerHud_->PrepareUpgradeHudTextTextures();
    } else {
        // 作成済みの成長ルールは戦闘や進化の処理で使えるように残す。
        // 旧来のアリーナの見えないUIだけを省き、後回しにはしない。
        playerEvolution_->LoadEvolutionCircuitTree();
        myEngine::StartupTrace::Count("player.unusedArenaUiSkipped");
    }
}

void Player::Update(myEngine::Camera* viewProjection, Stage& stage, BulletManager* bulletManager, float deltaTime, float uiDeltaTime)
{
    POINT mousePos;
    GetCursorPos(&mousePos);
    ScreenToClient(myEngine::WinApp::GetInstance()->GetHwnd(), &mousePos);
    mousePosition_ = {static_cast<float>(mousePos.x), static_cast<float>(mousePos.y)};
    myEngine::WinApp::GetInstance()->ClientToLogical(mousePosition_.x, mousePosition_.y);
    // UIにはスローの影響を受けない時間を渡す。操作による開閉も同じフレーム内で判定する。
    const bool evolutionUiWasOpen = isChangeMode_;
    if (!demoInputEnabled_ && ui_->arenaUiEnabled_) {
        playerEvolution_->UpdateEncyclopedia(uiDeltaTime);
        playerHud_->UpdateUpgradeHud(uiDeltaTime);
    }

    if (ui_->arenaUiEnabled_ && !demoInputEnabled_ && !(runModifiers_.enabled && runCheckpointEvolution_) &&
        input_->IsTrigger(input_->GetKey()[DIK_C], input_->GetPreKey()[DIK_C])) {
        if (isChangeMode_) {
            isChangeMode_ = false;
            evolutionCancelledEvent_ = true;
        } else {
            isChangeMode_ = true;
            playerEvolution_->PrepareStaticEvolutionTextTextures();
            playerEvolution_->PrepareEvolutionCircuitTextTextures();
        }
    }
    // 進化UIを操作したクリックやキー入力を、そのまま射撃・移動へ流さない。
    // 確定やキャンセルでこのフレーム中に閉じた場合も、次フレームまでゲーム入力を抑止する。
    if (evolutionUiWasOpen || isChangeMode_) {
        ui_->machineGunBtnSprite_->Update();
        return;
    }
    if (runModifiers_.enabled && runRoomAwaitInputRelease_ && !input_->IsPress(input_->GetMouseState().rgbButtons[0]) &&
        !input_->IsPress(input_->GetMouseState().rgbButtons[1])) {
        runRoomAwaitInputRelease_ = false;
    }

    if (!demoInputEnabled_ && !runModifiers_.enabled && skillPoints_ > 0) {
        // 1〜7キーが、それぞれ0〜6番目の能力の強化に対応する。
        static constexpr std::array<uint8_t, 7> kStatUpgradeKeys = {DIK_1, DIK_2, DIK_3, DIK_4, DIK_5, DIK_6, DIK_7};
        for (int index = 0; index < static_cast<int>(kStatUpgradeKeys.size()); ++index) {
            const uint8_t key = kStatUpgradeKeys[static_cast<size_t>(index)];
            if (input_->IsTrigger(input_->GetKey()[key], input_->GetPreKey()[key])) {
                ApplyStatUpgrade(index);
            }
        }
    }

    if (runModifiers_.enabled) {
        runDashAttackTimer_ = (std::max)(0.0f, runDashAttackTimer_ - deltaTime);
        runOverdriveTimer_ = (std::max)(0.0f, runOverdriveTimer_ - deltaTime);
        runOverdriveCooldown_ = (std::max)(0.0f, runOverdriveCooldown_ - deltaTime);
    }

    // バフタイマーの更新
    if (buffTimer_ > 0.0f) {
        buffTimer_ -= deltaTime;
        SpawnBuffParticle();
        if (buffTimer_ <= 0.0f) {
            isBuffActive_ = false;
        }
    }

    dt_ = deltaTime;
    empJammerTimer_ = (std::max)(0.0f, empJammerTimer_ - deltaTime);
    recentDashTimer_ = (std::max)(0.0f, recentDashTimer_ - deltaTime);

    invincibleTimer_ -= deltaTime;
    if (damageFeedbackTimer_ > 0.0f) {
        damageFeedbackTimer_ = (std::max)(0.0f, damageFeedbackTimer_ - deltaTime);
    }
    if (meleeComboTimer_ > 0.0f) {
        meleeComboTimer_ = (std::max)(0.0f, meleeComboTimer_ - deltaTime);
    } else {
        meleeComboStep_ = 0;
    }

    if (dashTimer_ > 0.0f) {
        dashTimer_ -= deltaTime;
        if (dashTimer_ <= 0.0f) {
            isDashing_ = false;
            isJustEvaded_ = false;
        }
    }

    if (dashCooldown_ > 0.0f) {
        dashCooldown_ -= deltaTime;
    }

    // 右クリックは機体ごとの特殊行動スロットとして扱う。
    if (runModifiers_.enabled) {
        // ダッシュの前に現在の方向を読む。新たに押した移動キーで
        // 次のフレームではなく即座に加速の向きを変えるため。
        inputDir_ = demoInputEnabled_ ? myEngine::Vector3{demoMove_.x, demoMove_.y, 0} : ReadMoveInput(input_);
        RotateToMouse(viewProjection);
    }
    if ((!runModifiers_.enabled || !runRoomAwaitInputRelease_) &&
        (demoInputEnabled_ ? demoDash_
                           : input_->IsTrigger(input_->GetMouseState().rgbButtons[1], input_->GetPreMouseState().rgbButtons[1]))) {
        demoDash_ = false;
        TryActivateSpecialAction();
    }
    if (saberCounterTimer_ > 0.0f) {
        saberCounterTimer_ = (std::max)(0.0f, saberCounterTimer_ - deltaTime);
    }

    // スタミナ回復
    if (!isDashing_) {
        stats_.stamina += stats_.staminaRecovery * deltaTime * (empJammerTimer_ > 0 ? .6f : 1.0f);
        if (stats_.stamina > stats_.maxStamina) {
            stats_.stamina = stats_.maxStamina;
        }
    }

    UpdateStealth(deltaTime);

    // ----------------------
    // 横入力
    // ----------------------
    RotateToMouse(viewProjection);
    inputDir_ = demoInputEnabled_ ? myEngine::Vector3{demoMove_.x, demoMove_.y, 0} : ReadMoveInput(input_);
    PlayerMovementInput movementInput{};
    movementInput.direction = inputDir_;
    movementInput.velocity = velocity_;
    movementInput.moveSpeed = stats_.moveSpeed;
    movementInput.acceleration = accel_;
    movementInput.deceleration = decel_;
    movementInput.deltaTime = deltaTime;
    movementInput.runEnabled = runModifiers_.enabled;
    movementInput.dashing = isDashing_;
    movementInput.railHeld = railCharge_.held;
    movementInput.spinActive = spinCycle_.remaining > 0;
    const PlayerMovementStep movementStep = CalculatePlayerMovement(movementInput);
    inputDir_ = movementStep.direction;
    velocity_ = movementStep.velocity;
    const int subStepCount = movementStep.subStepCount;
    const myEngine::Vector3 stepMove = movementStep.stepMove;
    // 衝突解決の順番を保つ: 各サブステップでX、次にYを適用する。
    for (int i = 0; i < subStepCount; ++i) {
        myEngine::Vector3 pos = GetWorldPosition();
        pos.x += stepMove.x;
        SetWorldPosition(pos);
        stage.ResolvePlayerCollision(*this, myEngine::X);

        pos = GetWorldPosition();
        pos.y += stepMove.y;
        SetWorldPosition(pos);
        stage.ResolvePlayerCollision(*this, myEngine::Y);
    }

    if (!isDead_ && !isDashing_ && myEngine::Length(inputDir_) > 0.05f) {
        movementParticleTimer_ -= deltaTime;
        if (movementParticleTimer_ <= 0.0f) {
            SpawnAfterimage();
            movementParticleTimer_ = movementParticleInterval_;
        }
    } else {
        movementParticleTimer_ = 0.0f;
    }

    // object_ の更新だけ（移動はしない）
    object_->SetTransform(worldTransform_);
    object_->Update();
    UpdateBarrelLayout();

    if (!isDead_) {
        // 攻撃処理
        if (runModifiers_.enabled && runDashBurstPending_) {
            runDashBurstPending_ = false;
            const auto synergy = MakeTankRunSynergy(runModifiers_, runOverdriveTimer_ > 0.0f);
            {
                MineDropEvent explosion{};
                explosion.position = GetWorldPosition();
                explosion.radius = synergy.explosionRadius;
                explosion.fuseTime = 0.06f;
                explosion.lifeTime = 0.12f;
                explosion.damage = static_cast<uint32_t>((std::max)(1.0f, std::round(stats_.bulletDamage * synergy.explosionDamageScale)));
                explosion.color = {1.8f, 0.65f, 0.14f, 1.0f};
                pendingMineDrops_.push_back(explosion);
                ++runDashExplosionsEmitted_;
            }
        }

        if (currentClass_ == ClassType::Smasher && !IsMeleeBuild()) {
            Smash(deltaTime);
        } else {
            Attack(bulletManager, deltaTime);
        }

        const PlayerClassConfig* activeClass = GetCurrentClassConfig();
        const size_t authoredSupport =
            runEvolutionActive_ && runAuthoredEvolutionActive_ ? static_cast<size_t>((std::max)(0, runEvolutionConfig_.maxDrones)) : 0u;
        const size_t supportLimit =
            authoredSupport +
            (runModifiers_.drones ? static_cast<size_t>(TankEffectCount(runModifiers_, tankrun::CardId::Drones, 2)) : 0u) +
            (runModifiers_.core == TankRunCore::Drone ? 2u : 0u);
        if (IsDroneBuild()) {
            runBulletManager_ = bulletManager;
            runSupportDroneTimer_ = (std::max)(0.0f, runSupportDroneTimer_ - deltaTime);
            if (runSupportDroneTimer_ <= 0.0f) {
                EnsureExpeditionDrones();
            }
        } else if (runModifiers_.enabled && !runRoomAwaitInputRelease_ && supportLimit > 0 && (!activeClass || !activeClass->usesDrone)) {
            runSupportDroneTimer_ = (std::max)(0.0f, runSupportDroneTimer_ - deltaTime);
            if (drones_.size() < supportLimit && runSupportDroneTimer_ <= 0.0f) {
                auto drone = std::make_unique<PlayerDrone>();
                drone->Initialize(worldTransform_.translate + dir_ * 0.8f, dir_ * 0.1f);
                drone->SetAttackControllerBulletManager(bulletManager);
                ConfigureRunDrone(*drone);
                drones_.push_back(std::move(drone));
                runSupportDroneTimer_ = 1.2f;
            }
        }
        const size_t bulletsBeforeDroneUpdates = bulletManager->GetBulletCount();
        size_t companionIndex = 0;
        std::array<bool, 48> spreadUsed{};
        for (auto& drone : drones_) {
            if (runModifiers_.enabled) {
                ConfigureRunDrone(*drone);
                drone->SetRunOwner(this, static_cast<int>(companionIndex));
                const bool wantsAttack = !runRoomAwaitInputRelease_ && !ui_->upgradeHudMouseCaptured_ &&
                                         (demoInputEnabled_ ? demoShoot_ : input_->IsPress(input_->GetMouseState().rgbButtons[0]));
                myEngine::Vector3 aim = runAimWorld_;
                if (IsDroneBuild() && !runHomingTargets_.empty() && (runModifiers_.autonomousSpread || runModifiers_.droneFocus)) {
                    std::array<float, 48> distances{};
                    const auto from = runModifiers_.autonomousSpread ? drone->GetWorldPosition() : runAimWorld_;
                    const int count = static_cast<int>((std::min)(runHomingTargets_.size(), distances.size()));
                    for (int n = 0; n < count; ++n) {
                        distances[static_cast<size_t>(n)] = myEngine::Length(runHomingTargets_[static_cast<size_t>(n)] - from);
                    }
                    const int chosen = tankspecial::ChooseSpreadTarget(distances.data(), spreadUsed.data(), count);
                    if (chosen >= 0 && (runModifiers_.autonomousSpread || distances[static_cast<size_t>(chosen)] < 6.0f)) {
                        aim = runHomingTargets_[static_cast<size_t>(chosen)];
                        if (runModifiers_.autonomousSpread) {
                            spreadUsed[static_cast<size_t>(chosen)] = true;
                            if (wantsAttack) {
                                specialCombatStats_.spreadTargets |= uint32_t{1} << (chosen % 32);
                            }
                        }
                    }
                }
                if (expeditionCombatStyleSelected_ || demoInputEnabled_) {
                    drone->SetRunInput(aim, wantsAttack);
                }
                if (IsDroneBuild()) {
                    // ドローンを機体の周りに等間隔で並べる角度（ラジアン）。
                    const float orbit = static_cast<float>(companionIndex) * 2.0f * std::numbers::pi_v<float> /
                                        static_cast<float>((std::max)(size_t{1}, drones_.size()));
                    const float radius =
                        GetCombatStyleProfile(tankbuild::Style::Drone).droneFormationRadius + (runModifiers_.droneLaserLink ? 2.0f : 0.0f);
                    drone->SetRunFollowOffset({std::cos(orbit) * radius, std::sin(orbit) * radius, 0});
                }
            }
            ++companionIndex;
            drone->SetNeonDepthAimEnabled(neonDepthAimEnabled_);
            drone->Update(viewProjection, stage, worldTransform_.translate, runModifiers_.enabled ? deltaTime : 1.0f / 60.0f);
        }
        if (runModifiers_.enabled && runCheckpointEvolution_ && bulletManager->GetBulletCount() > bulletsBeforeDroneUpdates) {
            primaryAttackPerformedEvent_ = true;
            ++primaryAttackCount_;
        }

        drones_.erase(std::remove_if(drones_.begin(), drones_.end(),
                                     [](const std::unique_ptr<PlayerDrone>& drone) {
                                         return drone->IsDead();
                                     }),
                      drones_.end());
        if (IsDroneBuild() && drones_.size() < static_cast<size_t>(GetExpeditionDroneLimit()) && runSupportDroneTimer_ <= 0.0f) {
            runSupportDroneTimer_ = 1.2f;
        }
        UpdateRunProjectiles(bulletManager, deltaTime);
    }

    ui_->machineGunBtnSprite_->Update();

    if (hp_ <= 0) {
        Die();
    }
    if (isExploding_) {
        UpdateParticles(deltaTime);
    }
}

void Player::Draw(bool drawBody)
{
    // ドローンの描画
    for (auto& drone : drones_) {
        if (!drone->UsesNeonVisual()) {
            drone->Draw();
        }
    }

    if (drawBody) {
        DrawBodyOnly();
    }
}

void Player::DrawBodyOnly()
{
    if (isDead_) {
        if (deathChargeTimer_ <= 0.0f) {
            return;
        }

        const float progress = 1.0f - deathChargeTimer_ / deathChargeDuration_;
        const float charge = progress * progress;
        myEngine::Transform chargeTransform = worldTransform_;
        chargeTransform.scale = worldTransform_.scale * (1.0f + charge * 0.65f);
        const myEngine::Vector4 savedColor = object_->GetColor();
        const bool savedLighting = object_->IsLightingEnabled();
        object_->SetTransform(chargeTransform);
        object_->SetLighting(false);
        object_->SetColor({2.4f, 2.4f, 2.4f, 1.0f});
        object_->Update();
        object_->Draw();
        object_->SetTransform(worldTransform_);
        object_->SetColor(savedColor);
        object_->SetLighting(savedLighting);
        object_->Update();
        return;
    }

    if (!isDead_) {
        // 無敵時間中は点滅

        if (invincibleTimer_ > 0.0f && !isJustEvaded_) {
            if (static_cast<int>(invincibleTimer_ * 10) % 5 == 0) {
                // 透明度を下げる
                if (isStealth_) {
                    SetVehicleAlpha(0.2f);
                } else {
                    SetVehicleAlpha(0.5f);
                }
                if (damageFeedbackTimer_ > 0.0f) {
                    myEngine::Transform drawTransform = worldTransform_;
                    const float t = damageFeedbackTimer_ / damageFeedbackDuration_;
                    const float pulse = std::sin(t * 3.14159265f) * 0.10f;
                    drawTransform.scale = worldTransform_.scale * (1.0f + pulse);
                    object_->SetTransform(drawTransform);
                    object_->Update();
                }
                object_->Draw();
                DrawBarrels();
                return;
            }
        }
        if (!isStealth_) {
            SetVehicleAlpha(1.0f);
        }
        if (damageFeedbackTimer_ > 0.0f) {
            myEngine::Transform drawTransform = worldTransform_;
            const float t = damageFeedbackTimer_ / damageFeedbackDuration_;
            const float pulse = std::sin(t * 3.14159265f) * 0.10f;
            drawTransform.scale = worldTransform_.scale * (1.0f + pulse);
            object_->SetTransform(drawTransform);
            object_->Update();
        }
        object_->Draw();
        DrawBarrels();
    }
}

void Player::DrawSprite()
{
    myEngine::SpriteCommon::GetInstance()->PreDraw(myEngine::kNormal);
    playerHud_->DrawUpgradeHud();
}

std::vector<PlayerDrone*> Player::GetDronePtrs() const
{
    std::vector<PlayerDrone*> result;
    result.reserve(drones_.size());
    for (const auto& d : drones_) {
        result.push_back(d.get());
    }
    return result;
}

myEngine::Vector3 Player::GetWorldPosition() const
{
    // ワールド座標を入れる
    myEngine::Vector3 worldPos;
    // ワールド行列の平行移動成分を取得(ワールド座標)
    worldPos.x = worldTransform_.translate.x;
    worldPos.y = worldTransform_.translate.y;
    worldPos.z = worldTransform_.translate.z;

    return worldPos;
}

bool Player::TryPerfectDodgeFromHostileContact(bool dodgeable)
{
    if (!isJustEvaded_ && isDashing_ &&
        TankCanPerfectDodge(runModifiers_.enabled && runModifiers_.perfectDodge, dodgeable, kDashDuration - dashTimer_,
                            TankEffectPower(runModifiers_, tankrun::CardId::PerfectDodge))) {
        requestSlow_ = true;
        isJustEvaded_ = true;
        ++perfectDodgeCount_;
        invincibleTimer_ = dashTimer_;
        buffTimer_ = kBuffDuration * TankEffectPower(runModifiers_, tankrun::CardId::PerfectDodge);
        if (runModifiers_.enabled && runModifiers_.capacitor) {
            const float power = TankEffectPower(runModifiers_, tankrun::CardId::Capacitor);
            buffTimer_ += 1.5f * power;
            stats_.stamina = (std::min)(stats_.maxStamina, stats_.stamina + power);
            HealRunPlayer((std::max)(1, static_cast<int>(std::round(4.0f * power))));
        }
        if (runModifiers_.enabled && runModifiers_.overdrive) {
            runOverdriveTimer_ = 2.4f * TankEffectPower(runModifiers_, tankrun::CardId::Overdrive);
            runOverdriveCooldown_ = 4.0f;
        }
        isBuffActive_ = !runCheckpointEvolution_ || runModifiers_.capacitor;
        maxCharge_ = 5.0f;
        return true;
    }

    return false;
}

void Player::OnCollision(Collider* other)
{
    if (const auto* resource = dynamic_cast<const ExpEnemy*>(other); resource && resource->IsRunResource()) {
        return;
    }

    if (currentClass_ == ClassType::Assassin) {
        isStealth_ = false;
        stealthTimer_ = 0.0f;
    }

    if ((invincibleTimer_ > 0.0f && !isDashing_) || isJustEvaded_ || isSmash_) {
        // 無敵中に敵の弾を受けてもノックバックしない
        if (other->GetCollisionAttribute() == kCollisionAttributeEnemyBullet) {
            return;
        }
    }

    if (TryPerfectDodgeFromHostileContact(other->GetCollisionAttribute() == kCollisionAttributeEnemyBullet)) {
        return;
    }

    myEngine::Vector3 hitDir = worldTransform_.translate - other->GetWorldPosition();

    if (myEngine::Length(hitDir) < 0.0001f) {
        // 押し戻す方向が定まらない場合は、後続の被ダメージ処理にも進まない。
        return;
    }

    hitDir = myEngine::Normalize(hitDir);

    const float kKnockBackPower = 0.15f;

    velocity_ += hitDir * kKnockBackPower * other->GetHitPower() * (dt_ * 60.0f);
    const float maxKnockSpeed = isDashing_ ? 0.32f : 0.20f;
    if (myEngine::Length(velocity_) > maxKnockSpeed) {
        velocity_ = myEngine::Normalize(velocity_) * maxKnockSpeed;
    }

    if (other->GetCollisionAttribute() == kCollisionAttributeEnemy || other->GetCollisionAttribute() == kCollisionAttributeExpEnemy ||
        other->GetCollisionAttribute() == kCollisionAttributeEnemyBullet) {
        TakeDamage(other->GetDamage(), 0.45f);
    }
}

uint32_t Player::ReceiveHostileHazard(uint32_t amount, bool dodgeable)
{
    if (isDead_ || amount == 0) {
        return 0;
    }
    if (currentClass_ == ClassType::Assassin) {
        isStealth_ = false;
        stealthTimer_ = 0.0f;
    }
    if ((invincibleTimer_ > 0.0f && !isDashing_) || isJustEvaded_ || isSmash_) {
        return 0;
    }
    if (TryPerfectDodgeFromHostileContact(dodgeable)) {
        return 0;
    }
    const int before = hp_;
    TakeDamage(amount, 0.45f);
    return static_cast<uint32_t>((std::max)(0, before - hp_));
}

bool Player::TryDashImpact(Collider* target)
{
    if (!target || isDead_ || !isDashing_) {
        return false;
    }
    auto* regular = dynamic_cast<ExpEnemy*>(target);
    auto* boss = dynamic_cast<Enemy*>(target);
    if ((!regular && !boss) || (regular && (regular->IsDead() || regular->IsRunResource())) ||
        (boss && (!boss->IsRunEncounterEnabled() || boss->IsDead()))) {
        return false;
    }
    const uint64_t id = target->GetCollisionId();
    if (std::find(dashImpactTargets_.begin(), dashImpactTargets_.end(), id) != dashImpactTargets_.end()) {
        // このダッシュで処理済みなら、受付時間を過ぎてもtrueを返して通常の双方通知を抑える。
        return true;
    }
    if (kDashDuration - dashTimer_ > kJustEvadeWindow) {
        return false;
    }
    dashImpactTargets_.push_back(id);
    myEngine::Vector3 direction = target->GetWorldPosition() - GetWorldPosition();
    if (myEngine::Length(direction) < 0.001f) {
        direction = velocity_;
    }
    if (myEngine::Length(direction) < 0.001f) {
        direction = {1, 0, 0};
    }
    direction = myEngine::Normalize(direction);
    const bool powered = runModifiers_.enabled && runModifiers_.impactDrive;
    const float strength = powered ? 1.0f + 0.5f * TankEffectPower(runModifiers_, tankrun::CardId::ImpactDrive) : 1.0f;
    const uint32_t damage =
        static_cast<uint32_t>((std::max)(1.0f, std::round((stats_.bulletDamage * 2.5f + stats_.bodyDamage) * strength)));
    // 速度だけを与え、位置の更新と壁への補正は各敵の通常移動に任せる。
    if (regular) {
        regular->ApplyKnockback(direction, 0.62f * strength);
        ArmWallSmash(regular, strength);
        regular->TakeDirectionalDamage(damage, GetWorldPosition(), true);
    }
    if (boss) {
        boss->ApplyKnockback(direction, 0.62f * strength);
        boss->TakeDamage(damage);
    }
    pendingDashImpacts_.push_back({target->GetWorldPosition(), direction, boss != nullptr, powered});
    velocity_ = velocity_ * 0.72f;
    // 体当たりの成立はジャスト回避とは独立しており、回避状態・スロー要求・弾への無敵は付与しない。
    return true;
}

std::vector<Player::DashImpactEvent> Player::ConsumeDashImpactEvents()
{
    auto events = std::move(pendingDashImpacts_);
    pendingDashImpacts_.clear();
    return events;
}

myEngine::AABB Player::GetAABB()
{
    myEngine::Vector3 worldPos = GetWorldPosition();

    myEngine::AABB aabb;

    aabb.min = {worldPos.x - kWidth / 2.0f, worldPos.y - kHeight / 2.0f, worldPos.z - kWidth / 2.0f};
    aabb.max = {worldPos.x + kWidth / 2.0f, worldPos.y + kHeight / 2.0f, worldPos.z + kWidth / 2.0f};

    return aabb;
}

void Player::Damage(uint32_t amount)
{
    TakeDamage(amount, 0.45f);
}

void Player::TakeDamage(uint32_t amount, float invincibleTime)
{
    if (isDead_ || amount == 0 || invincibleTimer_ > 0.0f || debugNoDamage_) {
        return;
    }
    if (saberCounterTimer_ > 0.0f) {
        // 受付中の反撃はHP減少と被ダメージ回数の記録より先に処理する。
        const PlayerClassConfig* config = GetCurrentClassConfig();
        if (config && config->specialActionId == "saber_counter") {
            TriggerSaberCounter(*config);
            return;
        }
    }

    const uint32_t appliedDamage = runModifiers_.enabled && runCheckpointEvolution_ ? runMaintenance_.MitigateDamage(amount) : amount;
    ++damageTakenCount_;
    // 符号付きHPへ変換する前に残HPで制限し、極端な開発用ダメージでも整数変換を安全にする。
    hp_ -= static_cast<int>((std::min)(appliedDamage, static_cast<uint32_t>((std::max)(0, hp_))));
    if (hp_ < 0) {
        hp_ = 0;
    }

    TriggerDamageFeedback();
    invincibleTimer_ = invincibleTime;
    if (hp_ <= 0) {
        Die();
    }
}

std::vector<Player::SpecialCombatEvent> Player::ConsumeSpecialCombatEvents()
{
    auto events = std::move(pendingSpecialCombatEvents_);
    pendingSpecialCombatEvents_.clear();
    return events;
}

void Player::Die()
{
    if (isDead_) {
        return;
    }

    isDead_ = true;
    isExploding_ = true;

    SpawnParticles();

    // 生成済みの弾はここでは消去しない。弾の消去はBulletManager側で行う。
}

bool Player::isFinished()
{
    if (isDead_ && !isExploding_) {
        return true;
    }
    return false;
}

void Player::UpdateDefeatPresentation(float deltaTime)
{
    if (isExploding_) {
        UpdateParticles((std::max)(0.0f, deltaTime));
    }
}

bool Player::ConsumeEvolutionConfirmed()
{
    const bool confirmed = evolutionConfirmedEvent_;
    evolutionConfirmedEvent_ = false;
    return confirmed;
}

void Player::CloseEvolutionUiForTutorial()
{
    isChangeMode_ = false;
}

bool Player::ConsumeEvolutionCancelled()
{
    const bool cancelled = evolutionCancelledEvent_;
    evolutionCancelledEvent_ = false;
    return cancelled;
}

bool Player::GetTankButtonVisualData(const std::string& classId, TankButtonVisualData& output) const
{
    const PlayerClassConfig* config = GetClassConfig(classId);
    if (!config) {
        return false;
    }
    output = {};
    output.classId = config->id;
    output.rank = (std::clamp)(config->requiredRank, 1, 4);
    output.bodyScale = config->bodyScale;
    output.bodyFillColor = config->bodyFillColor;
    output.bodyOutlineColor = config->bodyOutlineColor;
    output.weaponMounts = config->barrels;
    output.usesDrone = config->usesDrone;
    switch (config->bodyShape) {
    case BodyShape::Box:
        output.bodyShape = TankButtonBodyShape::Box;
        break;
    case BodyShape::Triangle:
        output.bodyShape = TankButtonBodyShape::Triangle;
        break;
    case BodyShape::Pentagon:
        output.bodyShape = TankButtonBodyShape::Pentagon;
        break;
    case BodyShape::Circle:
    default:
        output.bodyShape = TankButtonBodyShape::Circle;
        break;
    }
    if (classId == "Twin") {
        output.hiraganaName = "ついん";
    } else if (classId == "MachineGun") {
        output.hiraganaName = "ましんがん";
    } else if (classId == "Overseer") {
        output.hiraganaName = "おーばーしあ";
    } else if (classId == "Basic") {
        output.hiraganaName = "べーしっく";
    } else {
        output.hiraganaName = config->displayName;
    }
    return true;
}

std::vector<Player::LaserShotEvent> Player::ConsumeLaserShotEvents()
{
    std::vector<LaserShotEvent> events = std::move(pendingLaserShots_);
    pendingLaserShots_.clear();
    return events;
}

std::vector<Player::MineDropEvent> Player::ConsumeMineDropEvents()
{
    std::vector<MineDropEvent> events = std::move(pendingMineDrops_);
    pendingMineDrops_.clear();
    return events;
}

std::vector<Player::MeleeSlashEvent> Player::ConsumeMeleeSlashEvents()
{
    std::vector<MeleeSlashEvent> events = std::move(pendingMeleeSlashes_);
    pendingMeleeSlashes_.clear();
    return events;
}

myEngine::Vector3 Player::RotateDirection(const myEngine::Vector3& direction, float angleDeg) const
{
    const float rad = angleDeg * 3.1415926535f / 180.0f;
    return {direction.x * cosf(rad) - direction.y * sinf(rad), direction.x * sinf(rad) + direction.y * cosf(rad), direction.z};
}

void Player::InitializeBarrels()
{
    barrels_.clear();
    const PlayerClassConfig* config = GetCurrentClassConfig();
    const size_t barrelCount = config ? config->barrels.size() : 1u;
    barrels_.reserve(barrelCount);
    for (size_t i = 0; i < barrelCount; ++i) {
        BarrelModel barrel{};
        barrel.object = std::make_unique<myEngine::Object3d>();
        barrel.object->Initialize();
        barrel.object->SetModel(config ? config->barrels[i].model : "gunBarrel.obj");
        barrel.object->SetColor(myEngine::Vector4(0.48f, 0.86f, 0.22f, 1.0f));
        baseBarrelColor_ = myEngine::Vector4(0.48f, 0.86f, 0.22f, 1.0f);
        barrel.transform = myEngine::InitWorldTransform();
        barrel.transform.scale = config ? config->barrels[i].scale : myEngine::Vector3{1.25f, 0.24f, 0.24f};
        barrels_.push_back(std::move(barrel));
    }
}

void Player::UpdateBarrelLayout()
{
    if (barrels_.empty()) {
        return;
    }

    const myEngine::Vector3 forward = myEngine::Length(dir_) > 0.0001f ? myEngine::Normalize(dir_) : myEngine::Vector3{1.0f, 0.0f, 0.0f};
    const myEngine::Vector3 right = {-forward.y, forward.x, 0.0f};
    const PlayerClassConfig* config = GetCurrentClassConfig();
    const float recoilReturn = 0.055f;

    for (size_t i = 0; i < barrels_.size(); ++i) {
        BarrelModel& barrel = barrels_[i];
        const WeaponMountConfig barrelConfig = (config && i < config->barrels.size()) ? config->barrels[i] : WeaponMountConfig{};
        const bool active = config ? i < config->barrels.size() : i == 0;

        barrel.recoilOffset = (std::max)(0.0f, barrel.recoilOffset - recoilReturn * dt_ * 60.0f);
        barrel.muzzleFlashTimer = (std::max)(0.0f, barrel.muzzleFlashTimer - dt_);
        barrel.localOffset = forward * (barrelConfig.offset.x - barrel.recoilOffset) + right * barrelConfig.offset.y +
                             myEngine::Vector3{0.0f, 0.0f, barrelConfig.offset.z};
        barrel.transform.translate = worldTransform_.translate + barrel.localOffset;
        barrel.transform.rotate = worldTransform_.rotate;
        barrel.transform.rotate.z += barrelConfig.angleDeg * 3.1415926535f / 180.0f;
        barrel.transform.scale = active ? barrelConfig.scale : myEngine::Vector3{0.0f, 0.0f, 0.0f};
        barrel.object->SetTransform(barrel.transform);
        barrel.object->Update();
    }
}

void Player::DrawBarrels()
{
    const PlayerClassConfig* config = GetCurrentClassConfig();
    const size_t activeCount = config ? config->barrels.size() : 1u;
    for (size_t i = 0; i < barrels_.size() && i < activeCount; ++i) {
        barrels_[i].object->Draw();
    }
}

void Player::SetVehicleAlpha(float alpha)
{
    float flash = 0.0f;
    if (damageFeedbackTimer_ > 0.0f && damageFeedbackDuration_ > 0.0f) {
        const float t = damageFeedbackTimer_ / damageFeedbackDuration_;
        flash = t * t * 0.75f;
    }
    myEngine::Vector4 vehicleColor = LerpColor(baseVehicleColor_, {1.0f, 1.0f, 1.0f, baseVehicleColor_.w}, flash);
    vehicleColor.w = alpha;
    object_->SetColor(vehicleColor);
    for (BarrelModel& barrel : barrels_) {
        if (barrel.object) {
            myEngine::Vector4 barrelColor = LerpColor(baseBarrelColor_, {1.0f, 1.0f, 1.0f, baseBarrelColor_.w}, flash);
            barrelColor.w = alpha;
            barrel.object->SetColor(barrelColor);
        }
    }
}

void Player::TriggerDamageFeedback()
{
    damageFeedbackTimer_ = damageFeedbackDuration_;
}

void Player::SpawnParticles()
{
    myEngine::Vector3 center = GetWorldPosition();
    myEngine::ParticleManager::GetInstance()->EmitNeonDeathEffect(center, {1.45f, 1.30f, 0.72f, 1.0f}, {0.08f, 1.25f, 1.55f, 0.0f}, 1.0f);
    deathChargeTimer_ = deathChargeDuration_;
    deathEffectTimer_ = 1.85f;
}

void Player::UpdateParticles(float deltaTime)
{
    deathChargeTimer_ = (std::max)(0.0f, deathChargeTimer_ - deltaTime);
    deathEffectTimer_ -= deltaTime;
    if (deathEffectTimer_ <= 0.0f) {
        isExploding_ = false;
    }
}

bool Player::ConsumeStatUpgradePerformedEvent()
{
    const bool performed = statUpgradePerformedEvent_;
    statUpgradePerformedEvent_ = false;
    return performed;
}

void Player::UpdateStealth(float deltaTime)
{
    // Assassinの仕様: しばらく経つとステルス
    if (currentClass_ == ClassType::Assassin) {
        // 移動入力があるか、攻撃しているかをチェック
        if (myEngine::Length(velocity_) > 0.1f || input_->IsPress(input_->GetMouseState().rgbButtons[0])) {
            stealthTimer_ = 0.0f;
            isStealth_ = false;
        } else {
            stealthTimer_ += deltaTime;
            if (stealthTimer_ >= 2.0f) { // 2秒静止でステルス
                isStealth_ = true;
            }
        }
    }

    // Ninjaの仕様: ダッシュまたはジャスト回避でステルス
    if (currentClass_ == ClassType::Ninja) {
        if (isDashing_ || isJustEvaded_) {
            isStealth_ = true;
            stealthTimer_ = 1.5f; // 1.5秒間持続
        }

        if (stealthTimer_ > 0) {
            stealthTimer_ -= deltaTime;
        } else {
            isStealth_ = false;
        }
    }

    // アルファ値の適用 (描画時に反映させる)
    float targetAlpha = isStealth_ ? 0.2f : 1.0f;
    stealthAlpha_ = myEngine::Lerp(stealthAlpha_, targetAlpha, 0.1f);
    // 実際のモデルのカラーに適用
    object_->SetAlpha(stealthAlpha_);
    for (BarrelModel& barrel : barrels_) {
        if (barrel.object) {
            barrel.object->SetAlpha(stealthAlpha_);
        }
    }
}

void Player::SpawnCasing()
{
    myEngine::ParticleManager::GetInstance()->Emit("CasingSpark", GetWorldPosition() + dir_ * 1.0f, 2);
}

void Player::SpawnAfterimage()
{
    myEngine::Vector3 position = GetWorldPosition();
    myEngine::Vector3 moveDirection = velocity_;
    moveDirection.z = 0.0f;
    if (myEngine::Length(moveDirection) > 0.001f) {
        position -= myEngine::Normalize(moveDirection) * 0.65f;
    }
    myEngine::ParticleManager::GetInstance()->Emit("DashDust", position, 1);
    myEngine::ParticleManager::GetInstance()->EmitNeonMovementEffect(position, moveDirection);
}

std::vector<Player::NeonBarrelLayout> Player::GetNeonBarrelLayouts() const
{
    std::vector<NeonBarrelLayout> layouts;
    if (IsDroneBuild()) {
        return layouts;
    }
    if (IsMeleeBuild()) {
        NeonBarrelLayout blade{};
        blade.isMelee = true;
        blade.offset = {0.60f, -0.40f, 0.0f};
        blade.scale = {runModifiers_.bladeReach ? 2.4f : 1.85f, 0.22f, 0.22f};
        blade.angleRad = -0.22f;
        blade.barrelColor = {0.18f, 1.35f, 1.70f, 1.0f};
        blade.outlineColor = {0.65f, 1.6f, 1.75f, 1.0f};
        layouts.push_back(blade);
        return layouts;
    }
    const PlayerClassConfig* config = GetCurrentClassConfig();
    if (!config) {
        NeonBarrelLayout layout{};
        if (!barrels_.empty()) {
            layout.muzzleFlashRatio =
                kMuzzleFlashDuration > 0.0f ? (std::clamp)(barrels_[0].muzzleFlashTimer / kMuzzleFlashDuration, 0.0f, 1.0f) : 0.0f;
        }
        layouts.push_back(layout);
        return layouts;
    }

    layouts.reserve(config->barrels.size());
    for (size_t i = 0; i < config->barrels.size(); ++i) {
        const WeaponMountConfig& barrel = config->barrels[i];
        NeonBarrelLayout layout{};
        layout.offset = barrel.offset;
        layout.scale = barrel.scale;
        layout.angleRad = barrel.angleDeg * 3.1415926535f / 180.0f;
        layout.isMelee = barrel.weaponType == WeaponType::Melee;
        layout.shape = barrel.barrelShape;
        layout.fireGroup = barrel.fireGroup;
        layout.barrelColor = barrel.barrelColor;
        layout.outlineColor = barrel.outlineColor;
        if (i < barrels_.size()) {
            layout.recoilOffset = barrels_[i].recoilOffset;
            layout.muzzleFlashRatio =
                kMuzzleFlashDuration > 0.0f ? (std::clamp)(barrels_[i].muzzleFlashTimer / kMuzzleFlashDuration, 0.0f, 1.0f) : 0.0f;
        }
        layouts.push_back(layout);
    }
    return layouts;
}

Player::NeonBodyLayout Player::GetNeonBodyLayout() const
{
    NeonBodyLayout layout{};
    const PlayerClassConfig* config = GetCurrentClassConfig();
    if (!config) {
        return layout;
    }
    layout.shape = config->bodyShape;
    layout.scale = config->bodyScale;
    layout.fillColor = config->bodyFillColor;
    layout.outlineColor = config->bodyOutlineColor;
    return layout;
}

float Player::GetDamageFeedbackRatio() const
{
    if (damageFeedbackDuration_ <= 0.0f) {
        return 0.0f;
    }
    return (std::clamp)(damageFeedbackTimer_ / damageFeedbackDuration_, 0.0f, 1.0f);
}

void Player::SpawnBuffParticle()
{
    myEngine::ParticleManager::GetInstance()->Emit("DashDust", GetWorldPosition(), 1);
}

myEngine::Vector2 Player::WorldToScreen(const myEngine::Vector3& worldPos, myEngine::Camera* camera)
{
    // 1. ビュープロジェクション行列で変換

    myEngine::Matrix4x4 matVP = camera->GetViewMatrix() * camera->GetProjectionMatrix();
    myEngine::Vector3 ndcPos = myEngine::TransformMatrix(worldPos, matVP);

    // 2. NDC座標 (-1.0 ~ 1.0) をスクリーン座標 (0 ~ ウィンドウ幅/高) に変換
    // ※ WinAppなどのシングルトンから画面サイズを取得してください
    float screenX = (ndcPos.x + 1.0f) * 0.5f * myEngine::WinApp::kClientWidth;
    float screenY = (1.0f - ndcPos.y) * 0.5f * myEngine::WinApp::kClientHeight;

    return {screenX, screenY};
}

Player::Player()
    : ui_(std::make_unique<PlayerUiState>()), playerHud_(std::make_unique<PlayerHud>(*this, *ui_)),
      playerEvolution_(std::make_unique<PlayerEvolution>(*this, *ui_)),
      playerClassEditor_(std::make_unique<PlayerClassEditor>(*this, *ui_)), playerWeapons_(std::make_unique<PlayerWeapons>(*this)),
      playerProgression_(std::make_unique<PlayerProgression>(*this))
{
}

void Player::DrawEvolutionAfterPostEffects()
{
    playerEvolution_->DrawEvolutionAfterPostEffects();
}

void Player::DrawUpgradeHudAfterPostEffects()
{
    playerHud_->DrawUpgradeHudAfterPostEffects();
}

void Player::AppendGameplayNeonTextLabels(std::vector<myEngine::TextLabel*>& labels) const
{
    playerHud_->AppendGameplayNeonTextLabels(labels);
}

void Player::PrepareUpgradeHudTextTextures()
{
    playerHud_->PrepareUpgradeHudTextTextures();
}

void Player::InitializeEncyclopedia()
{
    playerEvolution_->InitializeEncyclopedia();
}

void Player::UpdateEncyclopedia(float uiDeltaTime)
{
    playerEvolution_->UpdateEncyclopedia(uiDeltaTime);
}

void Player::DrawEncyclopedia()
{
    playerEvolution_->DrawEncyclopedia();
}

void Player::DrawTankCodex()
{
    playerEvolution_->DrawTankCodex();
}

void Player::DrawPlayerClassEditor()
{
    playerClassEditor_->DrawPlayerClassEditor();
}

void Player::DrawUpgradeHudDebugImGui()
{
    playerHud_->DrawUpgradeHudDebugImGui();
}

void Player::DrawEvolutionUiStyleEditor()
{
    playerEvolution_->DrawEvolutionUiStyleEditor();
}

const Player::UiProfileStats& Player::GetUpgradeHudProfileStats() const
{
    return playerHud_->GetUpgradeHudProfileStats();
}

const Player::UiProfileStats& Player::GetEvolutionUiProfileStats() const
{
    return playerEvolution_->GetEvolutionUiProfileStats();
}

#if defined(USE_IMGUI) && !defined(NDEBUG)
Player::UpgradeHudDebugSnapshot Player::GetUpgradeHudDebugSnapshot() const
{
    return playerHud_->GetUpgradeHudDebugSnapshot();
}
#endif

void Player::Attack(BulletManager* bulletManager, float deltaTime)
{
    playerWeapons_->Attack(bulletManager, deltaTime);
}

void Player::DroneShoot(BulletManager* bulletManager)
{
    playerWeapons_->DroneShoot(bulletManager);
}

void Player::Smash(float deltaTime)
{
    playerWeapons_->Smash(deltaTime);
}

void Player::ApplyRunProjectileRules(AttackParam& param, bool applyFan) const
{
    playerWeapons_->ApplyRunProjectileRules(param, applyFan);
}

float Player::GetRunFireIntervalScale() const
{
    return playerWeapons_->GetRunFireIntervalScale();
}

myEngine::Vector3 Player::GetRailChargeMuzzle() const
{
    return playerWeapons_->GetRailChargeMuzzle();
}

void Player::AttackRailCannon(BulletManager* bullets, bool pressed, float dt)
{
    playerWeapons_->AttackRailCannon(bullets, pressed, dt);
}

void Player::UpdateSpecialCombat(Stage& stage, BulletManager* bullets, Enemy* boss, EnemyManager* enemies, float dt)
{
    playerWeapons_->UpdateSpecialCombat(stage, bullets, boss, enemies, dt);
}

void Player::SetRunHomingTargets(const std::vector<myEngine::Vector3>& targets)
{
    playerWeapons_->SetRunHomingTargets(targets);
}

void Player::UpdateRunProjectiles(BulletManager* bulletManager, float deltaTime)
{
    playerWeapons_->UpdateRunProjectiles(bulletManager, deltaTime);
}

void Player::ConfigureRunDrone(PlayerDrone& drone) const
{
    playerWeapons_->ConfigureRunDrone(drone);
}

bool Player::TryActivateSpecialAction()
{
    return playerWeapons_->TryActivateSpecialAction();
}

bool Player::ActivatePerfectDodge(const PlayerClassConfig& config)
{
    return playerWeapons_->ActivatePerfectDodge(config);
}

bool Player::ActivateSaberCounter(const PlayerClassConfig& config)
{
    return playerWeapons_->ActivateSaberCounter(config);
}

void Player::TriggerSaberCounter(const PlayerClassConfig& config)
{
    playerWeapons_->TriggerSaberCounter(config);
}

bool Player::FireConfiguredClass(const PlayerClassConfig& config, BulletManager* bulletManager, float baseReload,
                                 myEngine::Vector3& recoilDir, float& recoilPower)
{
    return playerWeapons_->FireConfiguredClass(config, bulletManager, baseReload, recoilDir, recoilPower);
}

void Player::RefreshAdditiveArmaments()
{
    playerWeapons_->RefreshAdditiveArmaments();
}

void Player::ResetAdditionalAbilities()
{
    playerWeapons_->ResetAdditionalAbilities();
}

bool Player::TryStartSpinBlade(bool pressed)
{
    return playerWeapons_->TryStartSpinBlade(pressed);
}

uint32_t Player::NotifyDroneHit(int drone, Collider* target, uint32_t originalDamage)
{
    return playerWeapons_->NotifyDroneHit(drone, target, originalDamage);
}

float Player::GetDroneTargetDamageScale(const Collider* target, bool boss) const
{
    return playerWeapons_->GetDroneTargetDamageScale(target, boss);
}

void Player::ArmWallSmash(ExpEnemy* target, float strength)
{
    playerWeapons_->ArmWallSmash(target, strength);
}

void Player::UpdateAdditionalAbilities(Stage& stage, BulletManager* bullets, Enemy* boss, EnemyManager* enemies, float dt)
{
    playerWeapons_->UpdateAdditionalAbilities(stage, bullets, boss, enemies, dt);
}

void Player::AddExp(int amount)
{
    playerProgression_->AddExp(amount);
}

void Player::SetRunCurrencyMode(bool enabled)
{
    playerProgression_->SetRunCurrencyMode(enabled);
}

void Player::InstallRunAuthoredClasses(const tankcontent::Catalog& catalog)
{
    playerProgression_->InstallRunAuthoredClasses(catalog);
}

std::vector<RunEvolutionChoice> Player::GetRunAuthoredEvolutionChoices() const
{
    return playerProgression_->GetRunAuthoredEvolutionChoices();
}

bool Player::ChooseRunAuthoredClass(const std::string& id)
{
    return playerProgression_->ChooseRunAuthoredClass(id);
}

void Player::ApplyBalanceConfig(const BalanceConfig& config)
{
    playerProgression_->ApplyBalanceConfig(config);
}

void Player::SetRunModifiers(const TankRunModifiers& modifiers)
{
    playerProgression_->SetRunModifiers(modifiers);
}

void Player::ApplyCombatStyleBalance(const TankCombatStyleBalances& profiles)
{
    playerProgression_->ApplyCombatStyleBalance(profiles);
}

float Player::GetRunBaseReloadFrames() const
{
    return playerProgression_->GetRunBaseReloadFrames();
}

bool Player::SetExpeditionCombatStyle(tankbuild::Style style)
{
    return playerProgression_->SetExpeditionCombatStyle(style);
}

int Player::GetExpeditionDroneLimit() const
{
    return playerProgression_->GetExpeditionDroneLimit();
}

void Player::EnsureExpeditionDrones()
{
    playerProgression_->EnsureExpeditionDrones();
}

void Player::ConfigurePrototypeLoadout(int archetype)
{
    playerProgression_->ConfigurePrototypeLoadout(archetype);
}

void Player::HealRunPlayer(int amount)
{
    playerProgression_->HealRunPlayer(amount);
}

bool Player::SpendRunHealth(int amount)
{
    return playerProgression_->SpendRunHealth(amount);
}

std::vector<RunEvolutionChoice> Player::GetRunEvolutionChoices() const
{
    return playerProgression_->GetRunEvolutionChoices();
}

void Player::PrepareRunEvolution()
{
    playerProgression_->PrepareRunEvolution();
}

bool Player::ChooseRunEvolution(const std::string& id)
{
    return playerProgression_->ChooseRunEvolution(id);
}

bool Player::AwardRunMaintenancePoint(int clearedRoom)
{
    return playerProgression_->AwardRunMaintenancePoint(clearedRoom);
}

std::array<RunMaintenanceChoice, 3> Player::GetRunMaintenanceChoices() const
{
    return playerProgression_->GetRunMaintenanceChoices();
}

bool Player::SpendRunMaintenancePoint(int stat)
{
    return playerProgression_->SpendRunMaintenancePoint(stat);
}

bool Player::RefundRunMaintenancePoint(int stat)
{
    return playerProgression_->RefundRunMaintenancePoint(stat);
}

void Player::ResetRunRoomState(const myEngine::Vector3& position)
{
    playerProgression_->ResetRunRoomState(position);
}

Player::RunCombatSnapshot Player::GetRunCombatSnapshot() const
{
    return playerProgression_->GetRunCombatSnapshot();
}

int Player::GetNextLevelExp() const
{
    return playerProgression_->GetNextLevelExp();
}

void Player::Evolve(ClassType newClass)
{
    playerProgression_->Evolve(newClass);
}

void Player::EvolveById(const std::string& classId)
{
    playerProgression_->EvolveById(classId);
}

bool Player::IsRunCompatibleClass(const PlayerClassConfig& config) const
{
    return playerProgression_->IsRunCompatibleClass(config);
}

bool Player::IsEvolutionClassVisible(const std::string& classId) const
{
    return playerProgression_->IsEvolutionClassVisible(classId);
}

bool Player::CanEvolveTo(const std::string& classId) const
{
    return playerProgression_->CanEvolveTo(classId);
}

bool Player::HasEvolutionEdge(const std::string& from, const std::string& to) const
{
    return playerProgression_->HasEvolutionEdge(from, to);
}

bool Player::TryConfirmEvolutionById(const std::string& classId)
{
    return playerProgression_->TryConfirmEvolutionById(classId);
}

bool Player::LoadPlayerClassConfigs(const std::string& path)
{
    return playerProgression_->LoadPlayerClassConfigs(path);
}

bool Player::ReloadPlayerClassConfigs(const std::string& path)
{
    return playerProgression_->ReloadPlayerClassConfigs(path);
}

Player::PlayerClassConfig Player::CreateDefaultClassConfig(ClassType type) const
{
    return playerProgression_->CreateDefaultClassConfig(type);
}

void Player::SavePlayerClassConfigs(const std::string& path) const
{
    playerProgression_->SavePlayerClassConfigs(path);
}

const Player::PlayerClassConfig* Player::GetClassConfig(ClassType type) const
{
    return playerProgression_->GetClassConfig(type);
}

const Player::PlayerClassConfig* Player::GetClassConfig(const std::string& classId) const
{
    return playerProgression_->GetClassConfig(classId);
}

const Player::PlayerClassConfig* Player::GetCurrentClassConfig() const
{
    return playerProgression_->GetCurrentClassConfig();
}

Player::PlayerClassConfig* Player::GetMutableClassConfig(const std::string& classId)
{
    return playerProgression_->GetMutableClassConfig(classId);
}

int Player::GetRankFromLevel(int level) const
{
    return playerProgression_->GetRankFromLevel(level);
}

int Player::GetUpgradeLevel(int index) const
{
    return playerProgression_->GetUpgradeLevel(index);
}

const char* Player::GetCurrentClassName() const
{
    return playerProgression_->GetCurrentClassName();
}

bool Player::ApplyStatUpgrade(int index)
{
    return playerProgression_->ApplyStatUpgrade(index);
}

bool Player::RefundStatUpgrade(int index)
{
    return playerProgression_->RefundStatUpgrade(index);
}

void Player::RecalculateStatsFromBase(bool healToFull)
{
    playerProgression_->RecalculateStatsFromBase(healToFull);
}

std::vector<Player::DroneAbilityVisual> Player::GetDroneAbilityVisuals() const
{
    std::vector<DroneAbilityVisual> result;
    if (!IsDroneBuild()) {
        return result;
    }
    for (const auto& drone : drones_) {
        if (!drone || drone->IsDead()) {
            continue;
        }
        const auto& mission = drone->GetRunMission();
        if (mission.GetPhase() == tankspecial::DronePhase::Escort) {
            continue;
        }
        // 表示用の進行度。帰還にも突撃の制限時間を使うため、帰還の時間切れまでの完了率ではない。
        const float duration = mission.GetPhase() == tankspecial::DronePhase::Warning ? mission.GetWarningDuration()
                               : mission.GetPhase() == tankspecial::DronePhase::Rebuilding
                                   ? tankspecial::DroneMission::kRebuildSeconds
                                   : tankspecial::DroneMission::kChargingTimeoutSeconds;
        result.push_back({drone->GetWorldPosition(), drone->GetRunMissionTarget(), mission.GetPhase(),
                          (std::clamp)(mission.GetElapsed() / duration, 0.0f, 1.0f), mission.IsBomb()});
    }
    return result;
}
