#pragma once
#include "game/weapon/CombatTypes.h"
#include "DeveloperTools.h"
#define NOMINMAX
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
#include "game/debug/NeonSkinnedPreview.h"
#include "game/debug/GameplayScenarioSession.h"
#endif
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <vector>
#include <unordered_map>
#include <d3d12.h>
#include <wrl.h>
#include "Audio.h"
#include "debugCamera.h"
#include "Dump.h"
#include "Easing.h"
#include "Resource.h"
#include "Sprite.h"
#include "TextLabel.h"
#include "WinApp.h"
#include "Object3d.h"
#include "Model.h"
#include "ModelManager.h"
#include "SrvManager.h"
#include "CollisionManager.h"
#include "MapChip.h"
#include "Fade.h"
#include "Stage.h"
#include "BulletManager.h"
#include "EnemyManager.h"
#include "IScene.h"
#include "ObjectPostEffect.h"
#include "RingManager.h"
#include "NeonGridRenderer.h"
#include "TrailManager.h"
#include "Skybox.h"
#include "game/level/LevelLoader.h"
#include "game/effects/ScreenEffectDirector.h"
#include "game/render/NeonProjectileRenderer.h"
#include "game/enemy/visual/NeonBossVisual.h"
#include "game/enemy/visual/BossVisualBridge.h"
#include "game/enemy/actor/NeonDepthConfig.h"
#include "game/enemy/visual/NeonDepthEffects.h"
#include "game/flow/CombatFlowController.h"
#include "game/ui/NeonTextEffect.h"
#include "game/ui/TankRewardCard.h"
#include "game/run/TankRunDirector.h"
#include "game/run/TankExpeditionDirector.h"
#include "game/run/TankExpeditionAudio.h"
#include "game/run/TankExpeditionTutorial.h"
#include "game/run/TankGuidedCombatTutorial.h"
#include "game/run/TankExpeditionMap.h"
#include "game/run/TankExpeditionTransition.h"
#include "game/run/TankExpeditionRooms.h"
#include "game/run/TankExpeditionContent.h"
#include "game/editor/ExpeditionRoomEditor.h"
#include "game/editor/ExpeditionMapEditor.h"
#include "game/editor/ExpeditionContentEditor.h"

#include "game/session/TitleDemoStatus.h"

namespace gameplay {
using GameFlowState = CombatFlowState;

using PostEffectPulse = IScene::PostEffectPulse;
using ScreenEffectState = IScene::ScreenEffectState;
using DeveloperShowcaseState = IScene::DeveloperShowcaseState;
using RenderProfile = IScene::RenderProfile;

/// @brief 戦闘検証時の開始条件と観測結果を保持する。
struct CombatValidationProbe {
    std::string id;
    float age = 0, pathLength = 0, maxStep = 0, reloadSeconds = 0, sampleAge = 0;
    cg2::Vector3 previousPosition{};
    bool hasPrevious = false, previousReload = false, phase2 = false;
    unsigned shots = 0, dashes = 0, reloads = 0, phases = 0, patterns = 0;
    int previousAmmo = 0, reloadViolations = 0, wallIntersections = 0, tunnelingViolations = 0, playerBulletSamples = 0;
    size_t projectileSamples = 0, maxProjectiles = 0;
    nlohmann::json trace = nlohmann::json::array();
};

/// @brief 遠征の命中火花の位置・色・残り時間を保持する。
struct ExpeditionHitSpark {
    cg2::Vector3 position{}, direction{};
    float age = 0;
};

/// @brief 遠征ルートの1ノードの描画資源と表示状態を保持する。
struct MapNodeVisual {
    cg2::Vector2 center{};
    float focus = 0;
    std::unique_ptr<cg2::Sprite> halo, rim, fill;
    std::unique_ptr<cg2::TextLabel> icon, label, state;
};

/// @brief 遠征ルートの1接続の描画資源と表示状態を保持する。
struct MapEdgeVisual {
    std::string from, to;
    std::unique_ptr<cg2::Sprite> glow, line, pulse;
};

/// @brief 遠征で回収する通貨の位置・移動・回収状態を保持する。
struct ExpeditionCreditOrb {
    cg2::Vector3 position{}, velocity{};
    cg2::Vector2 launch{};
    float age = 0, flight = 0;
    int value = 0;
    bool flying = false;
    std::unique_ptr<cg2::Sprite> sprite;
};

/// @brief 遠征中に配置する回収資源の位置と種類を表す。
struct RunResource {
    cg2::Vector3 position{};
    float respawn = 0;
    bool active = false;
};

/// @brief 遠征の破裂演出の位置と進行状態を保持する。
struct RunBurst {
    cg2::Vector3 position{};
    float age = 0;
    bool resource = false;
};

/// @brief 対象を追従するHPバーの描画資源と表示状態を保持する。
struct FollowHpBar {
    std::unique_ptr<cg2::Sprite> outline;
    std::unique_ptr<cg2::Sprite> background;
    std::unique_ptr<cg2::Sprite> fill;
};

/// @brief レベル配置オブジェクトの描画資源と姿勢を保持する。
struct LevelVisualObject {
    std::string name;
    std::unique_ptr<cg2::Object3d> object;
};

/// @brief 現在のボス段階と発動済み条件を保持する。
struct RuntimeBossPhase {
    LevelBossPhase phase;
    bool activated = false;
};

/// @brief 調整画面の選択・編集中の値・保存状態を保持する。
struct BalanceEditorState {
    bool initialized = false;
    bool defaultRandomSpawnEnabled = true;
    int playerMaxHp = 10000;
    float playerReloadSpeed = 10.0f;
    float playerBulletDamage = 1.0f;
    float playerBulletSpeed = 0.3f;
    float playerMoveSpeed = 0.2f;
    float playerStaminaRecovery = 1.0f;
    float playerMaxStamina = 3.0f;
    int playerBodyDamage = 3;
    float playerHealthRegenUpgrade = 0.08f;
    float maxHpUpgradeAmount = 0.10f;
    float playerBodyDamageUpgrade = 0.10f;
    float playerBulletSpeedUpgrade = 0.08f;
    float playerBulletDamageUpgrade = 0.10f;
    float playerReloadUpgrade = 0.07f;
    float playerMoveSpeedUpgrade = 0.06f;
    float playerMinReloadSpeed = 3.0f;
    bool healToFull = false;
    int damageBlock = 90;
    int bossContact = 45;
    int expEnemyContact = 15;
    int shooterContact = 25;
    int shooterBullet = 18;
    float shooterDetectionRadius = 18.0f;
    float shooterTurnSpeed = 5.5f;
    float shooterFireInterval = 1.25f;
    float shooterBulletSpeed = 0.20f;
    float bossBulletSpeed = 0.4f;
    int bossMaxHp = 1050;
    int bossBulletCount = 2;
    float bossSpreadAngleDeg = 30.0f;
    float bossCooldown = 0.15f;
    int bossBulletDamage = 12;
    float bossBulletHp = 0.0f;
    float bossBulletPenetration = 0.0f;
    bool bossRandomSpread = true;
    int bossAttackPattern = 0;
    bool expEnemyHostileToBoss = false;
    int bossExpEnemyDamage = 12;
    int bossHealOnExpEnemyKill = 30;
    int bossKillsPerLevel = 3;
    int bossMaxHpGainPerLevel = 20;
    int bossDamageGainPerLevel = 2;
    bool bossLevelingModeEnabled = true;
    float bossLevelingEnterDistance = 24.0f;
    float bossLevelingExitDistance = 16.0f;
    float bossLevelingSearchRadius = 80.0f;
    float bossAimTurnHalfSeconds = 1.0f;
    std::string statusMessage;
};

enum class TutorialStep {
    Move,
    Shoot,
    Dash,
    Phase1Complete,
    Upgrade,
    EvolutionUnlocked,
    Evolution,
    TutorialComplete,
};

/// @brief チュートリアルの課題・表示・進行条件を指定する。
struct TutorialConfig {
    bool enabled = true;
    float moveDistance = 3.0f;
    float stepCompleteDelay = 0.35f;
    float phase1CompleteDisplayDuration = 1.0f;
    float evolutionUnlockedDisplayDuration = 1.0f;
    float tutorialCompleteDisplayDuration = 1.5f;
};

/// @brief HPバーの一括描画で使うマテリアルのGPUバッファを保持する。
struct HpBarMaterialBuffer {
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    cg2::Material* data = nullptr;
};

struct PlayerMeleeSlash;

/// @brief 特殊戦闘イベント・表示終点と、フラッシュ開始からの経過秒を保持する。
struct SpecialCombatFlash {
    Player::SpecialCombatEvent event;
    float age = 0;
    cg2::Vector3 end{};
};

/// @brief 射撃強化イベントと、フラッシュ開始からの経過秒を保持する。
struct BuildCombatFlash {
    BulletManager::BuildEvent event;
    float age = 0;
};

/// @brief 特殊投射物の外観と表示状態を保持する。
struct SpecialProjectileVisual {
    cg2::Vector3 position, direction;
    float radius;
    Bullet::SpecialKind kind;
};

/// @brief HPバーを表示するか判断するための距離と条件を保持する。
struct HpBarVisibility {
    int lastHp = -1;
    int lastMaxHp = -1;
    float visibleTimer = 0.0f;
    float alpha = 0.0f;
};

/// @brief ポストエフェクトの1区間の所要時間を記録する。
struct PostProfileEntry {
    const char* name = "";
    float ms = 0.0f;
    bool active = false;
};

#if defined(USE_IMGUI) && !defined(NDEBUG)
/// @brief 性能記録を開始・終了する条件を指定する。
struct PerformanceCaptureConditions {
    std::string label;
    Player::UpgradeHudDebugSnapshot upgradeHud{};
    int postProfileMode = 0;
    std::string postProfileModeName;
    bool gridPostEnabled = false;
    bool stagePostEnabled = false;
    bool bulletTrailPostEnabled = false;
    bool playerPostEnabled = false;
    bool enemyPostEnabled = false;
    bool expEnemyPostEnabled = false;
    bool trailAutoFireEnabled = false;
    bool d3d12DebugLayerEnabled = false;
};
#endif

#if defined(USE_IMGUI) && !defined(NDEBUG)
/// @brief 性能記録の1フレームの計測値とゲーム状態を保持する。
struct PerformanceCaptureFrame {
    uint32_t frameIndex = 0;
    float fps = 0.0f;
    IScene::RenderProfile render{};
    std::array<PostProfileEntry, 16> postEntries{};
    size_t postEntryCount = 0;
    Player::UiProfileStats upgradeHud{};
    Player::UiProfileStats evolutionUi{};
    int playerLevel = 0;
    int skillPoints = 0;
    bool upgradeHudListVisible = false;
    Player::UpgradeHudDebugSnapshot upgradeHudAfterPlayerUpdate{};
    Player::UpgradeHudDebugSnapshot upgradeHudAfterCollision{};
    Player::UpgradeHudDebugSnapshot upgradeHudAtCapture{};
    size_t enemyCount = 0;
    size_t expEnemyCount = 0;
    size_t bulletCount = 0;
    size_t playerBulletCount = 0;
    size_t enemyBulletCount = 0;
    size_t hostileExpEnemyBulletCount = 0;
    size_t bulletTrailCount = 0;
    cg2::TrailManager::DrawStats trailDrawStats{};
    size_t playerLaserCount = 0;
    size_t playerMineCount = 0;
    size_t playerMeleeSlashCount = 0;
    size_t neonTriangleParticleCount = 0;
};
#endif

struct DepthSavedCamera {
    cg2::Vector3 position{}, rotation{};
    cg2::Vector2 jitter{};
    float fovY = 0, aspect = 0, nearClip = 0, farClip = 0;
};

/// @brief 自機のネオン残像の姿勢・色・寿命を保持する。
struct PlayerNeonAfterimage {
    cg2::Vector3 position{};
    cg2::Vector3 direction{0.0f, -1.0f, 0.0f};
    float life = 0.0f;
};

/// @brief 命中適用後のレーザーの端点・幅・色・残り表示秒を保持する。威力は保持しない。
struct PlayerLaserBeam {
    cg2::Vector3 start{};
    cg2::Vector3 end{};
    float width = 0.18f;
    float life = 0.0f;
    float maxLife = 0.12f;
    cg2::Vector4 color{0.25f, 1.0f, 0.95f, 1.0f};
};

/// @brief 自機の地雷の位置・起爆条件・寿命を保持する。
struct PlayerMine {
    cg2::Vector3 position{};
    float radius = 3.2f;
    float fuse = 0.45f;
    float life = 5.0f;
    float maxLife = 5.0f;
    uint32_t damage = 1;
    float rotation = 0.0f;
    cg2::Vector4 color{1.0f, 0.25f, 0.95f, 1.0f};
};

/// @brief 起爆後の爆発表示の位置・半径・色・残り表示秒を保持する。威力は保持しない。
struct PlayerMineExplosion {
    cg2::Vector3 position{};
    float radius = 3.2f;
    float life = 0.28f;
    float maxLife = 0.28f;
    cg2::Vector4 color{1.0f, 0.25f, 0.95f, 1.0f};
};

/// @brief 自機の斬撃の形状・威力・有効時間を保持する。
struct PlayerMeleeSlash {
    cg2::Vector3 origin{};
    cg2::Vector3 followAnchor{};
    cg2::Vector3 direction{1.0f, 0.0f, 0.0f};
    float range = 3.4f;
    float arcDeg = 105.0f;
    float startAngleDeg = -52.5f;
    float endAngleDeg = 52.5f;
    float bladeLengthScale = 1.0f;
    float hiltSideOffset = 0.0f;
    float windupAngleDeg = -100.0f;
    float returnAngleDeg = 58.0f;
    float width = 0.20f;
    float windupDuration = 0.08f;
    float swingDuration = 0.18f;
    float recoveryDuration = 0.10f;
    float elapsed = 0.0f;
    uint32_t damage = 1;
    bool hitApplied = false;
    float knockback = 0;
    bool finisher = false;
    // 同じ斬撃の重複命中照合に使う借用アドレス。所有せず、この履歴から対象のメンバーへアクセスしない。
    std::vector<const Collider*> hitTargets;
    float life = 0.18f;
    float maxLife = 0.18f;
    cg2::Vector4 color{0.55f, 1.25f, 1.0f, 1.0f};
    cg2::TrailInstance* trail = nullptr;
};

/// @brief 近接コンボの段数ごとの斬撃外観を指定する。
struct MeleeComboVisualProfile {
    float startAngleDeg = -70.0f;
    float endAngleDeg = 45.0f;
    float durationScale = 1.0f;
    float bladeLengthScale = 1.0f;
    float bladeWidthScale = 1.0f;
    float hiltSideOffset = 0.0f;
    float windupAngleDeg = -100.0f;
    float returnAngleDeg = 58.0f;
    cg2::Vector4 colorScale{1.0f, 1.0f, 1.0f, 1.0f};
};

/// @brief ネオン三角形演出の位置・速度・色・寿命を保持する。
struct NeonTriangleParticle {
    cg2::Vector3 position{};
    cg2::Vector3 velocity{};
    cg2::Vector3 initialVelocity{};
    float radius = 0.35f;
    float rotation = 0.0f;
    float angularVelocity = 0.0f;
    float lineWidth = 0.055f;
    float life = 0.0f;
    float maxLife = 0.35f;
    float tiltRad = 0.0f;
    int trailCopies = 0;
    bool isBillboard = false;
    cg2::NeonParticleShape shape = cg2::NeonParticleShape::Triangle;
    float endRadius = -1.0f;
    cg2::Vector4 color{1.0f, 0.4f, 1.0f, 1.0f};
};
} // namespace gameplay
