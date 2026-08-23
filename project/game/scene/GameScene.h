#pragma once
#define NOMINMAX
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
#include "game/ui/NeonTextEffect.h"

// ゲームシーン
class GameScene : public IScene {

public:

	/// <summary>
	/// コンストラクタ
	/// </summary>
	GameScene();

	/// <summary>
	/// デストラクタ
	/// </summary>
	~GameScene();

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize() override;

	/// <summary>
	/// 更新
	/// </summary>
	void Update() override;

	/// <summary>
	/// 描画
	/// </summary>
	void Draw() override;

	void DrawShadow() override;

	void DrawPostEffect3D() override;
	void DrawAfterPostEffect3D() override;

	/// <summary>
	/// 描画
	/// </summary>
	void DrawSprite() override;

	bool IsFinished() const override { return finished_; }

	Object3d* GetBallObj() { return ballObj_.get(); }

	float GetFinalDeltaTime() const override { return finalDeltaTime; }
	float GetPostGaussianIntensity() const override { return sceneFadeBlurIntensity_; }
	PostEffectPulse GetPostEffectPulse() const override { return deathPostPulse_; }
	ScreenEffectState GetScreenEffectState() const override;
	void SetRenderProfile(const IScene::RenderProfile& profile) override;
	
	std::string GetNextSceneName() const override;

private:
	struct FollowHpBar {
		std::unique_ptr<Sprite> outline;
		std::unique_ptr<Sprite> background;
		std::unique_ptr<Sprite> fill;
	};
	struct LevelVisualObject {
		std::string name;
		std::unique_ptr<Object3d> object;
	};
	struct RuntimeBossPhase {
		LevelBossPhase phase;
		bool activated = false;
	};
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
	struct TutorialConfig {
		bool enabled = true;
		float moveDistance = 3.0f;
		float stepCompleteDelay = 0.35f;
		float phase1CompleteDisplayDuration = 1.0f;
		float evolutionUnlockedDisplayDuration = 1.0f;
		float tutorialCompleteDisplayDuration = 1.5f;
	};

	void InitializeFollowHpBars(size_t count);
	void InitializeFollowHpBarBatch();
	void DrawFollowHpBar(const void* ownerKey, const Vector3& worldPos, int hp, int maxHp, float width, float yOffset);
	void DrawFollowStaminaBar(const Vector3& worldPos, float stamina, float maxStamina, float width, float yOffset);
	void QueueHpBarQuad(std::vector<VertexData>& vertices, const Vector2& center, const Vector2& size);
	struct HpBarMaterialBuffer {
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		Material* data = nullptr;
	};

	void DrawHpBarBatch(uint32_t startVertex, uint32_t vertexCount, const std::string& textureFilePath, const HpBarMaterialBuffer& material);
	void DrawHpBarBatches();
	Vector2 WorldToScreen(const Vector3& worldPos) const;
	void DrawNeonGridPass(bool includeStageBlockOutlines = true);
	void DrawStageBlockNeonPass();
	void QueueStageBlockNeonOutlines();
	void DrawExpEnemyNeonFillModels();
	void DrawExpEnemyNeonDepthLines();
	void TriggerDeathPostPulse(const Vector3& worldPosition, float strength);
	void UpdateDeathPostPulse(float deltaTime);
	void QueueExpEnemyNeonShapes(const Vector3& cameraRight, const Vector3& cameraUp, const Vector3& cameraForward);
	void QueueActorNeonBillboards(const Vector3& cameraRight, const Vector3& cameraUp, bool drawBodies = true, bool drawBarrels = true);
	void DrawActorNeonBodyFillPass();
	void UpdatePlayerNeonAfterimages(float deltaTime);
	struct PlayerMeleeSlash;
	void SpawnPlayerLaser(const Player::LaserShotEvent& event);
	void UpdatePlayerLasers(float deltaTime);
	void QueuePlayerLasers(const Vector3& cameraForward);
	void SpawnPlayerMine(const Player::MineDropEvent& event);
	void UpdatePlayerMines(float deltaTime);
	void DetonatePlayerMine(size_t index);
	void QueuePlayerMines(const Vector3& cameraRight, const Vector3& cameraUp, const Vector3& cameraForward);
	void SpawnPlayerMeleeSlash(const Player::MeleeSlashEvent& event);
	void UpdatePlayerMeleeSlashes(float deltaTime);
	void QueuePlayerMeleeSlashes();
	TrailConfig MakePlayerMeleeTrailConfig(const PlayerMeleeSlash& slash, float alphaScale = 1.0f) const;
	void ComputePlayerMeleeBladeSection(const PlayerMeleeSlash& slash, float progress, Vector3& base, Vector3& tip) const;
	void UpdateNeonTriangleParticles(float deltaTime);
	void QueueNeonTriangleParticles(const Vector3& cameraRight, const Vector3& cameraUp, const Vector3& cameraForward);
	bool IsNearCamera2D(const Vector3& worldPos, float halfWidth, float halfHeight, float margin = 0.0f) const;
	void ResetPostProfileEntries();
	void AddPostProfileEntry(const char* name, float ms, bool active);
	void UpdatePostProfileText();
	void DrawPerformanceBreakdownImGui();
#if defined(USE_IMGUI) && !defined(NDEBUG)
	void DrawPerformanceCaptureImGui();
	void StartPerformanceCapture();
	void CapturePerformanceFrame();
	bool WritePerformanceCaptureFiles();
#endif
	bool IsPostProfileCategoryEnabled(const char* category) const;
	const char* GetPostProfileModeName() const;
	Vector2 GetStagePostCacheUvOffset(const Vector3& currentCameraPos) const;
	bool LoadLevelFile(LevelData& outLevel) const;
	void ReloadLevelData(bool resetSpawnPositions);
	void ClearAppliedLevelData();
	void ApplyLevelData(const LevelData& levelData);
	void ApplyLevelBalance(const nlohmann::json& balanceJson);
	void DrawGameSceneDebugImGui();
	void DrawPostEffectParamControls(const char* labelPrefix, BloomParam& param);
	bool LoadGamePostEffectConfig(const std::string& filePath = "resources/configs/gamePostEffects.json");
	bool SaveGamePostEffectConfig(const std::string& filePath = "resources/configs/gamePostEffects.json") const;
	nlohmann::json BuildGamePostEffectConfig() const;
	void ApplyGamePostEffectConfig(const nlohmann::json& configJson);
	bool LoadGameVisualConfig(const std::string& filePath = "resources/configs/gameVisuals.json");
	bool SaveGameVisualConfig(const std::string& filePath = "resources/configs/gameVisuals.json") const;
	nlohmann::json BuildGameVisualConfig() const;
	void ApplyGameVisualConfig(const nlohmann::json& configJson);
	void ApplyGameTextAppearance();
	void DrawGameTextBloom();
	void DrawLevelAIDitorBalanceLab(bool embedded = false);
	void LoadBalanceEditorFromJson(const nlohmann::json& balanceJson);
	nlohmann::json BuildBalanceJsonFromEditor() const;
	bool SaveBalanceEditorToLevelFile(const std::string& filePath);
	bool WriteBalanceAIHandoff(const std::string& filePath) const;
	void ApplyLevelObject(const LevelObject& levelObject, bool allowBossSpawn);
	void AddLevelSpawnArea(const LevelSpawnArea& spawnArea);
	void AddLevelSpawnAreaFromObject(const LevelObject& levelObject);
	void UpdateLevelBossPhases();
	void ApplyBossPhaseTuning(const LevelBossPhase& phase);
	void ApplyLevelEffectPreset(const nlohmann::json& effectJson);
	void QueueLevelEditorPreview();
	void QueueLevelObjectPreview(const LevelObject& levelObject, const Vector4& color);
	void QueueLevelSpawnAreaPreview(const LevelSpawnArea& spawnArea, const Vector4& color);
	bool AddLevelItem(const LevelObject& levelObject);
	void UpdateLevelItems();
	void DrawLevelItems();
	void DrawBulletStatusDebugOverlay();
	void DrawBulletStatusDebugTable();
	void InitializeSubmissionUi();
	bool LoadTutorialConfig(const std::string& filePath = "resources/configs/tutorial.json");
	void InitializeTutorialUi();
	void UpdateTutorial(float deltaTime);
	void EnterTutorialStep(TutorialStep step);
	void CompleteTutorialStep();
	void GrantTutorialUpgradeReward();
	void GrantTutorialEvolutionReward();
	void UpdateTutorialText();
	void DrawTutorialUi();
	bool IsTutorialCombatSuppressed() const { return tutorialConfig_.enabled; }
	void UpdateGameFlow(float baseDeltaTime);
	void UpdateGameplayEventEffects(float baseDeltaTime, bool justDodgeTriggered);
	void BeginBossDefeatSequence();
	void BeginGameOver();
	void EnterResultState(bool stageClear);
	void ConfirmResultSelection();
	void UpdateResultText();
	void SetEventCallout(const std::string& text, float duration);
	void InitializePlayerClassConfigWatch();
	void UpdatePlayerClassConfigWatch(float deltaTime);
	bool ReloadPlayerClassConfig(bool automatic);
	Vector2 WorldToScreenUv(const Vector3& worldPos) const;

	struct HpBarVisibility {
		int lastHp = -1;
		int lastMaxHp = -1;
		float visibleTimer = 0.0f;
		float alpha = 0.0f;
	};

	struct PostProfileEntry {
		const char* name = "";
		float ms = 0.0f;
		bool active = false;
	};

#if defined(USE_IMGUI) && !defined(NDEBUG)
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
	};

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
		size_t enemyCount = 0;
		size_t expEnemyCount = 0;
		size_t bulletCount = 0;
		size_t playerBulletCount = 0;
		size_t enemyBulletCount = 0;
		size_t hostileExpEnemyBulletCount = 0;
		size_t bulletTrailCount = 0;
		size_t playerLaserCount = 0;
		size_t playerMineCount = 0;
		size_t playerMeleeSlashCount = 0;
		size_t neonTriangleParticleCount = 0;
	};
#endif


	std::unique_ptr<DebugCamera> debugCamera;
	std::unique_ptr<Camera> camera;
	
	//std::unique_ptr<Object3d> object3d;
	std::unique_ptr<Object3d> enemyObject_;
	std::unique_ptr<Object3d> object3d3;

	std::unique_ptr<Object3d> playerObject_;

	std::unique_ptr<Object3d> ballObj_;
	std::unique_ptr<Object3d> ball_;

	std::unique_ptr<Object3d> groundObj_;

	// 入力
	Input* input_;

	// ワールドトランスフォーム
	Transform worldTransform_;

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
	std::unique_ptr<RingManager> collisionDebugRingManager_;
	std::unique_ptr<NeonGridRenderer> neonGridRenderer_;
	std::unique_ptr<TrailManager> playerMeleeTrailManager_;
	std::unique_ptr<Skybox> skybox_;
	std::unique_ptr<ObjectPostEffect> neonGridPostEffect_;
	std::unique_ptr<ObjectPostEffect> bulletTrailPostEffect_;
	std::unique_ptr<ObjectPostEffect> particlePostEffect_;
	std::unique_ptr<ObjectPostEffect> playerPostEffect_;
	std::unique_ptr<ObjectPostEffect> enemyPostEffect_;
	std::unique_ptr<ObjectPostEffect> expEnemyPostEffect_;
	std::unique_ptr<ObjectPostEffect> sharedObjectBloomPostEffect_;
	std::unique_ptr<ObjectPostEffect> stagePostEffect_;
	LevelData currentLevelData_;
	BalanceEditorState balanceEditor_;
	std::vector<LevelVisualObject> levelItems_;
	std::vector<RuntimeBossPhase> levelBossPhases_;

	// 終了フラグ
	bool finished_ = false;
	std::string nextSceneName_ = "TITLE";

	std::unique_ptr<Fade> fade_ = nullptr;
	Phase phase_ = Phase::kFadeIn;

	std::unique_ptr<Sprite> shotGide;
	std::unique_ptr<Sprite> wasdGide;
	std::unique_ptr<Sprite> dashGide;
	std::unique_ptr<Sprite> toTitleGide;
	std::unique_ptr<TextLabel> dashGuideText_;
	std::unique_ptr<TextLabel> moveGuideText_;
	std::unique_ptr<TextLabel> titleGuideText_;
	std::unique_ptr<TextLabel> controlGuideText_;
	std::unique_ptr<TextLabel> fpsText_;
	std::unique_ptr<TextLabel> postProfileText_;
	std::unique_ptr<TextLabel> flowBannerText_;
	std::unique_ptr<TextLabel> resultSummaryText_;
	std::unique_ptr<TextLabel> resultMenuText_;
	std::unique_ptr<TextLabel> eventCalloutText_;
	std::unique_ptr<Sprite> tutorialPanel_;
	std::unique_ptr<TextLabel> tutorialTitleText_;
	std::unique_ptr<TextLabel> tutorialInputText_;
	std::unique_ptr<TextLabel> tutorialDescriptionText_;
	std::unique_ptr<NeonTextEffect> gameTextNeonEffect_;
	int gameTextFontMode_ = 1;
	bool gameTextNeonEnabled_ = true;
	bool gameTextOutlineEnabled_ = false;
	Vector4 gameTextOutlineColor_{ 0.0f, 0.0f, 0.0f, 0.9f };
	float gameTextOutlineThickness_ = 1.0f;
	NeonTextEffectStyle gameTextNeonStyle_{};
	std::vector<FollowHpBar> followHpBars_;
	std::array<std::vector<VertexData>, 4> hpBarBackgroundVertices_;
	std::array<std::vector<VertexData>, 4> hpBarFillVertices_;
	std::array<std::vector<VertexData>, 4> hpBarOutlineVertices_;
	std::unordered_map<const void*, HpBarVisibility> hpBarVisibility_;
	std::array<HpBarMaterialBuffer, 12> hpBarMaterials_;
	Microsoft::WRL::ComPtr<ID3D12Resource> hpBarVertexResource_;
	D3D12_VERTEX_BUFFER_VIEW hpBarVertexBufferView_{};
	VertexData* hpBarVertexData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> hpBarTransformResource_;
	TransformationMatrix* hpBarTransformData_ = nullptr;
	size_t followHpBarIndex_ = 0;
	bool showFollowHpBars_ = true;
	bool showPlayerStaminaBar_ = true;
	bool showControlGuide_ = true;
	TutorialConfig tutorialConfig_{};
	TutorialStep tutorialStep_ = TutorialStep::Move;
	bool tutorialStepCompleting_ = false;
	bool tutorialUiVisible_ = false;
	float tutorialStepCompleteTimer_ = 0.0f;
	float tutorialPhase1CompleteTimer_ = 0.0f;
	float tutorialEvolutionUnlockedTimer_ = 0.0f;
	float tutorialCompleteTimer_ = 0.0f;
	float tutorialMoveDistance_ = 0.0f;
	Vector3 tutorialPreviousPlayerPosition_{};
	bool tutorialUpgradeRewardGranted_ = false;
	bool tutorialEvolutionRewardGranted_ = false;
	bool tutorialEvolutionUiWasOpen_ = false;
	bool tutorialCompleteExitReady_ = false;

	// カメラ合わせフラグ
	bool cameraFollow_ = true;

	Vector3 direction = { 0.0f, -1.0f, 0.0f };
	float insensity = 1.0f;
	float shininess = 10.0f;

	float timeScale_ = 1.0f; // 1.0 が通常、0.2 なら 5倍スロー
	float finalDeltaTime = 1.0f / 60.0f;
	enum class GameFlowState {
		Playing,
		BossDefeatSequence,
		StageClear,
		GameOver,
	};
	GameFlowState gameFlowState_ = GameFlowState::Playing;
	ScreenEffectDirector screenEffectDirector_{};
	float gameFlowTimer_ = 0.0f;
	float bossDefeatSequenceDuration_ = 1.55f;
	float bossDefeatImpactDelayTimer_ = 0.0f;
	bool bossDefeatImpactTriggered_ = false;
	float playTime_ = 0.0f;
	float eventCalloutTimer_ = 0.0f;
	std::filesystem::file_time_type playerClassConfigObservedWriteTime_{};
	std::filesystem::file_time_type playerClassConfigLoadedWriteTime_{};
	float playerClassConfigPollTimer_ = 0.0f;
	float playerClassConfigDebounceTimer_ = 0.0f;
	bool playerClassConfigHasObservedWriteTime_ = false;
	bool playerClassConfigHasLoadedWriteTime_ = false;
	bool playerClassConfigReloadPending_ = false;
	int resultSelection_ = 0;
	int justDodgeCount_ = 0;
	int damageTaken_ = 0;
	int defeatedEnemies_ = 0;
	int previousPlayerHp_ = -1;
	int previousBossHp_ = -1;
	bool previousDashing_ = false;
	bool bossEntryTriggered_ = false;
	bool bossDefeatHandled_ = false;
	bool playerDeathHandled_ = false;
	std::chrono::steady_clock::time_point fpsLastSampleTime_{};
	float fpsAccumulatedTime_ = 0.0f;
	int fpsFrameCount_ = 0;
	bool enablePlayerPostEffect_ = true;
	bool enableEnemyPostEffect_ = true;
	bool enableExpEnemyPostEffect_ = true;
	bool enableStagePostEffect_ = false;
#ifdef USE_IMGUI
	bool showPostProfileOverlay_ = true;
#else
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
	std::array<char, 128> performanceCaptureLabel_{};
	bool performanceCaptureActive_ = false;
	bool performanceCaptureSkipCurrentFrame_ = false;
	PerformanceCaptureConditions performanceCaptureConditions_{};
	std::vector<PerformanceCaptureFrame> performanceCaptureFrames_;
	std::string performanceCaptureLastCsvPath_;
	std::string performanceCaptureStatus_;
#endif
	bool stagePostCacheValid_ = false;
	Vector3 stagePostCacheCameraPos_{};
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
	Vector4 worldGridColor_ = { 0.12f, 0.42f, 1.0f, 0.15f };
	float actorGridRadius_ = 5.4f;
	float actorGridSpacing_ = 1.0f;
	float actorGridLineWidth_ = 0.1f;
	float neonLineSoftEdgeRatio_ = 0.42f;
	float neonLineCoreIntensity_ = 1.35f;
	Vector4 playerGridColor_ = { 0.50f, 1.0f, 0.35f, 1.0f };
	Vector4 enemyGridColor_ = { 1.0f, 0.18f, 0.24f, 1.0f };
	Vector4 expEnemyGridColor_ = { 1.0f, 0.32f, 0.58f, 1.0f };
	bool showStageBlockNeonOutlines_ = true;
	bool showStageNormalBlockBodies_ = true;
	float stageBlockNeonLineWidth_ = 0.10f;
	float stageBlockNeonDepthBias_ = 0.035f;
	Vector4 stageBlockNeonColor_ = { 0.55f, 1.0f, 0.32f, 1.0f };
	bool showStageDamageBlockNeonOutlines_ = true;
	Vector4 stageDamageBlockNeonColor_ = { 1.20f, 0.035f, 0.02f, 1.0f };
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
	float bossNeonBarrelForwardOffset_ = 0.92f;
	float bossNeonBarrelSideOffset_ = 0.0f;
	float bossNeonBarrelLengthScale_ = 1.15f;
	float bossNeonBarrelWidthScale_ = 0.24f;
	float bossNeonBarrelAngleDeg_ = 0.0f;
	bool fillActorNeonBodies_ = true;
	Vector4 actorNeonBodyFillColor_ = { 0.006f, 0.010f, 0.016f, 0.92f };
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
	struct PlayerNeonAfterimage {
		Vector3 position{};
		Vector3 direction{ 0.0f, -1.0f, 0.0f };
		float life = 0.0f;
	};
	std::vector<PlayerNeonAfterimage> playerNeonAfterimages_;
	struct PlayerLaserBeam {
		Vector3 start{};
		Vector3 end{};
		float width = 0.18f;
		float life = 0.0f;
		float maxLife = 0.12f;
		Vector4 color{ 0.25f, 1.0f, 0.95f, 1.0f };
	};
	std::vector<PlayerLaserBeam> playerLaserBeams_;
	struct PlayerMine {
		Vector3 position{};
		float radius = 3.2f;
		float fuse = 0.45f;
		float life = 5.0f;
		float maxLife = 5.0f;
		uint32_t damage = 1;
		float rotation = 0.0f;
		Vector4 color{ 1.0f, 0.25f, 0.95f, 1.0f };
	};
	std::vector<PlayerMine> playerMines_;
	struct PlayerMineExplosion {
		Vector3 position{};
		float radius = 3.2f;
		float life = 0.28f;
		float maxLife = 0.28f;
		Vector4 color{ 1.0f, 0.25f, 0.95f, 1.0f };
	};
	std::vector<PlayerMineExplosion> playerMineExplosions_;
	struct PlayerMeleeSlash {
		Vector3 origin{};
		Vector3 followAnchor{};
		Vector3 direction{ 1.0f, 0.0f, 0.0f };
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
		float life = 0.18f;
		float maxLife = 0.18f;
		Vector4 color{ 0.55f, 1.25f, 1.0f, 1.0f };
		TrailInstance* trail = nullptr;
	};
	struct MeleeComboVisualProfile {
		float startAngleDeg = -70.0f;
		float endAngleDeg = 45.0f;
		float durationScale = 1.0f;
		float bladeLengthScale = 1.0f;
		float bladeWidthScale = 1.0f;
		float hiltSideOffset = 0.0f;
		float windupAngleDeg = -100.0f;
		float returnAngleDeg = 58.0f;
		Vector4 colorScale{ 1.0f, 1.0f, 1.0f, 1.0f };
	};
	std::vector<PlayerMeleeSlash> playerMeleeSlashes_;
	std::vector<MeleeComboVisualProfile> playerMeleeComboVisuals_ = {
		{ -75.0f, 45.0f, 0.92f, 0.96f, 0.90f, -0.04f, -105.0f, 58.0f, { 0.90f, 1.05f, 1.10f, 1.0f } },
		{ 70.0f, -55.0f, 1.00f, 1.02f, 1.00f, 0.05f, 110.0f, 58.0f, { 1.08f, 0.94f, 1.08f, 1.0f } },
		{ -145.0f, 135.0f, 1.28f, 1.16f, 1.30f, 0.00f, -178.0f, 58.0f, { 1.12f, 1.02f, 0.82f, 1.0f } }
	};
	struct NeonTriangleParticle {
		Vector3 position{};
		Vector3 velocity{};
		Vector3 initialVelocity{};
		float radius = 0.35f;
		float rotation = 0.0f;
		float angularVelocity = 0.0f;
		float lineWidth = 0.055f;
		float life = 0.0f;
		float maxLife = 0.35f;
		float tiltRad = 0.0f;
		int trailCopies = 0;
		bool isBillboard = false;
		Vector4 color{ 1.0f, 0.4f, 1.0f, 1.0f };
	};
	std::vector<NeonTriangleParticle> neonTriangleParticles_;
	float neonParticleTriangleGlowWidthScale_ = 3.2f;
	float neonParticleTriangleCoreWidthScale_ = 0.28f;
	float neonParticleTriangleBrightness_ = 1.45f;
	float neonParticleTriangleTrailSpacing_ = 0.48f;
	float neonParticleTriangleBirthScale_ = 0.52f;
	int neonParticleTriangleTrailCopies_ = 3;
	int neonTriangleEffectMode_ = 0;
	Vector3 neonTriangleDemoCenter_ = { 30.0f, 30.0f, 1.2f };
	float neonTriangleDemoRadius_ = 2.2f;
	float neonTriangleDemoLineWidth_ = 0.16f;
	float neonTriangleDemoRotateSpeed_ = 0.75f;
	float neonTriangleDemoRotation_ = 0.0f;
	Vector4 neonTriangleDemoColor_ = { 0.15f, 0.95f, 1.0f, 1.0f };

};
