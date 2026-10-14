#include "game/render/session/StageNeonRenderer.h"
#include <set>
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

void StageNeonRenderer::DrawNeonGridPass(bool includeStageBlockOutlines)
{
    if (!world_.resources.neonGridRenderer_) {
        return;
    }

    world_.resources.neonGridRenderer_->BeginFrame();
    world_.resources.neonGridRenderer_->SetLineStyle(world_.presentation.neonLineSoftEdgeRatio_,
                                                     world_.presentation.neonLineCoreIntensity_);
    const auto& surface = world_.presentation.stageSurface_;
    const bool surfaceFloor = world_.run.prototypeRun_ && surface.enabled && surface.floorEnabled;
    const float gridZ = surfaceFloor ? surface.floorDepth - stagesurface::kGridClearance : -0.04f;
    const float localGridZ = surfaceFloor ? gridZ : -0.03f;
    float minimumGridWidth = 0.0f;
    if (surfaceFloor) {
        const bool debug = cg2::Object3dCommon::GetInstance()->GetIsDebugCamera();
        const auto& view = debug ? world_.resources.debugCamera->GetViewMatrix() : world_.resources.camera->GetViewMatrix();
        const auto& projection = debug ? world_.resources.debugCamera->GetProjectionMatrix() : world_.resources.camera->GetProjectionMatrix();
        auto focus = world_.resourceNeonRenderer->GetFloorVisibilityCenter();
        focus.z = gridZ;
        const float depth = std::abs(cg2::TransformMatrix(focus, view).z);
        const auto cameraWorld = cg2::Inverse(view);
        const float facing = (std::max)(0.35f, std::abs(cameraWorld.m[2][2]));
        const float pixels = (std::max)(1.0f, cg2::Object3dCommon::GetInstance()->GetDxCommon()->GetRenderTargetState().viewport.Height);
        minimumGridWidth = surface.gridMinPixelWidth * depth * 2.0f /
            ((std::max)(0.001f, std::abs(projection.m[1][1])) * pixels * facing);
    }
    auto gridWidth = [&](float authored) { return authored > 0.0f ? (std::max)(authored, minimumGridWidth) : authored; };
    auto gridColor = [&](cg2::Vector4 authored, float width) {
        // Preserve emitted energy when widening subpixel lines in distant/tilted views.
        if (width > 0.0f) authored.w *= width / gridWidth(width);
        return authored;
    };
    if (world_.presentation.showNeonGrid_) {
        const float minX = 0.0f;
        const float minY = 0.0f;
        const float maxX = MapChip::kBlockWidth * static_cast<float>(MapChip::kNumBlockHorizontal - 1);
        const float maxY = MapChip::kBlockHeight * static_cast<float>(MapChip::kNumBlockVirtical - 1);
        world_.resources.neonGridRenderer_->QueueWorldGrid(minX, maxX, minY, maxY, world_.presentation.worldGridSpacing_,
                                                           gridWidth(world_.presentation.worldGridLineWidth_),
                                                           gridColor(world_.presentation.worldGridColor_, world_.presentation.worldGridLineWidth_), gridZ);
    }
    if (world_.presentation.showActorLocalGrid_) {
        const float fieldMinX = 0.0f;
        const float fieldMinY = 0.0f;
        const float fieldMaxX = MapChip::kBlockWidth * static_cast<float>(MapChip::kNumBlockHorizontal - 1);
        const float fieldMaxY = MapChip::kBlockHeight * static_cast<float>(MapChip::kNumBlockVirtical - 1);
        if (!world_.resources.player_->IsDead()) {
            world_.resources.neonGridRenderer_->QueueLocalGridClipped(
                world_.resources.player_->GetWorldPosition(), world_.presentation.actorGridRadius_, world_.presentation.actorGridSpacing_,
                gridWidth(world_.presentation.actorGridLineWidth_), gridColor(world_.presentation.playerGridColor_, world_.presentation.actorGridLineWidth_),
                fieldMinX, fieldMaxX, fieldMinY, fieldMaxY, localGridZ);
        }
        if ((world_.gameplayQueries->IsRunRivalActive() && !world_.resources.enemy_->IsDead())) {
            world_.resources.neonGridRenderer_->QueueLocalGridClipped(
                world_.resources.enemy_->GetWorldPosition(), world_.presentation.actorGridRadius_ * 1.15f,
                world_.presentation.actorGridSpacing_, gridWidth(world_.presentation.actorGridLineWidth_),
                gridColor(world_.presentation.enemyGridColor_, world_.presentation.actorGridLineWidth_),
                fieldMinX, fieldMaxX, fieldMinY, fieldMaxY, localGridZ);
        }
        int expEnemyLocalGridCount = 0;
        for (ExpEnemy* expEnemy : world_.resources.enemyManager_->GetEnemyPtrs()) {
            if (expEnemy && !expEnemy->IsDead()) {
                if (world_.presentation.maxExpEnemyLocalGrids_ >= 0 &&
                    expEnemyLocalGridCount >= world_.presentation.maxExpEnemyLocalGrids_) {
                    break;
                }
                if (world_.presentation.cullActorLocalGrid_ &&
                    !world_.resourceNeonRenderer->IsNearCamera2D(expEnemy->GetWorldPosition(), 38.0f, 24.0f,
                                                                 world_.presentation.actorGridRadius_)) {
                    continue;
                }
                world_.resources.neonGridRenderer_->QueueLocalGridClipped(
                    expEnemy->GetWorldPosition(), world_.presentation.actorGridRadius_ * 0.6f, world_.presentation.actorGridSpacing_,
                    gridWidth(world_.presentation.actorGridLineWidth_ * 0.8f),
                    gridColor(world_.presentation.expEnemyGridColor_, world_.presentation.actorGridLineWidth_ * 0.8f), fieldMinX, fieldMaxX, fieldMinY,
                    fieldMaxY, localGridZ);
                ++expEnemyLocalGridCount;
            }
        }
    }
    if (world_.presentation.showLevelAIDitorPreview_) {
        world_.gameplayEditor->QueueLevelEditorPreview();
    }
    if (world_.run.prototypeRun_)
        world_.arenaRunController->QueueTankRunTelegraph();
    if (includeStageBlockOutlines &&
        (world_.presentation.showStageBlockNeonOutlines_ || world_.presentation.showStageDamageBlockNeonOutlines_)) {
        QueueStageBlockNeonOutlines();
    }
    // Draw the floor, then dark actor faces, then the rims and combat glyphs.
    // Filling after the glow pass hid the stems / inner marks of combat roles.
    uint32_t foregroundStart = 0;
    if (world_.presentation.fillActorNeonBodies_ &&
        (world_.presentation.playerNeonRenderMode_ == 1 || world_.presentation.bossNeonRenderMode_ == 1 ||
         world_.presentation.expEnemyNeonRenderMode_ == 1)) {
        const cg2::Matrix4x4 vp = cg2::Object3dCommon::GetInstance()->GetIsDebugCamera()
                                      ? world_.resources.debugCamera->GetViewProjectionMatrix()
                                      : world_.resources.camera->GetViewProjectionMatrix();
        world_.resources.neonGridRenderer_->DrawRange(0, world_.resources.neonGridRenderer_->GetVertexCount(), vp,
                                                      world_.resources.neonDepthCameraScoped_ &&
                                                          !cg2::Object3dCommon::GetInstance()->GetDxCommon()->HasCurrentDSV());
        world_.actorNeonRenderer->DrawActorNeonBodyFillPass(false);
        foregroundStart = world_.resources.neonGridRenderer_->GetVertexCount();
    }
    const bool queueExpEnemyNeonInGridPass =
        world_.presentation.expEnemyNeonRenderMode_ == 1 || world_.presentation.expEnemyNeonRenderMode_ == 2;
    const bool queueActorBillboards = world_.presentation.playerNeonRenderMode_ == 1 || world_.presentation.bossNeonRenderMode_ == 1 ||
                                      !world_.presentation.playerNeonAfterimages_.empty();
    if (world_.presentation.showNeonTriangleDemo_ || queueExpEnemyNeonInGridPass || queueActorBillboards ||
        !world_.presentation.playerLaserBeams_.empty() || !world_.presentation.playerMines_.empty() ||
        !world_.presentation.playerMineExplosions_.empty() || !world_.presentation.playerMeleeSlashes_.empty() ||
        !world_.presentation.neonTriangleParticles_.empty()) {
        cg2::Matrix4x4 viewMatrix = cg2::Object3dCommon::GetInstance()->GetIsDebugCamera() ? world_.resources.debugCamera->GetViewMatrix()
                                                                                           : world_.resources.camera->GetViewMatrix();
        cg2::Matrix4x4 cameraWorld = cg2::Inverse(viewMatrix);
        cg2::Vector3 cameraRight = cg2::Normalize({cameraWorld.m[0][0], cameraWorld.m[0][1], cameraWorld.m[0][2]});
        cg2::Vector3 cameraUp = cg2::Normalize({cameraWorld.m[1][0], cameraWorld.m[1][1], cameraWorld.m[1][2]});
        cg2::Vector3 cameraForward = cg2::Normalize({cameraWorld.m[2][0], cameraWorld.m[2][1], cameraWorld.m[2][2]});
        if (queueExpEnemyNeonInGridPass) {
            world_.resourceNeonRenderer->QueueExpEnemyNeonShapes(cameraRight, cameraUp, cameraForward);
        }
        if (queueActorBillboards) {
            world_.actorNeonRenderer->QueueActorNeonBillboards(cameraRight, cameraUp);
        }
        world_.playerAttackEffects->QueuePlayerLasers(cameraForward);
        world_.playerAttackEffects->QueuePlayerMines(cameraRight, cameraUp, cameraForward);
        world_.playerAttackEffects->QueuePlayerMeleeSlashes();
        world_.combatEffects->QueueNeonTriangleParticles(cameraRight, cameraUp, cameraForward);
        if (world_.presentation.showNeonTriangleDemo_) {
            world_.resources.neonGridRenderer_->QueueBillboardTriangle(
                world_.presentation.neonTriangleDemoCenter_, world_.presentation.neonTriangleDemoRadius_,
                world_.validation.neonTriangleDemoRotation_, world_.validation.neonTriangleDemoLineWidth_,
                world_.validation.neonTriangleDemoColor_, cameraRight, cameraUp, cameraForward);
        }
    }

    world_.playerAttackEffects->QueueSpecialCombatPresentation();
    cg2::Matrix4x4 vp = cg2::Object3dCommon::GetInstance()->GetIsDebugCamera() ? world_.resources.debugCamera->GetViewProjectionMatrix()
                                                                               : world_.resources.camera->GetViewProjectionMatrix();
    world_.resources.neonGridRenderer_->DrawRange(
        foregroundStart, world_.resources.neonGridRenderer_->GetVertexCount() - foregroundStart, vp,
        world_.resources.neonDepthCameraScoped_ && !cg2::Object3dCommon::GetInstance()->GetDxCommon()->HasCurrentDSV());
}

void StageNeonRenderer::DrawStageBlockNeonPass()
{
    if (!world_.resources.neonGridRenderer_ ||
        (!world_.presentation.showStageBlockNeonOutlines_ && !world_.presentation.showStageDamageBlockNeonOutlines_)) {
        return;
    }

    const uint32_t startVertex = world_.resources.neonGridRenderer_->GetVertexCount();
    world_.resources.neonGridRenderer_->SetLineStyle(world_.presentation.neonLineSoftEdgeRatio_,
                                                     world_.presentation.neonLineCoreIntensity_);
    QueueStageBlockNeonOutlines();
    const uint32_t vertexCount = world_.resources.neonGridRenderer_->GetVertexCount() - startVertex;
    if (vertexCount == 0) {
        return;
    }

    cg2::Matrix4x4 vp = cg2::Object3dCommon::GetInstance()->GetIsDebugCamera() ? world_.resources.debugCamera->GetViewProjectionMatrix()
                                                                               : world_.resources.camera->GetViewProjectionMatrix();
    world_.resources.neonGridRenderer_->DrawRange(startVertex, vertexCount, vp);
}

void StageNeonRenderer::QueueStageBlockNeonOutlines()
{
    if (!world_.resources.stage_ || !world_.resources.neonGridRenderer_) {
        return;
    }

    cg2::Matrix4x4 viewMatrix = cg2::Object3dCommon::GetInstance()->GetIsDebugCamera() ? world_.resources.debugCamera->GetViewMatrix()
                                                                                       : world_.resources.camera->GetViewMatrix();
    cg2::Matrix4x4 cameraWorld = cg2::Inverse(viewMatrix);
    cg2::Vector3 cameraForward = cg2::Normalize({cameraWorld.m[2][0], cameraWorld.m[2][1], cameraWorld.m[2][2]});
    const cg2::Vector3 cameraPos = GetActiveCameraPosition(world_.resources.camera.get(), world_.resources.debugCamera.get());
    const auto& surface = world_.presentation.stageSurface_;
    const bool surfaceBlocks = world_.run.prototypeRun_ && surface.enabled && surface.blocksEnabled;
    if (surfaceBlocks && surface.wallStyle == 1) {
        // Join cell grids without a separately bloomed frame around every tile.
        // A deduplicated segment set also removes shared rectangle seams.
        std::set<std::array<int,6>> emitted;
        for (const auto& wall : world_.resources.stage_->GetSurfaceWalls(surface)) {
            const bool damage = wall.type == MapChipType::kDamageBlock;
            if ((damage && !world_.presentation.showStageDamageBlockNeonOutlines_) ||
                (!damage && !world_.presentation.showStageBlockNeonOutlines_)) continue;
            const auto visual = Stage::SurfaceWallTransform(wall, surface);
            if (!world_.resourceNeonRenderer->IsNearCamera2D(visual.translate,
                38.0f+std::abs(visual.scale.x),24.0f+std::abs(visual.scale.y),3.0f)) continue;
            const auto matrix = cg2::MakeAffineMatrix(visual.scale,visual.rotate,visual.translate);
            auto world = [&](cg2::Vector3 p) { return cg2::TransformMatrix(p,matrix); };
            auto color = wall.exterior ? cg2::Vector4{1.0f,.10f,.22f,.80f} :
                (damage ? world_.presentation.stageDamageBlockNeonColor_ : world_.presentation.stageBlockNeonColor_);
            float pulse = 1.0f;
            if (damage) pulse = world_.presentation.stageDamageBlockPulseMin_ +
                (world_.presentation.stageDamageBlockPulseMax_-world_.presentation.stageDamageBlockPulseMin_)*
                (.5f+.5f*std::sin(world_.presentation.stageDamageBlockPulseTime_*world_.presentation.stageDamageBlockPulseSpeed_));
            color.x*=surface.blockEmission*pulse; color.y*=surface.blockEmission*pulse; color.z*=surface.blockEmission*pulse;
            auto line = [&](cg2::Vector3 a,cg2::Vector3 b,cg2::Vector3 normal,bool rim) {
                const auto n=cg2::Normalize(world(normal)-world({0,0,0}));
                a=world(a)+n*.012f; b=world(b)+n*.012f;
                std::array<int,3> ka{int(std::round(a.x*1000)),int(std::round(a.y*1000)),int(std::round(a.z*1000))};
                std::array<int,3> kb{int(std::round(b.x*1000)),int(std::round(b.y*1000)),int(std::round(b.z*1000))};
                if (kb<ka) std::swap(ka,kb);
                if (!emitted.insert({ka[0],ka[1],ka[2],kb[0],kb[1],kb[2]}).second) return;
                cg2::NeonContourStyle style;
                style.roundCaps=false; style.coreWidthRatio=.28f; style.coreIntensity=1.0f;
                style.coreWhiteMix=.02f; style.shoulderIntensity=.80f; style.haloIntensity=.20f; style.haloAlpha=.04f;
                auto c=color;
                const float gain=(rim?1.0f:.46f)*(std::abs(normal.z)>.5f?1.0f:.65f);
                c.x*=gain; c.y*=gain; c.z*=gain;
                world_.resources.neonGridRenderer_->QueueContourLine(a,b,surface.wallLineWidth,c,cameraForward,style);
            };
            auto visible = [&](cg2::Vector3 normal) {
                return cg2::Dot(cg2::Normalize(world(normal)-world({0,0,0})),cameraPos-world(normal))>=0.0f;
            };
            const int nx=(std::max)(1,int(std::round(std::abs(visual.scale.x))));
            const int ny=(std::max)(1,int(std::round(std::abs(visual.scale.y))));
            const int nz=wall.isolated?1:(wall.exterior?(std::clamp)(int(std::round(surface.exteriorHeight/2)),1,6):surface.stackLevels);
            auto coordinate=[](int i,int n) { return -1.0f+2.0f*float(i)/float(n); };
            for (float z:{-1.0f,1.0f}) if (visible({0,0,z})) {
                for (int i=0;i<=nx;++i) { const float x=coordinate(i,nx); line({x,-1,z},{x,1,z},{0,0,z},false); }
                for (int i=0;i<=ny;++i) { const float y=coordinate(i,ny); line({-1,y,z},{1,y,z},{0,0,z},false); }
            }
            for (float y:{-1.0f,1.0f}) if (visible({0,y,0})) {
                for (int i=0;i<=nx;++i) { const float x=coordinate(i,nx); line({x,y,-1},{x,y,1},{0,y,0},false); }
                for (int i=0;i<=nz;++i) { const float z=coordinate(i,nz); line({-1,y,z},{1,y,z},{0,y,0},i==0||i==nz); }
            }
            for (float x:{-1.0f,1.0f}) if (visible({x,0,0})) {
                for (int i=0;i<=ny;++i) { const float y=coordinate(i,ny); line({x,y,-1},{x,y,1},{x,0,0},false); }
                for (int i=0;i<=nz;++i) { const float z=coordinate(i,nz); line({x,-1,z},{x,1,z},{x,0,0},i==0||i==nz); }
            }
        }
        return;
    }
    const float faceExtent = surfaceBlocks ? 1.0f - stagesurface::kBevel : 1.0f;

    auto divisionCount = [](float scale) {
        return (std::max)(1, static_cast<int>(std::round(std::abs(scale))));
    };

    const auto& blocks = world_.resources.stage_->GetBlocks();
    for (const std::vector<Block>& row : blocks) {
        for (const Block& block : row) {
            const bool isNormalBlock = block.type == MapChipType::kBlock;
            const bool isDamageBlock = block.type == MapChipType::kDamageBlock;
            if (!block.isActive || (!isNormalBlock && !isDamageBlock)) {
                continue;
            }
            if ((isNormalBlock && !world_.presentation.showStageBlockNeonOutlines_) ||
                (isDamageBlock && !world_.presentation.showStageDamageBlockNeonOutlines_)) {
                continue;
            }
            if (world_.presentation.cullActorLocalGrid_ &&
                !world_.resourceNeonRenderer->IsNearCamera2D(block.originalPos, 38.0f, 24.0f, MapChip::kBlockWidth * 1.5f)) {
                continue;
            }

            const cg2::Transform visual = surfaceBlocks ? stagesurface::BlockTransform(block.worldTransform, surface) : block.worldTransform;
            const cg2::Matrix4x4 transform = cg2::MakeAffineMatrix(visual.scale, visual.rotate, visual.translate);

            auto localToWorld = [&](const cg2::Vector3& local) {
                return cg2::TransformMatrix(local, transform);
            };
            cg2::Vector4 blockNeonColor = world_.presentation.stageBlockNeonColor_;
            if (isDamageBlock) {
                const float wave = 0.5f + 0.5f * std::sin(world_.presentation.stageDamageBlockPulseTime_ *
                                                          world_.presentation.stageDamageBlockPulseSpeed_);
                const float pulse = world_.presentation.stageDamageBlockPulseMin_ +
                                    (world_.presentation.stageDamageBlockPulseMax_ - world_.presentation.stageDamageBlockPulseMin_) * wave;
                blockNeonColor = world_.presentation.stageDamageBlockNeonColor_;
                blockNeonColor.x *= pulse;
                blockNeonColor.y *= pulse;
                blockNeonColor.z *= pulse;
                blockNeonColor.w *= 0.75f + wave * 0.25f;
            }
            if (surfaceBlocks) {
                blockNeonColor.x *= surface.blockEmission;
                blockNeonColor.y *= surface.blockEmission;
                blockNeonColor.z *= surface.blockEmission;
            }
            auto queueLocalLine = [&](const cg2::Vector3& start, const cg2::Vector3& end, const cg2::Vector3& localNormal, bool boundary) {
                cg2::Vector3 worldStart = localToWorld(start);
                cg2::Vector3 worldEnd = localToWorld(end);
                cg2::Vector3 worldNormal = localToWorld(localNormal) - localToWorld({0.0f, 0.0f, 0.0f});
                if (cg2::Length(worldNormal) > 0.0001f && world_.presentation.stageBlockNeonDepthBias_ > 0.0f) {
                    worldNormal = cg2::Normalize(worldNormal);
                    worldStart = worldStart + worldNormal * world_.presentation.stageBlockNeonDepthBias_;
                    worldEnd = worldEnd + worldNormal * world_.presentation.stageBlockNeonDepthBias_;
                }
                cg2::NeonContourStyle contour;
                contour.coreWidthRatio = boundary ? 0.13f : 0.10f;
                contour.coreIntensity = boundary ? 1.8f : 1.0f;
                contour.coreWhiteMix = boundary ? 0.12f : 0.0f;
                contour.shoulderIntensity = boundary ? 1.5f : 0.85f;
                contour.haloIntensity = boundary ? 0.55f : 0.28f;
                contour.haloAlpha = boundary ? 0.09f : 0.07f;
                cg2::Vector4 faceColor = blockNeonColor;
                const float faceLight = std::abs(localNormal.z) > 0.5f ? 1.0f : 0.58f;
                const float light = faceLight * (boundary ? 1.0f : 0.55f);
                faceColor.x *= light;
                faceColor.y *= light;
                faceColor.z *= light;
                contour.roundCaps = false; // Subdivision endpoints already meet neighboring block edges.
                world_.resources.neonGridRenderer_->QueueContourLine(
                    worldStart, worldEnd, world_.presentation.stageBlockNeonLineWidth_ * (boundary ? 0.82f : 0.48f), faceColor,
                    cameraForward, contour);
            };
            auto shouldDrawFace = [&](const cg2::Vector3& localCenter, const cg2::Vector3& localNormal) {
                const cg2::Vector3 worldCenter = localToWorld(localCenter);
                const cg2::Vector3 worldNormal = localToWorld(localCenter + localNormal) - worldCenter;
                const cg2::Vector3 toCamera = cameraPos - worldCenter;
                const float normalLength = cg2::Length(worldNormal);
                const float toCameraLength = cg2::Length(toCamera);
                if (normalLength <= 0.0001f || toCameraLength <= 0.0001f) {
                    return true;
                }
                const float facingDot = cg2::Dot(worldNormal / normalLength, toCamera / toCameraLength);
                return facingDot >= -0.08f;
            };
            auto localCoord = [faceExtent](int index, int divisions) {
                return faceExtent * (-1.0f + 2.0f * (static_cast<float>(index) / static_cast<float>(divisions)));
            };

            const int divX = divisionCount(block.worldTransform.scale.x);
            const int divY = divisionCount(block.worldTransform.scale.y);
            const int divZ = surfaceBlocks ? surface.sideLineDivisions : divisionCount(block.worldTransform.scale.z);

            for (float z : {-1.0f, 1.0f}) {
                const cg2::Vector3 faceNormal = {0.0f, 0.0f, z};
                if (!shouldDrawFace({0.0f, 0.0f, z}, {0.0f, 0.0f, z})) {
                    continue;
                }
                for (int i = 0; i <= divX; ++i) {
                    const float x = localCoord(i, divX);
                    queueLocalLine({x, -faceExtent, z}, {x, faceExtent, z}, faceNormal, i == 0 || i == divX);
                }
                for (int i = 0; i <= divY; ++i) {
                    const float y = localCoord(i, divY);
                    queueLocalLine({-faceExtent, y, z}, {faceExtent, y, z}, faceNormal, i == 0 || i == divY);
                }
            }

            for (float y : {-1.0f, 1.0f}) {
                const cg2::Vector3 faceNormal = {0.0f, y, 0.0f};
                if (!shouldDrawFace({0.0f, y, 0.0f}, {0.0f, y, 0.0f})) {
                    continue;
                }
                for (int i = 0; i <= divX; ++i) {
                    const float x = localCoord(i, divX);
                    queueLocalLine({x, y, -faceExtent}, {x, y, faceExtent}, faceNormal, i == 0 || i == divX);
                }
                for (int i = 0; i <= divZ; ++i) {
                    const float z = localCoord(i, divZ);
                    queueLocalLine({-faceExtent, y, z}, {faceExtent, y, z}, faceNormal, i == 0 || i == divZ);
                }
            }

            for (float x : {-1.0f, 1.0f}) {
                const cg2::Vector3 faceNormal = {x, 0.0f, 0.0f};
                if (!shouldDrawFace({x, 0.0f, 0.0f}, {x, 0.0f, 0.0f})) {
                    continue;
                }
                for (int i = 0; i <= divY; ++i) {
                    const float y = localCoord(i, divY);
                    queueLocalLine({x, y, -faceExtent}, {x, y, faceExtent}, faceNormal, i == 0 || i == divY);
                }
                for (int i = 0; i <= divZ; ++i) {
                    const float z = localCoord(i, divZ);
                    queueLocalLine({x, -faceExtent, z}, {x, faceExtent, z}, faceNormal, i == 0 || i == divZ);
                }
            }
        }
    }
}

cg2::Vector2 StageNeonRenderer::GetStagePostCacheUvOffset(const cg2::Vector3& currentCameraPos) const
{
    const cg2::Vector2 cacheCenterScreen = world_.gameplayHud->WorldToScreen(
        {world_.presentation.stagePostCacheCameraPos_.x, world_.presentation.stagePostCacheCameraPos_.y, 0.0f});
    const cg2::Vector2 currentCenterScreen = world_.gameplayHud->WorldToScreen({currentCameraPos.x, currentCameraPos.y, 0.0f});
    return {(cacheCenterScreen.x - currentCenterScreen.x) / static_cast<float>(cg2::WinApp::kClientWidth),
            (cacheCenterScreen.y - currentCenterScreen.y) / static_cast<float>(cg2::WinApp::kClientHeight)};
}
} // namespace gameplay
