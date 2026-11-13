#include "game/run/session/SurvivorRunController.h"
#include "Calculation.h"
#include "game/render/neon/NeonGridRenderer.h"
#include "game/session/GameplaySystems.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

// 育つ戦車の試作のバリアとスタミナ（計画書 p3 の X3）。右クリックを押している間、自機の前に扇形のバリアを張り、敵の弾と寄ってくる敵を止める。

namespace gameplay {
namespace {
constexpr float kPi = 3.14159265f;

/// @brief 網目の線の1本（球の上の2点）。pole からの角度と、pole のまわりの向きで持つ。
struct NetSegment {
    float polarA, azimuthA, polarB, azimuthB;
};

/// @brief サッカーボール（切頂二十面体）の、五角形の中心を上にした半球の線。1本の辺を数本に分け、扇で切るときの形をきれいにする。
const std::vector<NetSegment>& BarrierNet()
{
    static const std::vector<NetSegment> net = [] {
        const float phi = (1.0f + std::sqrt(5.0f)) * 0.5f;
        // 3種類の点の、符号と順回りの並びの組み合わせで60頂点になる。
        std::vector<myEngine::Vector3> vertices;
        const std::array<myEngine::Vector3, 3> seeds{myEngine::Vector3{0.0f, 1.0f, 3.0f * phi}, myEngine::Vector3{1.0f, 2.0f + phi, 2.0f * phi},
                                                     myEngine::Vector3{phi, 2.0f, 2.0f * phi + 1.0f}};
        for (const auto& seed : seeds) {
            for (float sx : {1.0f, -1.0f}) {
                for (float sy : {1.0f, -1.0f}) {
                    for (float sz : {1.0f, -1.0f}) {
                        if ((seed.x == 0.0f && sx < 0.0f)) {
                            continue;
                        }
                        const myEngine::Vector3 p{seed.x * sx, seed.y * sy, seed.z * sz};
                        vertices.push_back(p);
                        vertices.push_back({p.y, p.z, p.x});
                        vertices.push_back({p.z, p.x, p.y});
                    }
                }
            }
        }
        // 五角形の中心（二十面体の頂点）を上にする座標軸。
        const myEngine::Vector3 pole = myEngine::Normalize(myEngine::Vector3{0.0f, 1.0f, phi});
        const myEngine::Vector3 axisX = myEngine::Normalize(myEngine::Cross(pole, myEngine::Vector3{1.0f, 0.0f, 0.0f}));
        const myEngine::Vector3 axisY = myEngine::Cross(pole, axisX);
        std::vector<NetSegment> segments;
        constexpr int kPieces = 5;
        auto toPolar = [&](const myEngine::Vector3& point, float& polar, float& azimuth) {
            const myEngine::Vector3 unit = myEngine::Normalize(point);
            polar = std::acos((std::clamp)(myEngine::Dot(unit, pole), -1.0f, 1.0f));
            azimuth = std::atan2(myEngine::Dot(unit, axisY), myEngine::Dot(unit, axisX));
        };
        for (size_t a = 0; a < vertices.size(); ++a) {
            for (size_t b = a + 1; b < vertices.size(); ++b) {
                // 辺の長さは2。
                if (std::abs(myEngine::Length(vertices[a] - vertices[b]) - 2.0f) > 0.01f) {
                    continue;
                }
                for (int piece = 0; piece < kPieces; ++piece) {
                    const float t0 = static_cast<float>(piece) / kPieces, t1 = static_cast<float>(piece + 1) / kPieces;
                    NetSegment segment{};
                    toPolar(vertices[a] + (vertices[b] - vertices[a]) * t0, segment.polarA, segment.azimuthA);
                    toPolar(vertices[a] + (vertices[b] - vertices[a]) * t1, segment.polarB, segment.azimuthB);
                    if (segment.polarA <= kPi * 0.5f && segment.polarB <= kPi * 0.5f) {
                        segments.push_back(segment);
                    }
                }
            }
        }
        return segments;
    }();
    return net;
}

/// @brief 角度の差を -π〜π にする。
float WrapAngle(float angle)
{
    while (angle > kPi) {
        angle -= kPi * 2.0f;
    }
    while (angle < -kPi) {
        angle += kPi * 2.0f;
    }
    return angle;
}
} // namespace

bool SurvivorRunController::IsInsideBarrier(const myEngine::Vector3& position, float margin) const
{
    const myEngine::Vector3 player = world_.resources.player_->GetWorldPosition();
    const myEngine::Vector3 delta{position.x - player.x, position.y - player.y, 0.0f};
    const float distance = myEngine::Length(delta);
    if (distance > config_.barrier.radius + margin) {
        return false;
    }
    if (distance < 0.001f) {
        return true;
    }
    const myEngine::Vector3 direction = world_.resources.player_->GetDirection();
    const float facing = std::atan2(direction.y, direction.x);
    return std::abs(WrapAngle(std::atan2(delta.y, delta.x) - facing)) <= config_.barrier.arcDegrees * 0.5f * kPi / 180.0f;
}

void SurvivorRunController::UpdateBarrier(float dt, bool wanted)
{
    const auto& barrier = config_.barrier;
    barrierExhausted_ = (std::max)(0.0f, barrierExhausted_ - dt);
    barrierFlash_ = (std::max)(0.0f, barrierFlash_ - dt * 4.0f);
    barrierAge_ += dt;
    // 尽きたら、ボタンを一度放すまで張り直さない（押し続けて張ったり消えたりしないように）。
    if (!wanted) {
        barrierAwaitRelease_ = false;
    }
    barrierOn_ = wanted && !barrierAwaitRelease_ && barrierExhausted_ <= 0.0f && barrierStamina_ > 0.0f;

    // 機体の技の強化（K7）：バリアの強さ（power）でスタミナの減り方を割る。3段目は弾を止めると Q のゲージが溜まる。
    const survivor::AbilityDefinition* heroBarrier = HeroMode() ? FindAbility(survivor::AbilityKind::Barrier) : nullptr;
    const float staminaScale = heroBarrier ? 1.0f / (std::max)(0.1f, heroBarrier->power) : 1.0f;
    if (barrierOn_) {
        barrierRegenWait_ = barrier.regenDelay;
        barrierStamina_ -= barrier.drainPerSecond * staminaScale * dt;
        barrierSeconds_ += dt;
        auto& player = *world_.resources.player_;
        const myEngine::Vector3 center = player.GetWorldPosition();
        // 敵の弾：入ってきたら消し、スタミナを減らす。
        for (Bullet* bullet : world_.resources.bulletManager_->GetBulletPtrs()) {
            if (!bullet || bullet->IsDead() || bullet->GetOwner() == kPlayer || !IsInsideBarrier(bullet->GetWorldPosition(), bullet->GetRadius())) {
                continue;
            }
            bullet->ApplyBulletDurabilityDamage(1000000.0f);
            barrierStamina_ -= barrier.hitCost * staminaScale;
            barrierFlash_ = 1.0f;
            ++barrierBlocked_;
            if (heroBarrier && UpgradeTier(1) >= 3) {
                AddUltimateCharge(HeroDamageUnit(3.0f));
            }
            // パーク「盾の手当て」：弾を止めるたびに少し回復する。
            if (HasPerk(survivor::PerkId::BarrierHeal)) {
                player.HealRunPlayer(static_cast<int>(PerkValue(survivor::PerkId::BarrierHeal)));
            }
        }
        // 敵：バリアの外の縁まで押し返す。触れている数だけ、スタミナが少しずつ減る。
        for (ExpEnemy* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
            if (!enemy || enemy->IsDead() || !IsInsideBarrier(enemy->GetWorldPosition(), enemy->GetRadius())) {
                continue;
            }
            const myEngine::Vector3 position = enemy->GetWorldPosition();
            myEngine::Vector3 away{position.x - center.x, position.y - center.y, 0.0f};
            const float distance = myEngine::Length(away);
            away = distance > 0.001f ? away / distance : player.GetDirection();
            const myEngine::Vector3 pushed = center + away * (barrier.radius + enemy->GetRadius());
            // 壁の中へは押し込まない（そのときは止めるだけにする）。
            if (!world_.resources.stage_->IsCollisionWithAnyBlock(pushed, enemy->GetRadius())) {
                enemy->SetWorldPosition({pushed.x, pushed.y, position.z});
            }
            barrierStamina_ -= barrier.contactPerSecond * dt;
            ++barrierPushes_;
        }
        if (barrierStamina_ <= 0.0f) {
            // 尽きたら、少しの間は張れない。
            barrierStamina_ = 0.0f;
            barrierExhausted_ = barrier.exhaustedSeconds;
            barrierAwaitRelease_ = true;
            barrierOn_ = false;
            ++barrierBreaks_;
            world_.combatFlow->SetEventCallout("スタミナ切れ", 0.8f);
        }
    } else if (barrierRegenWait_ > 0.0f) {
        barrierRegenWait_ -= dt;
    } else {
        barrierStamina_ = (std::min)(barrier.staminaMax, barrierStamina_ + barrier.regenPerSecond * dt);
    }
    world_.presentation.playerStaminaRatio_ = barrierStamina_ / (std::max)(1.0f, barrier.staminaMax);
    world_.presentation.playerStaminaExhausted_ = barrierExhausted_ > 0.0f;
}

void SurvivorRunController::QueueBarrier(myEngine::NeonGridRenderer& renderer, const myEngine::Vector3& cameraForward) const
{
    if (!barrierOn_) {
        return;
    }
    const auto& barrier = config_.barrier;
    const myEngine::Vector3 center = world_.resources.player_->GetWorldPosition() + myEngine::Vector3{0.0f, 0.0f, 0.35f};
    const myEngine::Vector3 direction = world_.resources.player_->GetDirection();
    const float facing = std::atan2(direction.y, direction.x);
    const float halfArc = barrier.arcDegrees * 0.5f * kPi / 180.0f;
    // 網目は扇の中で固定し、機体の向きと一緒に回す。弾を止めた瞬間は白く光らせる。
    const float pulse = barrierFlash_;
    const myEngine::Vector4 color{0.30f + 1.5f * pulse, 0.80f + 1.1f * pulse, 1.30f + 0.7f * pulse, 0.60f + 0.35f * pulse};
    auto line = [&](const myEngine::Vector3& a, const myEngine::Vector3& b, float width) {
        renderer.QueueCameraFacingLine(a, b, width, color, cameraForward);
    };
    auto point = [&](float polar, float azimuth) {
        const float r = barrier.radius * polar / (kPi * 0.5f);
        const float a = facing + azimuth;
        return center + myEngine::Vector3{std::cos(a) * r, std::sin(a) * r, 0.0f};
    };
    for (const auto& segment : BarrierNet()) {
        if (std::abs(segment.azimuthA) > halfArc || std::abs(segment.azimuthB) > halfArc) {
            continue;
        }
        // 自機のすぐ近くは線を出さない（機体が見えるように）。
        if (segment.polarA < 0.28f || segment.polarB < 0.28f) {
            continue;
        }
        line(point(segment.polarA, segment.azimuthA), point(segment.polarB, segment.azimuthB), 0.07f);
    }
    // 外の縁と両脇の線で、扇の形をはっきりさせる。
    constexpr int kRimPieces = 24;
    const float rimPolar = kPi * 0.5f;
    for (int i = 0; i < kRimPieces; ++i) {
        const float a0 = -halfArc + 2.0f * halfArc * static_cast<float>(i) / kRimPieces;
        const float a1 = -halfArc + 2.0f * halfArc * static_cast<float>(i + 1) / kRimPieces;
        line(point(rimPolar, a0), point(rimPolar, a1), 0.13f);
    }
    line(point(0.28f, -halfArc), point(rimPolar, -halfArc), 0.09f);
    line(point(0.28f, halfArc), point(rimPolar, halfArc), 0.09f);
}

size_t SurvivorRunController::BarrierNetSegmentCount() const
{
    return BarrierNet().size();
}

} // namespace gameplay
