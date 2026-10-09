#include "game/render/session/ResourceNeonRenderer.h"
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

void ResourceNeonRenderer::QueueExpEnemyNeonShapes(const cg2::Vector3& cameraRight, const cg2::Vector3& cameraUp,
                                                   const cg2::Vector3& cameraForward)
{
    if (!world_.resources.enemyManager_ || !world_.resources.neonGridRenderer_) {
        return;
    }

    auto maxScaleComponent = [](const cg2::Vector3& scale) {
        return (std::max)({scale.x, scale.y, scale.z});
    };

    auto transformLocalPoint = [](const cg2::Vector3& local, const cg2::Vector3& center, const cg2::Vector3& rotate,
                                  const cg2::Vector3& scale) {
        const cg2::Matrix4x4 transform = cg2::MakeAffineMatrix(scale, rotate, center);
        return cg2::TransformMatrix(local, transform);
    };

    cg2::NeonContourStyle wireContour = cg2::ActorNeonContourStyle();
    wireContour.roundCaps = false;
    auto queueWireLine = [&](const cg2::Vector3& a, const cg2::Vector3& b, float width, const cg2::Vector4& color) {
        world_.resources.neonGridRenderer_->QueueContourLine(a, b, width, color, cameraForward, wireContour);
    };

    auto queueCube = [&](const cg2::Vector3& center, float size, const cg2::Vector3& rotate, const cg2::Vector3& scale, float lineWidth,
                         const cg2::Vector4& color) {
        const float h = size * 0.5f;
        const float z = size * 0.5f;
        cg2::Vector3 corners[8] = {{-h, -h, -z}, {h, -h, -z}, {h, h, -z}, {-h, h, -z}, {-h, -h, z}, {h, -h, z}, {h, h, z}, {-h, h, z}};
        for (cg2::Vector3& corner : corners) {
            corner = transformLocalPoint(corner, center, rotate, scale);
        }
        const int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
        for (const auto& edge : edges) {
            queueWireLine(corners[edge[0]], corners[edge[1]], lineWidth, color);
        }
    };

    auto queueTriangularPyramid = [&](const cg2::Vector3& center, float radius, const cg2::Vector3& rotate, const cg2::Vector3& scale,
                                      float lineWidth, const cg2::Vector4& color) {
        constexpr float kTwoPi = 6.28318530718f;
        const float depth = radius * 0.75f;
        cg2::Vector3 points[4]{};
        for (int i = 0; i < 3; ++i) {
            const float angle = -kTwoPi * 0.25f + static_cast<float>(i) * (kTwoPi / 3.0f);
            points[i] = transformLocalPoint({std::cos(angle) * radius, std::sin(angle) * radius, -depth * 0.35f}, center, rotate, scale);
        }
        points[3] = transformLocalPoint({0.0f, 0.0f, depth * 0.65f}, center, rotate, scale);
        queueWireLine(points[0], points[1], lineWidth, color);
        queueWireLine(points[1], points[2], lineWidth, color);
        queueWireLine(points[2], points[0], lineWidth, color);
        queueWireLine(points[0], points[3], lineWidth, color);
        queueWireLine(points[1], points[3], lineWidth, color);
        queueWireLine(points[2], points[3], lineWidth, color);
    };

    auto queuePentagonalPrism = [&](const cg2::Vector3& center, float radius, const cg2::Vector3& rotate, const cg2::Vector3& scale,
                                    float lineWidth, const cg2::Vector4& color) {
        constexpr float kTwoPi = 6.28318530718f;
        const float depth = radius * 0.85f;
        cg2::Vector3 points[10]{};
        for (int i = 0; i < 5; ++i) {
            const float angle = -kTwoPi * 0.25f + static_cast<float>(i) * (kTwoPi / 5.0f);
            const cg2::Vector3 front = {std::cos(angle) * radius, std::sin(angle) * radius, -depth * 0.5f};
            const cg2::Vector3 back = {front.x, front.y, depth * 0.5f};
            points[i] = transformLocalPoint(front, center, rotate, scale);
            points[i + 5] = transformLocalPoint(back, center, rotate, scale);
        }
        for (int i = 0; i < 5; ++i) {
            const int next = (i + 1) % 5;
            queueWireLine(points[i], points[next], lineWidth, color);
            queueWireLine(points[i + 5], points[next + 5], lineWidth, color);
            queueWireLine(points[i], points[i + 5], lineWidth, color);
        }
    };

    auto queueCircle3D = [&](const cg2::Vector3& center, float radius, const cg2::Vector3& rotate, const cg2::Vector3& scale, int axis,
                             float lineWidth, const cg2::Vector4& color) {
        constexpr float kTwoPi = 6.28318530718f;
        constexpr int kSegments = 20;
        cg2::Vector3 previous{};
        for (int i = 0; i <= kSegments; ++i) {
            const float angle = static_cast<float>(i) * (kTwoPi / static_cast<float>(kSegments));
            cg2::Vector3 local{};
            if (axis == 0) {
                local = {0.0f, std::cos(angle) * radius, std::sin(angle) * radius};
            } else if (axis == 1) {
                local = {std::cos(angle) * radius, 0.0f, std::sin(angle) * radius};
            } else {
                local = {std::cos(angle) * radius, std::sin(angle) * radius, 0.0f};
            }
            const cg2::Vector3 current = transformLocalPoint(local, center, rotate, scale);
            if (i > 0) {
                queueWireLine(previous, current, lineWidth, color);
            }
            previous = current;
        }
    };

    auto queueShooter = [&](const cg2::Vector3& center, float radius, const cg2::Vector3& rotate, const cg2::Vector3& scale,
                            float lineWidth, const cg2::Vector4& color) {
        queueCircle3D(center, radius, rotate, scale, 0, lineWidth, color);
        queueCircle3D(center, radius, rotate, scale, 1, lineWidth, color);
        queueCircle3D(center, radius, rotate, scale, 2, lineWidth, color);

        const float barrelLength = radius * 0.9f;
        const float barrelHalf = radius * 0.22f;
        const float barrelOffset = radius * 0.72f;
        cg2::Vector3 corners[8] = {{-barrelHalf, -barrelOffset, -barrelHalf},
                                   {barrelHalf, -barrelOffset, -barrelHalf},
                                   {barrelHalf, -barrelOffset - barrelLength, -barrelHalf},
                                   {-barrelHalf, -barrelOffset - barrelLength, -barrelHalf},
                                   {-barrelHalf, -barrelOffset, barrelHalf},
                                   {barrelHalf, -barrelOffset, barrelHalf},
                                   {barrelHalf, -barrelOffset - barrelLength, barrelHalf},
                                   {-barrelHalf, -barrelOffset - barrelLength, barrelHalf}};
        for (cg2::Vector3& corner : corners) {
            corner = transformLocalPoint(corner, center, rotate, scale);
        }
        const int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
        for (const auto& edge : edges) {
            queueWireLine(corners[edge[0]], corners[edge[1]], lineWidth, color);
        }
    };

    auto queueBillboardPolygon = [&](const cg2::Vector3& center, int sides, float radius, float rotation, float lineWidth,
                                     const cg2::Vector4& color, bool luminous = true) {
        constexpr float kTwoPi = 6.28318530718f;
        if (sides < 3 || sides > 48)
            return;
        cg2::Vector3 points[48]{};
        for (int i = 0; i < sides; ++i) {
            const float angle = rotation - kTwoPi * 0.25f + static_cast<float>(i) * (kTwoPi / static_cast<float>(sides));
            points[i] = center + cameraRight * (std::cos(angle) * radius) + cameraUp * (std::sin(angle) * radius);
        }
        if (luminous)
            world_.resources.neonGridRenderer_->QueueContourPolygon(points, static_cast<uint32_t>(sides), lineWidth, color, cameraForward,
                                                                    cg2::ActorNeonContourStyle());
        else
            for (int i = 0; i < sides; ++i)
                world_.resources.neonGridRenderer_->QueueCameraFacingLine(points[i], points[(i + 1) % sides], lineWidth, color,
                                                                          cameraForward);
    };

    auto queueBillboardShooter = [&](const cg2::Vector3& center, float radius, float rotation, float lineWidth, const cg2::Vector4& color,
                                     float warningRatio, float muzzleFlashRatio) {
        warningRatio = (std::clamp)(warningRatio, 0.0f, 1.0f);
        muzzleFlashRatio = (std::clamp)(muzzleFlashRatio, 0.0f, 1.0f);
        const float warningImpact = warningRatio * warningRatio;
        if (warningRatio > 0.0f) {
            const float warningRadiusScale = 1.50f - warningRatio * 0.42f;
            const cg2::Vector4 warningColor{1.80f + warningImpact * 0.60f, 0.25f + warningImpact * 1.20f, 0.08f + warningImpact * 0.55f,
                                            0.28f + warningRatio * 0.72f};
            queueBillboardPolygon(center, 20, radius * warningRadiusScale, rotation, lineWidth * (0.75f + warningRatio * 0.55f),
                                  warningColor, false);
        }

        const cg2::Vector4 bodyColor{color.x + warningImpact * 0.65f, color.y + warningImpact * 0.85f, color.z + warningImpact * 0.32f,
                                     color.w};
        const cg2::Vector4 barrelColor{bodyColor.x + warningImpact * 0.35f, bodyColor.y + warningImpact * 0.30f,
                                       bodyColor.z + warningImpact * 0.12f, bodyColor.w};
        queueBillboardPolygon(center, 20, radius, rotation, lineWidth, bodyColor);
        const float c = std::cos(rotation);
        const float s = std::sin(rotation);
        const cg2::Vector3 down = cameraRight * s + cameraUp * -c;
        const cg2::Vector3 side = cameraRight * c + cameraUp * s;
        const cg2::Vector3 base = center + down * (radius * 0.65f);
        const cg2::Vector3 tip = center + down * (radius * 1.45f);
        const float half = radius * 0.18f;
        const cg2::Vector3 barrelPoints[]{base - side * half, tip - side * half, tip + side * half, base + side * half};
        world_.resources.neonGridRenderer_->QueueContourPolygon(barrelPoints, 4, lineWidth, barrelColor, cameraForward,
                                                                cg2::ActorNeonContourStyle());

        if (muzzleFlashRatio > 0.0f) {
            const float flashRadius = radius * (0.10f + muzzleFlashRatio * 0.18f);
            const cg2::Vector4 flashColor{2.60f, 1.85f, 0.65f, muzzleFlashRatio};
            queueBillboardPolygon(tip, 12, flashRadius, rotation, lineWidth * 1.40f, flashColor);
            queueWireLine(tip - side * flashRadius, tip + side * flashRadius, lineWidth * 1.20f, flashColor);
            queueWireLine(tip - down * flashRadius, tip + down * flashRadius, lineWidth * 1.20f, flashColor);
        }
    };

    for (ExpEnemy* expEnemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
        if (!expEnemy || expEnemy->IsDead() || !expEnemy->IsShapeNeonRenderTarget()) {
            continue;
        }
        if (expEnemy->IsExpeditionCombatRole()) {
            expEnemy->QueueCombatVisuals(*world_.resources.neonGridRenderer_, cameraRight, cameraUp, cameraForward,
                                         world_.presentation.expEnemyNeonLineWidth_);
            continue;
        }
        const cg2::Vector3 visualScale = expEnemy->GetVisualScale();
        const float scalePulse = maxScaleComponent(visualScale);
        float shapeRadius = world_.presentation.expEnemyNeonSquareSize_ * 0.5f;
        if (expEnemy->GetType() == ExpEnemyType::Triangle) {
            shapeRadius = world_.presentation.expEnemyNeonTriangleRadius_;
        } else if (expEnemy->GetType() == ExpEnemyType::Pentagon) {
            shapeRadius = world_.presentation.expEnemyNeonPentagonRadius_;
        } else if (expEnemy->GetType() == ExpEnemyType::Shooter) {
            shapeRadius = world_.presentation.expEnemyNeonShooterRadius_ * 1.5f;
        }
        if (world_.presentation.cullActorLocalGrid_ &&
            !IsNearCamera2D(expEnemy->GetWorldPosition(), 38.0f, 24.0f, shapeRadius * scalePulse + 1.0f)) {
            continue;
        }

        const cg2::Vector3 center = expEnemy->GetWorldPosition() +
                                    (world_.presentation.expEnemyNeonRenderMode_ >= 2 ? cg2::Vector3{} : cg2::Vector3{0.0f, 0.0f, 0.35f});
        const cg2::Vector4 color = expEnemy->GetVisualColor();
        if (world_.presentation.expEnemyNeonRenderMode_ >= 2) {
            if (expEnemy->GetType() == ExpEnemyType::Triangle) {
                queueTriangularPyramid(center, world_.presentation.expEnemyNeonTriangleRadius_, expEnemy->GetVisualRotate(), visualScale,
                                       world_.presentation.expEnemyNeonLineWidth_, color);
            } else if (expEnemy->GetType() == ExpEnemyType::Pentagon) {
                queuePentagonalPrism(center, world_.presentation.expEnemyNeonPentagonRadius_, expEnemy->GetVisualRotate(), visualScale,
                                     world_.presentation.expEnemyNeonLineWidth_, color);
            } else if (expEnemy->GetType() == ExpEnemyType::Shooter) {
                queueShooter(center, world_.presentation.expEnemyNeonShooterRadius_, expEnemy->GetVisualRotate(), visualScale,
                             world_.presentation.expEnemyNeonLineWidth_, color);
            } else {
                queueCube(center, world_.presentation.expEnemyNeonSquareSize_, expEnemy->GetVisualRotate(), visualScale,
                          world_.presentation.expEnemyNeonLineWidth_, color);
            }
        } else if (expEnemy->GetType() == ExpEnemyType::Triangle) {
            world_.resources.neonGridRenderer_->QueueBillboardContourTriangle(
                center, world_.presentation.expEnemyNeonTriangleRadius_ * scalePulse, expEnemy->GetVisualRotation(),
                world_.presentation.expEnemyNeonLineWidth_, color, cameraRight, cameraUp, cameraForward, cg2::ActorNeonContourStyle());
        } else if (expEnemy->GetType() == ExpEnemyType::Pentagon) {
            queueBillboardPolygon(center, 5, world_.presentation.expEnemyNeonPentagonRadius_ * scalePulse, expEnemy->GetVisualRotation(),
                                  world_.presentation.expEnemyNeonLineWidth_, color);
        } else if (expEnemy->GetType() == ExpEnemyType::Shooter) {
            queueBillboardShooter(center, world_.presentation.expEnemyNeonShooterRadius_ * scalePulse, expEnemy->GetVisualRotation(),
                                  world_.presentation.expEnemyNeonLineWidth_, color, expEnemy->GetShooterWarningRatio(),
                                  expEnemy->GetShooterMuzzleFlashRatio());
        } else {
            world_.resources.neonGridRenderer_->QueueBillboardContourRectangle(
                center,
                {world_.presentation.expEnemyNeonSquareSize_ * scalePulse, world_.presentation.expEnemyNeonSquareSize_ * scalePulse},
                expEnemy->GetVisualRotation(), world_.presentation.expEnemyNeonLineWidth_, color, cameraRight, cameraUp, cameraForward,
                cg2::ActorNeonContourStyle());
        }
    }
}

void ResourceNeonRenderer::DrawExpEnemyNeonFillModels()
{
    if (!world_.resources.enemyManager_) {
        return;
    }

    for (ExpEnemy* expEnemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
        if (!expEnemy || expEnemy->IsDead() || !expEnemy->IsShapeNeonRenderTarget()) {
            continue;
        }
        float shapeRadius = world_.presentation.expEnemyNeonSquareSize_ * 0.5f;
        if (expEnemy->GetType() == ExpEnemyType::Triangle) {
            shapeRadius = world_.presentation.expEnemyNeonTriangleRadius_;
        } else if (expEnemy->GetType() == ExpEnemyType::Pentagon) {
            shapeRadius = world_.presentation.expEnemyNeonPentagonRadius_;
        } else if (expEnemy->GetType() == ExpEnemyType::Shooter) {
            shapeRadius = world_.presentation.expEnemyNeonShooterRadius_ * 1.5f;
        }
        const cg2::Vector3 visualScale = expEnemy->GetVisualScale();
        const float scalePulse = (std::max)({visualScale.x, visualScale.y, visualScale.z});
        if (world_.presentation.cullActorLocalGrid_ &&
            !IsNearCamera2D(expEnemy->GetWorldPosition(), 38.0f, 24.0f, shapeRadius * scalePulse + 1.0f)) {
            continue;
        }
        expEnemy->DrawNeonFillBodyOnly();
    }
}

void ResourceNeonRenderer::DrawExpEnemyNeonDepthLines()
{
    if (!world_.resources.neonGridRenderer_) {
        return;
    }

    cg2::Matrix4x4 viewMatrix = cg2::Object3dCommon::GetInstance()->GetIsDebugCamera() ? world_.resources.debugCamera->GetViewMatrix()
                                                                                       : world_.resources.camera->GetViewMatrix();
    cg2::Matrix4x4 cameraWorld = cg2::Inverse(viewMatrix);
    cg2::Vector3 cameraRight = cg2::Normalize({cameraWorld.m[0][0], cameraWorld.m[0][1], cameraWorld.m[0][2]});
    cg2::Vector3 cameraUp = cg2::Normalize({cameraWorld.m[1][0], cameraWorld.m[1][1], cameraWorld.m[1][2]});
    cg2::Vector3 cameraForward = cg2::Normalize({cameraWorld.m[2][0], cameraWorld.m[2][1], cameraWorld.m[2][2]});

    world_.resources.neonGridRenderer_->BeginFrame();
    world_.resources.neonGridRenderer_->SetLineStyle(world_.presentation.neonLineSoftEdgeRatio_,
                                                     world_.presentation.neonLineCoreIntensity_);
    QueueExpEnemyNeonShapes(cameraRight, cameraUp, cameraForward);

    cg2::Matrix4x4 vp = cg2::Object3dCommon::GetInstance()->GetIsDebugCamera() ? world_.resources.debugCamera->GetViewProjectionMatrix()
                                                                               : world_.resources.camera->GetViewProjectionMatrix();
    world_.resources.neonGridRenderer_->DrawAll(vp);
}

cg2::Vector3 ResourceNeonRenderer::GetFloorVisibilityCenter() const
{
    // A tilted camera's eye is outside the room in XY. Floor culling follows
    // its displayed focus; facing/depth calculations still use the real eye.
    return world_.resources.neonDepthCameraScoped_
               ? world_.resources.neonDepthFloorViewCenter_
               : GetActiveCameraPosition(world_.resources.camera.get(), world_.resources.debugCamera.get());
}

bool ResourceNeonRenderer::IsNearCamera2D(const cg2::Vector3& worldPos, float halfWidth, float halfHeight, float margin) const
{
    const cg2::Vector3 cameraPos = GetFloorVisibilityCenter();
    return worldPos.x >= cameraPos.x - halfWidth - margin && worldPos.x <= cameraPos.x + halfWidth + margin &&
           worldPos.y >= cameraPos.y - halfHeight - margin && worldPos.y <= cameraPos.y + halfHeight + margin;
}
} // namespace gameplay
