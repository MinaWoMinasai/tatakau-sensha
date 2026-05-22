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
	
	std::string GetNextSceneName() const override;

private:
	struct FollowHpBar {
		std::unique_ptr<Sprite> outline;
		std::unique_ptr<Sprite> background;
		std::unique_ptr<Sprite> fill;
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
	void DrawNeonGridPass();

	struct HpBarVisibility {
		int lastHp = -1;
		int lastMaxHp = -1;
		float visibleTimer = 0.0f;
		float alpha = 0.0f;
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
	std::unique_ptr<ObjectPostEffect> neonGridPostEffect_;
	std::unique_ptr<ObjectPostEffect> bulletTrailPostEffect_;
	std::unique_ptr<ObjectPostEffect> playerPostEffect_;
	std::unique_ptr<ObjectPostEffect> enemyPostEffect_;
	std::unique_ptr<ObjectPostEffect> expEnemyPostEffect_;
	std::unique_ptr<ObjectPostEffect> stagePostEffect_;

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
	std::unique_ptr<TextLabel> fpsText_;
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
	bool enableStagePostEffect_ = true;
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
	bool showNeonGrid_ = true;
	bool showActorLocalGrid_ = true;
	bool enableNeonGridPostEffect_ = true;
	bool enableBulletTrailPostEffect_ = true;
	float worldGridSpacing_ = 2.0f;
	float worldGridLineWidth_ = 0.075f;
	Vector4 worldGridColor_ = { 0.12f, 0.42f, 1.0f, 0.15f };
	float actorGridRadius_ = 5.4f;
	float actorGridSpacing_ = 1.0f;
	float actorGridLineWidth_ = 0.1f;
	Vector4 playerGridColor_ = { 0.50f, 1.0f, 0.35f, 1.0f };
	Vector4 enemyGridColor_ = { 1.0f, 0.18f, 0.24f, 1.0f };
	Vector4 expEnemyGridColor_ = { 1.0f, 0.32f, 0.58f, 1.0f };

};
