#include "game/render/settings/RunVisualSettings.h"
#include "game/session/GameplaySystems.h"
#include <initializer_list>
#include <fstream>
#include <cstdlib>

namespace gameplay {

void RunVisualSettings::InitializeTankRunVisuals()
{
    if (!world_.run.prototypeRun_)
        return;
    world_.presentation.stageSurface_.enabled = true;
    // Stage presentation is independent of the older expeditionAuthored marker.
    // A saved legacy switch must survive this run's defaults as well as restart.
    try {
        std::ifstream file("resources/configs/gameVisuals.json");
        nlohmann::json visual;
        file >> visual;
        if (visual.is_object() && visual.contains("stageSurface")) {
            world_.gameplaySettings->ApplyGameVisualConfig(nlohmann::json{{"stageSurface", visual["stageSurface"]}});
        }
    } catch (...) { /* Existing config load reports parse errors; defaults remain usable. */ }


    // Vector-monitor arena: a black stage drawn only with thin emissive lines.
    // The floor grid is a dim blue lattice, walls are bright cyan wireframes,
    // and each actor tints the floor lattice around it with its own color.
    world_.presentation.worldGridLineWidth_ = 0.040f;
    world_.presentation.worldGridColor_ = {0.16f, 0.24f, 0.95f, 0.55f};
    world_.presentation.showNeonGrid_ = true;
    world_.presentation.showActorLocalGrid_ = true;
    world_.presentation.actorGridSpacing_ = world_.presentation.worldGridSpacing_;
    world_.presentation.actorGridRadius_ = 9.0f;
    world_.presentation.actorGridLineWidth_ = 0.055f;
    world_.presentation.playerGridColor_ = {0.45f, 1.25f, 0.30f, 0.85f};
    world_.presentation.enemyGridColor_ = {1.30f, 0.16f, 0.24f, 0.80f};
    world_.presentation.expEnemyGridColor_ = {1.20f, 0.26f, 0.62f, 0.55f};
    world_.presentation.maxExpEnemyLocalGrids_ = 8;
    world_.presentation.stageBlockNeonLineWidth_ = 0.085f;
    world_.presentation.stageBlockNeonColor_ = {0.24f, 0.62f, 1.70f, 1.0f};
    world_.presentation.stageDamageBlockNeonColor_ = {1.45f, 0.10f, 0.20f, 1.0f};
    world_.presentation.actorNeonBillboardLineWidth_ = 0.15f;
    world_.presentation.actorNeonBodyFillColor_ = {0.006f, 0.010f, 0.016f, 0.92f};
    world_.presentation.playerNeonRenderMode_ = 1;
    world_.presentation.bossNeonRenderMode_ = 1;
    world_.presentation.expEnemyNeonRenderMode_ = 1;
    world_.presentation.fillActorNeonBodies_ = true;
    world_.presentation.showStageBlockNeonOutlines_ = true;
    world_.presentation.showStageDamageBlockNeonOutlines_ = true;
    world_.presentation.neonLineSoftEdgeRatio_ = 0.42f;
    world_.presentation.neonLineCoreIntensity_ = 1.35f;

    world_.presentation.enableNeonGridPostEffect_ = true;
    world_.presentation.enableBulletTrailPostEffect_ = true;
    world_.presentation.enableParticlePostEffect_ = true;
    world_.presentation.postProfileMode_ = 0;

    // These are generated postprocess contours, not the intentional frame
    // lines of the tanks, resource shapes, and arena geometry.
    const auto removeGeneratedOutline = [](cg2::ObjectPostEffect* effect) {
        if (!effect)
            return;
        cg2::BloomParam param = effect->GetParam();
        param.outlineWidth = 0.0f;
        param.outlineBloomIntensity = 0.0f;
        param.outlineBloomWidth = 0.0f;
        param.outlineColor = {0.0f, 0.0f, 0.0f};
        param.depthOutlineEnabled = 0.0f;
        param.depthOutlineScale = 0.0f;
        effect->SetParam(param);
    };
    for (cg2::ObjectPostEffect* effect :
         {world_.resources.playerPostEffect_.get(), world_.resources.enemyPostEffect_.get(), world_.resources.expEnemyPostEffect_.get(),
          world_.resources.stagePostEffect_.get(), world_.resources.neonGridPostEffect_.get(),
          world_.resources.bulletTrailPostEffect_.get(), world_.resources.particlePostEffect_.get(),
          world_.resources.sharedObjectBloomPostEffect_.get()}) {
        removeGeneratedOutline(effect);
    }
    const auto restoreEmitterBloom = [](cg2::ObjectPostEffect* effect, float intensity) {
        if (!effect)
            return;
        cg2::BloomParam param = effect->GetParam();
        param.threshold = 0.0f;
        param.intensity = intensity;
        param.gaussianIntensity = 0.0f;
        param.fullScreenBoxBlurBlend = 0.0f;
        effect->SetParam(param);
    };
    // Phosphor persistence already leaves a trail; keep shots as crisp dashes with a tighter halo.
    restoreEmitterBloom(world_.resources.bulletTrailPostEffect_.get(), 1.4f);
    restoreEmitterBloom(world_.resources.particlePostEffect_.get(), 1.65f);
    restoreEmitterBloom(world_.resources.neonGridPostEffect_.get(), 1.0f);
    if (world_.run.expeditionRun_ && world_.resources.bulletManager_) {
        // Keep emitter bloom and enemy shots bright; only shorten/narrow the
        // player's overlapping trails so telegraphs remain visible in a volley.
        auto trail = world_.resources.bulletManager_->GetTrailSettings();
        trail.playerHalfWidth = (std::min)(trail.playerHalfWidth, 0.17f);
        trail.playerTrailLifetimeScale = 0.55f;
        trail.playerTrailAlphaScale = 0.68f;
        world_.resources.bulletManager_->SetTrailSettings(trail);
    }

    world_.combat.gameTextOutlineEnabled_ = false;
    world_.combat.gameTextNeonEnabled_ = true;
    world_.gameplayHud->ApplyGameTextAppearance();
    // Run defaults above remain the migration baseline. Explicit F3 saves win
    // after those defaults, so authoring survives restart without changing the
    // appearance of older installations that have never saved this editor.
    if (world_.run.expeditionRun_) {
        auto authored = [](const char* path) {
            try {
                std::ifstream file(path);
                nlohmann::json j;
                file >> j;
                if (j.is_object() && j.value("expeditionAuthored", false))
                    return j;
            }
            catch (...) {
            }
            return nlohmann::json::object();
        };
        world_.gameplaySettings->ApplyGamePostEffectConfig(authored("resources/configs/gamePostEffects.json"));
        const auto visual = authored("resources/configs/gameVisuals.json");
        if (!visual.empty())
            world_.gameplaySettings->ApplyGameVisualConfig(visual);
    }
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    // Reproducible A/B fixtures; never persisted and never compiled into Release.
    char testMode[16]{};
    if (GetEnvironmentVariableA("CG2_STAGE_SURFACE_TEST_MODE", testMode, sizeof(testMode)) > 0) {
        const std::string mode(testMode);
        if (mode == "phase1") world_.presentation.stageSurface_ = stagesurface::Phase1Settings();
        auto& surface = world_.presentation.stageSurface_;
        if (mode == "legacy" || mode == "floor" || mode == "blocks" || mode == "full") {
            surface.enabled = mode != "legacy";
            surface.floorEnabled = mode == "floor" || mode == "full";
            surface.blocksEnabled = mode == "blocks" || mode == "full";
        }
    }
    auto overrideInt = [](const char* name, int& target, int high) {
        char value[16]{};
        const auto length = GetEnvironmentVariableA(name, value, sizeof(value));
        if (!length || length >= sizeof(value)) return;
        char* end = nullptr;
        const long parsed = std::strtol(value, &end, 10);
        if (end != value && *end == '\0' && parsed >= -1 && parsed <= high) target = static_cast<int>(parsed);
    };
    overrideInt("CG2_STAGE_SURFACE_TEST_DEBUG", world_.presentation.developerStageSurfaceDebugView_, 13);
    overrideInt("CG2_STAGE_SURFACE_TEST_SSAO", world_.presentation.developerStageSurfaceSSAO_, 1);
    overrideInt("CG2_STAGE_SURFACE_TEST_SSR", world_.presentation.developerStageSurfaceSSR_, 1);
    overrideInt("CG2_STAGE_SURFACE_TEST_BLOOM", world_.presentation.developerBloomComparisonMode_, 3);
    int tilt = 0;
    overrideInt("CG2_STAGE_SURFACE_TEST_TILT", tilt, 1);
    world_.presentation.developerStageTilt_ = tilt == 1;
    int localLight = -1;
    overrideInt("CG2_STAGE_SURFACE_TEST_LIGHT", localLight, 1);
    if (localLight >= 0) world_.presentation.stageSurface_.floorLocalLight = localLight == 1;
#endif
    world_.presentation.stagePostCacheValid_ = false;
}

} // namespace gameplay
