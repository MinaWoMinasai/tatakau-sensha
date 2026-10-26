// Playerの描画と見た目（本体・砲塔・ネオン表示の配置・透明度・残像やパーティクル・機体の見た目の表示用データ）。
#include "game/player/combat/PlayerSystems.h"
#include "FixedTimestep.h"
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
} // namespace

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

    const myEngine::Vector3 forward = myEngine::NormalizeOr(dir_, myEngine::Vector3{1.0f, 0.0f, 0.0f});
    const myEngine::Vector3 right = {-forward.y, forward.x, 0.0f};
    const PlayerClassConfig* config = GetCurrentClassConfig();
    const float recoilReturn = 0.055f;

    for (size_t i = 0; i < barrels_.size(); ++i) {
        BarrelModel& barrel = barrels_[i];
        const WeaponMountConfig barrelConfig = (config && i < config->barrels.size()) ? config->barrels[i] : WeaponMountConfig{};
        const bool active = config ? i < config->barrels.size() : i == 0;

        barrel.recoilOffset = (std::max)(0.0f, barrel.recoilOffset - recoilReturn * dt_ * myEngine::kFixedStepsPerSecond);
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
