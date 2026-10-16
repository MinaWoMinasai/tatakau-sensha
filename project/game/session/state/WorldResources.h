#pragma once
#include "game/session/GameplayTypes.h"

namespace gameplay {
/// @brief WorldResourcesに属する資源と実行状態を保持する。
struct WorldResources {
    std::unique_ptr<myEngine::DebugCamera> debugCamera;

    std::unique_ptr<myEngine::Camera> camera;

    // 演出は戦闘の状態を持たない。Enemyのスナップショットは衝突解決の後で使う。
    std::unique_ptr<NeonBossVisual> neonBossVisual_;

    BossVisualBridge bossVisualBridge_;

    bool neonBossVisualEnabled_ = true;

    myEngine::Vector3 neonDepthFloorViewCenter_{44, 31, 0};

    neondepth::Config neonDepthConfig_{}, neonDepthPendingConfig_{};

    std::string neonDepthConfigStatus_;

    std::unique_ptr<NeonDepthEffects> neonDepthEffects_;

    uint64_t neonDepthFrameId_ = 0;

    float neonDepthIntroDelta_ = 0;

    bool neonDepthSkipRequested_ = false;

    DepthSavedCamera neonDepthSavedCamera_;

    bool neonDepthCameraScoped_ = false;

    bool neonDepthSavedDebugCamera_ = false;

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    bool normalRouteReplayActive_ = false;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    bool neonDepthFixtureTriggered_ = false, neonDepthPhaseTwoInjected_ = false, neonDepthForcedHpThisFrame_ = false;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    bool neonDepthResumeSeen_ = false;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    unsigned neonDepthPauseBegin_ = 0, neonDepthPauseEnd_ = 0, neonDepthCycleOneMask_ = 0, neonDepthCycleTwoMask_ = 0;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    uint64_t neonDepthDashInstance_ = 0, neonDepthRealCoreDamage_ = 0;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    int neonDepthPreviousBossHp_ = 0;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    neondepth::Snapshot neonDepthPauseSnapshot_{};
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    nlohmann::json neonDepthOperations_ = nlohmann::json::array();
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    NeonShowcaseCapture gameplayScenarioCapture_;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    gameplaytest::Snapshot gameplayScenarioCaptureSnapshot_;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    std::string gameplayScenarioCaptureName_;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    bool gameplayScenarioInitialized_ = false, gameplayScenarioCapturePending_ = false;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    bool gameplayScenarioEventApplied_ = false, gameplayScenarioRestartRequested_ = false;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    bool gameplayScenarioFinishDeferred_ = false;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    bool gameplayScenarioCaptureRecording_ = false;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    unsigned gameplayScenarioRecordingSequence_ = 0;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    float gameplayScenarioCombatDt_ = 0.0f, gameplayScenarioPresentationDt_ = 0.0f;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    unsigned gameplayScenarioTransitionStep_ = 0;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    std::unique_ptr<NeonSkinnedPreview> neonSkinnedPreview_;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    bool selectNeonSkinnedPreviewTab_ = false;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    bool selectNeonBossTab_ = false;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    NeonShowcaseCapture neonBossCapture_;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    bool neonBossAutoTest_ = false, neonBossDeveloperStartPending_ = false;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    bool neonBossDeveloperFreeze_ = false;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    int neonBossProbe_ = 0, neonBossCaptureStep_ = 0;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    int neonBossProfileStep_ = 0;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    float neonBossValidationAge_ = 0;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    std::string neonBossCaptureName_;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    nlohmann::json neonBossValidationCaptures_ = nlohmann::json::array();
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    std::vector<std::string> neonBossValidationErrors_;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    unsigned neonBossDeadShots_ = 0, neonBossDeadDashCount_ = 0;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    myEngine::Vector3 neonBossDeadPosition_{};
#endif

    std::unique_ptr<myEngine::Object3d> enemyObject_;

    std::unique_ptr<myEngine::Object3d> object3d3;

    std::unique_ptr<myEngine::Object3d> playerObject_;

    std::unique_ptr<myEngine::Object3d> ballObj_;

    std::unique_ptr<myEngine::Object3d> ball_;

    std::unique_ptr<myEngine::Object3d> groundObj_;

    // 入力
    myEngine::Input* input_;

    // ワールドトランスフォーム
    myEngine::Transform worldTransform_;

    // プレイヤー
    std::unique_ptr<Player> player_;

    // 敵
    std::unique_ptr<Enemy> enemy_;

    // 経験値敵
    std::unique_ptr<EnemyManager> enemyManager_;

    // ステージ
    std::unique_ptr<Stage> stage_;

    // 弾マネージャ
    std::unique_ptr<BulletManager> bulletManager_;

    // 衝突マネージャ
    std::unique_ptr<CollisionManager> collisionManager_;

    std::unique_ptr<myEngine::RingManager> collisionDebugRingManager_;

    std::unique_ptr<myEngine::NeonGridRenderer> neonGridRenderer_;

    std::unique_ptr<NeonProjectileRenderer> neonProjectileRenderer_;

    std::unique_ptr<myEngine::TrailManager> playerMeleeTrailManager_;

    std::unique_ptr<myEngine::Skybox> skybox_;

    std::unique_ptr<myEngine::ObjectPostEffect> neonGridPostEffect_;

    std::unique_ptr<myEngine::ObjectPostEffect> bulletTrailPostEffect_;

    std::unique_ptr<myEngine::ObjectPostEffect> particlePostEffect_;

    std::unique_ptr<myEngine::ObjectPostEffect> playerPostEffect_;

    std::unique_ptr<myEngine::ObjectPostEffect> enemyPostEffect_;

    std::unique_ptr<myEngine::ObjectPostEffect> expEnemyPostEffect_;

    std::unique_ptr<myEngine::ObjectPostEffect> sharedObjectBloomPostEffect_;

    std::unique_ptr<myEngine::ObjectPostEffect> stagePostEffect_;

    LevelData currentLevelData_;

    BalanceEditorState balanceEditor_;

    std::vector<LevelVisualObject> levelItems_;

    std::vector<RuntimeBossPhase> levelBossPhases_;
};
} // namespace gameplay
