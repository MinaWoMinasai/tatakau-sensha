#pragma once
#include "NeonBossVisualState.h"
#include "NeonDepthPresentation.h"
#include "Object3d.h"
#include "game/render/neon/NeonSkinnedRenderer.h"
#include <memory>
#include <string>

/// @brief ボスの見た目を更新するために戦闘側から渡す値。
struct NeonBossVisualInput {
    myEngine::Vector3 position{};
    float aimAngleRadians = 0.0f;
    float radius = 2.0f;
    int hp = 1, maxHp = 1;
    float damageFeedback = 0.0f;
    bool alive = true, encounterActive = false, phaseTwo = false;
    NeonBossAction action = NeonBossAction::Idle;
    bool depthEncounter = false;
    neondepth::Snapshot depth{};
};

/// @brief ボスの見た目の処理回数。検証で使う。
struct NeonBossVisualStats {
    unsigned resourceCreates = 0, resourceReleases = 0, animationSelections = 0;
    unsigned modelUpdates = 0, draws = 0;
    unsigned depthPoseFreezes = 0, depthFormationBounds = 0, depthFallbackLoads = 0;
};

// ゲームプレイ -> 値 -> 演出。所有するのは描画リソースだけで、その状態がHP、
// 当たり判定、AI、攻撃タイミングとしてゲームプレイ層へ戻ることはない。
class NeonBossVisual {
public:
    ~NeonBossVisual();
    /// @brief カメラを受け取り、描画の準備をする。
    void Initialize(myEngine::Camera* camera, myEngine::DebugCamera* debugCamera);
    // 前回のGPU送信が完了した後の、フレーム境界でだけリセットする。
    void ResetEncounter();
    // NeonSkinnedRenderer::BeginFrameと同じフレーム境界の約束事。
    // ゲームプレイとCameraのUpdateの後、シーンのHDR描画の前に1回呼ぶ。
    void Update(const NeonBossVisualInput& input, float deltaTime);
    // 有効な編集はリセット時か次の攻撃インスタンスで反映する。死亡のタイミングは確定済み。
    bool SetDepthProfile(const neondepth::PresentationConfig& profile);

    const neondepth::PresentationConfig& GetDepthProfile() const
    {
        return depthProfile_;
    }

    bool GetDepthSocketWorld(neondepth::Socket socket, myEngine::Vector3& output) const;

    neondepth::Life GetDepthLife() const
    {
        return state_.GetDepthLifecycle().GetLife();
    }

    neondepth::Formation GetDepthFormation() const
    {
        return depthFormation_;
    }

    /// @brief ボスのモデルを描画する。
    void Draw();

    void SetEnabled(bool enabled)
    {
        enabled_ = enabled;
    }

    /// @brief 奥行きのない遭遇戦で、宿敵の頭を照準へ回さず、常に画面の上へ向けて置くか（育つ戦車の試作。立ち姿に見せる）。
    /// @param heightScale そのときの大きさの倍率（既定の大きさに掛ける）。
    /// @note 既定は頭を照準へ向ける置き方（既存の遠征）。
    void SetUpright(bool upright, float heightScale = 1.0f)
    {
        upright_ = upright;
        uprightHeightScale_ = heightScale;
    }

    bool IsEnabled() const
    {
        return enabled_;
    }

    bool IsVisible() const;

    bool IsFinished() const
    {
        return state_.GetLife() == NeonBossVisualLife::Finished;
    }

    bool HasResources() const
    {
        return model_ != nullptr && object_ != nullptr && renderer_ != nullptr;
    }

    size_t GetDrawConstantBufferCount() const
    {
        return renderer_ ? renderer_->GetDrawConstantBufferCount() : 0;
    }

    const NeonBossVisualState& GetState() const
    {
        return state_;
    }

    const NeonBossVisualStats& GetStats() const
    {
        return stats_;
    }

    const std::string& GetStatus() const
    {
        return status_;
    }

    float GetDissolveProgress() const
    {
        return state_.GetDissolveProgress();
    }

    std::string GetAnimationName() const;
    float GetAnimationTime() const;

    const myEngine::Matrix4x4& GetWorldMatrix() const
    {
        return world_;
    }

    const myEngine::NeonSkinnedParams& GetParams() const
    {
        return params_;
    }

private:
    /// @brief 通常ボス用のモデルと描画リソースを用意する。成功したらtrueを返す。
    bool EnsureResources();
    /// @brief Depthボス用のモデルと描画リソースを用意する。成功したらtrueを返す。
    bool EnsureDepthResources();
    /// @brief Depthボスの見た目を更新する。
    void UpdateDepth(const NeonBossVisualInput& input, float permittedDelta);
    /// @brief Depthボスのアニメーション指示を適用する。
    bool ApplyDepthMotion(const neondepth::MotionCommand& command);
    /// @brief Depthボスの床上の配置と姿勢を更新する。
    bool UpdateDepthPlacement(const NeonBossVisualInput& input, const neondepth::BodyMotion& motion);
    /// @brief Depthボスの崩壊演出を開始する。
    void StartDepthCollapse(const NeonBossVisualInput& input);
    /// @brief Depthボスの崩壊演出を進める。
    void UpdateDepthCollapse(float collapseProgress);
    /// @brief Depthボスの登場時の形成演出を進める。
    void UpdateDepthFormation();
    /// @brief 描画リソースを解放する。
    void ReleaseResources();
    /// @brief 通常ボスの位置と向きを更新する。
    void UpdatePlacement(const NeonBossVisualInput& input);
    /// @brief 死亡演出を開始する。
    void StartDeath();
    /// @brief アニメーションを進める。
    void UpdateAnimation(float deltaTime);
    myEngine::Matrix4x4 GetCameraWorld() const;
    myEngine::Vector3 GetCameraPosition() const;

    myEngine::Camera* camera_ = nullptr;
    myEngine::DebugCamera* debugCamera_ = nullptr;
    std::unique_ptr<myEngine::SkinnedModel> model_;
    std::unique_ptr<myEngine::Object3d> object_;
    std::unique_ptr<myEngine::NeonSkinnedRenderer> renderer_;
    NeonBossVisualState state_;
    NeonBossVisualStats stats_;
    myEngine::NeonSkinnedParams params_{};
    myEngine::Vector3 modelCenter_{};
    float modelHeight_ = 1.6f;
    float lastAliveRadius_ = 2.0f;
    bool upright_ = false;
    float uprightHeightScale_ = 1.0f;
    myEngine::Matrix4x4 world_{};
    bool enabled_ = true, loadAttempted_ = false, encounterActive_ = false;
    std::string status_;
    neondepth::PresentationConfig depthProfile_{}, pendingDepthProfile_{}, deathDepthProfile_{};
    neondepth::BindPlacement depthBind_{};
    neondepth::PlacementInput depthPlacement_{}, deathDepthPlacement_{};
    neondepth::MotionGate depthMotionGate_{};
    neondepth::Formation depthFormation_{0, 0, 0};
    myEngine::NeonDissolveParams depthFormationDissolve_{};
    uint64_t depthProfileInstance_ = 0;
    bool depthMode_ = false, depthModeLatched_ = false, depthProfilePending_ = false;
    bool depthPlacementValid_ = false, depthPoseFrozen_ = false;
    bool depthFormationBoundsReady_ = false, depthBodySuppressed_ = false;
};
