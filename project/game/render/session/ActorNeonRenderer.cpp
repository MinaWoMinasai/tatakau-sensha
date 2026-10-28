#include "game/render/session/ActorNeonRenderer.h"
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

void ActorNeonRenderer::QueueActorNeonBillboards(const myEngine::Vector3& cameraRight, const myEngine::Vector3& cameraUp, bool drawBodies,
                                                 bool drawBarrels)
{
    if (!world_.resources.neonGridRenderer_) {
        return;
    }

    auto directionToRotation = [&](const myEngine::Vector3& worldDirection) {
        myEngine::Vector3 direction = worldDirection;
        if (myEngine::Length(direction) < 0.001f) {
            direction = {0.0f, -1.0f, 0.0f};
        }
        direction = myEngine::Normalize(direction);
        return std::atan2(myEngine::Dot(direction, cameraRight), -myEngine::Dot(direction, cameraUp));
    };

    auto lerpColor = [](const myEngine::Vector4& a, const myEngine::Vector4& b, float t) {
        t = (std::clamp)(t, 0.0f, 1.0f);
        return myEngine::Vector4{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
    };
    auto rotateDirection = [](const myEngine::Vector3& direction, float angle) {
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        return myEngine::Vector3{direction.x * c - direction.y * s, direction.x * s + direction.y * c, direction.z};
    };

    auto queueTankBillboard = [&](const myEngine::Vector3& center, const myEngine::Vector3& direction, float radius, float lineWidth,
                                  const myEngine::Vector4& color, const Player::NeonBodyLayout* bodyLayout,
                                  const std::vector<Player::NeonBarrelLayout>* barrelLayouts, bool allowIdleMeleeSaber,
                                  bool allowMuzzleFlash, float emission) {
        constexpr float kTwoPi = 6.28318530718f;
        auto queueBodyPolygon = [&](int segments, float rotation, const myEngine::Vector2& scale, const myEngine::Vector4& outlineColor) {
            myEngine::Vector4 emitted = outlineColor;
            emitted.x *= emission;
            emitted.y *= emission;
            emitted.z *= emission;
            tankneon::QueueBodyOutline(*world_.resources.neonGridRenderer_, center, radius, lineWidth, segments, rotation, scale, emitted,
                                       cameraRight, cameraUp, true);
        };

        myEngine::Vector3 mainDirection = direction;
        if (myEngine::Length(mainDirection) < 0.001f) {
            mainDirection = {0.0f, -1.0f, 0.0f};
        }
        mainDirection = myEngine::Normalize(mainDirection);
        const float mainRotation = directionToRotation(mainDirection);
        const myEngine::Vector3 mainForward = cameraRight * std::sin(mainRotation) + cameraUp * -std::cos(mainRotation);
        const myEngine::Vector3 mainRight = cameraRight * std::cos(mainRotation) + cameraUp * std::sin(mainRotation);

        if (drawBodies) {
            const Player::NeonBodyLayout defaultBody{};
            const Player::NeonBodyLayout& body = bodyLayout ? *bodyLayout : defaultBody;
            myEngine::Vector4 outlineColor = body.outlineColor;
            if (outlineColor.w <= 0.001f) {
                outlineColor = color;
            } else {
                outlineColor.w *= color.w;
                outlineColor = lerpColor(outlineColor, {1.8f, 1.8f, 1.8f, outlineColor.w},
                                         allowMuzzleFlash && world_.resources.player_
                                             ? std::pow(world_.resources.player_->GetDamageFeedbackRatio(), 2.0f) * 0.75f
                                             : 0.0f);
            }
            switch (body.shape) {
            case Player::BodyShape::Box:
                queueBodyPolygon(4, mainRotation + kTwoPi * 0.125f, body.scale, outlineColor);
                break;
            case Player::BodyShape::Triangle:
                queueBodyPolygon(3, mainRotation - kTwoPi * 0.25f, body.scale, outlineColor);
                break;
            case Player::BodyShape::Pentagon:
                queueBodyPolygon(5, mainRotation - kTwoPi * 0.25f, body.scale, outlineColor);
                break;
            case Player::BodyShape::Circle:
            default:
                queueBodyPolygon(28, mainRotation, body.scale, outlineColor);
                break;
            }
        }

        auto queueIdleMeleeSaber = [&](const Player::NeonBarrelLayout& layout) {
            if (!allowIdleMeleeSaber || !world_.presentation.showPlayerIdleMeleeSaber_ ||
                !world_.presentation.playerMeleeSlashes_.empty()) {
                return;
            }
            const float sideSign = layout.offset.y < -0.05f ? -1.0f : 1.0f;
            const float angle = world_.presentation.playerIdleSaberAngleDeg_ * 3.1415926535f / 180.0f * sideSign;
            const myEngine::Vector3 saberDir = myEngine::Normalize(mainForward * std::cos(angle) + mainRight * std::sin(angle));
            const myEngine::Vector3 saberRight = myEngine::Normalize(mainRight * std::cos(angle) - mainForward * std::sin(angle));
            const myEngine::Vector3 hiltCenter = center + mainForward * (world_.presentation.playerIdleSaberForwardOffset_ * radius) +
                                                 mainRight * (world_.presentation.playerIdleSaberSideOffset_ * radius * sideSign) +
                                                 myEngine::Vector3{0.0f, 0.0f, layout.offset.z};
            const float bladeLength = (std::max)(0.05f, world_.presentation.playerIdleSaberLength_) * radius;
            const float bladeWidth = (std::max)(0.005f, world_.presentation.playerIdleSaberBladeWidth_) * radius;
            const float hiltLength = (std::max)(0.02f, world_.presentation.playerIdleSaberHiltLength_) * radius;
            const myEngine::Vector3 hiltBase = hiltCenter - saberDir * (hiltLength * 0.45f);
            const myEngine::Vector3 bladeBase = hiltCenter + saberDir * (hiltLength * 0.42f);
            const myEngine::Vector3 bladeTip = bladeBase + saberDir * bladeLength;

            myEngine::Vector4 hiltColor = color;
            hiltColor.w *= 0.88f;
            world_.resources.neonGridRenderer_->QueueLine(hiltBase, bladeBase, bladeWidth * 0.72f, hiltColor);
            world_.resources.neonGridRenderer_->QueueLine(hiltCenter - saberRight * (bladeWidth * 1.55f),
                                                          hiltCenter + saberRight * (bladeWidth * 1.55f), bladeWidth * 0.42f, hiltColor);

            myEngine::Vector4 outerColor = color;
            outerColor.x *= 1.20f;
            outerColor.y *= 1.20f;
            outerColor.z *= 1.20f;
            outerColor.w *= 0.28f;
            world_.resources.neonGridRenderer_->QueueLine(bladeBase, bladeTip,
                                                          bladeWidth * world_.presentation.playerIdleSaberOuterWidthScale_, outerColor);

            myEngine::Vector4 haloColor = color;
            haloColor.x *= 1.35f;
            haloColor.y *= 1.35f;
            haloColor.z *= 1.35f;
            haloColor.w *= 0.78f;
            world_.resources.neonGridRenderer_->QueueLine(bladeBase + saberDir * (bladeLength * 0.03f), bladeTip, bladeWidth, haloColor);

            myEngine::Vector4 coreColor = {1.0f, 1.0f, 1.0f, color.w * 0.82f};
            world_.resources.neonGridRenderer_->QueueLine(bladeBase + saberDir * (bladeLength * 0.08f), bladeTip,
                                                          bladeWidth * world_.presentation.playerIdleSaberCoreWidthScale_, coreColor);
        };

        auto queueBarrel = [&](const Player::NeonBarrelLayout& layout) {
            if (layout.isMelee) {
                queueIdleMeleeSaber(layout);
                return;
            }
            auto groupColor = [&](int group, const myEngine::Vector4& fallback) {
                static const myEngine::Vector4 colors[] = {{0.55f, 1.00f, 0.35f, 1.0f}, {1.00f, 0.86f, 0.25f, 1.0f},
                                                           {0.30f, 0.92f, 1.00f, 1.0f}, {1.00f, 0.36f, 0.72f, 1.0f},
                                                           {0.76f, 0.54f, 1.00f, 1.0f}, {1.00f, 0.58f, 0.32f, 1.0f}};
                if (group < 0) {
                    return fallback;
                }
                myEngine::Vector4 result = colors[static_cast<size_t>(group) % (sizeof(colors) / sizeof(colors[0]))];
                result.w = fallback.w > 0.001f ? fallback.w : color.w;
                return result;
            };
            const myEngine::Vector3 barrelDirection = myEngine::Normalize(rotateDirection(mainDirection, layout.angleRad));
            const float barrelRotation = directionToRotation(barrelDirection);
            const myEngine::Vector3 barrelForward = cameraRight * std::sin(barrelRotation) + cameraUp * -std::cos(barrelRotation);
            const myEngine::Vector3 barrelRight = cameraRight * std::cos(barrelRotation) + cameraUp * std::sin(barrelRotation);
            const myEngine::Vector3 barrelCenter = center + mainForward * ((layout.offset.x - layout.recoilOffset) * radius) +
                                                   mainRight * (layout.offset.y * radius) + myEngine::Vector3{0.0f, 0.0f, layout.offset.z};
            float length = (std::max)(radius * 0.28f, layout.scale.x * radius * 0.72f);
            float half = (std::max)(lineWidth * 1.5f, layout.scale.y * radius * 0.75f);
            if (layout.shape == BarrelShape::Heavy) {
                length *= 1.12f;
                half *= 1.45f;
            } else if (layout.shape == BarrelShape::Short) {
                length *= 0.58f;
                half *= 1.08f;
            } else if (layout.shape == BarrelShape::Wide) {
                length *= 0.86f;
                half *= 1.80f;
            }
            const myEngine::Vector3 base = barrelCenter - barrelForward * (length * 0.20f);
            const myEngine::Vector3 tip = barrelCenter + barrelForward * (length * 0.80f);
            myEngine::Vector4 barrelColor = groupColor(layout.fireGroup, layout.outlineColor.w > 0.001f ? layout.outlineColor : color);
            barrelColor.w *= color.w;
            const float muzzleFlashRatio = allowMuzzleFlash ? (std::clamp)(layout.muzzleFlashRatio, 0.0f, 1.0f) : 0.0f;
            const float muzzleFlash = muzzleFlashRatio * muzzleFlashRatio;
            barrelColor = lerpColor(barrelColor, {1.80f, 1.80f, 1.50f, barrelColor.w}, muzzleFlash * 0.80f);
            barrelColor.x *= emission;
            barrelColor.y *= emission;
            barrelColor.z *= emission;
            tankneon::QueueBarrelOutline(*world_.resources.neonGridRenderer_, base, tip, barrelRight, half, lineWidth, barrelColor,
                                         layout.shape == BarrelShape::Trapezoid, true, myEngine::Cross(cameraRight, cameraUp));

            if (muzzleFlashRatio > 0.0f) {
                auto queueMuzzleRing = [&](float ringRadius, int segments, float ringLineWidth, const myEngine::Vector4& ringColor) {
                    myEngine::Vector3 previous{};
                    for (int i = 0; i <= segments; ++i) {
                        const float angle = static_cast<float>(i % segments) * (kTwoPi / static_cast<float>(segments));
                        const myEngine::Vector3 current =
                            tip + cameraRight * (std::cos(angle) * ringRadius) + cameraUp * (std::sin(angle) * ringRadius);
                        if (i > 0) {
                            world_.resources.neonGridRenderer_->QueueLine(previous, current, ringLineWidth, ringColor);
                        }
                        previous = current;
                    }
                };

                const float coreRadius = radius * (0.08f + muzzleFlashRatio * 0.04f);
                const float haloRadius = radius * (0.15f + muzzleFlashRatio * 0.09f);
                const myEngine::Vector4 coreColor{2.20f, 2.10f, 1.65f, muzzleFlashRatio};
                const myEngine::Vector4 haloColor{1.35f + muzzleFlash * 0.45f, 1.55f + muzzleFlash * 0.35f, 0.75f + muzzleFlash * 0.35f,
                                                  muzzleFlashRatio * 0.42f};
                queueMuzzleRing(haloRadius, 14, lineWidth * 0.65f, haloColor);
                queueMuzzleRing(coreRadius, 10, lineWidth * 1.15f, coreColor);

                const float sparkLength = radius * (0.10f + muzzleFlashRatio * 0.10f);
                const myEngine::Vector3 sparkTip = tip + barrelForward * sparkLength;
                world_.resources.neonGridRenderer_->QueueLine(tip, sparkTip, lineWidth * 1.15f, coreColor);
                world_.resources.neonGridRenderer_->QueueLine(tip, sparkTip + barrelRight * coreRadius, lineWidth * 0.72f, haloColor);
                world_.resources.neonGridRenderer_->QueueLine(tip, sparkTip - barrelRight * coreRadius, lineWidth * 0.72f, haloColor);
            }
        };

        if (!drawBarrels) {
            return;
        }
        if (barrelLayouts && !barrelLayouts->empty()) {
            for (const Player::NeonBarrelLayout& layout : *barrelLayouts) {
                queueBarrel(layout);
            }
        } else {
            queueBarrel({});
        }
    };

    const std::vector<Player::NeonBarrelLayout> playerBarrels =
        world_.resources.player_ ? world_.resources.player_->GetNeonBarrelLayouts() : std::vector<Player::NeonBarrelLayout>{};
    const Player::NeonBodyLayout playerBody =
        world_.resources.player_ ? world_.resources.player_->GetNeonBodyLayout() : Player::NeonBodyLayout{};

    for (const PlayerNeonAfterimage& afterimage : world_.presentation.playerNeonAfterimages_) {
        const float lifeRatio =
            (std::clamp)(afterimage.life / (std::max)(0.001f, world_.presentation.playerAfterimageLifetime_), 0.0f, 1.0f);
        myEngine::Vector4 color = world_.presentation.playerGridColor_;
        color.w *= world_.presentation.playerAfterimageAlpha_ * lifeRatio * lifeRatio;
        const float radius = world_.presentation.playerNeonBillboardRadius_ * (0.88f + lifeRatio * 0.12f);
        queueTankBillboard(afterimage.position + myEngine::Vector3{0.0f, 0.0f, 0.35f}, afterimage.direction, radius,
                           world_.presentation.actorNeonBillboardLineWidth_, color, &playerBody, &playerBarrels, false, false,
                           world_.presentation.playerNeonEmission_);
    }

    if (world_.presentation.playerNeonRenderMode_ == 1 && world_.resources.player_ && !world_.resources.player_->IsDead()) {
        myEngine::Vector4 color = world_.presentation.playerGridColor_;
        const float feedback = world_.resources.player_->GetDamageFeedbackRatio();
        const float pulse = std::sin(feedback * 3.14159265f) * 0.10f;
        color = lerpColor(color, {1.8f, 1.8f, 1.8f, color.w}, feedback * feedback * 0.75f);
        if (world_.resources.player_->IsDashing()) {
            color.w *= world_.presentation.playerDashCurrentAlpha_;
        }
        queueTankBillboard(world_.resources.player_->GetWorldPosition() + myEngine::Vector3{0.0f, 0.0f, 0.35f},
                           world_.resources.player_->GetDirection(), world_.presentation.playerNeonBillboardRadius_ * (1.0f + pulse),
                           world_.presentation.actorNeonBillboardLineWidth_, color, &playerBody, &playerBarrels, true, true,
                           world_.presentation.playerNeonEmission_);
        if (drawBarrels && world_.combat.showFollowHpBars_ && (!world_.run.IsExpeditionRun() || world_.run.tankExpedition_.IsCombat())) {
            // 浮かんだHP/スタミナバーの代わりに、本体に沿った分割ゲージを表示する。
            // HPは本体色の下側の弧、スタミナはそれより短い上側の弧。
            const myEngine::Vector3 center = world_.resources.player_->GetWorldPosition() + myEngine::Vector3{0.0f, 0.0f, 0.35f};
            const float radius = world_.presentation.playerNeonBillboardRadius_;
            auto queueSegmentedArc = [&](float arcRadius, float centerAngle, float span, int segments, float ratio,
                                         const myEngine::Vector4& lit, float thickness) {
                const float step = span / static_cast<float>(segments);
                const int litSegments =
                    static_cast<int>(std::ceil((std::clamp)(ratio, 0.0f, 1.0f) * static_cast<float>(segments) - 0.001f));
                for (int i = 0; i < segments; ++i) {
                    const float angle = centerAngle - span * 0.5f + step * (static_cast<float>(i) + 0.5f);
                    const myEngine::Vector3 radial = cameraRight * std::sin(angle) - cameraUp * std::cos(angle);
                    const myEngine::Vector3 tangent = cameraRight * std::cos(angle) + cameraUp * std::sin(angle);
                    const myEngine::Vector3 middle = center + radial * arcRadius;
                    const float half = arcRadius * step * 0.36f;
                    const bool on = i < litSegments;
                    const myEngine::Vector4 segmentColor = on ? lit : myEngine::Vector4{lit.x * 0.18f, lit.y * 0.18f, lit.z * 0.18f, 0.45f};
                    world_.resources.neonGridRenderer_->QueueLine(middle - tangent * half, middle + tangent * half, thickness,
                                                                  segmentColor);
                }
            };
            const float hpRatio = static_cast<float>(world_.resources.player_->GetHp()) /
                                  static_cast<float>((std::max)(1, world_.resources.player_->GetMaxHp()));
            myEngine::Vector4 hpColor = world_.presentation.playerGridColor_;
            hpColor = lerpColor({hpColor.x * 1.25f, hpColor.y * 1.25f, hpColor.z * 1.25f, 1.0f}, {1.9f, 0.35f, 0.25f, 1.0f},
                                hpRatio < 0.3f ? 0.75f : 0.0f);
            queueSegmentedArc(radius * 1.55f, 0.0f, 2.05f, 12, hpRatio, hpColor, radius * 0.30f);
            if (world_.combat.showPlayerStaminaBar_) {
                const Player::PlayerStats& stats = world_.resources.player_->GetStats();
                const float staminaRatio = stats.maxStamina > 0.0f ? stats.stamina / stats.maxStamina : 0.0f;
                queueSegmentedArc(radius * 1.52f, 3.14159265f, 1.15f, 6, staminaRatio, {1.75f, 1.45f, 0.35f, 1.0f}, radius * 0.20f);
            }
        }
    }
    if (!world_.bossPresentation->UseNeonBossVisual() && world_.presentation.bossNeonRenderMode_ == 1 && world_.resources.enemy_ &&
        (world_.gameplayQueries->IsRunRivalActive() && !world_.resources.enemy_->IsDead())) {
        const float feedback = world_.resources.enemy_->GetDamageFeedbackRatio();
        const float impact = feedback * feedback;
        const myEngine::Vector4 color =
            lerpColor(world_.presentation.enemyGridColor_, {1.8f, 1.8f, 1.8f, world_.presentation.enemyGridColor_.w},
                      (std::min)(1.0f, impact * 0.95f));
        Player::NeonBodyLayout bossBody{};
        bossBody.outlineColor = color;
        Player::NeonBarrelLayout bossBarrel{};
        bossBarrel.offset = {world_.presentation.bossNeonBarrelForwardOffset_, world_.presentation.bossNeonBarrelSideOffset_, 0.0f};
        bossBarrel.scale = {world_.presentation.bossNeonBarrelLengthScale_, world_.presentation.bossNeonBarrelWidthScale_,
                            world_.presentation.bossNeonBarrelWidthScale_};
        bossBarrel.angleRad = world_.presentation.bossNeonBarrelAngleDeg_ * 3.1415926535f / 180.0f;
        bossBarrel.fireGroup = -1;
        bossBarrel.outlineColor = color;
        const std::vector<Player::NeonBarrelLayout> bossBarrels = {bossBarrel};
        queueTankBillboard(
            world_.resources.enemy_->GetWorldPosition() + myEngine::Vector3{0.0f, 0.0f, 0.35f}, world_.resources.enemy_->GetAimDirection(),
            world_.presentation.bossNeonBillboardRadius_ * (1.0f + impact * 0.07f), world_.presentation.actorNeonBillboardLineWidth_, color,
            &bossBody, &bossBarrels, false, false, world_.presentation.bossNeonEmission_);
    }
    if (world_.run.UsesPrototypeRules() && world_.resources.player_ && world_.presentation.playerNeonRenderMode_ == 1) {
        for (PlayerDrone* drone : world_.resources.player_->GetDronePtrs()) {
            if (!drone || !drone->IsVisualVisible()) {
                continue;
            }
            constexpr float radius = 0.78f;
            const myEngine::Vector3 center = drone->GetWorldPosition() + myEngine::Vector3{0.0f, 0.0f, 0.35f};
            const myEngine::Vector4 color{0.24f, 1.0f, 0.78f, 0.95f};
            Player::NeonBodyLayout droneBody{};
            droneBody.outlineColor = color;
            Player::NeonBarrelLayout droneBarrel{};
            droneBarrel.offset = {0.86f, 0.0f, 0.0f};
            droneBarrel.scale = {0.72f, 0.18f, 0.18f};
            droneBarrel.fireGroup = -1;
            droneBarrel.outlineColor = color;
            droneBarrel.muzzleFlashRatio = drone->GetNeonMuzzleFlashRatio();
            const std::vector<Player::NeonBarrelLayout> droneBarrels{droneBarrel};
            queueTankBillboard(center, drone->GetAimDirection(), radius, world_.presentation.actorNeonBillboardLineWidth_ * 0.75f, color,
                               &droneBody, &droneBarrels, false, true, world_.presentation.playerNeonEmission_);
            // 大きな光る円盤を描かずに、短い推進の筋で動きを伝える。
            const myEngine::Vector3 motion = drone->GetMove();
            const float speed = myEngine::Length(motion);
            if (drawBarrels && speed > 0.03f) {
                const myEngine::Vector3 backward = motion / speed;
                world_.resources.neonGridRenderer_->QueueLine(center - backward * radius,
                                                              center - backward * (radius + (std::min)(0.9f, speed * 3.0f)), 0.055f,
                                                              {0.20f, 0.88f, 1.1f, 0.24f});
            }
        }
    }
}

void ActorNeonRenderer::DrawActorNeonBodyFillPass(bool restoreOutlines, BodySet bodies)
{
    const bool drawRound = bodies != BodySet::Enemies;
    const bool drawEnemies = bodies != BodySet::RoundActors;
    if (!world_.resources.neonGridRenderer_ || !world_.presentation.fillActorNeonBodies_) {
        return;
    }

    const myEngine::Matrix4x4 viewMatrix = myEngine::Object3dCommon::GetInstance()->GetIsDebugCamera()
                                               ? world_.resources.debugCamera->GetViewMatrix()
                                               : world_.resources.camera->GetViewMatrix();
    const myEngine::Matrix4x4 cameraWorld = myEngine::Inverse(viewMatrix);
    const myEngine::Vector3 cameraRight = myEngine::Normalize({cameraWorld.m[0][0], cameraWorld.m[0][1], cameraWorld.m[0][2]});
    const myEngine::Vector3 cameraUp = myEngine::Normalize({cameraWorld.m[1][0], cameraWorld.m[1][1], cameraWorld.m[1][2]});
    const myEngine::Matrix4x4 vp = myEngine::Object3dCommon::GetInstance()->GetIsDebugCamera()
                                       ? world_.resources.debugCamera->GetViewProjectionMatrix()
                                       : world_.resources.camera->GetViewProjectionMatrix();

    auto directionToRotation = [&](const myEngine::Vector3& worldDirection) {
        myEngine::Vector3 direction = worldDirection;
        if (myEngine::Length(direction) < 0.001f) {
            direction = {0.0f, -1.0f, 0.0f};
        }
        direction = myEngine::Normalize(direction);
        return std::atan2(myEngine::Dot(direction, cameraRight), -myEngine::Dot(direction, cameraUp));
    };
    // このパスの本体は、現在の視点から照らされた光沢のある黒い立体になる。
    const myEngine::Vector3 eye{cameraWorld.m[3][0], cameraWorld.m[3][1], cameraWorld.m[3][2]};
    world_.resources.neonGridRenderer_->SetBodyShadingEye(&eye);
    auto queueRoundBody = [&](const myEngine::Vector3& center, float radius, const myEngine::Vector4& tint) {
        constexpr int kSegments = 32;
        myEngine::Vector3 points[kSegments]{};
        for (int i = 0; i < kSegments; ++i) {
            const float angle = 6.28318530718f * static_cast<float>(i) / static_cast<float>(kSegments);
            points[i] = center + cameraRight * (std::cos(angle) * radius) + cameraUp * (std::sin(angle) * radius);
        }
        world_.resources.neonGridRenderer_->QueueBeveledPolygonFill(points, kSegments, world_.presentation.actorNeonBodyFillColor_, tint,
                                                                    myEngine::Normalize(cameraUp - cameraRight * 0.5f));
    };
    // ネオンの砲身の輪郭の下に黒い砲のブロックを置く（配置計算は輪郭パスと同じ）。
    auto queueBarrelBlocks = [&](const myEngine::Vector3& center, const myEngine::Vector3& direction, float radius,
                                 const std::vector<Player::NeonBarrelLayout>& layouts, const myEngine::Vector4& tint) {
        myEngine::Vector3 mainDirection =
            myEngine::Length(direction) < 0.001f ? myEngine::Vector3{0.0f, -1.0f, 0.0f} : myEngine::Normalize(direction);
        const float mainRotation = directionToRotation(mainDirection);
        const myEngine::Vector3 mainForward = cameraRight * std::sin(mainRotation) + cameraUp * -std::cos(mainRotation);
        const myEngine::Vector3 mainRight = cameraRight * std::cos(mainRotation) + cameraUp * std::sin(mainRotation);
        for (const Player::NeonBarrelLayout& layout : layouts) {
            if (layout.isMelee) {
                continue;
            }
            const float c = std::cos(layout.angleRad), s = std::sin(layout.angleRad);
            const myEngine::Vector3 barrelDirection = myEngine::Normalize(
                myEngine::Vector3{mainDirection.x * c - mainDirection.y * s, mainDirection.x * s + mainDirection.y * c, 0.0f});
            const float barrelRotation = directionToRotation(barrelDirection);
            const myEngine::Vector3 barrelForward = cameraRight * std::sin(barrelRotation) + cameraUp * -std::cos(barrelRotation);
            const myEngine::Vector3 barrelRight = cameraRight * std::cos(barrelRotation) + cameraUp * std::sin(barrelRotation);
            const myEngine::Vector3 barrelCenter = center + mainForward * ((layout.offset.x - layout.recoilOffset) * radius) +
                                                   mainRight * (layout.offset.y * radius) + myEngine::Vector3{0.0f, 0.0f, layout.offset.z};
            float length = (std::max)(radius * 0.28f, layout.scale.x * radius * 0.72f);
            float half = layout.scale.y * radius * 0.75f;
            if (layout.shape == BarrelShape::Heavy) {
                length *= 1.12f;
                half *= 1.45f;
            } else if (layout.shape == BarrelShape::Short) {
                length *= 0.58f;
                half *= 1.08f;
            } else if (layout.shape == BarrelShape::Wide) {
                length *= 0.86f;
                half *= 1.80f;
            }
            const myEngine::Vector3 base = barrelCenter - barrelForward * (length * 0.20f);
            const myEngine::Vector3 tip = barrelCenter + barrelForward * (length * 0.80f);
            world_.resources.neonGridRenderer_->QueueShadedBox(base, tip, barrelRight, half * 0.92f, radius * 0.34f,
                                                               world_.presentation.actorNeonBodyFillColor_, tint);
        }
    };
    auto queueBodyFill = [&](const myEngine::Vector3& center, const myEngine::Vector3& direction, float radius,
                             const Player::NeonBodyLayout& body, const myEngine::Vector4& fillColor) {
        constexpr float kTwoPi = 6.28318530718f;
        const float mainRotation = directionToRotation(direction);
        int segments = 32;
        float rotation = mainRotation;
        switch (body.shape) {
        case Player::BodyShape::Box:
            segments = 4;
            rotation = mainRotation + kTwoPi * 0.125f;
            break;
        case Player::BodyShape::Triangle:
            segments = 3;
            rotation = mainRotation - kTwoPi * 0.25f;
            break;
        case Player::BodyShape::Pentagon:
            segments = 5;
            rotation = mainRotation - kTwoPi * 0.25f;
            break;
        case Player::BodyShape::Circle:
        default:
            segments = 32;
            break;
        }
        myEngine::Vector3 points[32]{};
        for (int i = 0; i < segments; ++i) {
            const float angle = rotation + kTwoPi * i / segments;
            points[i] =
                center + cameraRight * (std::cos(angle) * radius * body.scale.x) + cameraUp * (std::sin(angle) * radius * body.scale.y);
        }
        const myEngine::Vector4 tint = body.outlineColor.w > 0.001f ? body.outlineColor : world_.presentation.playerGridColor_;
        world_.resources.neonGridRenderer_->QueueBeveledPolygonFill(points, segments, fillColor, tint,
                                                                    myEngine::Normalize(cameraUp - cameraRight * 0.5f));
    };

    const uint32_t fillStart = world_.resources.neonGridRenderer_->GetVertexCount();
    // Bloomをかけた線のキャプチャの後にシーンへ描く。丸い本体はリングの内側に収まり、
    // リングの内側のハローを覆うので、黒い球として見える。
    const float roundBodyScale = bodies == BodySet::RoundActors ? 0.86f / 0.96f : 1.0f;
    if (drawRound && world_.presentation.playerNeonRenderMode_ == 1 && world_.resources.player_ && !world_.resources.player_->IsDead()) {
        const float feedback = world_.resources.player_->GetDamageFeedbackRatio();
        const float pulse = std::sin(feedback * 3.14159265f) * 0.10f;
        queueBarrelBlocks(world_.resources.player_->GetWorldPosition() + myEngine::Vector3{0.0f, 0.0f, 0.345f},
                          world_.resources.player_->GetDirection(), world_.presentation.playerNeonBillboardRadius_ * (1.0f + pulse),
                          world_.resources.player_->GetNeonBarrelLayouts(), world_.presentation.playerGridColor_);
        queueBodyFill(world_.resources.player_->GetWorldPosition() + myEngine::Vector3{0.0f, 0.0f, 0.345f},
                      world_.resources.player_->GetDirection(),
                      world_.presentation.playerNeonBillboardRadius_ * (1.0f + pulse) * 0.96f * roundBodyScale,
                      world_.resources.player_->GetNeonBodyLayout(), world_.presentation.actorNeonBodyFillColor_);
    }
    if (drawRound && !world_.bossPresentation->UseNeonBossVisual() && world_.presentation.bossNeonRenderMode_ == 1 &&
        world_.resources.enemy_ && (world_.gameplayQueries->IsRunRivalActive() && !world_.resources.enemy_->IsDead())) {
        const float feedback = world_.resources.enemy_->GetDamageFeedbackRatio();
        const float impact = feedback * feedback;
        queueRoundBody(world_.resources.enemy_->GetWorldPosition() + myEngine::Vector3{0.0f, 0.0f, 0.345f},
                       world_.presentation.bossNeonBillboardRadius_ * (1.0f + impact * 0.07f) * 0.96f * roundBodyScale,
                       world_.presentation.enemyGridColor_);
    }
    if (drawRound && world_.run.UsesPrototypeRules() && world_.resources.player_ && world_.presentation.playerNeonRenderMode_ == 1) {
        for (PlayerDrone* drone : world_.resources.player_->GetDronePtrs()) {
            if (!drone || !drone->IsVisualVisible()) {
                continue;
            }
            queueRoundBody(drone->GetWorldPosition() + myEngine::Vector3{0.0f, 0.0f, 0.345f}, 0.78f * 0.96f * roundBodyScale,
                           {0.24f, 1.0f, 0.78f, 0.95f});
        }
    }
    if (drawEnemies && world_.resources.enemyManager_ && world_.presentation.expEnemyNeonRenderMode_ == 1) {
        const myEngine::Vector3 cameraForward = myEngine::Cross(cameraRight, cameraUp);
        for (ExpEnemy* enemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
            if (!enemy || enemy->IsDead() || !enemy->IsShapeNeonRenderTarget() ||
                !world_.resourceNeonRenderer->IsNearCamera2D(enemy->GetWorldPosition(), 38.0f, 24.0f, 3.0f)) {
                continue;
            }
            if (enemy->IsExpeditionCombatRole()) {
                enemy->QueueCombatVisuals(*world_.resources.neonGridRenderer_, cameraRight, cameraUp, cameraForward,
                                          world_.presentation.expEnemyNeonLineWidth_, true);
                continue;
            }
            const auto type = enemy->GetType();
            const int sides = type == ExpEnemyType::Triangle   ? 3
                              : type == ExpEnemyType::Pentagon ? 5
                              : type == ExpEnemyType::Shooter  ? 20
                                                               : 4;
            const float size = type == ExpEnemyType::Triangle   ? world_.presentation.expEnemyNeonTriangleRadius_
                               : type == ExpEnemyType::Pentagon ? world_.presentation.expEnemyNeonPentagonRadius_
                               : type == ExpEnemyType::Shooter  ? world_.presentation.expEnemyNeonShooterRadius_
                                                                : world_.presentation.expEnemyNeonSquareSize_ * 0.70710678f;
            const myEngine::Vector3 visual = enemy->GetVisualScale();
            const float scale = (std::max)({visual.x, visual.y, visual.z});
            const float rotation = enemy->GetVisualRotation() + (sides == 4 ? 0.78539816f : -1.57079633f);
            myEngine::Vector3 points[20]{};
            const myEngine::Vector3 center = enemy->GetWorldPosition() + myEngine::Vector3{0, 0, 0.345f};
            for (int i = 0; i < sides; ++i) {
                const float a = rotation + 6.28318530718f * i / sides;
                points[i] = center + (cameraRight * std::cos(a) + cameraUp * std::sin(a)) * (size * scale * 0.96f);
            }
            world_.resources.neonGridRenderer_->QueueBeveledPolygonFill(points, sides, world_.presentation.actorNeonBodyFillColor_,
                                                                        enemy->GetVisualColor(),
                                                                        myEngine::Normalize(cameraUp - cameraRight * 0.5f));
        }
    }
    world_.resources.neonGridRenderer_->SetBodyShadingEye(nullptr);
    const uint32_t fillCount = world_.resources.neonGridRenderer_->GetVertexCount() - fillStart;
    world_.resources.neonGridRenderer_->DrawRangeSolid(fillStart, fillCount, vp);
    if (!restoreOutlines) {
        return;
    }

    const uint32_t outlineStart = world_.resources.neonGridRenderer_->GetVertexCount();
    QueueActorNeonBillboards(cameraRight, cameraUp, true, false);
    const uint32_t outlineCount = world_.resources.neonGridRenderer_->GetVertexCount() - outlineStart;
    world_.resources.neonGridRenderer_->DrawRange(outlineStart, outlineCount, vp);
}
} // namespace gameplay
