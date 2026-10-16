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

void ResourceNeonRenderer::QueueExpEnemyNeonShapes(const myEngine::Vector3& cameraRight, const myEngine::Vector3& cameraUp,
                                                   const myEngine::Vector3& cameraForward)
{
    if (!world_.resources.enemyManager_ || !world_.resources.neonGridRenderer_) {
        return;
    }

    auto maxScaleComponent = [](const myEngine::Vector3& scale) {
        return (std::max)({scale.x, scale.y, scale.z});
    };

    auto transformLocalPoint = [](const myEngine::Vector3& local, const myEngine::Vector3& center, const myEngine::Vector3& rotate,
                                  const myEngine::Vector3& scale) {
        const myEngine::Matrix4x4 transform = myEngine::MakeAffineMatrix(scale, rotate, center);
        return myEngine::TransformMatrix(local, transform);
    };

    myEngine::NeonContourStyle wireContour = myEngine::ActorNeonContourStyle();
    wireContour.roundCaps = false;
    auto queueWireLine = [&](const myEngine::Vector3& a, const myEngine::Vector3& b, float width, const myEngine::Vector4& color) {
        world_.resources.neonGridRenderer_->QueueContourLine(a, b, width, color, cameraForward, wireContour);
    };

    auto queueCube = [&](const myEngine::Vector3& center, float size, const myEngine::Vector3& rotate, const myEngine::Vector3& scale, float lineWidth,
                         const myEngine::Vector4& color) {
        const float h = size * 0.5f;
        const float z = size * 0.5f;
        myEngine::Vector3 corners[8] = {{-h, -h, -z}, {h, -h, -z}, {h, h, -z}, {-h, h, -z}, {-h, -h, z}, {h, -h, z}, {h, h, z}, {-h, h, z}};
        for (myEngine::Vector3& corner : corners) {
            corner = transformLocalPoint(corner, center, rotate, scale);
        }
        const int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
        for (const auto& edge : edges) {
            queueWireLine(corners[edge[0]], corners[edge[1]], lineWidth, color);
        }
    };

    auto queueTriangularPyramid = [&](const myEngine::Vector3& center, float radius, const myEngine::Vector3& rotate, const myEngine::Vector3& scale,
                                      float lineWidth, const myEngine::Vector4& color) {
        constexpr float kTwoPi = 6.28318530718f;
        const float depth = radius * 0.75f;
        myEngine::Vector3 points[4]{};
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

    auto queuePentagonalPrism = [&](const myEngine::Vector3& center, float radius, const myEngine::Vector3& rotate, const myEngine::Vector3& scale,
                                    float lineWidth, const myEngine::Vector4& color) {
        constexpr float kTwoPi = 6.28318530718f;
        const float depth = radius * 0.85f;
        myEngine::Vector3 points[10]{};
        for (int i = 0; i < 5; ++i) {
            const float angle = -kTwoPi * 0.25f + static_cast<float>(i) * (kTwoPi / 5.0f);
            const myEngine::Vector3 front = {std::cos(angle) * radius, std::sin(angle) * radius, -depth * 0.5f};
            const myEngine::Vector3 back = {front.x, front.y, depth * 0.5f};
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

    auto queueCircle3D = [&](const myEngine::Vector3& center, float radius, const myEngine::Vector3& rotate, const myEngine::Vector3& scale, int axis,
                             float lineWidth, const myEngine::Vector4& color) {
        constexpr float kTwoPi = 6.28318530718f;
        constexpr int kSegments = 20;
        myEngine::Vector3 previous{};
        for (int i = 0; i <= kSegments; ++i) {
            const float angle = static_cast<float>(i) * (kTwoPi / static_cast<float>(kSegments));
            myEngine::Vector3 local{};
            if (axis == 0) {
                local = {0.0f, std::cos(angle) * radius, std::sin(angle) * radius};
            } else if (axis == 1) {
                local = {std::cos(angle) * radius, 0.0f, std::sin(angle) * radius};
            } else {
                local = {std::cos(angle) * radius, std::sin(angle) * radius, 0.0f};
            }
            const myEngine::Vector3 current = transformLocalPoint(local, center, rotate, scale);
            if (i > 0) {
                queueWireLine(previous, current, lineWidth, color);
            }
            previous = current;
        }
    };

    auto queueShooter = [&](const myEngine::Vector3& center, float radius, const myEngine::Vector3& rotate, const myEngine::Vector3& scale,
                            float lineWidth, const myEngine::Vector4& color) {
        queueCircle3D(center, radius, rotate, scale, 0, lineWidth, color);
        queueCircle3D(center, radius, rotate, scale, 1, lineWidth, color);
        queueCircle3D(center, radius, rotate, scale, 2, lineWidth, color);

        const float barrelLength = radius * 0.9f;
        const float barrelHalf = radius * 0.22f;
        const float barrelOffset = radius * 0.72f;
        myEngine::Vector3 corners[8] = {{-barrelHalf, -barrelOffset, -barrelHalf},
                                   {barrelHalf, -barrelOffset, -barrelHalf},
                                   {barrelHalf, -barrelOffset - barrelLength, -barrelHalf},
                                   {-barrelHalf, -barrelOffset - barrelLength, -barrelHalf},
                                   {-barrelHalf, -barrelOffset, barrelHalf},
                                   {barrelHalf, -barrelOffset, barrelHalf},
                                   {barrelHalf, -barrelOffset - barrelLength, barrelHalf},
                                   {-barrelHalf, -barrelOffset - barrelLength, barrelHalf}};
        for (myEngine::Vector3& corner : corners) {
            corner = transformLocalPoint(corner, center, rotate, scale);
        }
        const int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
        for (const auto& edge : edges) {
            queueWireLine(corners[edge[0]], corners[edge[1]], lineWidth, color);
        }
    };

    auto queueBillboardPolygon = [&](const myEngine::Vector3& center, int sides, float radius, float rotation, float lineWidth,
                                     const myEngine::Vector4& color, bool luminous = true) {
        constexpr float kTwoPi = 6.28318530718f;
        if (sides < 3 || sides > 48)
            return;
        myEngine::Vector3 points[48]{};
        for (int i = 0; i < sides; ++i) {
            const float angle = rotation - kTwoPi * 0.25f + static_cast<float>(i) * (kTwoPi / static_cast<float>(sides));
            points[i] = center + cameraRight * (std::cos(angle) * radius) + cameraUp * (std::sin(angle) * radius);
        }
        if (luminous)
            world_.resources.neonGridRenderer_->QueueContourPolygon(points, static_cast<uint32_t>(sides), lineWidth, color, cameraForward,
                                                                    myEngine::ActorNeonContourStyle());
        else
            for (int i = 0; i < sides; ++i)
                world_.resources.neonGridRenderer_->QueueCameraFacingLine(points[i], points[(i + 1) % sides], lineWidth, color,
                                                                          cameraForward);
    };

    auto queueBillboardShooter = [&](const myEngine::Vector3& center, float radius, float rotation, float lineWidth, const myEngine::Vector4& color,
                                     float warningRatio, float muzzleFlashRatio) {
        warningRatio = (std::clamp)(warningRatio, 0.0f, 1.0f);
        muzzleFlashRatio = (std::clamp)(muzzleFlashRatio, 0.0f, 1.0f);
        const float warningImpact = warningRatio * warningRatio;
        if (warningRatio > 0.0f) {
            const float warningRadiusScale = 1.50f - warningRatio * 0.42f;
            const myEngine::Vector4 warningColor{1.80f + warningImpact * 0.60f, 0.25f + warningImpact * 1.20f, 0.08f + warningImpact * 0.55f,
                                            0.28f + warningRatio * 0.72f};
            queueBillboardPolygon(center, 20, radius * warningRadiusScale, rotation, lineWidth * (0.75f + warningRatio * 0.55f),
                                  warningColor, false);
        }

        const myEngine::Vector4 bodyColor{color.x + warningImpact * 0.65f, color.y + warningImpact * 0.85f, color.z + warningImpact * 0.32f,
                                     color.w};
        const myEngine::Vector4 barrelColor{bodyColor.x + warningImpact * 0.35f, bodyColor.y + warningImpact * 0.30f,
                                       bodyColor.z + warningImpact * 0.12f, bodyColor.w};
        queueBillboardPolygon(center, 20, radius, rotation, lineWidth, bodyColor);
        const float c = std::cos(rotation);
        const float s = std::sin(rotation);
        const myEngine::Vector3 down = cameraRight * s + cameraUp * -c;
        const myEngine::Vector3 side = cameraRight * c + cameraUp * s;
        const myEngine::Vector3 base = center + down * (radius * 0.65f);
        const myEngine::Vector3 tip = center + down * (radius * 1.45f);
        const float half = radius * 0.18f;
        const myEngine::Vector3 barrelPoints[]{base - side * half, tip - side * half, tip + side * half, base + side * half};
        world_.resources.neonGridRenderer_->QueueContourPolygon(barrelPoints, 4, lineWidth, barrelColor, cameraForward,
                                                                myEngine::ActorNeonContourStyle());

        if (muzzleFlashRatio > 0.0f) {
            const float flashRadius = radius * (0.10f + muzzleFlashRatio * 0.18f);
            const myEngine::Vector4 flashColor{2.60f, 1.85f, 0.65f, muzzleFlashRatio};
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
        const myEngine::Vector3 visualScale = expEnemy->GetVisualScale();
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

        const myEngine::Vector3 center = expEnemy->GetWorldPosition() +
                                    (world_.presentation.expEnemyNeonRenderMode_ >= 2 ? myEngine::Vector3{} : myEngine::Vector3{0.0f, 0.0f, 0.35f});
        const myEngine::Vector4 color = expEnemy->GetVisualColor();
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
                world_.presentation.expEnemyNeonLineWidth_, color, cameraRight, cameraUp, cameraForward, myEngine::ActorNeonContourStyle());
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
                myEngine::ActorNeonContourStyle());
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
        const myEngine::Vector3 visualScale = expEnemy->GetVisualScale();
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

    myEngine::Matrix4x4 viewMatrix = myEngine::Object3dCommon::GetInstance()->GetIsDebugCamera() ? world_.resources.debugCamera->GetViewMatrix()
                                                                                       : world_.resources.camera->GetViewMatrix();
    myEngine::Matrix4x4 cameraWorld = myEngine::Inverse(viewMatrix);
    myEngine::Vector3 cameraRight = myEngine::Normalize({cameraWorld.m[0][0], cameraWorld.m[0][1], cameraWorld.m[0][2]});
    myEngine::Vector3 cameraUp = myEngine::Normalize({cameraWorld.m[1][0], cameraWorld.m[1][1], cameraWorld.m[1][2]});
    myEngine::Vector3 cameraForward = myEngine::Normalize({cameraWorld.m[2][0], cameraWorld.m[2][1], cameraWorld.m[2][2]});

    world_.resources.neonGridRenderer_->BeginFrame();
    world_.resources.neonGridRenderer_->SetLineStyle(world_.presentation.neonLineSoftEdgeRatio_,
                                                     world_.presentation.neonLineCoreIntensity_);
    QueueExpEnemyNeonShapes(cameraRight, cameraUp, cameraForward);

    myEngine::Matrix4x4 vp = myEngine::Object3dCommon::GetInstance()->GetIsDebugCamera() ? world_.resources.debugCamera->GetViewProjectionMatrix()
                                                                               : world_.resources.camera->GetViewProjectionMatrix();
    world_.resources.neonGridRenderer_->DrawAll(vp);
}

myEngine::Vector3 ResourceNeonRenderer::GetFloorVisibilityCenter() const
{
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    if (world_.presentation.developerStageTiltApplied_) return world_.presentation.developerStageBaseCamera_;
#endif
    // A tilted camera's eye is outside the room in XY. Floor culling follows
    // its displayed focus; facing/depth calculations still use the real eye.
    return world_.resources.neonDepthCameraScoped_
               ? world_.resources.neonDepthFloorViewCenter_
               : GetActiveCameraPosition(world_.resources.camera.get(), world_.resources.debugCamera.get());
}

bool ResourceNeonRenderer::IsNearCamera2D(const myEngine::Vector3& worldPos, float halfWidth, float halfHeight, float margin) const
{
    const myEngine::Vector3 cameraPos = GetFloorVisibilityCenter();
    return worldPos.x >= cameraPos.x - halfWidth - margin && worldPos.x <= cameraPos.x + halfWidth + margin &&
           worldPos.y >= cameraPos.y - halfHeight - margin && worldPos.y <= cameraPos.y + halfHeight + margin;
}
} // namespace gameplay
