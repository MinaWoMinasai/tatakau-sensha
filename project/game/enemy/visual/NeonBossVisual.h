#pragma once
#include "NeonBossVisualState.h"
#include "Object3d.h"
#include "DirectX/engine/3d/neon/NeonSkinnedRenderer.h"
#include <memory>
#include <string>

struct NeonBossVisualInput {
    cg2::Vector3 position{};
    float aimAngleRadians = 0.0f;
    float radius = 2.0f;
    int hp = 1, maxHp = 1;
    float damageFeedback = 0.0f;
    bool alive = true, encounterActive = false, phaseTwo = false;
    NeonBossAction action = NeonBossAction::Idle;
};

struct NeonBossVisualStats {
    unsigned resourceCreates = 0, resourceReleases = 0, animationSelections = 0;
    unsigned modelUpdates = 0, draws = 0;
};

// Gameplay -> values -> presentation. Owns only rendering resources; its state
// never feeds HP, collision, AI or attack timing back into the gameplay layer.
class NeonBossVisual {
public:
    ~NeonBossVisual();
    void Initialize(cg2::Camera* camera, cg2::DebugCamera* debugCamera);
    // Reset only at a frame boundary after the previous GPU submission completed.
    void ResetEncounter();
    // Same frame-boundary contract as NeonSkinnedRenderer::BeginFrame; call once
    // after gameplay and Camera Update, before Scene HDR Draw.
    void Update(const NeonBossVisualInput& input, float deltaTime);
    void Draw();
    void SetEnabled(bool enabled) { enabled_ = enabled; }
    bool IsEnabled() const { return enabled_; }
    bool IsVisible() const;
    bool IsFinished() const { return state_.GetLife() == NeonBossVisualLife::Finished; }
    bool HasResources() const { return model_ != nullptr && object_ != nullptr && renderer_ != nullptr; }
    size_t GetDrawConstantBufferCount() const { return renderer_ ? renderer_->GetDrawConstantBufferCount() : 0; }
    const NeonBossVisualState& GetState() const { return state_; }
    const NeonBossVisualStats& GetStats() const { return stats_; }
    const std::string& GetStatus() const { return status_; }
    float GetDissolveProgress() const { return state_.GetDissolveProgress(); }
    std::string GetAnimationName() const;
    float GetAnimationTime() const;
    const cg2::Matrix4x4& GetWorldMatrix() const { return world_; }
    const cg2::NeonSkinnedParams& GetParams() const { return params_; }

private:
    bool EnsureResources();
    void ReleaseResources();
    void UpdatePlacement(const NeonBossVisualInput& input);
    void StartDeath();
    void UpdateAnimation(float deltaTime);
    cg2::Matrix4x4 GetCameraWorld() const;
    cg2::Vector3 GetCameraPosition() const;

    cg2::Camera* camera_ = nullptr;
    cg2::DebugCamera* debugCamera_ = nullptr;
    std::unique_ptr<cg2::SkinnedModel> model_;
    std::unique_ptr<cg2::Object3d> object_;
    std::unique_ptr<cg2::NeonSkinnedRenderer> renderer_;
    NeonBossVisualState state_;
    NeonBossVisualStats stats_;
    cg2::NeonSkinnedParams params_{};
    cg2::Vector3 modelCenter_{};
    float modelHeight_ = 1.6f;
    float lastAliveRadius_ = 2.0f;
    cg2::Matrix4x4 world_{};
    bool enabled_ = true, loadAttempted_ = false, encounterActive_ = false;
    std::string status_;
};
