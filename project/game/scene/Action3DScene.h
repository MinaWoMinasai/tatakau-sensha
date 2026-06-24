#pragma once

#include <memory>
#include <string>

#include "IScene.h"
#include "Camera.h"
#include "DebugCamera.h"
#include "Input.h"
#include "Object3d.h"
#include "SkinCluster.h"
#include "NeonGridRenderer.h"

// 課題用の最小3Dアクションシーン。
// GPU Skinningで描く人型をWASDで移動し、マウスで三人称カメラを操作する。
class Action3DScene : public IScene {
public:
	void Initialize() override;
	void Update() override;
	void Draw() override;
	void DrawShadow() override;
	void DrawPostEffect3D() override;
	void DrawSprite() override;

	bool IsFinished() const override { return finished_; }
	float GetFinalDeltaTime() const override { return finalDeltaTime_; }

private:
	void UpdatePlayer();
	void UpdatePlayerCombat();
	void UpdateBoss();
	void UpdateLockOn();
	void UpdateThirdPersonCamera();
	void UpdateJointFeatures();
	void BuildSkeletonDebugGeometry();
	Matrix4x4 GetPlayerWorldMatrix() const;
	Vector3 GetJointWorldPosition(int32_t jointIndex) const;
	void DrawControlWindow();
	void DamagePlayer(float damage);
	void ResetCombat();

	enum class BossAttackState {
		Idle,
		Telegraph,
		Attack,
		Recovery,
		Defeated,
	};

	Input* input_ = nullptr;
	std::unique_ptr<Camera> camera_;
	std::unique_ptr<DebugCamera> debugCamera_;
	std::unique_ptr<Object3d> ground_;
	std::unique_ptr<Object3d> playerObject_;
	std::unique_ptr<SkinnedModel> playerModel_;
	std::unique_ptr<Object3d> bossObject_;
	std::unique_ptr<SkinnedModel> bossModel_;
	std::unique_ptr<Object3d> weaponObject_;
	std::unique_ptr<NeonGridRenderer> skeletonRenderer_;
	Animation idleAnimation_;
	AnimationPlayer idleAnimationPlayer_;
	AnimationPlayer walkAnimationPlayer_;

	Vector3 playerPosition_ = { 0.0f, -25.0f, 0.0f };
	float playerYaw_ = 0.0f;
	float moveSpeed_ = 25.0f;
	Vector3 lastMoveDirection_ = { 0.0f, 0.0f, 1.0f };
	Vector3 dodgeDirection_ = { 0.0f, 0.0f, 1.0f };
	float playerHealth_ = 100.0f;
	float playerMaxHealth_ = 100.0f;
	float stamina_ = 100.0f;
	float maxStamina_ = 100.0f;
	float staminaRegen_ = 24.0f;
	float dodgeCost_ = 28.0f;
	float dodgeSpeed_ = 72.0f;
	float dodgeTimer_ = 0.0f;
	float dodgeDuration_ = 0.34f;
	float invincibleTimer_ = 0.0f;
	float dodgeParticleTimer_ = 0.0f;
	float playerAttackTimer_ = 0.0f;
	float playerAttackDuration_ = 0.48f;
	float playerAttackDamage_ = 25.0f;
	float playerAttackRange_ = 14.0f;
	bool playerAttackHit_ = false;
	Vector3 bossPosition_ = { 0.0f, -25.0f, 58.0f };
	float bossYaw_ = 3.14159265f;
	float bossHealth_ = 200.0f;
	float bossMaxHealth_ = 200.0f;
	float bossAttackDamage_ = 25.0f;
	float bossAttackRange_ = 11.0f;
	float bossMoveSpeed_ = 9.0f;
	float bossLungeSpeed_ = 54.0f;
	float bossStateTimer_ = 1.2f;
	Vector3 bossAttackDirection_ = { 0.0f, 0.0f, -1.0f };
	BossAttackState bossAttackState_ = BossAttackState::Idle;
	bool bossAttackHit_ = false;
	bool combatFinished_ = false;
	bool playerWon_ = false;
	bool bossLoaded_ = false;
	bool lockOnEnabled_ = true;
	float lockOnMaxDistance_ = 140.0f;
	float lockOnCameraTurnSpeed_ = 7.0f;

	float cameraYaw_ = 0.0f;
	float cameraPitch_ = 0.32f;
	float cameraDistance_ = 82.0f;
	float cameraHeight_ = 13.0f;
	float mouseSensitivity_ = 0.0035f;
	float gamepadCameraSpeed_ = 2.4f;

	int32_t rightHandJointIndex_ = -1;
	int32_t selectedJointIndex_ = 0;
	Vector3 rightHandWorldPosition_{};
	Vector3 weaponLocalScale_ = { 0.20f, 0.20f, 0.20f };
	Vector3 weaponLocalRotate_ = { 0.0f, 0.0f, 1.5707963f };
	Vector3 weaponLocalOffset_ = { 0.0f, 0.0f, 0.0f };
	float handParticleTimer_ = 0.0f;
	float handParticleInterval_ = 0.06f;
	float skeletonLineWidth_ = 0.12f;
	float locomotionBlend_ = 0.0f;
	float animationBlendDuration_ = 0.28f;
	bool useAnimationBlend_ = true;
	bool showSkeletonDebug_ = true;
	bool showSelectedJointAxes_ = true;
	bool attachWeaponToHand_ = true;
	bool emitParticlesFromHand_ = true;

	bool playerLoaded_ = false;
	std::string playerStatus_;
	bool finished_ = false;
	float finalDeltaTime_ = 1.0f / 60.0f;
};
