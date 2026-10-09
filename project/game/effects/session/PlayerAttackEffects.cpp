#include "game/effects/session/PlayerAttackEffects.h"
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

void PlayerAttackEffects::UpdatePlayerNeonAfterimages(float deltaTime)
{
    for (PlayerNeonAfterimage& afterimage : world_.presentation.playerNeonAfterimages_) {
        afterimage.life -= deltaTime;
    }
    world_.presentation.playerNeonAfterimages_.erase(std::remove_if(world_.presentation.playerNeonAfterimages_.begin(),
                                                                    world_.presentation.playerNeonAfterimages_.end(),
                                                                    [](const PlayerNeonAfterimage& afterimage) {
                                                                        return afterimage.life <= 0.0f;
                                                                    }),
                                                     world_.presentation.playerNeonAfterimages_.end());

    if (!world_.resources.player_ || world_.resources.player_->IsDead() || !world_.resources.player_->IsDashing()) {
        world_.presentation.playerAfterimageSpawnTimer_ = 0.0f;
        return;
    }

    world_.presentation.playerAfterimageSpawnTimer_ -= deltaTime;
    if (world_.presentation.playerAfterimageSpawnTimer_ <= 0.0f) {
        cg2::Vector3 direction = world_.resources.player_->GetDirection();
        if (cg2::Length(direction) < 0.001f) {
            direction = {0.0f, -1.0f, 0.0f};
        }
        world_.presentation.playerNeonAfterimages_.push_back(
            {world_.resources.player_->GetWorldPosition(), cg2::Normalize(direction), world_.presentation.playerAfterimageLifetime_});
        world_.presentation.playerAfterimageSpawnTimer_ = world_.presentation.playerAfterimageInterval_;
    }
}

void PlayerAttackEffects::SpawnPlayerLaser(const Player::LaserShotEvent& event)
{
    if (cg2::Length(event.direction) < 0.0001f) {
        return;
    }

    const cg2::Vector3 direction = cg2::Normalize(event.direction);
    // 0.35ワールド単位ごとの球判定で壁までの長さを近似する。厳密な線分交差ではない。
    // 最初に接触したサンプルを表示と命中判定の終点に使う。
    float range = (std::max)(0.1f, event.range);
    if (world_.resources.stage_) {
        const float step = 0.35f;
        for (float d = step; d <= range; d += step) {
            const cg2::Vector3 sample = event.origin + direction * d;
            if (world_.resources.stage_->IsCollisionWithAnyBlock(sample, (std::max)(0.03f, event.width * 0.35f))) {
                range = d;
                break;
            }
        }
    }

    const cg2::Vector3 end = event.origin + direction * range;
    PlayerLaserBeam beam{};
    beam.start = event.origin;
    beam.end = end;
    beam.width = (std::max)(0.02f, event.width);
    beam.life = (std::max)(0.01f, event.duration);
    beam.maxLife = beam.life;
    beam.color = event.color;
    world_.presentation.playerLaserBeams_.push_back(beam);

    // 発射イベント1件につき、この時点の各対象へ一度だけダメージを渡す。
    // 表示状態には威力を保持せず、残存中のレーザー表示から再度ダメージを与えない。
    bool emittedImpact = false;
    if (world_.resources.enemy_ && (world_.gameplayQueries->IsRunRivalActive() && !world_.resources.enemy_->IsDead())) {
        float t = 0.0f;
        const float distance = DistancePointToSegment2D(world_.resources.enemy_->GetWorldPosition(), event.origin, end, &t);
        if (distance <= world_.resources.enemy_->GetRadius() + beam.width * 0.75f) {
            world_.resources.enemy_->TakeDamage(event.damage);
            cg2::ParticleManager::GetInstance()->EmitNeonImpactEffect(event.origin + (end - event.origin) * t, direction * -1.0f,
                                                                      event.color, 8);
            emittedImpact = true;
        }
    }
    if (world_.resources.enemyManager_) {
        for (ExpEnemy* expEnemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
            if (!expEnemy || expEnemy->IsDead()) {
                continue;
            }
            float t = 0.0f;
            const float distance = DistancePointToSegment2D(expEnemy->GetWorldPosition(), event.origin, end, &t);
            if (distance <= expEnemy->GetRadius() + beam.width * 0.75f) {
                expEnemy->TakeDirectionalDamage(event.damage, event.origin);
                if (!emittedImpact) {
                    cg2::ParticleManager::GetInstance()->EmitNeonImpactEffect(event.origin + (end - event.origin) * t, direction * -1.0f,
                                                                              event.color, 8);
                    emittedImpact = true;
                }
            }
        }
    }
}

void PlayerAttackEffects::UpdatePlayerLasers(float deltaTime)
{
    // 発射時に命中は処理済みなので、ここでは表示寿命だけを減らして消去する。
    for (PlayerLaserBeam& beam : world_.presentation.playerLaserBeams_) {
        beam.life -= deltaTime;
    }
    world_.presentation.playerLaserBeams_.erase(std::remove_if(world_.presentation.playerLaserBeams_.begin(),
                                                               world_.presentation.playerLaserBeams_.end(),
                                                               [](const PlayerLaserBeam& beam) {
                                                                   return beam.life <= 0.0f;
                                                               }),
                                                world_.presentation.playerLaserBeams_.end());
}

void PlayerAttackEffects::QueuePlayerLasers(const cg2::Vector3& cameraForward)
{
    if (!world_.resources.neonGridRenderer_) {
        return;
    }

    for (const PlayerLaserBeam& beam : world_.presentation.playerLaserBeams_) {
        const float t = beam.maxLife > 0.0f ? (std::clamp)(beam.life / beam.maxLife, 0.0f, 1.0f) : 0.0f;
        cg2::Vector4 color = beam.color;
        color.w *= t;
        world_.resources.neonGridRenderer_->QueueCameraFacingLine(beam.start, beam.end, beam.width * (0.70f + 0.35f * t), color,
                                                                  cameraForward);

        cg2::Vector4 coreColor = {1.0f, 1.0f, 1.0f, color.w};
        world_.resources.neonGridRenderer_->QueueCameraFacingLine(beam.start, beam.end, beam.width * 0.32f, coreColor, cameraForward);
    }
}

void PlayerAttackEffects::SpawnPlayerMine(const Player::MineDropEvent& event)
{
    PlayerMine mine{};
    mine.position = event.position;
    mine.radius = (std::max)(0.2f, event.radius);
    mine.fuse = (std::max)(0.0f, event.fuseTime);
    mine.life = (std::max)(0.1f, event.lifeTime);
    mine.maxLife = mine.life;
    mine.damage = event.damage;
    mine.rotation = cg2::Rand(0.0f, 6.28318530718f);
    mine.color = event.color;
    world_.presentation.playerMines_.push_back(mine);
    cg2::ParticleManager::GetInstance()->EmitNeonImpactEffect(mine.position, {0.0f, 1.0f, 0.0f}, mine.color, 5);
}

void PlayerAttackEffects::UpdatePlayerMines(float deltaTime)
{
    for (PlayerMineExplosion& explosion : world_.presentation.playerMineExplosions_) {
        explosion.life -= deltaTime;
    }
    world_.presentation.playerMineExplosions_.erase(std::remove_if(world_.presentation.playerMineExplosions_.begin(),
                                                                   world_.presentation.playerMineExplosions_.end(),
                                                                   [](const PlayerMineExplosion& explosion) {
                                                                       return explosion.life <= 0.0f;
                                                                   }),
                                                    world_.presentation.playerMineExplosions_.end());

    for (size_t i = 0; i < world_.presentation.playerMines_.size();) {
        PlayerMine& mine = world_.presentation.playerMines_[i];
        mine.fuse -= deltaTime;
        mine.life -= deltaTime;
        mine.rotation += deltaTime * 2.4f;

        // 寿命切れは待機時間に関係なく起爆する。待機時間を過ぎた地雷は対象との距離でも起爆する。
        bool shouldDetonate = mine.life <= 0.0f;
        if (!shouldDetonate && mine.fuse <= 0.0f) {
            if (world_.resources.enemy_ && (world_.gameplayQueries->IsRunRivalActive() && !world_.resources.enemy_->IsDead()) &&
                cg2::Length(world_.resources.enemy_->GetWorldPosition() - mine.position) <=
                    mine.radius + world_.resources.enemy_->GetRadius()) {
                shouldDetonate = true;
            }
            if (!shouldDetonate && world_.resources.enemyManager_) {
                for (ExpEnemy* expEnemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
                    if (expEnemy && !expEnemy->IsDead() &&
                        cg2::Length(expEnemy->GetWorldPosition() - mine.position) <= mine.radius + expEnemy->GetRadius()) {
                        shouldDetonate = true;
                        break;
                    }
                }
            }
        }

        if (shouldDetonate) {
            // 起爆で要素が消えるため、詰められた次の地雷を同じ添字で調べる。
            DetonatePlayerMine(i);
            continue;
        }
        ++i;
    }
}

void PlayerAttackEffects::DetonatePlayerMine(size_t index)
{
    if (index >= world_.presentation.playerMines_.size()) {
        return;
    }

    // 元の地雷を写してから範囲ダメージを適用し、最後にその要素を削除する。
    // 距離はZを含み、地形による遮蔽は調べない。正面装甲などの適用条件は対象の関数へ任せる。
    const PlayerMine mine = world_.presentation.playerMines_[index];
    if (world_.resources.enemy_ && (world_.gameplayQueries->IsRunRivalActive() && !world_.resources.enemy_->IsDead()) &&
        cg2::Length(world_.resources.enemy_->GetWorldPosition() - mine.position) <= mine.radius + world_.resources.enemy_->GetRadius()) {
        world_.resources.enemy_->TakeDamage(mine.damage);
    }
    if (world_.resources.enemyManager_) {
        for (ExpEnemy* expEnemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
            if (expEnemy && !expEnemy->IsDead() &&
                cg2::Length(expEnemy->GetWorldPosition() - mine.position) <= mine.radius + expEnemy->GetRadius()) {
                expEnemy->TakeDirectionalDamage(mine.damage, mine.position);
            }
        }
    }

    cg2::ParticleManager::GetInstance()->EmitNeonImpactEffect(mine.position, {0.0f, 1.0f, 0.0f}, mine.color, 18);
    world_.presentation.playerMineExplosions_.push_back({mine.position, mine.radius, 0.30f, 0.30f, mine.color});
    world_.presentation.playerMines_.erase(world_.presentation.playerMines_.begin() + static_cast<std::ptrdiff_t>(index));
}

void PlayerAttackEffects::QueuePlayerMines(const cg2::Vector3& cameraRight, const cg2::Vector3& cameraUp, const cg2::Vector3& cameraForward)
{
    if (!world_.resources.neonGridRenderer_) {
        return;
    }

    auto queueCircle = [&](const cg2::Vector3& center, float radius, float lineWidth, const cg2::Vector4& color) {
        const int segments = 36;
        cg2::Vector3 previous = center + cameraRight * radius;
        for (int i = 1; i <= segments; ++i) {
            const float angle = 6.28318530718f * static_cast<float>(i) / static_cast<float>(segments);
            const cg2::Vector3 current = center + (cameraRight * std::cos(angle) + cameraUp * std::sin(angle)) * radius;
            world_.resources.neonGridRenderer_->QueueLine(previous, current, lineWidth, color);
            previous = current;
        }
    };

    for (const PlayerMineExplosion& explosion : world_.presentation.playerMineExplosions_) {
        const float t = explosion.maxLife > 0.0f ? (std::clamp)(explosion.life / explosion.maxLife, 0.0f, 1.0f) : 0.0f;
        cg2::Vector4 color = explosion.color;
        color.w *= t;
        queueCircle(explosion.position, explosion.radius * (1.05f - t * 0.35f), 0.10f * (0.6f + t), color);
        cg2::Vector4 coreColor = {1.0f, 1.0f, 1.0f, color.w * 0.8f};
        queueCircle(explosion.position, explosion.radius * (0.55f + (1.0f - t) * 0.25f), 0.045f, coreColor);
    }

    for (const PlayerMine& mine : world_.presentation.playerMines_) {
        const float lifeRatio = mine.maxLife > 0.0f ? (std::clamp)(mine.life / mine.maxLife, 0.0f, 1.0f) : 0.0f;
        const float armed = mine.fuse <= 0.0f ? 1.0f : 0.45f;
        cg2::Vector4 color = mine.color;
        const float warningPulse = 0.5f + 0.5f * std::sin(mine.rotation * (mine.fuse <= 0.0f ? 8.0f : 3.0f));
        color.w *= (0.45f + 0.55f * armed) * (0.45f + 0.55f * lifeRatio) * (0.72f + warningPulse * 0.28f);
        const float markerRadius = 0.42f + warningPulse * 0.12f;
        world_.resources.neonGridRenderer_->QueueBillboardTriangle(mine.position, markerRadius, mine.rotation, 0.055f, color, cameraRight,
                                                                   cameraUp, cameraForward);
        world_.resources.neonGridRenderer_->QueueBillboardTriangle(mine.position, markerRadius * 0.72f, -mine.rotation * 1.35f, 0.04f,
                                                                   color, cameraRight, cameraUp, cameraForward);

        cg2::Vector4 rangeColor = color;
        rangeColor.w *= mine.fuse <= 0.0f ? 0.28f : 0.14f;
        queueCircle(mine.position, mine.radius * (0.96f + warningPulse * 0.04f), 0.035f + warningPulse * 0.02f, rangeColor);
    }
}

cg2::TrailConfig PlayerAttackEffects::MakePlayerMeleeTrailConfig(const PlayerMeleeSlash& slash, float alphaScale) const
{
    cg2::TrailConfig config{};
    cg2::Vector4 headColor = slash.color;
    headColor.x *= 1.45f;
    headColor.y *= 1.45f;
    headColor.z *= 1.45f;
    headColor.w *= 0.48f * alphaScale;
    config.startColor = headColor;
    config.endColor = {slash.color.x * 0.45f, slash.color.y * 0.45f, slash.color.z * 0.45f, 0.0f};
    config.interpolationSteps = 5;
    config.maxPoints = 18;
    config.lifetime = (std::max)(0.10f, slash.maxLife * 1.25f);
    config.startWidthScale = 1.0f;
    config.endWidthScale = 0.08f;
    config.widthCurvePower = 0.80f;
    config.colorCurvePower = 0.65f;
    return config;
}

void PlayerAttackEffects::ComputePlayerMeleeBladeSection(const PlayerMeleeSlash& slash, float progress, cg2::Vector3& base,
                                                         cg2::Vector3& tip) const
{
    constexpr float kPi = 3.1415926535f;
    const float x = (std::clamp)(progress, 0.0f, 1.0f);
    const float eased = x * x * (3.0f - 2.0f * x);
    const float angle = (slash.startAngleDeg + (slash.endAngleDeg - slash.startAngleDeg) * eased) * kPi / 180.0f;
    const cg2::Vector3 bladeDir = RotateVector2D(slash.direction, angle);
    const cg2::Vector3 right = {-slash.direction.y, slash.direction.x, 0.0f};
    const cg2::Vector3 hilt = slash.origin - slash.direction * (slash.range * 0.08f) + right * (slash.range * slash.hiltSideOffset);
    // 軌跡は刃の外側だけに置き、斬撃範囲内の対象を覆わない。
    base = hilt + bladeDir * (slash.range * slash.bladeLengthScale * 0.62f);
    tip = hilt + bladeDir * (slash.range * slash.bladeLengthScale * (0.92f + x * 0.08f));
}

void PlayerAttackEffects::SpawnPlayerMeleeSlash(const Player::MeleeSlashEvent& event)
{
    if (cg2::Length(event.direction) < 0.0001f) {
        return;
    }

    PlayerMeleeSlash slash{};
    slash.origin = event.origin;
    slash.followAnchor = world_.resources.player_ ? world_.resources.player_->GetWorldPosition() : event.origin;
    slash.direction = cg2::Normalize(event.direction);
    slash.range = (std::max)(0.3f, event.range);
    slash.arcDeg = (std::clamp)(event.arcDeg, 5.0f, 360.0f);
    slash.width = (std::max)(0.02f, event.width);
    slash.windupDuration = (std::max)(0.0f, event.windupDuration);
    slash.swingDuration = (std::max)(0.03f, event.duration);
    slash.recoveryDuration = (std::max)(0.0f, event.recoveryDuration);
    slash.damage = event.damage;
    slash.knockback = event.knockback;
    slash.finisher = event.comboStep % 3 == 2;
    if (world_.run.expeditionRun_)
        world_.run.tankExpeditionAudio_.Slash();
    slash.life = slash.windupDuration + slash.swingDuration + slash.recoveryDuration;
    slash.color = event.color;
    if (!world_.presentation.playerMeleeComboVisuals_.empty()) {
        const size_t profileIndex =
            static_cast<size_t>((std::max)(0, event.comboStep)) % world_.presentation.playerMeleeComboVisuals_.size();
        const MeleeComboVisualProfile& profile = world_.presentation.playerMeleeComboVisuals_[profileIndex];
        slash.startAngleDeg = profile.startAngleDeg;
        slash.endAngleDeg = profile.endAngleDeg;
        slash.bladeLengthScale = (std::max)(0.05f, profile.bladeLengthScale);
        slash.hiltSideOffset = profile.hiltSideOffset;
        slash.windupAngleDeg = profile.windupAngleDeg;
        slash.returnAngleDeg = profile.returnAngleDeg;
        slash.width *= (std::max)(0.05f, profile.bladeWidthScale);
        // 近接ビルドの通常斬撃・パリィ・3段目の斬撃波は同じ有効時間を使う。
        // そのビルドでは外観プロファイルのdurationScaleを斬撃時間へ掛けない。
        if (!world_.resources.player_ || !world_.resources.player_->IsMeleeBuild())
            slash.swingDuration *= (std::max)(0.05f, profile.durationScale);
        slash.life = slash.windupDuration + slash.swingDuration + slash.recoveryDuration;
        slash.color.x *= profile.colorScale.x;
        slash.color.y *= profile.colorScale.y;
        slash.color.z *= profile.colorScale.z;
        slash.color.w *= profile.colorScale.w;
    }
    if (world_.resources.player_ && world_.resources.player_->IsMeleeBuild()) {
        const float sign = event.comboStep % 3 == 1 ? -1.0f : 1.0f;
        slash.startAngleDeg = -slash.arcDeg * 0.5f * sign;
        slash.endAngleDeg = slash.arcDeg * 0.5f * sign;
        slash.bladeLengthScale = 1;
    }
    slash.maxLife = slash.life;
    world_.presentation.playerMeleeSlashes_.push_back(slash);
}

void PlayerAttackEffects::UpdatePlayerMeleeSlashes(float deltaTime)
{
    const bool canFollowPlayer = world_.resources.player_ != nullptr;
    const cg2::Vector3 playerPosition = canFollowPlayer ? world_.resources.player_->GetWorldPosition() : cg2::Vector3{};
    for (PlayerMeleeSlash& slash : world_.presentation.playerMeleeSlashes_) {
        // 発射時の方向は保ったまま、自機のXY移動量だけ斬撃の原点へ加える。
        if (canFollowPlayer) {
            cg2::Vector3 followDelta = playerPosition - slash.followAnchor;
            followDelta.z = 0.0f;
            slash.origin += followDelta;
            slash.followAnchor = playerPosition;
        }
        slash.elapsed += deltaTime;
        slash.life -= deltaTime;
        // 予備動作後から振り終わりまでの両端を含めて命中を調べる。回復動作中は新たに命中させない。
        if (slash.elapsed >= slash.windupDuration && slash.elapsed <= slash.windupDuration + slash.swingDuration) {
            const float halfArcRad = slash.arcDeg * 0.5f * 3.1415926535f / 180.0f;
            const float minDot = slash.arcDeg >= 359.0f ? -1.0f : std::cos(halfArcRad);
            // XYの射程/角度と、0.6単位ごとの壁サンプルで判定する。対象半径の手前までを調べる近似。
            auto hitTarget = [&](const cg2::Vector3& targetPos, float targetRadius) {
                cg2::Vector3 toTarget = targetPos - slash.origin;
                toTarget.z = 0.0f;
                const float distance = cg2::Length(toTarget);
                if (distance > 0.001f)
                    for (float t = 0.6f; t < distance - targetRadius; t += 0.6f)
                        if (world_.resources.stage_->IsCollisionWithAnyBlock(slash.origin + toTarget * (t / distance), 0.12f))
                            return false;
                return distance <= 0.0001f ||
                       (distance <= slash.range + targetRadius && cg2::Dot(cg2::Normalize(toTarget), slash.direction) >= minDot);
            };
            // この斬撃で処理した対象のアドレスを記録し、次フレームも同じ対象への重複適用を避ける。
            // 命中演出のhitAppliedは斬撃全体の初回用で、各対象の命中記録とは別に扱う。
            if (world_.resources.enemy_ && (world_.gameplayQueries->IsRunRivalActive() && !world_.resources.enemy_->IsDead()) &&
                std::find(slash.hitTargets.begin(), slash.hitTargets.end(), world_.resources.enemy_.get()) == slash.hitTargets.end() &&
                hitTarget(world_.resources.enemy_->GetWorldPosition(), world_.resources.enemy_->GetRadius())) {
                slash.hitTargets.push_back(world_.resources.enemy_.get());
                world_.resources.enemy_->ApplyKnockback(slash.direction, slash.knockback);
                world_.resources.enemy_->TakeDamage(slash.damage);
                if (!slash.hitApplied) {
                    world_.expeditionExperience->QueueExpeditionImpact(world_.resources.enemy_->GetWorldPosition(), slash.direction,
                                                                       slash.finisher);
                    slash.hitApplied = true;
                }
                cg2::ParticleManager::GetInstance()->EmitNeonImpactEffect(world_.resources.enemy_->GetWorldPosition(),
                                                                          slash.direction * -1.0f, slash.color, 10);
            }
            if (world_.resources.enemyManager_) {
                for (ExpEnemy* expEnemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
                    if (expEnemy && !expEnemy->IsDead() &&
                        std::find(slash.hitTargets.begin(), slash.hitTargets.end(), expEnemy) == slash.hitTargets.end() &&
                        hitTarget(expEnemy->GetWorldPosition(), expEnemy->GetRadius())) {
                        slash.hitTargets.push_back(expEnemy);
                        expEnemy->ApplyKnockback(slash.direction, slash.knockback);
                        // ノックバック速度を設定した後、後続更新の壁衝突を追跡できるようダメージ適用前に対象を登録する。
                        world_.resources.player_->ArmWallSmash(expEnemy);
                        expEnemy->TakeDirectionalDamage(slash.damage, slash.origin, true);
                        if (!slash.hitApplied) {
                            world_.expeditionExperience->QueueExpeditionImpact(expEnemy->GetWorldPosition(), slash.direction,
                                                                               slash.finisher);
                            slash.hitApplied = true;
                        }
                        cg2::ParticleManager::GetInstance()->EmitNeonImpactEffect(expEnemy->GetWorldPosition(), slash.direction * -1.0f,
                                                                                  slash.color, 8);
                    }
                }
            }
        }
        const float swingElapsed = slash.elapsed - slash.windupDuration;
        const bool isSwinging = swingElapsed >= 0.0f && swingElapsed < slash.swingDuration;
        if (world_.presentation.enablePlayerMeleeRibbonTrail_ && isSwinging && !slash.trail && world_.resources.playerMeleeTrailManager_) {
            slash.trail = world_.resources.playerMeleeTrailManager_->CreateInstance();
            slash.trail->SetIsPermanent(false);
            slash.trail->SetActive(true);
            slash.trail->SetConfig(MakePlayerMeleeTrailConfig(slash));
        }
        if (world_.presentation.enablePlayerMeleeRibbonTrail_ && isSwinging && slash.trail) {
            const float progress = (std::clamp)(swingElapsed / slash.swingDuration, 0.0f, 1.0f);
            cg2::Vector3 base{};
            cg2::Vector3 tip{};
            ComputePlayerMeleeBladeSection(slash, progress, base, tip);
            slash.trail->Update(deltaTime, tip, base, MakePlayerMeleeTrailConfig(slash));
        } else if (slash.trail) {
            slash.trail->SetActive(false);
            slash.trail = nullptr;
        }
    }
    world_.presentation.playerMeleeSlashes_.erase(std::remove_if(world_.presentation.playerMeleeSlashes_.begin(),
                                                                 world_.presentation.playerMeleeSlashes_.end(),
                                                                 [](const PlayerMeleeSlash& slash) {
                                                                     return slash.life <= 0.0f;
                                                                 }),
                                                  world_.presentation.playerMeleeSlashes_.end());
    if (world_.presentation.enablePlayerMeleeRibbonTrail_ && world_.resources.playerMeleeTrailManager_) {
        world_.resources.playerMeleeTrailManager_->Update(deltaTime);
    }
}

void PlayerAttackEffects::UpdateSpecialCombatPresentation(float dt)
{
    // ageは基準時間で進む経過秒。既存フラッシュの期限切れ分を消去してから、このフレームのイベントを追加する。
    for (auto& flash : world_.run.buildCombatFlashes_)
        flash.age += dt;
    std::erase_if(world_.run.buildCombatFlashes_, [](const auto& flash) {
        return flash.age > .30f;
    });
    for (auto& flash : world_.run.specialCombatFlashes_)
        flash.age += dt;
    std::erase_if(world_.run.specialCombatFlashes_, [](const auto& flash) {
        return flash.age > .24f;
    });
    world_.run.specialProjectileVisuals_.clear();
    if (!world_.resources.player_ || !world_.resources.bulletManager_)
        return;
    // 3つのConsumeは各待ち行列を取り出して空にする。表示枠が64件に達してもイベントは消費する。
    // 攻撃結果は適用済みで、ここではHPへ再適用しない。
    for (const auto& event : world_.resources.bulletManager_->ConsumeBuildEvents()) {
        if (world_.run.buildCombatFlashes_.size() < 64)
            world_.run.buildCombatFlashes_.push_back({event, 0});
        if (world_.run.expeditionRun_)
            world_.run.tankExpeditionAudio_.Hit();
    }
    for (const auto& event : world_.resources.player_->ConsumeSpecialCombatEvents()) {
        cg2::Vector3 end = event.origin;
        if (event.kind == Player::SpecialEventKind::RailShot) {
            // レールの見た目の終点を0.4単位の壁サンプルで求める。命中・威力の計算とは別。
            for (float distance = .4f; distance < 22; distance += .4f) {
                const auto next = event.origin + event.direction * distance;
                if (world_.resources.stage_->IsCollisionWithAnyBlock(next, .12f))
                    break;
                end = next;
            }
            world_.presentation.cameraShakeDuration_ = .10f;
            world_.presentation.cameraShakeTimer_ = .10f;
            world_.presentation.cameraShakePower_ = (std::max)(world_.presentation.cameraShakePower_, .09f);
            if (world_.run.expeditionRun_)
                world_.run.tankExpeditionAudio_.RailShot();
        } else if (event.kind == Player::SpecialEventKind::PerfectParry) {
            // 完全パリィで停止用タイマーを最低0.035秒に設定する。減速の適用は次回Updateで行う。
            world_.run.expeditionImpactHold_ = (std::max)(world_.run.expeditionImpactHold_, .035f);
            world_.combatFlow->SetEventCallout("パリィ成功！", .42f);
            if (world_.run.expeditionRun_)
                world_.run.tankExpeditionAudio_.Parry(true);
        } else if (event.kind == Player::SpecialEventKind::Parry) {
            if (world_.run.expeditionRun_)
                world_.run.tankExpeditionAudio_.Parry(false);
        } else if (event.kind == Player::SpecialEventKind::LinkHit && world_.run.expeditionRun_)
            world_.run.tankExpeditionAudio_.Hit();
        else if (event.kind == Player::SpecialEventKind::WallSmash || event.kind == Player::SpecialEventKind::DroneBomb) {
            if (world_.run.expeditionRun_)
                world_.run.tankExpeditionAudio_.ArmorBreak();
            world_.presentation.cameraShakeDuration_ = world_.presentation.cameraShakeTimer_ = .09f;
            world_.presentation.cameraShakePower_ = (std::max)(world_.presentation.cameraShakePower_, .07f);
        } else if (event.kind == Player::SpecialEventKind::DashSlash || event.kind == Player::SpecialEventKind::SpinBlade) {
            if (world_.run.expeditionRun_)
                world_.run.tankExpeditionAudio_.Slash();
        } else if (world_.run.expeditionRun_)
            world_.run.tankExpeditionAudio_.Hit();
        if (world_.run.specialCombatFlashes_.size() < 64)
            world_.run.specialCombatFlashes_.push_back({event, 0, end});
    }
    for (const auto& hit : world_.resources.bulletManager_->ConsumeSpecialImpacts()) {
        Player::SpecialCombatEvent event{};
        event.kind = hit.bulletCut ? Player::SpecialEventKind::Parry : Player::SpecialEventKind::LinkHit;
        event.origin = hit.position;
        event.direction = hit.direction;
        if (world_.run.specialCombatFlashes_.size() < 64)
            world_.run.specialCombatFlashes_.push_back({event, 0, hit.position});
        if (hit.bulletCut && world_.run.expeditionRun_)
            world_.run.tankExpeditionAudio_.Parry(false);
    }
    // 名前はRatioだが、取得値は蓄積秒。現在の最大1秒を前提に音の段階を判定する。
    const float charge = world_.resources.player_->GetRailChargeRatio();
    world_.run.railChargeAudioAge_ += dt;
    if (charge > 0 && world_.run.railChargeAudioAge_ > .30f && !world_.arenaRunController->IsTankRunMenuOpen()) {
        if (world_.run.expeditionRun_)
            world_.run.tankExpeditionAudio_.RailCharge(charge >= .999f);
        world_.run.railChargeAudioAge_ = 0;
    }
    // 借用弾はこの走査で位置・方向・半径を値へ写す。後の描画へ弾ポインターを持ち越さない。
    if (world_.resources.bulletManager_->GetBulletCount() > 0)
        for (const auto* bullet : world_.resources.bulletManager_->GetBulletPtrs()) {
            if (!bullet || bullet->IsDead() || bullet->GetSpecialKind() == Bullet::SpecialKind::None)
                continue;
            auto direction = bullet->GetMove();
            direction = cg2::Length(direction) > .0001f ? cg2::Normalize(direction) : cg2::Vector3{1, 0, 0};
            world_.run.specialProjectileVisuals_.push_back(
                {bullet->GetWorldPosition(), direction, bullet->GetRadius(), bullet->GetSpecialKind()});
        }
}

void PlayerAttackEffects::QueueSpecialCombatPresentation()
{
    if (!world_.resources.neonGridRenderer_ || !world_.resources.player_ || world_.arenaRunController->IsTankRunMenuOpen())
        return;
    tankspecialfx::Charge(*world_.resources.neonGridRenderer_, world_.resources.player_->GetRailChargeMuzzle(),
                          world_.resources.player_->GetRailChargeRatio(), world_.combat.playTime_);
    for (const auto& link : world_.resources.player_->GetDroneLaserLinks())
        tankspecialfx::Link(*world_.resources.neonGridRenderer_, link.start, link.end);
    for (const auto& mark : world_.resources.bulletManager_->GetMarkVisuals())
        for (int i = 0; i < mark.stacks; ++i) {
            const float angle = world_.combat.playTime_ * 1.6f + static_cast<float>(i) * 1.5707963f;
            const auto p = mark.position + cg2::Vector3{std::cos(angle) * 1.3f, std::sin(angle) * 1.3f, .04f};
            tankspecialfx::Ring(*world_.resources.neonGridRenderer_, p, .13f, .04f, {1.4f, .45f, .9f, .7f});
        }
    for (const auto& lock : world_.resources.player_->GetTargetLockVisuals()) {
        const float radius = 1.45f + (lock.remaining > 0 ? .08f * std::sin(world_.combat.playTime_ * 9) : .22f);
        tankspecialfx::Ring(*world_.resources.neonGridRenderer_, lock.position, radius, .045f,
                            {.25f, 1.3f, 1.5f, lock.remaining > 0 ? .6f : .25f});
        for (int i = 0; i < 4; ++i) {
            const float a = i * 1.5707963f;
            const cg2::Vector3 d{std::cos(a), std::sin(a), 0};
            world_.resources.neonGridRenderer_->QueueLine(lock.position + d * radius, lock.position + d * (radius + .25f), .045f,
                                                          {.35f, 1.6f, 1.8f, .7f});
        }
    }
    for (const auto& drone : world_.resources.player_->GetDroneAbilityVisuals()) {
        using Phase = tankspecial::DronePhase;
        if (drone.phase == Phase::Warning) {
            const float pulse = .45f + .3f * std::sin(world_.combat.playTime_ * 28);
            tankspecialfx::Ring(*world_.resources.neonGridRenderer_, drone.position, .65f + drone.progress * .25f, .05f,
                                drone.bomb ? cg2::Vector4{1.7f, .35f, .6f, pulse} : cg2::Vector4{1.5f, 1.0f, .25f, pulse});
            world_.resources.neonGridRenderer_->QueueLine(drone.position, drone.target, .018f, {.6f, .9f, 1.2f, .20f});
        } else if (drone.phase == Phase::Rebuilding) {
            tankspecialfx::Ring(*world_.resources.neonGridRenderer_, drone.position, .32f + drone.progress * .3f, .025f,
                                {.3f, 1.3f, 1.6f, .25f + drone.progress * .4f});
            for (int i = 0; i < 3; ++i) {
                const float a = world_.combat.playTime_ * 3 + i * 2.0944f;
                const cg2::Vector3 d{std::cos(a), std::sin(a), 0};
                world_.resources.neonGridRenderer_->QueueLine(drone.position + d * .8f, drone.position + d * (.8f - .45f * drone.progress),
                                                              .035f, {.4f, 1.5f, 1.8f, .5f});
            }
        } else if (drone.phase == Phase::Charging) {
            const auto v = drone.target - drone.position;
            if (cg2::Length(v) > .001f)
                world_.resources.neonGridRenderer_->QueueLine(drone.position - cg2::Normalize(v) * 1.3f, drone.position, .09f,
                                                              {.4f, 1.4f, 1.7f, .5f});
        }
    }
    if (world_.resources.player_->GetEmpRatio() > 0) {
        const auto center = world_.resources.player_->GetWorldPosition();
        for (int i = 0; i < 3; ++i) {
            const float a = world_.combat.playTime_ * 7 + i * 2.0944f;
            const cg2::Vector3 d{std::cos(a), std::sin(a), 0};
            world_.resources.neonGridRenderer_->QueueLine(center + d * 1.6f, center + d * 1.85f, .035f, {.7f, .4f, 1.5f, .32f});
        }
    }
    for (const auto& visual : world_.run.specialProjectileVisuals_) {
        if (visual.kind == Bullet::SpecialKind::SlashWave)
            tankspecialfx::Crescent(*world_.resources.neonGridRenderer_, visual.position, visual.direction, visual.radius);
        else if (visual.kind == Bullet::SpecialKind::Rail) {
            world_.resources.neonGridRenderer_->QueueLine(visual.position - visual.direction * 3.5f, visual.position, .15f,
                                                          {.6f, 1.6f, 2.1f, .65f});
            world_.resources.neonGridRenderer_->QueueLine(visual.position - visual.direction * 1.2f, visual.position, .06f,
                                                          {1.8f, 2.0f, 2.2f, .9f});
        }
    }
    for (const auto& flash : world_.run.specialCombatFlashes_) {
        const auto& event = flash.event;
        if (event.kind == Player::SpecialEventKind::RailShot) {
            const float alpha = 1 - flash.age / .24f;
            world_.resources.neonGridRenderer_->QueueLine(event.origin, flash.end, .12f, {.3f, 1.1f, 1.8f, alpha * .26f});
            tankspecialfx::Ring(*world_.resources.neonGridRenderer_, event.origin, .25f + flash.age * 4, .09f,
                                {1.3f, 1.5f, 2.0f, alpha * .7f});
        } else if (event.kind == Player::SpecialEventKind::DroneBomb || event.kind == Player::SpecialEventKind::WallSmash) {
            const float fade = 1 - flash.age / .24f;
            tankspecialfx::Ring(*world_.resources.neonGridRenderer_, event.origin, .3f + flash.age * event.strength * 4, .075f,
                                {1.6f, .55f, .9f, fade * .7f});
            tankspecialfx::Contact(*world_.resources.neonGridRenderer_, event.origin, flash.age, 1.0f, false);
        } else if (event.kind == Player::SpecialEventKind::DashSlash) {
            const auto d = cg2::Length(event.direction) > .001f ? cg2::Normalize(event.direction) : cg2::Vector3{1, 0, 0};
            world_.resources.neonGridRenderer_->QueueLine(event.origin - d * 3, event.origin + d * 2, .10f,
                                                          {.55f, 1.4f, 1.9f, (1 - flash.age / .24f) * .6f});
        } else
            tankspecialfx::Contact(*world_.resources.neonGridRenderer_, event.origin, flash.age,
                                   event.kind == Player::SpecialEventKind::LinkHit ? .6f : 1.0f,
                                   event.kind == Player::SpecialEventKind::PerfectParry);
    }
    for (const auto& flash : world_.run.buildCombatFlashes_) {
        const auto& e = flash.event;
        const float alpha = (1 - flash.age / .30f);
        if (e.kind == BulletManager::BuildEventKind::Chain) {
            const auto delta = e.end - e.start;
            const auto side = cg2::Length(delta) > .001f ? cg2::Normalize(cg2::Vector3{-delta.y, delta.x, 0}) : cg2::Vector3{0, 1, 0};
            auto previous = e.start;
            for (int i = 1; i <= 5; ++i) {
                const auto next = e.start + delta * (i / 5.0f) + side * (i == 5 ? 0 : (i % 2 ? .18f : -.18f));
                world_.resources.neonGridRenderer_->QueueLine(previous, next, .045f, {.45f, 1.5f, 1.9f, alpha * .75f});
                previous = next;
            }
        } else {
            const auto color = e.kind == BulletManager::BuildEventKind::Reflection ? cg2::Vector4{1.5f, .25f, 1.2f, alpha * .7f}
                                                                                   : cg2::Vector4{1.5f, .8f, .35f, alpha * .65f};
            tankspecialfx::Ring(*world_.resources.neonGridRenderer_, e.start, .3f + flash.age * e.strength * 5, .055f, color);
        }
    }
}

void PlayerAttackEffects::QueuePlayerMeleeSlashes()
{
    if (!world_.resources.neonGridRenderer_) {
        return;
    }

    constexpr float kPi = 3.1415926535f;
    auto smoothStep = [](float value) {
        const float x = (std::clamp)(value, 0.0f, 1.0f);
        return x * x * (3.0f - 2.0f * x);
    };
    auto directionAt = [&](const PlayerMeleeSlash& slash, float progress) {
        const float angle = (slash.startAngleDeg + (slash.endAngleDeg - slash.startAngleDeg) * smoothStep(progress)) * kPi / 180.0f;
        return RotateVector2D(slash.direction, angle);
    };
    auto queueBlade = [&](const cg2::Vector3& hilt, const cg2::Vector3& bladeDir, float length, float width, const cg2::Vector4& color,
                          float coreAlpha) {
        tankneon::QueueMeleeBlade(*world_.resources.neonGridRenderer_, hilt, bladeDir, length, width, color, coreAlpha,
                                  {world_.presentation.playerMeleeBladeOuterWidthScale_,
                                   world_.presentation.playerMeleeBladeHaloWidthScale_,
                                   world_.presentation.playerMeleeBladeCoreWidthScale_});
    };
    auto queueTipTrail = [&](const PlayerMeleeSlash& slash, const cg2::Vector3& hilt, float startProgress, float endProgress,
                             const cg2::Vector4& color) {
        const int segments = 12;
        cg2::Vector3 previous = hilt + directionAt(slash, startProgress) * (slash.range * slash.bladeLengthScale);
        for (int i = 1; i <= segments; ++i) {
            const float localT = static_cast<float>(i) / static_cast<float>(segments);
            const float progress = startProgress + (endProgress - startProgress) * localT;
            const cg2::Vector3 current = hilt + directionAt(slash, progress) * (slash.range * slash.bladeLengthScale);
            cg2::Vector4 segmentColor = color;
            segmentColor.w *= localT * 0.72f * world_.presentation.playerMeleeTrailAlphaScale_;
            world_.resources.neonGridRenderer_->QueueLine(
                previous, current, slash.width * (0.55f + localT * 0.55f) * world_.presentation.playerMeleeTrailWidthScale_, segmentColor);

            cg2::Vector4 coreColor = {1.0f, 1.0f, 1.0f, segmentColor.w * 0.55f};
            world_.resources.neonGridRenderer_->QueueLine(
                previous, current, slash.width * (0.10f + localT * 0.12f) * world_.presentation.playerMeleeTrailWidthScale_, coreColor);
            previous = current;
        }
    };

    for (const PlayerMeleeSlash& slash : world_.presentation.playerMeleeSlashes_) {
        const float t = slash.maxLife > 0.0f ? (std::clamp)(slash.life / slash.maxLife, 0.0f, 1.0f) : 0.0f;
        const float swingElapsed = slash.elapsed - slash.windupDuration;
        const bool isWindup = slash.elapsed < slash.windupDuration;
        const bool isSwinging = !isWindup && swingElapsed < slash.swingDuration;
        const float swingProgress = (std::clamp)(swingElapsed / slash.swingDuration, 0.0f, 1.0f);
        float windupProgress = 1.0f;
        float recoveryProgress = 0.0f;
        float displayAngleDeg = slash.startAngleDeg;
        if (isWindup) {
            windupProgress = slash.windupDuration > 0.0f ? smoothStep(slash.elapsed / slash.windupDuration) : 1.0f;
            displayAngleDeg = slash.windupAngleDeg + (slash.startAngleDeg - slash.windupAngleDeg) * windupProgress;
        } else if (isSwinging) {
            displayAngleDeg = slash.startAngleDeg + (slash.endAngleDeg - slash.startAngleDeg) * smoothStep(swingProgress);
        } else {
            const float recoveryElapsed = swingElapsed - slash.swingDuration;
            recoveryProgress = slash.recoveryDuration > 0.0f ? smoothStep(recoveryElapsed / slash.recoveryDuration) : 1.0f;
            displayAngleDeg = slash.endAngleDeg + (slash.returnAngleDeg - slash.endAngleDeg) * recoveryProgress;
        }
        const cg2::Vector3 right = {-slash.direction.y, slash.direction.x, 0.0f};
        const cg2::Vector3 attackHilt =
            slash.origin - slash.direction * (slash.range * 0.08f) + right * (slash.range * slash.hiltSideOffset);
        cg2::Vector3 idleHilt =
            slash.followAnchor +
            slash.direction * (world_.presentation.playerIdleSaberForwardOffset_ * world_.presentation.playerNeonBillboardRadius_) +
            right * (world_.presentation.playerIdleSaberSideOffset_ * world_.presentation.playerNeonBillboardRadius_);
        idleHilt.z = attackHilt.z;
        auto lerpPosition = [](const cg2::Vector3& a, const cg2::Vector3& b, float amount) {
            return a + (b - a) * (std::clamp)(amount, 0.0f, 1.0f);
        };
        const cg2::Vector3 hilt = isWindup ? lerpPosition(idleHilt, attackHilt, windupProgress)
                                           : (isSwinging ? attackHilt : lerpPosition(attackHilt, idleHilt, recoveryProgress));
        cg2::Vector4 color = slash.color;
        color.x *= 1.18f * world_.presentation.playerNeonEmission_;
        color.y *= 1.18f * world_.presentation.playerNeonEmission_;
        color.z *= 1.18f * world_.presentation.playerNeonEmission_;
        color.w *= (0.25f + t * 0.75f) * (std::min)(1.0f, world_.presentation.playerNeonEmission_);

        const cg2::Vector3 bladeDir = RotateVector2D(slash.direction, displayAngleDeg * kPi / 180.0f);
        if (isSwinging) {
            queueTipTrail(slash, hilt, (std::max)(0.0f, swingProgress - 0.42f), swingProgress, color);
            // Three nested crescent trails give the top-down slash depth without
            // hiding the center of the fight behind a full-screen flash.
            for (int band = 1; band <= 3; ++band) {
                const float radius = slash.range * (1.0f - static_cast<float>(band) * 0.09f);
                const float start = (std::max)(0.0f, swingProgress - 0.30f);
                cg2::Vector3 previous = hilt + directionAt(slash, start) * radius;
                for (int segment = 1; segment <= 9; ++segment) {
                    const float q = static_cast<float>(segment) / 9.0f;
                    const cg2::Vector3 point = hilt + directionAt(slash, start + (swingProgress - start) * q) * radius;
                    cg2::Vector4 glow = color;
                    glow.w *= q * (0.32f - 0.06f * band);
                    world_.resources.neonGridRenderer_->QueueLine(previous, point, slash.width * (1.25f - band * 0.2f), glow);
                    previous = point;
                }
            }
        }

        for (int i = 4; isSwinging && i >= 1; --i) {
            const float sampleProgress = swingProgress - static_cast<float>(i) * 0.085f;
            if (sampleProgress < 0.0f) {
                continue;
            }
            cg2::Vector4 afterColor = slash.color;
            afterColor.x *= 1.08f * world_.presentation.playerNeonEmission_;
            afterColor.y *= 1.08f * world_.presentation.playerNeonEmission_;
            afterColor.z *= 1.08f * world_.presentation.playerNeonEmission_;
            afterColor.w *= (std::min)(1.0f, world_.presentation.playerNeonEmission_) * t * (0.10f + 0.07f * static_cast<float>(5 - i)) *
                            world_.presentation.playerMeleeAfterimageAlphaScale_;
            const float afterLength = slash.range * slash.bladeLengthScale * (0.78f + 0.04f * static_cast<float>(5 - i));
            queueBlade(hilt, directionAt(slash, sampleProgress), afterLength, slash.width * 0.46f, afterColor, 0.20f);
        }

        const float motionLengthScale = isWindup ? 0.86f : (isSwinging ? 0.92f + swingProgress * 0.08f : 1.0f);
        queueBlade(hilt, bladeDir, slash.range * slash.bladeLengthScale * motionLengthScale, slash.width, color, 0.94f);
    }
}
} // namespace gameplay
