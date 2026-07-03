#pragma once
#define NOMINMAX
#include "Audio.h"
#include "debugCamera.h"
#include "Dump.h"
#include "Easing.h"
#include "Resource.h"
#include "Sprite.h"
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
#include "TrailManager.h"
#include "RingManager.h"
#include "CylinderManager.h"
#include "Skybox.h"
#include "EffectSequencer.h"
#include "ObjectPostEffect.h"
#include "Animation.h"
#include "Skeleton.h"
#include "SkinCluster.h"

// ゲームシーン
class TestScene : public IScene {

public:

	/// <summary>
	/// コンストラクタ
	/// </summary>
	TestScene();

	/// <summary>
	/// デストラクタ
	/// </summary>
	~TestScene();

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

	/// <summary>
	/// 描画
	/// </summary>
	void DrawSprite() override;

	bool IsFinished() const override { return finished_; }

	float GetFinalDeltaTime() const override { return finalDeltaTime; }

private:


	std::unique_ptr<DebugCamera> debugCamera;
	std::unique_ptr<Camera> camera;

	std::unique_ptr<Object3d> groundObj_;
	std::unique_ptr<Object3d> slopeGroundObj_;
	std::unique_ptr<Object3d> blockObj_;
	std::unique_ptr<Object3d> blockObj2_;
	std::unique_ptr<Object3d> effectStartMarker_;
	std::unique_ptr<Object3d> effectTargetMarker_;

	std::unique_ptr<TrailManager> trailManager_;
	TrailInstance* swordTrail_ = nullptr;
	std::unique_ptr<RingManager> ringManager_;
	std::unique_ptr<CylinderManager> cylinderManager_;
	std::unique_ptr<EffectSequencer> effectSequencer_;
	std::unique_ptr<ObjectPostEffect> objectPostEffect_;
	std::unique_ptr<Object3d> swordObj_;

	std::unique_ptr<Skybox> skybox_;

	// 入力
	Input* input_;

	// ワールドトランスフォーム
	Transform worldTransform_;

	bool finished_ = false;

	float finalDeltaTime = 1.0f / 60.0f;

	EffectProfile transplantTestProfile_;
	Vector3 effectStartPos_ = { -35.0f, 15.0f, 0.0f };
	Vector3 effectTargetPos_ = { 35.0f, 15.0f, 0.0f };
	bool autoFireEffect_ = false;
	bool enableObjectPostEffect_ = true;
	float autoFireTimer_ = 0.0f;
	RingEffectConfig ringConfig_;
	CylinderEffectConfig cylinderConfig_;

	Animation keyframeTestAnimation_;
	AnimationPlayer keyframeTestPlayer_;
	Skeleton keyframeTestSkeleton_;
	float keyframeTestPlaybackSpeed_ = 1.0f;
	bool assimpAnimationLoaded_ = false;
	std::string assimpAnimationStatus_;

	SkinningModelAsset skinningTestAsset_;
	Skeleton skinningTestSkeleton_;
	SkinCluster skinningTestCluster_;
	Animation skinningTestAnimation_;
	AnimationPlayer skinningTestPlayer_;
	std::unique_ptr<SkinnedModel> skinnedTestModel_;
	std::unique_ptr<Object3d> skinnedTestObject_;
	bool skinClusterLoaded_ = false;
	std::string skinClusterStatus_;
	std::unique_ptr<SkinnedModel> humanTestModel_;
	std::unique_ptr<Object3d> humanTestObject_;
	bool humanSkinningLoaded_ = false;
	std::string humanSkinningStatus_;
	Vector3 humanActionPosition_ = { 0.0f, -25.0f, 20.0f };
	float humanActionMoveSpeed_ = 25.0f;
	bool enableHumanActionControl_ = true;
	bool showLegacyTestObjects_ = false;
	bool showSimpleSkin_ = false;
	bool showHumanSkinning_ = true;

	// VRoidへMixamoモーションをリターゲットしたゲーム用GLB。
	std::unique_ptr<SkinnedModel> vrmTestModel_;
	std::unique_ptr<Object3d> vrmTestObject_;
	bool vrmTestLoaded_ = false;
	bool showVrmTestModel_ = true;
	std::string vrmTestStatus_;
	Vector3 vrmActionPosition_ = { 0.0f, -30.0f, 20.0f };
	float vrmActionMoveSpeed_ = 12.0f;
	float vrmActionRunMultiplier_ = 1.8f;
	float vrmAnimationBlendDuration_ = 0.18f;
	float vrmFacingTurnSpeed_ = 12.0f;
	float vrmFacingYaw_ = 0.0f;
	bool vrmLockFacing_ = true;
	bool enableVrmActionControl_ = true;
	std::string vrmCurrentAnimation_ = "Idle";
	enum class VrmActionState { Locomotion, Dodge, Jump, Attack };
	enum class VrmJumpPhase { Takeoff, Rising, Falling, Landing };
	VrmActionState vrmActionState_ = VrmActionState::Locomotion;
	VrmJumpPhase vrmJumpPhase_ = VrmJumpPhase::Takeoff;
	Vector3 vrmDodgeDirection_ = { 0.0f, 0.0f, 1.0f };
	float vrmDodgeDistance_ = 22.0f;
	float vrmDodgeElapsed_ = 0.0f;
	float vrmDodgePreviousProgress_ = 0.0f;
	float vrmVerticalVelocity_ = 0.0f;
	float vrmJumpSpeed_ = 30.0f;
	float vrmGravity_ = 32.0f;
	float vrmGroundY_ = -30.0f;
	float vrmTakeoffPlaybackSpeed_ = 1.35f;
	float vrmTakeoffLaunchPhase_ = 0.42f;
	float vrmLandingStartPhase_ = 0.30f;
	float vrmLandingPlaybackSpeed_ = 1.35f;
	float vrmAttackElapsed_ = 0.0f;
	float vrmAttackPlaybackSpeed_ = 1.45f;
	float vrmComboBufferStart_ = 0.18f;
	float vrmComboChainPoint_ = 0.58f;
	int vrmComboStep_ = 0;
	bool vrmComboQueued_ = false;
	bool vrmAttackHitActive_ = false;
	std::unique_ptr<ObjectPostEffect> swordPostEffect_;
	bool enableSwordBloom_ = true;
	Vector3 vrmSwordLocalOffset_ = { 0.0f, 0.0f, 0.0f };
	Vector3 vrmSwordLocalRotate_ = { 0.0f, 0.0f, 0.0f };
	Vector3 vrmSwordAttack2RotateCorrection_ = { 0.55f, 0.0f, 0.0f };
	Vector3 vrmSwordLocalScale_ = { 0.14f, 0.14f, 0.14f };
	bool vrmSwordAttached_ = false;
	bool enableSwordTrail_ = true;
	TrailConfig swordTrailConfig_{};
	float vrmSwordTrailBaseY_ = 0.35f;
	float vrmSwordTrailTipY_ = 6.0f;
	bool enableSlopeGround_ = true;
	Vector3 slopeGroundCenter_ = { 30.0f, 0.0f, 20.0f };
	Vector3 slopeGroundScale_ = { 20.0f, 2.0f, 15.0f };
	float slopeGroundAngle_ = 0.25f;

	float GetVrmGroundHeight(const Vector3& position) const;

};
