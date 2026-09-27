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

// ゲームシーン
class GameScene : public IScene {

public:

	/// <summary>
	/// コンストラクタ
	/// </summary>
	explicit GameScene(bool prototypeRun = false, bool expeditionRun = false);

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
	struct TitleDemoStatus {
		int stage = 0;
		float stageSeconds = 0, totalSeconds = 0;
		uint32_t stagesVisitedMask = 0;
		int shots = 0, dashes = 0, kills = 0, rewards = 0, routes = 0, maxPlayerBullets = 0;
	};
	void EnableTitleDemo() { titleDemo_ = true; }
	bool IsTitleDemo() const { return titleDemo_; }
	const TitleDemoStatus& GetTitleDemoStatus() const { return titleDemoStatus_; }
	void RequestTitleDemoCapture(const std::string& name);
	void CopyTitleDemoCapture();
	void FlushTitleDemoCapture();
	nlohmann::json GetTitleDemoBuild() const;
	
	std::string GetNextSceneName() const override;

private:
	void InitializeTitleDemo();
	void UpdateTitleDemo(float dt);
	void ResetTitleDemoStage(int stage);
	void VerifyTitleDemoTransition(float dt);
	float titleDemoTransitionTimer_ = 0;
	bool titleDemoTransitionVerified_ = false;
	bool titleDemo_ = false;
	TitleDemoStatus titleDemoStatus_{};
	bool titleDemoPreviousDash_ = false;
	size_t titleDemoPreviousBulletCount_ = 0;
	float titleDemoNavigationTimer_ = 0;
	Vector3 titleDemoMoveTarget_{};
	std::vector<Vector3> titleDemoPath_;
	void ApplyTankExpeditionRoomGeometry();
	void InitializeTankExpeditionBalance();
	void ApplyTankExpeditionRoomBalance();
	void UpdateTankExpeditionBalanceEditor();
	void DrawTankExpeditionBalanceEditor();
	void UpdateExpeditionAuthoringHub();
	bool expeditionAuthoringHubOpen_ = false;
	nlohmann::json expeditionPostDraft_;
	nlohmann::json expeditionVisualDraft_;
	std::string expeditionAuthoringStatus_;
	nlohmann::json expeditionStyleBalanceDraft_;
	bool tankExpeditionBalanceEditorOpen_ = false;
	void InitializeExpeditionMap();
	void UpdateExpeditionMap(float dt);
	void UpdateExpeditionAuthoring();
	void RefreshExpeditionMapUi();
	void DrawExpeditionMapUi();
	void EnterExpeditionMapNode(const std::string& id);
	void RequestExpeditionMapNode(const std::string& id);
	void BeginExpeditionPresentation(int action, const std::string& title, const std::string& detail, const Vector4& color);
	void UpdateExpeditionPresentation(float dt);
	void DrawExpeditionPresentation();
	void SelectExpeditionService(int option);
	void CompleteExpeditionMapCombat();
	bool StartAuthoredExpeditionRoom();
	void RefreshExpeditionServiceOffers();
	void InitializeExpeditionBuildCards();
	void RefreshExpeditionBuildCards();
	void UpdateExpeditionBuildCards(float dt);
	void SelectExpeditionBuildStyle(int index);
	bool IsExpeditionBuildCardScreen() const;
	void InitializeExpeditionExperience();
	void SpawnExpeditionCredits(const Vector3& position, int amount, bool flyImmediately = false);
	void UpdateExpeditionCredits(float dt, bool collectAll = false);
	void DrawExpeditionCredits();
	void DrawExpeditionVitals();
	void UpdateGuidedExpedition(float dt);
	void AcknowledgeGuidedExpedition();
	void DrawGuidedExpedition();
	void RefreshGuidedExpeditionUi();
	bool IsGuidedExpeditionPaused() const;
	void QueueExpeditionImpact(const Vector3& position, const Vector3& direction, bool finisher);
	void DrawExpeditionPointer(Vector2 target, bool right = true);
	bool IsIntroExpeditionService() const;
	int ExpeditionServicePrice(const std::string& id) const;
	void SetExpeditionBlueprint(int index);
	void UpdateExpeditionMapValidation(float dt);
	void InitializeCombatValidationFixture();
	bool UpdateCombatValidation(float dt);
	void BeginCombatValidationProbe(int index);
	void FinishCombatValidationProbe();
	void WriteCombatValidationReport(bool completed);
	void CaptureCombatValidation(const std::string& name);
	bool combatValidationEnabled_ = false, combatValidationRequested_ = false, combatValidationPhase2Injected_ = false;
	float combatValidationElapsed_ = 0;
	int combatValidationIndex_ = -1;
	struct CombatValidationProbe {
		std::string id;
		float age = 0, pathLength = 0, maxStep = 0, reloadSeconds = 0, sampleAge = 0;
		Vector3 previousPosition{};
		bool hasPrevious = false, previousReload = false, phase2 = false;
		unsigned shots = 0, dashes = 0, reloads = 0, phases = 0, patterns = 0;
		int previousAmmo = 0, reloadViolations = 0, wallIntersections = 0, tunnelingViolations = 0, playerBulletSamples = 0;
		size_t projectileSamples = 0, maxProjectiles = 0;
		nlohmann::json trace = nlohmann::json::array();
	} combatValidationProbe_;
	nlohmann::json combatValidationResults_ = nlohmann::json::array();
	std::vector<std::string> combatValidationErrors_, combatValidationCaptures_;
	bool expeditionMapEnabled_ = false;
	bool expeditionRoomEditorOpen_ = false, expeditionMapEditorOpen_ = false, expeditionContentEditorOpen_ = false;
	tankexp::MapDefinition expeditionMapDefinition_;
	tankexp::ExpeditionMapRun expeditionMapRun_;
	tankexp::RoomCatalog expeditionRooms_;
	tankcontent::Catalog expeditionContent_;
	tankexp::ExpeditionRoomEditor expeditionRoomEditor_;
	tankexp::MapEditor expeditionMapEditor_;
	tankcontent::ContentEditor expeditionContentEditor_;
	std::string expeditionMapSelection_, expeditionMapStatus_;
	std::vector<std::string> expeditionServiceOffers_;
	std::vector<std::string> expeditionIntroOffers_;
	uint32_t expeditionSeed_ = 0;
	std::unordered_map<std::string,int> expeditionPurchases_;
	// Retain purchased effects if their source is renamed/deleted while authoring.
	std::unordered_map<std::string,tankcontent::Upgrade> expeditionPurchasedModules_;
	std::array<float,tankrun::CardCount> ExpeditionEffectPowers() const;
	int expeditionServicePage_ = 0, expeditionBlueprint_ = 0;
	float expeditionMapScroll_ = 0;
	bool expeditionMapPreview_ = false;
	bool expeditionBuildChoice_ = false, expeditionBuildChosen_ = false;
	bool specialValidationEnabled_ = false;
	nlohmann::json specialValidation_;
	void InitializeSpecialValidationFixture();
	bool UpdateSpecialValidation(float dt);
	tankbuild::Style expeditionBuildStyle_ = tankbuild::Style::Shooter;
	int expeditionPendingBuild_ = -1;
	std::array<std::unique_ptr<TankRewardCard>,3> expeditionRewardCards_;
	tankexp::PresentationTransition expeditionTransition_;
	int expeditionTransitionAction_ = 0;
	int expeditionPendingService_ = -1;
	std::string expeditionPendingNode_;
	Vector4 expeditionTransitionColor_{0.3f,0.9f,1,1};
	std::unique_ptr<Sprite> expeditionCurtain_, expeditionTransitionPanel_, expeditionTransitionRail_, expeditionTransitionProgress_;
	std::unique_ptr<TextLabel> expeditionTransitionTitle_, expeditionTransitionDetail_;
	float expeditionPresentationClock_ = 0, expeditionUiErrorAge_ = 0;
	struct ExpeditionHitSpark {Vector3 position{},direction{};float age=0;};
	std::vector<ExpeditionHitSpark> expeditionHitSparks_;
	float expeditionHitSparkCooldown_=0;
	bool expeditionBossPhase2Seen_=false;
	std::string expeditionLastFocus_;
	std::array<float,3> expeditionCardFocus_{};
	struct MapNodeVisual {
		Vector2 center{};
		float focus = 0;
		std::unique_ptr<Sprite> halo, rim, fill;
		std::unique_ptr<TextLabel> icon, label, state;
	};
	struct MapEdgeVisual {std::string from,to;std::unique_ptr<Sprite> glow,line,pulse;};
	std::vector<MapNodeVisual> expeditionMapVisuals_;
	std::vector<MapEdgeVisual> expeditionMapEdges_;
	std::vector<std::unique_ptr<Sprite>> expeditionMapGrid_;
	std::unique_ptr<TextLabel> expeditionMapTitle_, expeditionMapSubtitle_, expeditionMapInfo_, expeditionMapLegend_, expeditionMapHelp_;
	std::array<std::unique_ptr<TextLabel>,3> expeditionBlueprintLabels_;
	std::array<std::unique_ptr<Sprite>,3> expeditionBlueprintButtons_;
	struct ExpeditionCreditOrb {
		Vector3 position{}, velocity{};
		Vector2 launch{};
		float age = 0, flight = 0;
		int value = 0;
		bool flying = false;
		std::unique_ptr<Sprite> sprite;
	};
	std::vector<ExpeditionCreditOrb> expeditionCredits_;
	std::unique_ptr<Sprite> expeditionCreditIcon_, expeditionCreditPulse_, expeditionStaminaTrack_, expeditionStaminaFill_;
	std::unique_ptr<TextLabel> expeditionCreditText_;
	std::unique_ptr<NeonTextEffect> expeditionCompleteGlow_;
	std::array<std::unique_ptr<Sprite>,4> expeditionSpotlight_;
	std::array<std::unique_ptr<Sprite>,6> expeditionPointer_;
	std::unique_ptr<Sprite> expeditionContinueButton_, expeditionSkipButton_;
	std::unique_ptr<TextLabel> expeditionContinueText_, expeditionSkipText_;
	float expeditionCreditPulseAge_ = 0, expeditionImpactHold_ = 0;
	int expeditionCreditsCollected_ = 0;
	bool expeditionCollectAll_ = false, expeditionClearRewardQueued_ = false;
	bool expeditionGuideActive_ = false, expeditionGuideShooterSpawned_ = false;
	tankexp::GuidedCombatTutorial expeditionGuide_;
	int expeditionGuideLastKills_ = 0;
	uint32_t expeditionGuideDamageCount_ = 0;
	uint32_t expeditionGuideAttackCount_ = 0;
	float expeditionGuideAge_ = 0;
	bool expeditionMapAutoTest_ = false;
	float expeditionMapTestElapsed_ = 0, expeditionMapTestAge_ = 0;
	std::string expeditionMapTestState_;
	std::vector<std::string> expeditionMapTestVisited_;
	int expeditionMapTestPurchases_ = 0, expeditionMapTestHeals_ = 0, expeditionMapTestEvolutions_ = 0;
	nlohmann::json tankExpeditionBalance_;
	void UpdateTankExpeditionTutorial(float dt);
	void UpdateTankExpeditionTutorialValidation(float dt);
	tankexp::TutorialValidationState tankExpeditionTutorialValidation_{};
	void DrawTankExpeditionTutorial();
	void RefreshTankExpeditionTutorialUi();
	tankexp::ExpeditionTutorial tankExpeditionTutorial_{};
	bool tankExpeditionTutorialSaved_ = false;
	bool tankExpeditionDetailsOpen_ = false;
	int tankExpeditionTutorialKills_ = 0;
	Vector3 tankExpeditionTutorialPrevious_{};
	std::unique_ptr<Sprite> tankExpeditionHpTrack_, tankExpeditionHpFill_;
	std::unique_ptr<Sprite> tankExpeditionExpTrack_, tankExpeditionExpFill_, tankExpeditionBuildPanel_;
	std::unique_ptr<TextLabel> tankExpeditionExpText_, tankExpeditionDetailsText_;
	void InitializeTankExpedition();
	void UpdateTankExpedition(float dt);
	void StartTankExpeditionRoom();
	void FinishTankExpeditionRoom();
	void SelectTankExpeditionOption(int index);
	void RefreshTankExpeditionUi();
	void DrawTankExpeditionUi();
	void UpdateTankExpeditionAudio(float dt);
	bool IsRunRivalActive() const { return !expeditionRun_ || tankExpeditionRivalActive_; }
	int GetTankExpeditionOptionCount() const;
	bool expeditionRun_ = false;
	tankexp::ExpeditionDirector tankExpedition_{};
	bool tankExpeditionRivalActive_ = false;
	bool tankExpeditionRoomPending_ = false;
	bool tankExpeditionResourceWon_ = false;
	bool tankExpeditionRewardOpen_ = false;
	bool tankExpeditionMaintenanceOpen_ = false;
	bool tankExpeditionResourceReleased_ = false;
	int tankExpeditionAutoMaintainedRoom_ = -1;
	int tankExpeditionValidationErrors_ = 0;
	bool tankExpeditionMusicEnabled_ = true;
	bool tankExpeditionEffectsEnabled_ = true;
	TankExpeditionAudio tankExpeditionAudio_;
	std::unordered_map<const ExpEnemy*,int> tankExpeditionEnemyHp_;
	std::unordered_map<const ExpEnemy*,bool> tankExpeditionEnemyWarning_;
	std::unordered_map<const ExpEnemy*,std::pair<uint64_t,uint64_t>> guardAudioCounts_;
	int tankExpeditionNodes_ = 0;
	int tankExpeditionSpawned_ = 0;
	int tankExpeditionAutoVariant_ = 0;
	int tankExpeditionCaptureIndex_ = 0;
	float tankExpeditionArrival_ = 0;
	std::vector<RunEvolutionChoice> tankExpeditionEvolutions_;
	std::unique_ptr<TextLabel> tankExpeditionMapText_;
	std::unique_ptr<TextLabel> tankExpeditionMaintenanceText_;
	std::unique_ptr<Sprite> tankExpeditionMaintenanceButton_;
	void InitializeTankRun();
	void InitializeTankRunVisuals();
	void UpdateTankRun(float dt);
	void DrawTankRunUi();
	void QueueTankRunTelegraph();
	void ApplyTankRunCards();
	void UpdateTankRunResources(float dt);
	void OnTankRunResourceClaim(size_t index, bool playerOwned);
	void OnTankRunEnemyDefeated(const Vector3& position);
	void SelectTankRunOption(int index);
	void RefreshTankRunUi();
	bool IsTankRunMenuOpen() const;
	void RequestTankRunCapture(const std::string& name);
	void CopyTankRunCapture();
	void FinishTankRunCapture();
	bool prototypeRun_ = false;
	tankrun::RunDirector tankRun_{};
	int tankRunSelection_ = 0;
	struct RunResource { Vector3 position{}; float respawn = 0; bool active = false; };
	struct RunBurst { Vector3 position{}; float age = 0; bool resource = false; };
	std::array<RunResource, 3> tankRunResources_{};
	std::vector<RunBurst> tankRunBursts_;
	int tankRunCombo_ = 0;
	int tankRunBestCombo_ = 0;
	float tankRunComboTime_ = 0;
	int tankRunLastBossLevel_ = 1;
	float tankRunMenuAge_ = 0.0f;
	float tankRunHudTimer_ = 0.0f;
	bool tankRunPaused_ = false;
	bool tankRunFinalStarted_ = false;
	bool tankRunAutoTest_ = false;
	float tankRunAutoTime_ = 0.0f;
	int tankRunAutoStep_ = 0;
	int tankRunAutoMenuIndex_ = 0;
	std::unique_ptr<Sprite> tankRunDimmer_;
	std::unique_ptr<Sprite> tankRunHudPanel_;
	std::unique_ptr<Sprite> tankRunBossTrack_;
	std::unique_ptr<Sprite> tankRunBossFill_;
	std::array<std::unique_ptr<Sprite>, 3> tankRunCards_;
	std::array<std::unique_ptr<TextLabel>, 3> tankRunCardTitles_;
	std::array<std::unique_ptr<TextLabel>, 3> tankRunCardBodies_;
	std::unique_ptr<TextLabel> tankRunHeading_;
	std::unique_ptr<TextLabel> tankRunDescription_;
	std::unique_ptr<TextLabel> tankRunFooter_;
	std::unique_ptr<TextLabel> tankRunHud_;
	std::unique_ptr<TextLabel> tankRunBuildText_;
	std::unique_ptr<TextLabel> tankRunObjectiveText_;
	std::unique_ptr<TextLabel> tankRunBossText_;
	Microsoft::WRL::ComPtr<ID3D12Resource> tankRunCaptureReadback_;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT tankRunCaptureLayout_{};
	std::string tankRunCapturePath_;
	bool tankRunCaptureCopied_ = false;
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
	void UpdateSpecialCombatPresentation(float deltaTime);
	void QueueSpecialCombatPresentation();
	struct SpecialCombatFlash {
		Player::SpecialCombatEvent event;
		float age=0;
		Vector3 end{};
	};
	std::vector<SpecialCombatFlash> specialCombatFlashes_;
	struct BuildCombatFlash {BulletManager::BuildEvent event;float age=0;};
	std::vector<BuildCombatFlash> buildCombatFlashes_;
	struct SpecialProjectileVisual {Vector3 position,direction;float radius;Bullet::SpecialKind kind;};
	std::vector<SpecialProjectileVisual> specialProjectileVisuals_;
	float railChargeAudioAge_=0;
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
		bool trailAutoFireEnabled = false;
		bool d3d12DebugLayerEnabled = false;
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
		TrailManager::DrawStats trailDrawStats{};
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
	std::vector<VertexData> staminaBarFillVertices_;
	std::array<std::vector<VertexData>, 4> hpBarOutlineVertices_;
	std::unordered_map<const void*, HpBarVisibility> hpBarVisibility_;
	std::array<HpBarMaterialBuffer, 13> hpBarMaterials_;
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
	Player::UpgradeHudDebugSnapshot upgradeHudAfterPlayerUpdate_{};
	Player::UpgradeHudDebugSnapshot upgradeHudAfterCollision_{};
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
	float playerNeonEmission_ = 1.0f, bossNeonEmission_ = 1.0f;
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
		float knockback = 0;
		bool finisher = false;
		std::vector<const Collider*> hitTargets;
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

	// Opt-in end-to-end experience validation; ordinary play never enters it.
	void InitializeExperienceValidation();
	bool UpdateExperienceValidation(float dt);
	void CaptureExperienceValidation(const std::string& name);
	void WriteExperienceValidationReport(bool completed);
	void BeginExperienceMeleeProbe();
	int experienceValidationVariant_ = 0;
	int experienceValidationStyle_ = 2, experiencePreviewRarity_ = 0;
	bool experienceBuildPreserved_ = false;
	bool experienceEvolutionVerified_ = false;
	int experienceDroneSamples_ = 0;
	float experienceValidationElapsed_ = 0, experienceValidationStateAge_ = 0, experienceMeleeAge_ = 0;
	std::string experienceValidationState_;
	std::vector<std::string> experienceValidationCaptures_, experienceValidationErrors_;
	std::vector<std::string> experienceValidationIntroOffers_;
	bool experienceMeleeStarted_ = false, experienceMeleeDone_ = false;
	bool experienceCreditArrivalEligible_ = false, experienceCreditDelivered_ = false;
	int experienceEarlyFlightSamples_ = 0, experiencePrematureCredits_ = 0, experienceGroundOrbSamples_ = 0;
	int experienceIntroWallet_ = -1, experienceAfterIntroWallet_ = -1, experienceInitialKills_ = 0;
	int experienceForcedLaterClears_ = 0, experiencePlayerBulletSamples_ = 0;
	int experienceMeleeMinHp_ = 500, experienceMeleeSlashSamples_ = 0, experienceMeleeBulletSamples_ = 0;
	float experienceMeleeDisplacement_ = 0;
	Vector3 experienceMeleeTargetStart_{};
	unsigned experienceGuideStageMask_ = 0, experienceSuccessfulDashes_ = 0;
	nlohmann::json experienceIntroOfferDetails_ = nlohmann::json::array();
};
