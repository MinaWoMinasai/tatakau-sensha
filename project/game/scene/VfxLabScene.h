#pragma once

#include <memory>
#include <string>

#include "Camera.h"
#include "DebugCamera.h"
#include "Input.h"
#include "IScene.h"
#include "ProceduralFlameRenderer.h"

class VfxLabBackgroundRenderer;

// Development-only scene ownership is enforced by the scene registry. The
// class itself stays small and directly owns the effects under development.
class VfxLabScene : public IScene {
public:
	VfxLabScene();
	~VfxLabScene() override;

	void Initialize() override;
	void Update() override;
	void Draw() override {}
	void DrawShadow() override {}
	void DrawPostEffect3D() override;
	void DrawSprite() override {}

	bool IsFinished() const override { return finished_; }
	float GetFinalDeltaTime() const override { return finalDeltaTime_; }
	std::string GetNextSceneName() const override { return nextSceneName_; }

private:
	enum class BackgroundMode {
		Black = 0,
		NeutralDarkGray = 1,
		Checker = 2,
	};

	void UpdateCamera();
	void DrawDebugWindow();

	Input* input_ = nullptr;
	std::unique_ptr<Camera> camera_;
	std::unique_ptr<DebugCamera> debugCamera_;
	std::unique_ptr<VfxLabBackgroundRenderer> backgroundRenderer_;
	std::unique_ptr<ProceduralFlameRenderer> proceduralFlame_;

	bool finished_ = false;
	std::string nextSceneName_ = "TITLE";
	float finalDeltaTime_ = 1.0f / 60.0f;

	BackgroundMode backgroundMode_ = BackgroundMode::Checker;
	float checkerScale_ = 14.0f;
	Vector4 checkerDarkColor_ = { 0.025f, 0.028f, 0.035f, 1.0f };
	Vector4 checkerLightColor_ = { 0.075f, 0.080f, 0.095f, 1.0f };

	float flameTime_ = 0.0f;
	bool showProceduralFlame_ = true;
	bool pauseProceduralFlame_ = false;
	Vector3 proceduralFlamePosition_ = { 0.0f, 0.0f, 0.0f };
	Vector2 proceduralFlameSize_ = { 8.0f, 16.0f };
	ProceduralFlameRenderer::Parameters proceduralFlameParameters_{};

	float cameraYaw_ = 0.0f;
	float cameraPitch_ = 0.04f;
	float cameraDistance_ = 27.0f;
	Vector3 cameraTarget_ = { 0.0f, 8.0f, 0.0f };
};
