#pragma once

class IScene {
public:
    virtual ~IScene() = default;
    virtual void Initialize() = 0;
    virtual void Update() = 0;
    virtual void Draw() = 0;
    virtual void DrawShadow() {}
    virtual void DrawPostEffect3D() {}
    virtual void DrawSprite() = 0;
	virtual float GetFinalDeltaTime() const { return 1.0f / 60.0f; } // デフォルトは60FPS 

    // シーン終了判定（SceneManagerがチェックする）
    virtual bool IsFinished() const = 0;
};
