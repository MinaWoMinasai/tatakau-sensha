#include "game/effects/session/CombatEffects.h"
#include "game/exp/ExpEnemy.h"
#include "game/render/session/TubeEffects.h"
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

void CombatEffects::TriggerDeathPostPulse(const myEngine::Vector3& worldPosition, float strength, float shakeScale)
{
    if (!world_.presentation.enableDeathPostPulse_) {
        return;
    }

    const myEngine::Vector2 screen = world_.gameplayHud->WorldToScreen(worldPosition);
    world_.presentation.deathPostPulse_.center = {(std::clamp)(screen.x / static_cast<float>(myEngine::WinApp::kClientWidth), 0.0f, 1.0f),
                                                  (std::clamp)(screen.y / static_cast<float>(myEngine::WinApp::kClientHeight), 0.0f, 1.0f)};
    world_.presentation.deathPostPulseScale_ = (std::clamp)(strength, 0.5f, 1.6f);
    world_.presentation.deathPostPulseTimer_ = world_.presentation.deathPostPulseDuration_;
    UpdateDeathPostPulse(0.0f);

    world_.presentation.cameraShakeTimer_ = world_.presentation.cameraShakeDuration_;
    world_.presentation.cameraShakePower_ =
        (std::max)(world_.presentation.cameraShakePower_, 0.65f * world_.presentation.deathPostPulseScale_ * (std::max)(0.0f, shakeScale));
}

void CombatEffects::UpdateDeathPostPulse(float deltaTime)
{
    if (world_.presentation.deathPostPulseTimer_ <= 0.0f || !world_.presentation.enableDeathPostPulse_) {
        world_.presentation.deathPostPulseTimer_ = 0.0f;
        world_.presentation.deathPostPulse_ = {};
        return;
    }

    world_.presentation.deathPostPulseTimer_ = (std::max)(0.0f, world_.presentation.deathPostPulseTimer_ - deltaTime);
    const float age = 1.0f - world_.presentation.deathPostPulseTimer_ / (std::max)(0.001f, world_.presentation.deathPostPulseDuration_);
    const float impactEnvelope = std::sin(age * myEngine::pi) * (1.0f - age * 0.35f);
    const float flashEnvelope = (1.0f - age) * (1.0f - age);
    world_.presentation.deathPostPulse_.radius = 0.025f + age * world_.presentation.deathPostShockwaveMaxRadius_;
    world_.presentation.deathPostPulse_.width = world_.presentation.deathPostShockwaveWidth_;
    world_.presentation.deathPostPulse_.strength =
        world_.presentation.deathPostShockwaveStrength_ * world_.presentation.deathPostPulseScale_ * impactEnvelope;
    world_.presentation.deathPostPulse_.bloomBoost =
        world_.presentation.deathPostBloomBoost_ * world_.presentation.deathPostPulseScale_ * flashEnvelope;
    world_.presentation.deathPostPulse_.chromAbAmount =
        world_.presentation.deathPostChromAbAmount_ * world_.presentation.deathPostPulseScale_ * impactEnvelope;
}

float CombatEffects::BirthShare(const myEngine::Vector3& position) const
{
    int overlapping = 0;
    for (const RecentKill& kill : recentKills_) {
        overlapping += kill.age < 0.12f && myEngine::Length(kill.position - position) < 8.0f ? 1 : 0;
    }
    return 1.0f / (1.0f + world_.presentation.runNeonLook_.effectOverlap * static_cast<float>((std::max)(0, overlapping - 1)));
}

void CombatEffects::SpawnBrokenTube(const ExpEnemy& enemy)
{
    const auto& look = world_.presentation.runNeonLook_;
    if (!look.tubeEffects || enemy.GetType() == ExpEnemyType::Shooter || !enemy.IsShapeNeonRenderTarget()) {
        return;
    }
    myEngine::Vector4 color = enemy.GetBaseColor();
    color.x *= look.enemyColor;
    color.y *= look.enemyColor;
    color.z *= look.enemyColor;
    color.w = 1.0f;
    recentKills_.push_back({enemy.GetWorldPosition(), color, 0.0f});
    const float share = BirthShare(enemy.GetWorldPosition());
    // 閃光：種類の色の広い淡い光を、倒した瞬間に 0.08 秒だけ出す。
    if (look.effectFlash > 0.0f) {
        NeonTriangleParticle flash{};
        flash.position = enemy.GetWorldPosition() + myEngine::Vector3{0.0f, 0.0f, 0.35f};
        flash.shape = myEngine::NeonParticleShape::Flash;
        flash.radius = 1.6f;
        flash.endRadius = 2.8f;
        flash.life = flash.maxLife = 0.08f;
        flash.birth = share;
        flash.color = color;
        world_.presentation.neonTriangleParticles_.push_back(flash);
    }
    if (!look.effectEdges) {
        return;
    }
    // 今の形の角を求め、辺ごとに、中心から外へ飛びながら回る線にする（角の決め方は敵の線と同じ）。
    // 速さ・向き・回り方・消える時刻は辺ごとにばらばらにする（見本の「選んだ形」）。長さは縮めない。
    auto random = [this](float low, float high) {
        return std::uniform_real_distribution<float>(low, high)(edgeRandom_);
    };
    const myEngine::Vector3 visual = enemy.GetVisualScale();
    const float scale = (std::max)({visual.x, visual.y, visual.z});
    const float rotation = enemy.GetVisualRotation();
    const int sides = enemy.GetType() == ExpEnemyType::Triangle ? 3 : enemy.GetType() == ExpEnemyType::Pentagon ? 5 : 4;
    const float radius = (sides == 3   ? world_.presentation.expEnemyNeonTriangleRadius_
                          : sides == 5 ? world_.presentation.expEnemyNeonPentagonRadius_
                                       : world_.presentation.expEnemyNeonSquareSize_ * 0.70710678f) *
                         scale;
    const float offset = rotation + (sides == 4 ? 0.78539816f : -1.57079633f);
    const myEngine::Vector3 center = enemy.GetWorldPosition() + myEngine::Vector3{0.0f, 0.0f, 0.35f};
    auto corner = [&](int index) {
        const float angle = offset + 6.28318530718f * static_cast<float>(index) / static_cast<float>(sides);
        return myEngine::Vector3{std::cos(angle) * radius, std::sin(angle) * radius, 0.0f};
    };
    for (int i = 0; i < sides; ++i) {
        const myEngine::Vector3 a = corner(i), b = corner(i + 1);
        const myEngine::Vector3 middle = (a + b) * 0.5f, along = b - a;
        NeonTriangleParticle particle{};
        particle.position = center + middle;
        const myEngine::Vector3 out = myEngine::NormalizeOr(middle, myEngine::Vector3{1.0f, 0.0f, 0.0f}, 0.001f);
        const float speed = look.edgeSpeed * (1.0f + look.edgeVary * random(-1.0f, 1.0f));
        const float turn = random(-0.45f, 0.45f) * look.edgeVary;
        particle.velocity = myEngine::Vector3{out.x * std::cos(turn) - out.y * std::sin(turn), out.x * std::sin(turn) + out.y * std::cos(turn), 0.0f} * speed;
        particle.initialVelocity = particle.velocity;
        // 線の向きは「上の向き」からの角度で持つ（ほかの粒子と同じ）。
        particle.rotation = std::atan2(along.y, along.x) - 1.57079633f;
        particle.angularVelocity = random(3.0f, 9.0f) * (random(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f) * (0.4f + look.edgeVary);
        particle.radius = particle.endRadius = myEngine::Length(along) * 0.5f;
        // 大きい敵は、辺の長さに合わせて光っている秒を少し伸ばす。
        particle.edgeOff = look.edgeHold * (std::clamp)(scale, 1.0f, look.edgeLong) + random(0.0f, look.edgeShift);
        particle.life = particle.maxLife = particle.edgeOff + (look.calmBlink ? 0.12f : 0.05f);
        particle.birth = share;
        particle.shape = myEngine::NeonParticleShape::Sliver;
        particle.edge = true;
        particle.color = color;
        world_.presentation.neonTriangleParticles_.push_back(particle);
    }
}

void CombatEffects::UpdateNeonTriangleParticles(float deltaTime)
{
    for (RecentKill& kill : recentKills_) {
        kill.age += deltaTime;
    }
    std::erase_if(recentKills_, [](const RecentKill& kill) {
        return kill.age > 0.3f;
    });
    bool hasImpact = false;
    myEngine::Vector3 lastImpact{};
    for (const myEngine::ParticleManager::NeonTriangleEvent& event :
         myEngine::ParticleManager::GetInstance()->ConsumeNeonTriangleEvents()) {
        // 管の線で描くとき（計画書 p10 の16章の M3）：白い玉は出さず、破片と火花は決めた割合だけ出す。
        const auto& look = world_.presentation.runNeonLook_;
        if (look.tubeEffects) {
            if (event.shape == myEngine::NeonParticleShape::Flash) {
                continue;
            }
            // 一度に出ている数の上限（まとめて倒したときに重くならないように）。輪はいつも出す。
            if (event.shape != myEngine::NeonParticleShape::Ring && world_.presentation.neonTriangleParticles_.size() >= 220) {
                continue;
            }
            const bool spark = event.shape == myEngine::NeonParticleShape::Spark || event.shape == myEngine::NeonParticleShape::Triangle;
            const float keep = event.shape == myEngine::NeonParticleShape::Ring ? (look.effectRing > 0.0f ? 1.0f : 0.0f)
                               : spark                                         ? look.effectSparks
                                                                               : look.effectDebris;
            // 割合の端数を貯めて間引く（乱数を使わない）。
            float& carry = spark ? tubeSparkCarry_ : tubeDebrisCarry_;
            if (event.shape != myEngine::NeonParticleShape::Ring) {
                carry += keep;
                if (carry < 1.0f) {
                    continue;
                }
                carry -= 1.0f;
            } else if (keep <= 0.0f) {
                continue;
            }
        }
        NeonTriangleParticle particle{};
        particle.position = event.position;
        particle.velocity = event.velocity;
        particle.initialVelocity = event.velocity;
        particle.radius = event.radius;
        particle.rotation = event.rotation;
        particle.angularVelocity = event.angularVelocity;
        particle.lineWidth = event.lineWidth;
        particle.life = (std::max)(0.01f, event.lifeTime);
        particle.maxLife = particle.life;
        particle.tiltRad = event.tiltRad;
        particle.trailCopies = static_cast<int>(event.trailCopies);
        particle.isBillboard = event.isBillboard;
        particle.shape = event.shape;
        particle.endRadius = event.endRadius;
        particle.color = event.color;
        if (look.tubeEffects) {
            // 倒した敵の破片と輪は、種類の色にそろえる（2色目の水色の破片も）。
            bool fromKill = false;
            for (const RecentKill& kill : recentKills_) {
                if (myEngine::Length(kill.position - particle.position) < 0.05f) {
                    particle.color = {kill.color.x, kill.color.y, kill.color.z, particle.color.w};
                    fromKill = true;
                    break;
                }
            }
            // 脇役（輪・欠け・火花）：明るさと寿命を下げて、飛ぶ辺を主役にする。命中の火花（カメラを向かない火花）は下げない。
            const bool impact = particle.shape == myEngine::NeonParticleShape::Spark && !particle.isBillboard;
            if (!impact) {
                particle.color.w *= look.effectSoft;
                if (particle.shape != myEngine::NeonParticleShape::Ring) {
                    particle.life *= look.effectSoftLife;
                    particle.maxLife = particle.life;
                }
            }
            if (particle.shape == myEngine::NeonParticleShape::Ring && !fromKill) {
                recentKills_.push_back({particle.position, particle.color, 0.0f});
            }
            // 撃破の欠けと火花は、同じ点から24個出るので、生まれた瞬間の強さを弱め、散るまでの間は薄くする（中心が白い玉に見えないように）。
            particle.fadeIn = fromKill && particle.shape != myEngine::NeonParticleShape::Ring ? 0.08f : 0.0f;
            particle.birth = BirthShare(particle.position) * (fromKill && particle.shape != myEngine::NeonParticleShape::Ring ? 0.3f : 1.0f);
            // 命中：火花のまとまりごとに1回、弾の色の淡い光を 0.10 秒だけ出す（主役は火花の線）。
            if (impact && look.effectFlash > 0.0f &&
                (!hasImpact || myEngine::Length(lastImpact - particle.position) > 0.6f)) {
                hasImpact = true;
                lastImpact = particle.position;
                NeonTriangleParticle flash = particle;
                flash.shape = myEngine::NeonParticleShape::Flash;
                flash.velocity = {};
                flash.radius = 1.2f;
                flash.endRadius = 1.5f;
                flash.life = flash.maxLife = 0.10f;
                flash.color.w = 0.2f;
                world_.presentation.neonTriangleParticles_.push_back(flash);
            }
            if (particle.shape == myEngine::NeonParticleShape::Ring) {
                particle.endRadius *= look.effectRing;
                // 爆発の閃光：白い玉ではなく、その色の広い淡い光を 0.08 秒だけ出す（撃破の閃光は倒した瞬間に出している）。
                if (look.effectFlash > 0.0f && !fromKill) {
                    NeonTriangleParticle flash = particle;
                    flash.shape = myEngine::NeonParticleShape::Flash;
                    flash.velocity = {};
                    flash.radius = particle.radius * 2.2f;
                    flash.endRadius = particle.radius * 4.2f;
                    flash.life = flash.maxLife = 0.08f;
                    world_.presentation.neonTriangleParticles_.push_back(flash);
                }
            }
        }
        world_.presentation.neonTriangleParticles_.push_back(particle);
    }

    for (NeonTriangleParticle& particle : world_.presentation.neonTriangleParticles_) {
        particle.life -= deltaTime;
        const float drag = particle.edge                                                 ? world_.presentation.runNeonLook_.edgeDrag
                           : particle.shape == myEngine::NeonParticleShape::Triangle ? 2.52573f
                                                                                     : myEngine::NeonParticleDrag(particle.shape);
        particle.velocity = particle.velocity * std::exp(-drag * deltaTime);
        particle.position += particle.velocity * deltaTime;
        particle.rotation += particle.angularVelocity * deltaTime;
        if (particle.edge) {
            particle.angularVelocity *= std::exp(-1.6f * deltaTime);
        }
    }

    world_.presentation.neonTriangleParticles_.erase(std::remove_if(world_.presentation.neonTriangleParticles_.begin(),
                                                                    world_.presentation.neonTriangleParticles_.end(),
                                                                    [](const NeonTriangleParticle& particle) {
                                                                        return particle.life <= 0.0f;
                                                                    }),
                                                     world_.presentation.neonTriangleParticles_.end());
}

void CombatEffects::QueueNeonTriangleParticles(const myEngine::Vector3& cameraRight, const myEngine::Vector3& cameraUp,
                                               const myEngine::Vector3& cameraForward)
{
    if (!world_.resources.neonGridRenderer_) {
        return;
    }

    const auto& tubeLook = world_.presentation.runNeonLook_;
    if (tubeLook.tubeEffects) {
        // 破片は1本の線（割れた管）、火花は短い線、輪は1本の管。自機のまわりは薄くする。
        for (const NeonTriangleParticle& particle : world_.presentation.neonTriangleParticles_) {
            const float age = std::clamp(1.0f - particle.life / particle.maxLife, 0.0f, 1.0f);
            const float endRadius = particle.endRadius >= 0.0f ? particle.endRadius : particle.radius * 0.6f;
            const float radius = particle.radius + (endRadius - particle.radius) * age;
            myEngine::Vector4 color = particle.color;
            color.w *= (1.0f - age * age) * tubefx::Clear(tubeLook, particle.position);
            const float c = std::cos(particle.rotation), sn = std::sin(particle.rotation);
            const float alive = particle.maxLife - particle.life;
            if (particle.fadeIn > 0.0f) {
                color.w *= (std::min)(1.0f, alive / particle.fadeIn);
            }
            const float boost = 1.0f + (tubefx::Birth(tubeLook, alive) - 1.0f) * particle.birth;
            auto& renderer = *world_.resources.neonGridRenderer_;
            if (particle.shape == myEngine::NeonParticleShape::Flash) {
                const myEngine::Vector4 light = tubefx::GlowColor(tubeLook, particle.color);
                const float peak = (std::max)({light.x, light.y, light.z, 0.0001f});
                const float strength = tubeLook.effectFlash / peak;
                renderer.QueueSoftEllipse(particle.position, cameraRight * radius, cameraUp * radius,
                                          {light.x * strength, light.y * strength, light.z * strength,
                                           (1.0f - age) * particle.color.w * particle.birth * tubefx::Clear(tubeLook, particle.position)},
                                          0.28f);
                continue;
            }
            if (particle.shape == myEngine::NeonParticleShape::Ring) {
                tubefx::Circle(renderer, tubeLook, particle.position, radius * 0.52f, 0.06f, color, 0.0f, 32, cameraForward, boost);
                continue;
            }
            const myEngine::Vector3 axisU = cameraRight * c + cameraUp * sn, axisV = cameraUp * c - cameraRight * sn;
            if (particle.edge) {
                // 主役の辺：消えるまで明るさを保ち、消える前に edgeBlink 回だけ暗くなり（0 にはしない）、消えた直後に暗い線が少し残る。
                float light = 1.0f;
                if (tubeLook.calmBlink) {
                    light = alive <= particle.edgeOff ? 1.0f : (std::max)(0.0f, 1.0f - (alive - particle.edgeOff) / 0.12f);
                } else if (alive > particle.edgeOff) {
                    light = 0.10f * (std::max)(0.0f, 1.0f - (alive - particle.edgeOff) / 0.05f);
                } else {
                    for (int blink = 1; blink <= tubeLook.edgeBlink; ++blink) {
                        const float at = particle.edgeOff - static_cast<float>(blink) * tubeLook.edgeGap * 1.9f;
                        if (alive > at && alive < at + tubeLook.edgeGap) {
                            light = 0.07f;
                        }
                    }
                }
                const bool dark = !tubeLook.calmBlink && light < 0.5f;
                myEngine::Vector4 lit = particle.color;
                lit.w = tubefx::Clear(tubeLook, particle.position) * (dark ? light * 3.0f : tubeLook.calmBlink ? light : 1.0f);
                tubefx::Line(renderer, tubeLook, particle.position - axisV * radius, particle.position + axisV * radius, 0.048f, lit, cameraForward,
                             dark ? 1.0f : tubeLook.edgeBright * boost, 1.0f, dark ? 0.33f : 1.0f);
                continue;
            }
            if (particle.shape == myEngine::NeonParticleShape::Spark || particle.shape == myEngine::NeonParticleShape::Triangle) {
                const float half = radius * 0.85f;
                tubefx::Line(renderer, tubeLook, particle.position - axisV * half, particle.position + axisV * half, 0.032f, color, cameraForward, boost, 1.5f);
                continue;
            }
            // 欠け：閉じた小さな形を管の線で描き、まわりに近いにじみを置く。
            std::array<std::array<float, 2>, 4> local{};
            const uint32_t count = myEngine::NeonParticlePoints(particle.shape, local);
            myEngine::Vector3 points[4]{};
            const float size = radius * 0.75f;
            for (uint32_t i = 0; i < count; ++i) {
                points[i] = particle.position + (axisU * local[i][0] + axisV * local[i][1]) * size;
            }
            if (tubeLook.effectGlow > 0.0f && tubeLook.nearGlow > 0.0f) {
                const float reach = tubeLook.nearGlowRadius;
                renderer.QueueSoftEllipse(particle.position, axisU * (size * 0.45f + reach), axisV * (size * 0.8f + reach),
                                          tubefx::GlowColor(tubeLook, color, boost * 2.2f));
            }
            // 角を丸める輪郭は1つで800頂点ほどになるので、小さい欠けは辺ごとの線（1本42頂点）で閉じる。
            const myEngine::Vector4 lit = tubefx::Color(tubeLook, color, boost);
            const myEngine::NeonContourStyle style = tubefx::Style(tubeLook);
            for (uint32_t i = 0; i < count; ++i) {
                renderer.QueueContourLine(points[i], points[(i + 1) % count], 0.026f * tubeLook.effectLine, lit, cameraForward, style);
            }
        }
        return;
    }
    for (const NeonTriangleParticle& particle : world_.presentation.neonTriangleParticles_) {
        if (particle.shape != myEngine::NeonParticleShape::Triangle) {
            const float age = std::clamp(1.0f - particle.life / particle.maxLife, 0.0f, 1.0f);
            const float radius = particle.radius + (particle.endRadius - particle.radius) * age;
            myEngine::Vector4 color = particle.color;
            color.w *= 1.0f - myEngine::NeonParticleFade(age, particle.shape);
            if (particle.shape == myEngine::NeonParticleShape::Flash) {
                // 同心円状の低アルファの円盤で、テクスチャやモデルのパーティクルを使わずに
                // 輪郭モードの閃光の縁を柔らかくする。
                for (int layer = 4; layer >= 0; --layer) {
                    myEngine::Vector4 flash = color;
                    flash.x *= 4.0f;
                    flash.y *= 4.0f;
                    flash.z *= 4.0f;
                    flash.w *= 0.12f + 0.055f * (4 - layer);
                    world_.resources.neonGridRenderer_->QueueBillboardDisc(particle.position, radius * (0.24f + 0.09f * layer), flash,
                                                                           cameraRight, cameraUp);
                }
                continue;
            }
            const float c = std::cos(particle.rotation), sn = std::sin(particle.rotation);
            const myEngine::Vector3 axisU = particle.isBillboard ? cameraRight * c + cameraUp * sn : myEngine::Vector3{c, sn, 0.0f};
            const myEngine::Vector3 axisV =
                particle.isBillboard
                    ? cameraUp * c - cameraRight * sn
                    : myEngine::Vector3{-sn * std::cos(particle.tiltRad), c * std::cos(particle.tiltRad), std::sin(particle.tiltRad)};
            myEngine::NeonContourStyle style = myEngine::ActorNeonContourStyle();
            style.coreWhiteMix = 0.48f;
            style.coreIntensity = 3.4f;
            style.shoulderIntensity = 2.2f;
            if (particle.shape == myEngine::NeonParticleShape::Ring) {
                myEngine::Vector3 points[32]{};
                for (int i = 0; i < 32; ++i) {
                    const float a = 6.28318530718f * i / 32.0f;
                    points[i] = particle.position + (axisU * std::cos(a) + axisV * std::sin(a)) * (radius * 0.52f);
                }
                world_.resources.neonGridRenderer_->QueueContourPolygon(points, 32, particle.lineWidth, color, cameraForward, style);
            } else if (particle.shape == myEngine::NeonParticleShape::Spark) {
                world_.resources.neonGridRenderer_->QueueContourLine(particle.position - axisV * (radius * 0.65f),
                                                                     particle.position + axisV * (radius * 0.75f), particle.lineWidth,
                                                                     color, cameraForward, style);
            } else {
                std::array<std::array<float, 2>, 4> local{};
                const uint32_t count = myEngine::NeonParticlePoints(particle.shape, local);
                myEngine::Vector3 points[4]{};
                for (uint32_t i = 0; i < count; ++i) {
                    points[i] = particle.position + (axisU * local[i][0] + axisV * local[i][1]) * radius;
                }
                world_.resources.neonGridRenderer_->QueueContourPolygon(points, count, particle.lineWidth, color, cameraForward, style);
            }
            continue;
        }
        myEngine::NeonContourStyle contour;
        contour.haloWidthScale = world_.presentation.neonParticleTriangleGlowWidthScale_;
        contour.coreWidthRatio = world_.presentation.neonParticleTriangleCoreWidthScale_;
        auto queueParticleTriangle = [&](const myEngine::Vector3& center, float radius, float rotation, float width,
                                         const myEngine::Vector4& drawColor, bool luminous = true) {
            if (particle.isBillboard) {
                if (luminous) {
                    world_.resources.neonGridRenderer_->QueueBillboardContourTriangle(center, radius, rotation, width, drawColor,
                                                                                      cameraRight, cameraUp, cameraForward, contour);
                } else {
                    world_.resources.neonGridRenderer_->QueueBillboardTriangle(center, radius, rotation, width, drawColor, cameraRight,
                                                                               cameraUp, cameraForward);
                }
                return;
            }
            const float c = std::cos(rotation);
            const float s = std::sin(rotation);
            const float tiltCos = std::cos(particle.tiltRad);
            const float tiltSin = std::sin(particle.tiltRad);
            const myEngine::Vector3 axisU = {c, s, 0.0f};
            const myEngine::Vector3 axisV = {-s * tiltCos, c * tiltCos, tiltSin};
            myEngine::Vector3 points[3]{};
            for (int i = 0; i < 3; ++i) {
                const float angle = -1.5707963268f + static_cast<float>(i) * 2.0943951024f;
                points[i] = center + axisU * (std::cos(angle) * radius) + axisV * (std::sin(angle) * radius);
            }
            if (luminous) {
                world_.resources.neonGridRenderer_->QueueContourPolygon(points, 3, width, drawColor, cameraForward, contour);
            } else {
                for (int i = 0; i < 3; ++i) {
                    world_.resources.neonGridRenderer_->QueueCameraFacingLine(points[i], points[(i + 1) % 3], width, drawColor,
                                                                              cameraForward);
                }
            }
        };
        const float t = particle.maxLife > 0.0f ? (std::clamp)(particle.life / particle.maxLife, 0.0f, 1.0f) : 0.0f;
        const float age = 1.0f - t;
        const float birth = (std::clamp)(age / 0.18f, 0.0f, 1.0f);
        const float scale = world_.presentation.neonParticleTriangleBirthScale_ +
                            (1.0f - world_.presentation.neonParticleTriangleBirthScale_) * (1.0f - (1.0f - birth) * (1.0f - birth));
        myEngine::Vector4 color = particle.color;
        color.x *= world_.presentation.neonParticleTriangleBrightness_;
        color.y *= world_.presentation.neonParticleTriangleBrightness_;
        color.z *= world_.presentation.neonParticleTriangleBrightness_;
        color.w *= t * (0.45f + birth * 0.55f);
        myEngine::Vector3 trailDirection{};
        if (myEngine::Length(particle.initialVelocity) > 0.0001f) {
            trailDirection = myEngine::Normalize(particle.initialVelocity);
        }
        const int trailCopies = (std::min)(world_.presentation.neonParticleTriangleTrailCopies_, particle.trailCopies);
        for (int copy = trailCopies; copy >= 1; --copy) {
            const float copyRatio = static_cast<float>(copy) / static_cast<float>((std::max)(1, trailCopies));
            myEngine::Vector4 trailColor = color;
            trailColor.w *= (1.0f - copyRatio * 0.72f) * 0.30f;
            queueParticleTriangle(
                particle.position -
                    trailDirection * (particle.radius * world_.presentation.neonParticleTriangleTrailSpacing_ * static_cast<float>(copy)),
                particle.radius * scale * (1.0f - copyRatio * 0.12f),
                particle.rotation - particle.angularVelocity * 0.018f * static_cast<float>(copy), particle.lineWidth * 0.72f, trailColor,
                false);
        }

        queueParticleTriangle(particle.position, particle.radius * scale, particle.rotation, particle.lineWidth, color);
    }
}
} // namespace gameplay
