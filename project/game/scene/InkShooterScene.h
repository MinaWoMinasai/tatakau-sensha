#pragma once
#include "IScene.h"
#include "Camera.h"
#include "Object3d.h"
#include "Input.h"
#include "TextLabel.h"
#include "NeonGridRenderer.h"
#include "game/ink/InkSimulation.h"
#include "game/ink/InkPaintRenderer.h"
#include "game/ink/InkLiquidRenderer.h"
#include "game/ink/InkReticleRenderer.h"
#include "game/ink/WeaponCatalog.h"
#include "game/ink/InkAudioDirector.h"
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
    void UpdateWeaponVisuals();
    void UpdateHud();
    void DrawDebug();
    void InitializeWeapons();
    void DrawWeaponEditor();
    bool SelectWeaponDraft(const std::string& id);
    bool CommitWeaponDraft();
    void EquipCatalogWeapon(const std::string& id);
    void CycleWeapon();
    void Reset();
    void StartReplay();
    void WriteGpuDiagnostics();
    void UpdateVisuals(float dt);
    void RequestCapture(const std::string& name);
    void CopyCapture();
    void FinishCapture();
    ink::Controls ReadControls(float dt);
    ink::Controls ReadValidationControls(float dt);
    ink::Controls ReadFidelityControls(float dt);
    ink::Controls ReadWeaponsValidationControls(float dt);
    ink::Controls ReadFeelValidationControls(float dt);
    std::unique_ptr<Object3d> MakeObject(const char* model, Vector4 color);
    ink::Simulation simulation_;
    InkPaintRenderer paint_;
    InkLiquidRenderer liquid_;
    InkReticleRenderer reticle_;
    InkAudioDirector audio_;
    ink::WeaponCatalog weaponCatalog_;
    ink::WeaponDefinition weaponDraft_;
    std::string weaponStatus_,selectedWeaponId_;
    std::filesystem::path weaponCatalogPath_="resources/configs/ink_weapons.json";
    bool weaponDraftDirty_=false,weaponCatalogDirty_=false;
    struct VisualBurst {
        ink::Vec3 position, velocity, normal={0,1,0};
        float age=0, lifetime=0.25f, radius=0.08f;
        bool ripple=false;
    };
    std::vector<VisualBurst> bursts_;
    std::unique_ptr<Camera> camera_;
    std::array<std::unique_ptr<Object3d>, 12> actor_;
    std::array<std::unique_ptr<Object3d>, 3> dummy_;
    std::unique_ptr<NeonGridRenderer> effects_;
    std::array<std::unique_ptr<Sprite>, 8> hud_;
    std::unique_ptr<TextLabel> title_, guide_, stateText_, dummyText_,weaponText_;
    std::array<std::unique_ptr<Object3d>,5> bow_;
    Input* input_=nullptr;
    ink::Controls controls_;
    ink::Vec3 aimPoint_;
    Vector3 cameraTarget_{};
    std::chrono::steady_clock::time_point lastTime_;
    float yaw_=0, pitch_=0.16f, accumulator_=0, elapsed_=0, fps_=60;
    float sensitivity_=0.0023f, cameraDistance_=4.9f, replayTime_=0, logTime_=0;
    float cameraHeight_=1.35f, shoulderOffset_=0.48f, fovY_=1.02f, followSharpness_=22;
    float visualKick_=0, formAge_=10, wakeTime_=0, dummyTextTime_=0;
    bool wetEdges_=true;
    uint64_t visualShotCount_=0;
    ink::PlayerState visualState_=ink::PlayerState::Human;
    int replaySection_=-1;
    bool extendedReplay_=false;
    bool fidelityReplay_=false;
    bool weaponsReplay_=false;
    bool feelReplay_=false;
    bool invertPitch_=false, finished_=false, captured_=false, debug_=false;
    bool cameraReady_=false, replay_=false, jumpPending_=false;
    bool previousDebugUi_=true;
    uint64_t gpuMessageStart_=0;
    std::ofstream replayLog_;
    Microsoft::WRL::ComPtr<ID3D12Resource> captureReadback_;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT captureLayout_{};
    std::string capturePath_;
    bool captureCopied_=false;
};
