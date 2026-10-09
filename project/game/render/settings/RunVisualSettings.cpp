#include "game/render/settings/RunVisualSettings.h"
#include "game/session/GameplaySystems.h"
#include <initializer_list>
#include <fstream>

namespace gameplay {

void RunVisualSettings::InitializeTankRunVisuals()
{
    if (!world_.run.prototypeRun_)
        return;

    // Keep the floor quiet while restoring the authored emitters. This engine
    // applies intensity in both blur passes and the composite, so values below
    // one attenuate the halo cubically rather than providing a slight dimmer.
    world_.presentation.worldGridLineWidth_ = 0.035f;
    world_.presentation.worldGridColor_ = {0.035f, 0.09f, 0.18f, 0.22f};
    world_.presentation.stageBlockNeonLineWidth_ = 0.085f;
    world_.presentation.stageBlockNeonColor_ = {0.10f, 0.64f, 0.92f, 0.90f};
    world_.presentation.stageDamageBlockNeonColor_ = {1.10f, 0.08f, 0.14f, 1.0f};
    world_.presentation.actorNeonBillboardLineWidth_ = 0.12f;
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
    restoreEmitterBloom(world_.resources.bulletTrailPostEffect_.get(), 2.0f);
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
    world_.presentation.stagePostCacheValid_ = false;
}

} // namespace gameplay
