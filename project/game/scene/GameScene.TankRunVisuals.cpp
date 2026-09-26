#include "GameScene.h"
#include <initializer_list>
#include <fstream>

void GameScene::InitializeTankRunVisuals() {
    if (!prototypeRun_) return;

    // Keep the floor quiet while restoring the authored emitters. This engine
    // applies intensity in both blur passes and the composite, so values below
    // one attenuate the halo cubically rather than providing a slight dimmer.
    worldGridLineWidth_ = 0.035f;
    worldGridColor_ = {0.035f, 0.09f, 0.18f, 0.22f};
    stageBlockNeonLineWidth_ = 0.085f;
    stageBlockNeonColor_ = {0.10f, 0.64f, 0.92f, 0.90f};
    stageDamageBlockNeonColor_ = {1.10f, 0.08f, 0.14f, 1.0f};
    actorNeonBillboardLineWidth_ = 0.12f;
    actorNeonBodyFillColor_ = {0.006f, 0.010f, 0.016f, 0.92f};
    playerNeonRenderMode_ = 1;
    bossNeonRenderMode_ = 1;
    expEnemyNeonRenderMode_ = 1;
    fillActorNeonBodies_ = true;
    showStageBlockNeonOutlines_ = true;
    showStageDamageBlockNeonOutlines_ = true;
    neonLineSoftEdgeRatio_ = 0.42f;
    neonLineCoreIntensity_ = 1.35f;

    enableNeonGridPostEffect_ = true;
    enableBulletTrailPostEffect_ = true;
    enableParticlePostEffect_ = true;
    postProfileMode_ = 0;

    // These are generated postprocess contours, not the intentional frame
    // lines of the tanks, resource shapes, and arena geometry.
    const auto removeGeneratedOutline = [](ObjectPostEffect* effect) {
        if (!effect) return;
        BloomParam param = effect->GetParam();
        param.outlineWidth = 0.0f;
        param.outlineBloomIntensity = 0.0f;
        param.outlineBloomWidth = 0.0f;
        param.outlineColor = {0.0f, 0.0f, 0.0f};
        param.depthOutlineEnabled = 0.0f;
        param.depthOutlineScale = 0.0f;
        effect->SetParam(param);
    };
    for (ObjectPostEffect* effect : {playerPostEffect_.get(), enemyPostEffect_.get(),
        expEnemyPostEffect_.get(), stagePostEffect_.get(), neonGridPostEffect_.get(),
        bulletTrailPostEffect_.get(), particlePostEffect_.get(), sharedObjectBloomPostEffect_.get()}) {
        removeGeneratedOutline(effect);
    }
    const auto restoreEmitterBloom = [](ObjectPostEffect* effect, float intensity) {
        if (!effect) return;
        BloomParam param = effect->GetParam();
        param.threshold = 0.0f;
        param.intensity = intensity;
        param.gaussianIntensity = 0.0f;
        param.fullScreenBoxBlurBlend = 0.0f;
        effect->SetParam(param);
    };
    restoreEmitterBloom(bulletTrailPostEffect_.get(), 2.0f);
    restoreEmitterBloom(particlePostEffect_.get(), 1.65f);
    restoreEmitterBloom(neonGridPostEffect_.get(), 1.0f);
    if(expeditionRun_&&bulletManager_) {
        // Keep emitter bloom and enemy shots bright; only shorten/narrow the
        // player's overlapping trails so telegraphs remain visible in a volley.
        auto& trail=bulletManager_->GetTrailSettings();
        trail.playerHalfWidth=(std::min)(trail.playerHalfWidth,0.17f);
        trail.playerTrailLifetimeScale=0.55f;
        trail.playerTrailAlphaScale=0.68f;
    }

    gameTextOutlineEnabled_ = false;
    gameTextNeonEnabled_ = true;
    ApplyGameTextAppearance();
    // Run defaults above remain the migration baseline. Explicit F3 saves win
    // after those defaults, so authoring survives restart without changing the
    // appearance of older installations that have never saved this editor.
    if(expeditionRun_) {
        auto authored=[](const char* path) {
            try {std::ifstream file(path);nlohmann::json j;file>>j;
                if(j.is_object()&&j.value("expeditionAuthored",false))return j;
            }catch(...) {}
            return nlohmann::json::object();
        };
        ApplyGamePostEffectConfig(authored("resources/configs/gamePostEffects.json"));
        const auto visual=authored("resources/configs/gameVisuals.json");
        if(!visual.empty())ApplyGameVisualConfig(visual);
    }
    stagePostCacheValid_ = false;
}
