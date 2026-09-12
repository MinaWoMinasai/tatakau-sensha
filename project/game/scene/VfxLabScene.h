#pragma once

#include <chrono>
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
	void RestartFlame();

	Input* input_ = nullptr;
	std::unique_ptr<Camera> camera_;
	std::unique_ptr<DebugCamera> debugCamera_;
	std::unique_ptr<VfxLabBackgroundRenderer> backgroundRenderer_;
	std::unique_ptr<ProceduralFlameRenderer> proceduralFlame_;

	bool finished_ = false;
	std::string nextSceneName_ = "TITLE";
	// The main loop also interprets this as the gameplay slow-motion flag.
	// Keep the effect's measured animation time separate from that contract.
	float finalDeltaTime_ = 1.0f / 60.0f;
	float frameDeltaTime_ = 1.0f / 60.0f;
	std::chrono::steady_clock::time_point lastFrameTime_{};

	BackgroundMode backgroundMode_ = BackgroundMode::Checker;
	float checkerScale_ = 12.0f;
	Vector4 checkerDarkColor_ = { 0.026f, 0.026f, 0.028f, 1.0f };
	Vector4 checkerLightColor_ = { 0.038f, 0.038f, 0.040f, 1.0f };

	float flameTime_ = 0.0f;
	bool showProceduralFlame_ = true;
	bool pauseProceduralFlame_ = false;
	bool showControls_ = true;
	bool restartRequested_ = false;
	Vector3 proceduralFlamePosition_ = { 0.0f, 0.0f, 0.0f };
	Vector2 proceduralFlameSize_ = { 8.0f, 16.0f };
	ProceduralFlameRenderer::Parameters proceduralFlameParameters_{};

	float cameraYaw_ = 0.0f;
	float cameraPitch_ = 0.04f;
	float cameraDistance_ = 42.0f;
	Vector3 cameraTarget_ = { 0.0f, 8.0f, 0.0f };
};
