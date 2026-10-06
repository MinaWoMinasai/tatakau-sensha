#pragma once
#include "NeonBossVisualState.h"
#include "NeonDepthPresentation.h"
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
    bool depthEncounter = false;
    neondepth::Snapshot depth{};
};

struct NeonBossVisualStats {
    unsigned resourceCreates = 0, resourceReleases = 0, animationSelections = 0;
    unsigned modelUpdates = 0, draws = 0;
    unsigned depthPoseFreezes = 0, depthFormationBounds = 0, depthFallbackLoads = 0;
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
    // Valid edits apply at reset or the next attack instance; death timing is latched.
    bool SetDepthProfile(const neondepth::PresentationConfig& profile);
    const neondepth::PresentationConfig& GetDepthProfile() const { return depthProfile_; }
    bool GetDepthSocketWorld(neondepth::Socket socket, cg2::Vector3& output) const;
    neondepth::Life GetDepthLife() const { return state_.GetDepthLifecycle().GetLife(); }
    neondepth::Formation GetDepthFormation() const { return depthFormation_; }
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
    bool EnsureDepthResources();
    void UpdateDepth(const NeonBossVisualInput& input, float permittedDelta);
    bool ApplyDepthMotion(const neondepth::MotionCommand& command);
    bool UpdateDepthPlacement(const NeonBossVisualInput& input, const neondepth::BodyMotion& motion);
    void StartDepthCollapse(const NeonBossVisualInput& input);
    void UpdateDepthCollapse(float collapseProgress);
    void UpdateDepthFormation();
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
    neondepth::PresentationConfig depthProfile_{}, pendingDepthProfile_{}, deathDepthProfile_{};
    neondepth::BindPlacement depthBind_{};
    neondepth::PlacementInput depthPlacement_{}, deathDepthPlacement_{};
    neondepth::MotionGate depthMotionGate_{};
    neondepth::Formation depthFormation_{0, 0, 0};
    cg2::NeonDissolveParams depthFormationDissolve_{};
    uint64_t depthProfileInstance_ = 0;
    bool depthMode_ = false, depthModeLatched_ = false, depthProfilePending_ = false;
    bool depthPlacementValid_ = false, depthPoseFrozen_ = false;
    bool depthFormationBoundsReady_ = false, depthBodySuppressed_ = false;
};
