#pragma once
#include <string>
#include "Struct.h"

class IScene {
public:
	struct PostEffectPulse {
		float bloomBoost = 0.0f;
		float chromAbAmount = 0.0f;
		Vector2 center = { 0.5f, 0.5f };
		float radius = 0.0f;
		float width = 0.05f;
		float strength = 0.0f;
	};
    struct RenderProfile {
		float frameTotalMs = 0.0f;
		float messagePumpMs = 0.0f;
		float inputImGuiBeginMs = 0.0f;
		float engineUpdateMs = 0.0f;
		float sceneUpdateMs = 0.0f;
		float imguiBuildMs = 0.0f;
		float drawSetupMs = 0.0f;
		float drawRecordMs = 0.0f;
        float scenePostMs = 0.0f;
        float globalBloomMs = 0.0f;
        float afterPostMs = 0.0f;
        float spriteMs = 0.0f;
		float imguiDrawMs = 0.0f;
		float postDrawMs = 0.0f;
		float submitCloseMs = 0.0f;
		float submitExecuteMs = 0.0f;
		float presentMs = 0.0f;
		float fenceWaitMs = 0.0f;
		float fpsLimitMs = 0.0f;
		float submitResetMs = 0.0f;
    };

    virtual ~IScene() = default;
    virtual void Initialize() = 0;
    virtual void Update() = 0;
    virtual void Draw() = 0;
    virtual void DrawShadow() {}
    virtual void DrawPostEffect3D() {}
    virtual void DrawAfterPostEffect3D() {}
    virtual void DrawSprite() = 0;
    virtual float GetFinalDeltaTime() const { return 1.0f / 60.0f; } // デフォルトは60FPS 
    virtual float GetPostGaussianIntensity() const { return 0.0f; }
    virtual PostEffectPulse GetPostEffectPulse() const { return {}; }
    virtual void SetRenderProfile(const RenderProfile& profile) { (void)profile; }

    // シーン終了判定（SceneManagerがチェックする）
    virtual bool IsFinished() const = 0;

    virtual std::string GetNextSceneName() const { return ""; }
};
