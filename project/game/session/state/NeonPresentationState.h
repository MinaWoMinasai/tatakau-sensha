#pragma once
#include "game/session/GameplayTypes.h"

namespace gameplay {
/// @brief NeonPresentationStateに属する資源と実行状態を保持する。
struct NeonPresentationState {
    bool enableExpEnemyPostEffect_ = true;

    bool enableStagePostEffect_ = false;

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    int developerBloomComparisonMode_ = -1;
#endif

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    int developerToneMappingMode_ = -1;
#endif

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    bool developerBloomFreeze_ = false;
#endif

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    NeonShowcaseCapture developerGameCapture_;
#endif

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    cg2::BloomParam developerCompositeParams_{};
#endif

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    bool developerCompositeParamsAvailable_ = false;
#endif

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    unsigned developerGameCaptureNumber_ = 0;
#endif

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    std::string developerGameCaptureDirectory_;
#endif

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    char developerGameCaptureLabel_[64] = "game_bloom";
#endif

#ifdef USE_IMGUI
    bool showPostProfileOverlay_ = true;
#endif

#if !(defined(USE_IMGUI))
    bool showPostProfileOverlay_ = false;
#endif

    int postProfileMode_ = 0;

    std::array<PostProfileEntry, 16> postProfileEntries_;

    size_t postProfileEntryCount_ = 0;

    std::array<float, 16> postProfileAccumulatedMs_{};

    std::array<float, 16> postProfileAverageMs_{};

    int postProfileAccumulatedFrames_ = 0;

    IScene::RenderProfile renderProfile_{};

#if defined(USE_IMGUI) && !defined(NDEBUG)
    int performanceCaptureFrameCount_ = 30;
#endif

#if defined(USE_IMGUI) && !defined(NDEBUG)
    std::array<char, 128> performanceCaptureLabel_{};
#endif

#if defined(USE_IMGUI) && !defined(NDEBUG)
    bool performanceCaptureActive_ = false;
#endif

#if defined(USE_IMGUI) && !defined(NDEBUG)
    bool performanceCaptureSkipCurrentFrame_ = false;
#endif

#if defined(USE_IMGUI) && !defined(NDEBUG)
    PerformanceCaptureConditions performanceCaptureConditions_{};
#endif

#if defined(USE_IMGUI) && !defined(NDEBUG)
    std::vector<PerformanceCaptureFrame> performanceCaptureFrames_;
#endif

#if defined(USE_IMGUI) && !defined(NDEBUG)
    std::string performanceCaptureLastCsvPath_;
#endif

#if defined(USE_IMGUI) && !defined(NDEBUG)
    std::string performanceCaptureStatus_;
#endif

#if defined(USE_IMGUI) && !defined(NDEBUG)
    Player::UpgradeHudDebugSnapshot upgradeHudAfterPlayerUpdate_{};
#endif

#if defined(USE_IMGUI) && !defined(NDEBUG)
    Player::UpgradeHudDebugSnapshot upgradeHudAfterCollision_{};
#endif

    bool stagePostCacheValid_ = false;

    cg2::Vector3 stagePostCacheCameraPos_{};

    float stagePostCacheRefreshPixels_ = 48.0f;

    float expEnemyPostVisibleHalfWidth_ = 20.0f;

    float expEnemyPostVisibleHalfHeight_ = 10.0f;

    bool slowMotionPostActive_ = false;

    bool keepPlayerColorDuringSlow_ = true;

    float slowPlayerChromAbAmount_ = 0.035f;

    float slowPlayerDistortionAmount_ = 0.018f;

    float slowPlayerGlitchAmount_ = 0.015f;

    float sceneFadeBlurTimer_ = 0.0f;

    float sceneFadeBlurDuration_ = 2.0f;

    float sceneFadeBlurIntensity_ = 0.0f;

    bool playerDeathShakeStarted_ = false;

    float cameraShakeTimer_ = 0.0f;

    float cameraShakeDuration_ = 0.65f;

    float cameraShakePower_ = 0.0f;

    bool showCollisionDebug_ = false;

    bool showCollisionDebugBullets_ = true;

    bool showBulletStatusDebugOverlay_ = false;

    bool showBulletStatusDebugTable_ = true;

    int bulletStatusDebugMaxLabels_ = 40;

    bool debugPlayerNoDamage_ = false;

    bool showGameDebugConsole_ = true;

    bool showParticleEditor_ = false;

    bool showPlayerClassEditor_ = false;

    bool showNeonGrid_ = true;

    bool showActorLocalGrid_ = true;

    bool showLevelAIDitorPreview_ = false;

    bool enableNeonGridPostEffect_ = true;

    bool enableBulletTrailPostEffect_ = true;

    bool enableParticlePostEffect_ = true;

    bool enableDeathPostPulse_ = true;

    PostEffectPulse deathPostPulse_{};

    float deathPostPulseTimer_ = 0.0f;

    float deathPostPulseDuration_ = 0.62f;

    float deathPostPulseScale_ = 1.0f;

    float deathPostBloomBoost_ = 0.65f;

    float deathPostChromAbAmount_ = 0.006f;

    float deathPostShockwaveStrength_ = 0.012f;

    float deathPostShockwaveWidth_ = 0.055f;

    float deathPostShockwaveMaxRadius_ = 0.72f;

    std::string postEffectConfigStatus_;

    std::string visualConfigStatus_;

    float worldGridSpacing_ = 2.0f;

    float worldGridLineWidth_ = 0.075f;

    cg2::Vector4 worldGridColor_ = {0.12f, 0.42f, 1.0f, 0.15f};

    float actorGridRadius_ = 5.4f;

    float actorGridSpacing_ = 1.0f;

    float actorGridLineWidth_ = 0.1f;

    float neonLineSoftEdgeRatio_ = 0.42f;

    float neonLineCoreIntensity_ = 1.35f;

    cg2::Vector4 playerGridColor_ = {0.50f, 1.0f, 0.35f, 1.0f};

    cg2::Vector4 enemyGridColor_ = {1.0f, 0.18f, 0.24f, 1.0f};

    cg2::Vector4 expEnemyGridColor_ = {1.0f, 0.32f, 0.58f, 1.0f};

    bool showStageBlockNeonOutlines_ = true;

    bool showStageNormalBlockBodies_ = true;

    float stageBlockNeonLineWidth_ = 0.10f;

    float stageBlockNeonDepthBias_ = 0.035f;

    cg2::Vector4 stageBlockNeonColor_ = {0.55f, 1.0f, 0.32f, 1.0f};

    bool showStageDamageBlockNeonOutlines_ = true;

    cg2::Vector4 stageDamageBlockNeonColor_ = {1.20f, 0.035f, 0.02f, 1.0f};

    float stageDamageBlockPulseSpeed_ = 5.0f;

    float stageDamageBlockPulseMin_ = 0.45f;

    float stageDamageBlockPulseMax_ = 1.35f;

    float stageDamageBlockPulseTime_ = 0.0f;

    bool cullActorLocalGrid_ = true;

    int maxExpEnemyLocalGrids_ = 18;

    bool showNeonTriangleDemo_ = true;

    int expEnemyNeonRenderMode_ = 0;

    float expEnemyNeonSquareSize_ = 1.8f;

    float expEnemyNeonTriangleRadius_ = 1.05f;

    float expEnemyNeonPentagonRadius_ = 1.05f;

    float expEnemyNeonShooterRadius_ = 0.95f;

    float expEnemyNeonLineWidth_ = 0.12f;

    int playerNeonRenderMode_ = 0;

    int bossNeonRenderMode_ = 0;

    float playerNeonBillboardRadius_ = 1.05f;

    float bossNeonBillboardRadius_ = 1.35f;

    float actorNeonBillboardLineWidth_ = 0.12f;

    float playerNeonEmission_ = 1.0f, bossNeonEmission_ = 1.0f;

    float bossNeonBarrelForwardOffset_ = 0.92f;

    float bossNeonBarrelSideOffset_ = 0.0f;

    float bossNeonBarrelLengthScale_ = 1.15f;

    float bossNeonBarrelWidthScale_ = 0.24f;

    float bossNeonBarrelAngleDeg_ = 0.0f;

    bool fillActorNeonBodies_ = true;

    cg2::Vector4 actorNeonBodyFillColor_ = {0.006f, 0.010f, 0.016f, 0.92f};

    float playerDashCurrentAlpha_ = 0.58f;

    float playerAfterimageAlpha_ = 0.42f;

    float playerAfterimageInterval_ = 0.045f;

    float playerAfterimageLifetime_ = 0.30f;

    float playerAfterimageSpawnTimer_ = 0.0f;

    bool showPlayerIdleMeleeSaber_ = true;

    bool enablePlayerMeleeRibbonTrail_ = false;

    float playerIdleSaberSideOffset_ = 0.72f;

    float playerIdleSaberForwardOffset_ = 0.18f;

    float playerIdleSaberLength_ = 1.18f;

    float playerIdleSaberAngleDeg_ = 58.0f;

    float playerIdleSaberHiltLength_ = 0.24f;

    float playerIdleSaberBladeWidth_ = 0.08f;

    float playerIdleSaberOuterWidthScale_ = 2.65f;

    float playerIdleSaberCoreWidthScale_ = 0.30f;

    float playerMeleeBladeOuterWidthScale_ = 2.35f;

    float playerMeleeBladeHaloWidthScale_ = 1.05f;

    float playerMeleeBladeCoreWidthScale_ = 0.26f;

    float playerMeleeTrailWidthScale_ = 1.0f;

    float playerMeleeTrailAlphaScale_ = 1.0f;

    float playerMeleeAfterimageAlphaScale_ = 1.0f;

    std::vector<PlayerNeonAfterimage> playerNeonAfterimages_;

    std::vector<PlayerLaserBeam> playerLaserBeams_;

    std::vector<PlayerMine> playerMines_;

    std::vector<PlayerMineExplosion> playerMineExplosions_;

    std::vector<PlayerMeleeSlash> playerMeleeSlashes_;

    std::vector<MeleeComboVisualProfile> playerMeleeComboVisuals_ = {
        {-75.0f, 45.0f, 0.92f, 0.96f, 0.90f, -0.04f, -105.0f, 58.0f, {0.90f, 1.05f, 1.10f, 1.0f}},
        {70.0f, -55.0f, 1.00f, 1.02f, 1.00f, 0.05f, 110.0f, 58.0f, {1.08f, 0.94f, 1.08f, 1.0f}},
        {-145.0f, 135.0f, 1.28f, 1.16f, 1.30f, 0.00f, -178.0f, 58.0f, {1.12f, 1.02f, 0.82f, 1.0f}}};

    std::vector<NeonTriangleParticle> neonTriangleParticles_;

    float neonParticleTriangleGlowWidthScale_ = 3.2f;

    float neonParticleTriangleCoreWidthScale_ = 0.28f;

    float neonParticleTriangleBrightness_ = 1.45f;

    float neonParticleTriangleTrailSpacing_ = 0.48f;

    float neonParticleTriangleBirthScale_ = 0.52f;

    int neonParticleTriangleTrailCopies_ = 3;

    int neonTriangleEffectMode_ = 0;

    cg2::Vector3 neonTriangleDemoCenter_ = {30.0f, 30.0f, 1.2f};

    float neonTriangleDemoRadius_ = 2.2f;
};
} // namespace gameplay
