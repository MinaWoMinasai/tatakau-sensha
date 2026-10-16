#include "game/render/session/GameplayRenderer.h"
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

void GameplayRenderer::Draw()
{

    myEngine::Object3dCommon::GetInstance()->PreDraw(myEngine::kNormal);
}

void GameplayRenderer::DrawPostEffect3D()
{

    world_.performanceMonitor->ResetPostProfileEntries();
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    // Bloom::PreDraw直後は3枚のScene MRT + D24S8。ObjectPostEffect captureより先に描く。
    if (world_.resources.neonSkinnedPreview_)
        world_.resources.neonSkinnedPreview_->Draw();
    if (world_.gameplayQueries->IsNeonShowcaseActive())
        return;
#endif
    if (world_.resources.player_) {
        for (PlayerDrone* drone : world_.resources.player_->GetDronePtrs()) {
            if (drone)
                drone->SetNeonVisual(world_.run.prototypeRun_ && world_.presentation.playerNeonRenderMode_ == 1);
        }
    }
    ExpEnemy::SetShapeNeonRenderMode(world_.presentation.expEnemyNeonRenderMode_);
    if (world_.resources.skybox_) {
    }
    myEngine::Object3dCommon::GetInstance()->PreDraw(myEngine::kNormal);
    const bool useGridPost =
        world_.presentation.enableNeonGridPostEffect_ && world_.performanceMonitor->IsPostProfileCategoryEnabled("Grid");
    const bool depthOverlay = world_.resources.enemy_ && world_.resources.enemy_->IsNeonDepthEncounterEnabled() &&
                              world_.resources.neonDepthEffects_ && world_.resources.neonDepthEffects_->HasResources();
    const bool useStagePost =
        world_.presentation.enableStagePostEffect_ && world_.performanceMonitor->IsPostProfileCategoryEnabled("Stage");
    const bool useBulletTrailPost =
        world_.presentation.enableBulletTrailPostEffect_ && world_.performanceMonitor->IsPostProfileCategoryEnabled("BulletTrail");
    const bool usePlayerPost = world_.combat.enablePlayerPostEffect_ && world_.performanceMonitor->IsPostProfileCategoryEnabled("Player");
    const bool useEnemyPost = world_.combat.enableEnemyPostEffect_ && world_.performanceMonitor->IsPostProfileCategoryEnabled("Enemy");
    const bool useExpEnemyPost =
        world_.presentation.enableExpEnemyPostEffect_ && world_.performanceMonitor->IsPostProfileCategoryEnabled("ExpEnemy");
    auto profile = [this](const char* name, bool active, auto&& drawFunc) {
        myEngine::RuntimeProfiler::GpuScope gpuScope(name);
        const auto start = std::chrono::steady_clock::now();
        drawFunc();
        const auto end = std::chrono::steady_clock::now();
        const float ms = std::chrono::duration<float, std::milli>(end - start).count();
        world_.performanceMonitor->AddPostProfileEntry(name, ms, active);
        myEngine::RuntimeProfiler::Get().AddCpu(name, ms);
    };

    stagesurface::Settings surface = world_.presentation.stageSurface_;
    surface.enabled = surface.enabled && world_.run.prototypeRun_;
    stagesurface::Sanitize(surface);
    if (world_.resources.player_ && world_.resources.player_->IsDead()) surface.floorLocalIntensity = 0.0f;
    const bool surfaceBlocks = surface.enabled && surface.blocksEnabled;
    if (surface.enabled && world_.resources.stage_) {
        profile("Stage Surfaces", true, [&]() {
            myEngine::Object3dCommon::GetInstance()->PreDraw(myEngine::kNone);
            const auto center = world_.resourceNeonRenderer->GetFloorVisibilityCenter();
            const auto lightPosition = world_.resources.player_ ? world_.resources.player_->GetWorldPosition() : center;
            const auto floors = world_.resources.stage_->DrawSurfaceFloor(surface, center, 38.0f, 24.0f, lightPosition);
            const auto blocks = world_.resources.stage_->DrawSurfaceBlocks(surface, center, 38.0f, 24.0f,
                world_.presentation.showStageNormalBlockBodies_);
            auto& diagnostics = myEngine::RuntimeProfiler::Get();
            diagnostics.SetCounter("Stage surface floor draws", floors);
            diagnostics.SetCounter("Stage surface block draws", blocks);
            diagnostics.SetCounter("Stage surface triangles", floors * 2u + world_.resources.stage_->GetSurfaceTriangles());
            myEngine::Object3dCommon::GetInstance()->PreDraw(myEngine::kNormal);
        });
    }
    world_.bossPresentation->DrawNeonBossVisual();
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    world_.depthValidation->DrawNeonDepthValidation();
#endif

    if (world_.presentation.expEnemyNeonRenderMode_ == 3) {
        profile("Exp Fill", true, [&]() {
            myEngine::Object3dCommon::GetInstance()->PreDraw(myEngine::kNormal);
            world_.resourceNeonRenderer->DrawExpEnemyNeonFillModels();
            world_.resourceNeonRenderer->DrawExpEnemyNeonDepthLines();
            myEngine::Object3dCommon::GetInstance()->PreDraw(myEngine::kNormal);
        });
    }

    if (useGridPost && !depthOverlay) {
        profile("Grid Post", true, [&]() {
            if (surface.enabled) world_.resources.neonGridPostEffect_->BeginCaptureWithCurrentDepth(true);
            else world_.resources.neonGridPostEffect_->BeginCapture();
            world_.stageNeonRenderer->DrawNeonGridPass(false);
            world_.resources.neonGridPostEffect_->EndCaptureAdditiveOnly();
            myEngine::Object3dCommon::GetInstance()->PreDraw(myEngine::kNormal);
        });
    } else if (!depthOverlay) {
        profile("Grid Draw", false, [&]() {
            world_.stageNeonRenderer->DrawNeonGridPass(false);
            myEngine::Object3dCommon::GetInstance()->PreDraw(myEngine::kNormal);
        });
    }
    if (!depthOverlay && world_.presentation.fillActorNeonBodies_ &&
        (world_.presentation.playerNeonRenderMode_ == 1 || world_.presentation.bossNeonRenderMode_ == 1)) {
        profile("Actor Bodies", true, [&]() {
            // Black glossy spheres over the bloomed lines, then their rings again on top.
            world_.actorNeonRenderer->DrawActorNeonBodyFillPass(true, ActorNeonRenderer::BodySet::RoundActors);
            myEngine::Object3dCommon::GetInstance()->PreDraw(myEngine::kNormal);
        });
    }
    if (depthOverlay && world_.presentation.fillActorNeonBodies_ &&
        (world_.presentation.playerNeonRenderMode_ == 1 || world_.presentation.bossNeonRenderMode_ == 1 ||
         world_.presentation.expEnemyNeonRenderMode_ == 1)) {
        profile("Actor Neon Fill", true, [&]() {
            world_.actorNeonRenderer->DrawActorNeonBodyFillPass();
            myEngine::Object3dCommon::GetInstance()->PreDraw(myEngine::kNormal);
        });
    } else {
        world_.performanceMonitor->AddPostProfileEntry("Actor Neon Fill", 0.0f, false);
    }

    profile("Base Objects", true, [&]() {
        world_.resources.player_->Draw(world_.presentation.playerNeonRenderMode_ == 0);
        if (!world_.bossPresentation->UseNeonBossVisual() && !world_.gameplayQueries->IsTutorialCombatSuppressed() &&
            world_.combat.combatFlow_.GetState() != GameFlowState::BossDefeatSequence) {
            world_.resources.enemy_->Draw(world_.presentation.bossNeonRenderMode_ == 0);
        }
        if (!world_.gameplayQueries->IsTutorialCombatSuppressed()) {
            world_.resources.enemyManager_->Draw(true);
        }
        world_.resources.bulletManager_->Draw();
        world_.levelRuntime->DrawLevelItems();
        if (!surfaceBlocks) world_.resources.stage_->DrawVisible(world_.resourceNeonRenderer->GetFloorVisibilityCenter(), 38.0f, 24.0f,
                                             world_.presentation.showStageNormalBlockBodies_);
    });
    world_.performanceMonitor->AddPostProfileEntry("Stage Glow", 0.0f, useStagePost);
    if (depthOverlay) {
        profile("Neon Depth FX Air", true, [&]() {
            world_.resources.neonDepthEffects_->DrawAir();
        });
        profile("Neon Depth FX Floor", true, [&]() {
            if (surface.enabled) world_.resources.neonGridPostEffect_->BeginCaptureWithCurrentDepth(true);
            else world_.resources.neonGridPostEffect_->BeginCapture();
            world_.resources.neonDepthEffects_->DrawFloor();
            world_.stageNeonRenderer->DrawNeonGridPass(false);
            world_.resources.neonGridPostEffect_->EndCaptureAdditiveOnly();
            myEngine::Object3dCommon::GetInstance()->PreDraw(myEngine::kNormal);
        });
    }

    if (world_.presentation.showStageBlockNeonOutlines_ || world_.presentation.showStageDamageBlockNeonOutlines_) {
        if (useGridPost) {
            profile("Stage Block Neon", true, [&]() {
                world_.resources.neonGridPostEffect_->BeginCaptureWithCurrentDepth(surface.enabled);
                world_.stageNeonRenderer->DrawStageBlockNeonPass();
                world_.resources.neonGridPostEffect_->EndCaptureAdditiveOnly();
                myEngine::Object3dCommon::GetInstance()->PreDraw(myEngine::kNormal);
            });
        } else {
            profile("Stage Block Neon", false, [&]() {
                world_.stageNeonRenderer->DrawStageBlockNeonPass();
                myEngine::Object3dCommon::GetInstance()->PreDraw(myEngine::kNormal);
            });
        }
    } else {
        world_.performanceMonitor->AddPostProfileEntry("Stage Block Neon", 0.0f, false);
    }

    {
        myEngine::Matrix4x4 vp = myEngine::Object3dCommon::GetInstance()->GetIsDebugCamera() ? world_.resources.debugCamera->GetViewProjectionMatrix()
                                                                                   : world_.resources.camera->GetViewProjectionMatrix();
        const bool hasTrailContent = world_.resources.bulletManager_->GetBulletCount() != 0 ||
                                     world_.resources.bulletManager_->HasDrawableTrails() ||
                                     (world_.presentation.enablePlayerMeleeRibbonTrail_ && world_.resources.playerMeleeTrailManager_ &&
                                      world_.resources.playerMeleeTrailManager_->HasDrawableInstances());
        // Prepare one immutable head batch for this frame, before either capture branch.
        const auto view = myEngine::Object3dCommon::GetInstance()->GetIsDebugCamera() ? world_.resources.debugCamera->GetViewMatrix()
                                                                                 : world_.resources.camera->GetViewMatrix();
        const auto cameraWorld = myEngine::Inverse(view);
        const myEngine::Vector3 forward{cameraWorld.m[2][0], cameraWorld.m[2][1], cameraWorld.m[2][2]};
        world_.resources.neonProjectileRenderer_->BeginFrame(world_.resources.bulletManager_->GetBulletPtrs(), forward);
        if (useBulletTrailPost && hasTrailContent) {
            profile("Trail Post", true, [&]() {
                if (surface.enabled) world_.resources.bulletTrailPostEffect_->BeginCaptureWithCurrentDepth(true);
                else world_.resources.bulletTrailPostEffect_->BeginCapture();
                world_.resources.bulletManager_->DrawTrails(vp);
                world_.resources.neonProjectileRenderer_->Draw(vp);
                if (world_.presentation.enablePlayerMeleeRibbonTrail_ && world_.resources.playerMeleeTrailManager_) {
                    world_.resources.playerMeleeTrailManager_->DrawAll(vp);
                }
                myEngine::Object3dCommon::GetInstance()->PreDraw(myEngine::kNormal);
                world_.resources.bulletManager_->Draw();
                world_.resources.bulletTrailPostEffect_->EndCaptureAdditiveOnly();
                myEngine::Object3dCommon::GetInstance()->PreDraw(myEngine::kNormal);
            });
        } else {
            profile("Trail Draw", false, [&]() {
                world_.resources.bulletManager_->DrawTrails(vp);
                world_.resources.neonProjectileRenderer_->Draw(vp);
                if (world_.presentation.enablePlayerMeleeRibbonTrail_ && world_.resources.playerMeleeTrailManager_) {
                    world_.resources.playerMeleeTrailManager_->DrawAll(vp);
                }
                myEngine::Object3dCommon::GetInstance()->PreDraw(myEngine::kNormal);
            });
        }
    }

    const bool useSharedObjectBloom =
        useStagePost || (usePlayerPost && !(world_.presentation.slowMotionPostActive_ && world_.presentation.keepPlayerColorDuringSlow_)) ||
        useEnemyPost || useExpEnemyPost;
    if (useSharedObjectBloom) {
        profile("Shared Glow", true, [&]() {
            const myEngine::Vector3 currentCameraPos = world_.resourceNeonRenderer->GetFloorVisibilityCenter();
            world_.resources.sharedObjectBloomPostEffect_->BeginCapture();
            myEngine::Object3dCommon::GetInstance()->PreDraw(myEngine::kNormal);
            if (useStagePost && !surfaceBlocks) {
                world_.resources.stage_->DrawVisible(currentCameraPos, 38.0f, 24.0f, world_.presentation.showStageNormalBlockBodies_);
            }
            if (usePlayerPost && world_.presentation.playerNeonRenderMode_ == 0 &&
                !(world_.presentation.slowMotionPostActive_ && world_.presentation.keepPlayerColorDuringSlow_)) {
                world_.resources.player_->DrawBodyOnly();
            }
            if (useEnemyPost && !world_.bossPresentation->UseNeonBossVisual() && !world_.gameplayQueries->IsTutorialCombatSuppressed() &&
                world_.presentation.bossNeonRenderMode_ == 0 && world_.combat.combatFlow_.GetState() != GameFlowState::BossDefeatSequence) {
                world_.resources.enemy_->DrawBodyOnly();
            }
            if (useExpEnemyPost && !world_.gameplayQueries->IsTutorialCombatSuppressed()) {
                world_.resources.enemyManager_->DrawBodyOnlyVisible(currentCameraPos, world_.presentation.expEnemyPostVisibleHalfWidth_,
                                                                    world_.presentation.expEnemyPostVisibleHalfHeight_);
            }
            world_.resources.sharedObjectBloomPostEffect_->EndCaptureBloomOnly();
            myEngine::Object3dCommon::GetInstance()->PreDraw(myEngine::kNormal);
        });
    } else {
        world_.performanceMonitor->AddPostProfileEntry("Shared Glow", 0.0f, false);
    }

    if (world_.presentation.showCollisionDebug_) {
        myEngine::Matrix4x4 vp = myEngine::Object3dCommon::GetInstance()->GetIsDebugCamera() ? world_.resources.debugCamera->GetViewProjectionMatrix()
                                                                                   : world_.resources.camera->GetViewProjectionMatrix();
        profile("Collision", true, [&]() {
            world_.resources.collisionDebugRingManager_->DrawAll(vp);
        });
    }

    const bool hasDrawableParticles = myEngine::ParticleManager::GetInstance()->HasDrawableParticles();
    if (world_.presentation.enableParticlePostEffect_ && hasDrawableParticles) {
        profile("Particle Glow", true, [&]() {
            world_.resources.particlePostEffect_->BeginCaptureWithCurrentDepth(surface.enabled);
            myEngine::ParticleManager::GetInstance()->Draw();
            world_.resources.particlePostEffect_->EndCaptureAdditiveOnly();
            myEngine::Object3dCommon::GetInstance()->PreDraw(myEngine::kNormal);
        });
    } else {
        profile("Particles", hasDrawableParticles, [&]() {
            myEngine::ParticleManager::GetInstance()->Draw();
        });
    }
    auto& diagnostics = myEngine::RuntimeProfiler::Get();
    if (diagnostics.IsRecording()) {
        const auto counts = world_.resources.bulletManager_->GetBulletCounts();
        const auto trails = world_.resources.bulletManager_->GetTrailDrawStats();
        diagnostics.SetCounter("Bullets: player", static_cast<double>(counts.player));
        diagnostics.SetCounter("Bullets: enemy", static_cast<double>(counts.enemy + counts.hostileExpEnemy));
        diagnostics.SetCounter("Enemies", static_cast<double>(world_.resources.enemyManager_->GetEnemyCount()));
        diagnostics.SetCounter("Particle count", myEngine::ParticleManager::GetInstance()->GetActiveCount());
        diagnostics.SetCounter("Trails (drawable)", static_cast<double>(trails.drawableInstances));
        diagnostics.SetCounter("Trail vertices", static_cast<double>(trails.generatedVertices));
        diagnostics.SetCounter("Trail draw calls", trails.drawCalls);
        diagnostics.SetCounter("Trail upload bytes", static_cast<double>(trails.uploadedBytes));
        diagnostics.SetCounter("Trail truncated vertices", static_cast<double>(trails.truncatedVertices));
    }
}

void GameplayRenderer::DrawAfterPostEffect3D()
{
    if (world_.gameplayQueries->IsNeonShowcaseActive())
        return;
    if (world_.resources.player_) {
        world_.resources.player_->DrawEvolutionAfterPostEffects();
        if (!world_.run.expeditionRun_)
            world_.resources.player_->DrawUpgradeHudAfterPostEffects();
    }
    world_.gameplayHud->DrawGameTextBloom();
    if (!world_.bossPresentation->UseNeonBossVisual() && world_.combat.combatFlow_.GetState() == GameFlowState::BossDefeatSequence &&
        world_.combat.enableEnemyPostEffect_ && world_.presentation.bossNeonRenderMode_ == 0) {
        myEngine::BloomParam savedBossParam = world_.resources.enemyPostEffect_->GetParam();
        myEngine::BloomParam defeatParam = savedBossParam;
        const float progress =
            world_.combat.combatFlow_.GetBossDefeatDuration() > 0.0f
                ? (std::clamp)(1.0f - world_.combat.combatFlow_.GetTimer() / world_.combat.combatFlow_.GetBossDefeatDuration(), 0.0f, 1.0f)
                : 1.0f;
        defeatParam.dissolveThreshold = progress;
        defeatParam.dissolveEdgeWidth = 0.075f;
        defeatParam.dissolveEdgeColor = {1.0f, 0.22f, 0.08f};
        // Keep the dissolving silhouette readable; the delayed full-screen pulse
        // supplies the later impact flash without washing out the dissolve edge.
        defeatParam.intensity = savedBossParam.intensity * 0.70f;
        world_.resources.enemyPostEffect_->SetParam(defeatParam);
        world_.resources.enemyPostEffect_->BeginCapture();
        myEngine::Object3dCommon::GetInstance()->PreDraw(myEngine::kNormal);
        world_.resources.enemy_->DrawBodyOnly();
        world_.resources.enemyPostEffect_->EndCaptureToBackBuffer();
        myEngine::Object3dCommon::GetInstance()->PreDraw(myEngine::kNormal);
        world_.resources.enemyPostEffect_->SetParam(savedBossParam);
    }

    if (!world_.combat.enablePlayerPostEffect_ || world_.presentation.playerNeonRenderMode_ != 0 ||
        !world_.presentation.slowMotionPostActive_ || !world_.presentation.keepPlayerColorDuringSlow_ ||
        world_.combat.combatFlow_.GetState() != GameFlowState::Playing) {
        return;
    }

    myEngine::BloomParam savedParam = world_.resources.playerPostEffect_->GetParam();
    myEngine::BloomParam slowParam = savedParam;
    if (slowParam.chromAbAmount < world_.presentation.slowPlayerChromAbAmount_) {
        slowParam.chromAbAmount = world_.presentation.slowPlayerChromAbAmount_;
    }
    if (slowParam.distortionAmount < world_.presentation.slowPlayerDistortionAmount_) {
        slowParam.distortionAmount = world_.presentation.slowPlayerDistortionAmount_;
    }
    if (slowParam.glitchAmount < world_.presentation.slowPlayerGlitchAmount_) {
        slowParam.glitchAmount = world_.presentation.slowPlayerGlitchAmount_;
    }
    world_.resources.playerPostEffect_->SetParam(slowParam);

    world_.resources.playerPostEffect_->BeginCapture();
    myEngine::Object3dCommon::GetInstance()->PreDraw(myEngine::kNormal);
    world_.resources.player_->DrawBodyOnly();
    world_.resources.playerPostEffect_->EndCaptureToBackBuffer();
    myEngine::Object3dCommon::GetInstance()->PreDraw(myEngine::kNormal);

    world_.resources.playerPostEffect_->SetParam(savedParam);
}

void GameplayRenderer::DrawShadow()
{
    // 影用の共通設定（PSOの切り替えなど）は Object3dCommon 側で行う

    // 影を落としたいモデルだけを描画
}
} // namespace gameplay
