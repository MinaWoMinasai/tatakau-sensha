#pragma once
#define NOMINMAX
#include <algorithm>
#include <array>
#include <chrono>
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
#include "Skybox.h"
#include "game/level/LevelLoader.h"

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
	void SetRenderProfile(const IScene::RenderProfile& profile) override { renderProfile_ = profile; }
	
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

	void InitializeFollowHpBars(size_t count);
	void InitializeFollowHpBarBatch();
	void DrawFollowHpBar(const void* ownerKey, const Vector3& worldPos, int hp, int maxHp, float width, float yOffset);
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
	void QueueExpEnemyNeonShapes(const Vector3& cameraRight, const Vector3& cameraUp, const Vector3& cameraForward);
	bool IsNearCamera2D(const Vector3& worldPos, float halfWidth, float halfHeight, float margin = 0.0f) const;
	void ResetPostProfileEntries();
	void AddPostProfileEntry(const char* name, float ms, bool active);
	void UpdatePostProfileText();
	void DrawPerformanceBreakdownImGui();
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
	std::unique_ptr<Skybox> skybox_;
	std::unique_ptr<ObjectPostEffect> neonGridPostEffect_;
	std::unique_ptr<ObjectPostEffect> bulletTrailPostEffect_;
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
	bool showControlGuide_ = true;

	// カメラ合わせフラグ
	bool cameraFollow_ = true;

	Vector3 direction = { 0.0f, -1.0f, 0.0f };
	float insensity = 1.0f;
	float shininess = 10.0f;

	float timeScale_ = 1.0f; // 1.0 が通常、0.2 なら 5倍スロー
	float finalDeltaTime;
	std::chrono::steady_clock::time_point fpsLastSampleTime_{};
	float fpsAccumulatedTime_ = 0.0f;
	int fpsFrameCount_ = 0;
	bool enablePlayerPostEffect_ = true;
	bool enableEnemyPostEffect_ = true;
	bool enableExpEnemyPostEffect_ = true;
	bool enableStagePostEffect_ = false;
	bool showPostProfileOverlay_ = true;
	int postProfileMode_ = 0;
	std::array<PostProfileEntry, 16> postProfileEntries_;
	size_t postProfileEntryCount_ = 0;
	std::array<float, 16> postProfileAccumulatedMs_{};
	std::array<float, 16> postProfileAverageMs_{};
	int postProfileAccumulatedFrames_ = 0;
	IScene::RenderProfile renderProfile_{};
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
	bool showLevelAIDitorPreview_ = true;
	bool enableNeonGridPostEffect_ = true;
	bool enableBulletTrailPostEffect_ = true;
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
	bool cullActorLocalGrid_ = true;
	int maxExpEnemyLocalGrids_ = 18;
	bool showNeonTriangleDemo_ = true;
	int expEnemyNeonRenderMode_ = 0;
	float expEnemyNeonSquareSize_ = 1.8f;
	float expEnemyNeonTriangleRadius_ = 1.05f;
	float expEnemyNeonPentagonRadius_ = 1.05f;
	float expEnemyNeonShooterRadius_ = 0.95f;
	float expEnemyNeonLineWidth_ = 0.12f;
	Vector3 neonTriangleDemoCenter_ = { 30.0f, 30.0f, 1.2f };
	float neonTriangleDemoRadius_ = 2.2f;
	float neonTriangleDemoLineWidth_ = 0.16f;
	float neonTriangleDemoRotateSpeed_ = 0.75f;
	float neonTriangleDemoRotation_ = 0.0f;
	Vector4 neonTriangleDemoColor_ = { 0.15f, 0.95f, 1.0f, 1.0f };

};
