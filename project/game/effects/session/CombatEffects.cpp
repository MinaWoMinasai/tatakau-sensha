#include "game/effects/session/CombatEffects.h"
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

void CombatEffects::TriggerDeathPostPulse(const cg2::Vector3& worldPosition, float strength)
{
    if (!world_.presentation.enableDeathPostPulse_) {
        return;
    }

    const cg2::Vector2 screen = world_.gameplayHud->WorldToScreen(worldPosition);
    world_.presentation.deathPostPulse_.center = {(std::clamp)(screen.x / static_cast<float>(cg2::WinApp::kClientWidth), 0.0f, 1.0f),
                                                  (std::clamp)(screen.y / static_cast<float>(cg2::WinApp::kClientHeight), 0.0f, 1.0f)};
    world_.presentation.deathPostPulseScale_ = (std::clamp)(strength, 0.5f, 1.6f);
    world_.presentation.deathPostPulseTimer_ = world_.presentation.deathPostPulseDuration_;
    UpdateDeathPostPulse(0.0f);

    world_.presentation.cameraShakeTimer_ = world_.presentation.cameraShakeDuration_;
    world_.presentation.cameraShakePower_ =
        (std::max)(world_.presentation.cameraShakePower_, 0.65f * world_.presentation.deathPostPulseScale_);
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
    const float impactEnvelope = std::sin(age * cg2::pi) * (1.0f - age * 0.35f);
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

void CombatEffects::UpdateNeonTriangleParticles(float deltaTime)
{
    for (const cg2::ParticleManager::NeonTriangleEvent& event : cg2::ParticleManager::GetInstance()->ConsumeNeonTriangleEvents()) {
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
        world_.presentation.neonTriangleParticles_.push_back(particle);
    }

    for (NeonTriangleParticle& particle : world_.presentation.neonTriangleParticles_) {
        particle.life -= deltaTime;
        const float drag = particle.shape == cg2::NeonParticleShape::Triangle ? 2.52573f : cg2::NeonParticleDrag(particle.shape);
        particle.velocity = particle.velocity * std::exp(-drag * deltaTime);
        particle.position += particle.velocity * deltaTime;
        particle.rotation += particle.angularVelocity * deltaTime;
    }

    world_.presentation.neonTriangleParticles_.erase(std::remove_if(world_.presentation.neonTriangleParticles_.begin(),
                                                                    world_.presentation.neonTriangleParticles_.end(),
                                                                    [](const NeonTriangleParticle& particle) {
                                                                        return particle.life <= 0.0f;
                                                                    }),
                                                     world_.presentation.neonTriangleParticles_.end());
}

void CombatEffects::QueueNeonTriangleParticles(const cg2::Vector3& cameraRight, const cg2::Vector3& cameraUp,
                                               const cg2::Vector3& cameraForward)
{
    if (!world_.resources.neonGridRenderer_) {
        return;
    }

    for (const NeonTriangleParticle& particle : world_.presentation.neonTriangleParticles_) {
        if (particle.shape != cg2::NeonParticleShape::Triangle) {
            const float age = std::clamp(1.0f - particle.life / particle.maxLife, 0.0f, 1.0f);
            const float radius = particle.radius + (particle.endRadius - particle.radius) * age;
            cg2::Vector4 color = particle.color;
            color.w *= 1.0f - cg2::NeonParticleFade(age, particle.shape);
            if (particle.shape == cg2::NeonParticleShape::Flash) {
                // Concentric low-alpha discs soften the edge of an outline-mode
                // flash without introducing textured/model particles.
                for (int layer = 4; layer >= 0; --layer) {
                    cg2::Vector4 flash = color;
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
            const cg2::Vector3 axisU = particle.isBillboard ? cameraRight * c + cameraUp * sn : cg2::Vector3{c, sn, 0.0f};
            const cg2::Vector3 axisV = particle.isBillboard ? cameraUp * c - cameraRight * sn
                                                            : cg2::Vector3{-sn * std::cos(particle.tiltRad), c * std::cos(particle.tiltRad),
                                                                           std::sin(particle.tiltRad)};
            cg2::NeonContourStyle style = cg2::ActorNeonContourStyle();
            style.coreWhiteMix = 0.48f;
            style.coreIntensity = 3.4f;
            style.shoulderIntensity = 2.2f;
            if (particle.shape == cg2::NeonParticleShape::Ring) {
                cg2::Vector3 points[32]{};
                for (int i = 0; i < 32; ++i) {
                    const float a = 6.28318530718f * i / 32.0f;
                    points[i] = particle.position + (axisU * std::cos(a) + axisV * std::sin(a)) * (radius * 0.52f);
                }
                world_.resources.neonGridRenderer_->QueueContourPolygon(points, 32, particle.lineWidth, color, cameraForward, style);
            } else if (particle.shape == cg2::NeonParticleShape::Spark) {
                world_.resources.neonGridRenderer_->QueueContourLine(particle.position - axisV * (radius * 0.65f),
                                                                     particle.position + axisV * (radius * 0.75f), particle.lineWidth,
                                                                     color, cameraForward, style);
            } else {
                std::array<std::array<float, 2>, 4> local{};
                const uint32_t count = cg2::NeonParticlePoints(particle.shape, local);
                cg2::Vector3 points[4]{};
                for (uint32_t i = 0; i < count; ++i)
                    points[i] = particle.position + (axisU * local[i][0] + axisV * local[i][1]) * radius;
                world_.resources.neonGridRenderer_->QueueContourPolygon(points, count, particle.lineWidth, color, cameraForward, style);
            }
            continue;
        }
        cg2::NeonContourStyle contour;
        contour.haloWidthScale = world_.presentation.neonParticleTriangleGlowWidthScale_;
        contour.coreWidthRatio = world_.presentation.neonParticleTriangleCoreWidthScale_;
        auto queueParticleTriangle = [&](const cg2::Vector3& center, float radius, float rotation, float width,
                                         const cg2::Vector4& drawColor, bool luminous = true) {
            if (particle.isBillboard) {
                if (luminous)
                    world_.resources.neonGridRenderer_->QueueBillboardContourTriangle(center, radius, rotation, width, drawColor,
                                                                                      cameraRight, cameraUp, cameraForward, contour);
                else
                    world_.resources.neonGridRenderer_->QueueBillboardTriangle(center, radius, rotation, width, drawColor, cameraRight,
                                                                               cameraUp, cameraForward);
                return;
            }
            const float c = std::cos(rotation);
            const float s = std::sin(rotation);
            const float tiltCos = std::cos(particle.tiltRad);
            const float tiltSin = std::sin(particle.tiltRad);
            const cg2::Vector3 axisU = {c, s, 0.0f};
            const cg2::Vector3 axisV = {-s * tiltCos, c * tiltCos, tiltSin};
            cg2::Vector3 points[3]{};
            for (int i = 0; i < 3; ++i) {
                const float angle = -1.5707963268f + static_cast<float>(i) * 2.0943951024f;
                points[i] = center + axisU * (std::cos(angle) * radius) + axisV * (std::sin(angle) * radius);
            }
            if (luminous)
                world_.resources.neonGridRenderer_->QueueContourPolygon(points, 3, width, drawColor, cameraForward, contour);
            else
                for (int i = 0; i < 3; ++i)
                    world_.resources.neonGridRenderer_->QueueCameraFacingLine(points[i], points[(i + 1) % 3], width, drawColor,
                                                                              cameraForward);
        };
        const float t = particle.maxLife > 0.0f ? (std::clamp)(particle.life / particle.maxLife, 0.0f, 1.0f) : 0.0f;
        const float age = 1.0f - t;
        const float birth = (std::clamp)(age / 0.18f, 0.0f, 1.0f);
        const float scale = world_.presentation.neonParticleTriangleBirthScale_ +
                            (1.0f - world_.presentation.neonParticleTriangleBirthScale_) * (1.0f - (1.0f - birth) * (1.0f - birth));
        cg2::Vector4 color = particle.color;
        color.x *= world_.presentation.neonParticleTriangleBrightness_;
        color.y *= world_.presentation.neonParticleTriangleBrightness_;
        color.z *= world_.presentation.neonParticleTriangleBrightness_;
        color.w *= t * (0.45f + birth * 0.55f);
        cg2::Vector3 trailDirection{};
        if (cg2::Length(particle.initialVelocity) > 0.0001f) {
            trailDirection = cg2::Normalize(particle.initialVelocity);
        }
        const int trailCopies = (std::min)(world_.presentation.neonParticleTriangleTrailCopies_, particle.trailCopies);
        for (int copy = trailCopies; copy >= 1; --copy) {
            const float copyRatio = static_cast<float>(copy) / static_cast<float>((std::max)(1, trailCopies));
            cg2::Vector4 trailColor = color;
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
