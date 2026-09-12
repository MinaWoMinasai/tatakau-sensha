#pragma once
#include "IScene.h"
#include "Camera.h"
#include "Object3d.h"
#include "Input.h"
#include "TextLabel.h"
#include "NeonGridRenderer.h"
#include "game/ink/InkSimulation.h"
#include "game/ink/InkPaintRenderer.h"
#include <array>
#include <chrono>
#include <fstream>
#include <memory>

class InkShooterScene : public IScene {
public:
    ~InkShooterScene() override;
    void Initialize() override;
    void Update() override;
    void Draw() override {}
    void DrawPostEffect3D() override;
    void DrawSprite() override;
    bool IsFinished() const override { return finished_; }
    std::string GetNextSceneName() const override { return "TITLE"; }
    // The engine interprets values below 1/60 as a slow-motion/grayscale request.
    float GetFinalDeltaTime() const override { return 1.0f / 60.0f; }
    ScreenEffectState GetScreenEffectState() const override {
        ScreenEffectState state{}; state.suppressPostEffectDebugUi=true; return state;
    }
private:
    void SetCaptured(bool captured);
    void UpdateCamera(float dt);
    void UpdateModels();
    void UpdateHud();
    void DrawDebug();
    void Reset();
    void StartReplay();
    void WriteGpuDiagnostics();
    ink::Controls ReadControls(float dt);
    std::unique_ptr<Object3d> MakeObject(const char* model, Vector4 color);
    ink::Simulation simulation_;
    InkPaintRenderer paint_;
    std::unique_ptr<Camera> camera_;
    std::array<std::unique_ptr<Object3d>, 10> actor_;
    std::unique_ptr<NeonGridRenderer> effects_;
    std::array<std::unique_ptr<Sprite>, 8> hud_;
    std::unique_ptr<TextLabel> title_, guide_, stateText_;
    Input* input_=nullptr;
    ink::Controls controls_;
    ink::Vec3 aimPoint_;
    Vector3 cameraTarget_{};
    std::chrono::steady_clock::time_point lastTime_;
    float yaw_=0, pitch_=0.16f, accumulator_=0, elapsed_=0, fps_=60;
    float sensitivity_=0.0023f, cameraDistance_=4.6f, replayTime_=0, logTime_=0;
    bool invertPitch_=false, finished_=false, captured_=false, debug_=false;
    bool cameraReady_=false, replay_=false, jumpPending_=false;
    bool previousDebugUi_=true;
    uint64_t gpuMessageStart_=0;
    std::ofstream replayLog_;
};
