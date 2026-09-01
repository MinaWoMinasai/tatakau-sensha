#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Camera.h"
#include "DebugCamera.h"
#include "Input.h"
#include "IScene.h"
#include "Object3d.h"

class UnderwaterLabScene : public IScene {
public:
	void Initialize() override;
	void Update() override;
	void Draw() override {}
	void DrawPostEffect3D() override;
	void DrawSprite() override {}

	bool IsFinished() const override { return finished_; }
	float GetFinalDeltaTime() const override { return finalDeltaTime_; }
	std::string GetNextSceneName() const override { return nextSceneName_; }

private:
	std::unique_ptr<Object3d> MakeObject(
		const std::string& modelName,
		const Vector3& translate,
		const Vector3& scale,
		const Vector4& color,
		bool lighting);
	void ApplyCausticsSettings();
	void AdvanceCausticsAnimation();
	void UpdateCausticsFrameState();
	void DrawDebugWindow();

	Input* input_ = nullptr;
	std::unique_ptr<Camera> camera_;
	std::unique_ptr<DebugCamera> debugCamera_;
	std::unique_ptr<Object3d> floor_;
	std::unique_ptr<Object3d> background_;
	std::vector<std::unique_ptr<Object3d>> depthBoxes_;

	bool finished_ = false;
	std::string nextSceneName_ = "TITLE";
	float finalDeltaTime_ = 1.0f / 60.0f;
	bool enableCaustics_ = true;
	float causticsScale_ = 0.035f;
	float causticsIntensity_ = 0.25f;
	Vector3 causticsColor_ = { 0.75f, 0.92f, 1.0f };
	bool causticsAnimationEnabled_ = true;
	float causticsPlaybackTime_ = 0.0f;
	float causticsLoopDuration_ = 4.0f;
	uint32_t causticsFrameCount_ = 24;
	uint32_t causticsAtlasColumns_ = 6;
	uint32_t causticsAtlasRows_ = 4;
	bool causticsFreezeFrame_ = false;
	int causticsManualFrameIndex_ = 0;
	uint32_t causticsCurrentFrame_ = 0;
	uint32_t causticsNextFrame_ = 1;
	float causticsFrameBlend_ = 0.0f;
};
