#pragma once
#include <string>

class IScene {
public:
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

    // シーン終了判定（SceneManagerがチェックする）
    virtual bool IsFinished() const = 0;

    virtual std::string GetNextSceneName() const { return ""; }
};
