#pragma once

#include <memory>
#include <string>

#include "IScene.h"
#include "Camera.h"
#include "DebugCamera.h"
#include "Input.h"
#include "Object3d.h"
#include "SkinCluster.h"

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
	void UpdateThirdPersonCamera();
	void DrawControlWindow();

	Input* input_ = nullptr;
	std::unique_ptr<Camera> camera_;
	std::unique_ptr<DebugCamera> debugCamera_;
	std::unique_ptr<Object3d> ground_;
	std::unique_ptr<Object3d> playerObject_;
	std::unique_ptr<SkinnedModel> playerModel_;

	Vector3 playerPosition_ = { 0.0f, -25.0f, 0.0f };
	float playerYaw_ = 0.0f;
	float moveSpeed_ = 25.0f;

	float cameraYaw_ = 0.0f;
	float cameraPitch_ = 0.32f;
	float cameraDistance_ = 82.0f;
	float cameraHeight_ = 13.0f;
	float mouseSensitivity_ = 0.0035f;

	bool playerLoaded_ = false;
	std::string playerStatus_;
	bool finished_ = false;
	float finalDeltaTime_ = 1.0f / 60.0f;
};
