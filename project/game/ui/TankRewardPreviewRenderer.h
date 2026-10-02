#pragma once
#include <array>
#include <memory>
#include "RenderTexture.h"
#include "NeonGridRenderer.h"
#include "PostEffect.h"
#include "TrailManager.h"
#include "game/player/actor/Bullet.h"
#include "TankCombatNeonGeometry.h"
#include "TankRewardCardDemo.h"

/// @brief 報酬プレビューに表示する機体と攻撃の外観を指定する。
struct TankRewardPreviewAppearance {
    cg2::Vector4 playerColor{0.5f, 1, 0.35f, 1}, bodyFill{0.035f, 0.055f, 0.065f, 1};
    cg2::Vector4 droneColor{0.24f, 1, 0.78f, 0.95f}, meleeColor{0.3f, 1.35f, 1.6f, 0.95f};
    cg2::Vector4 gridColor{0.08f, 0.18f, 0.19f, 0.5f}, enemyColor{1.25f, 0.24f, 0.20f, 1};
    float playerRadius = 0.66f, lineWidth = 0.05f, softEdgeRatio = 0.42f, coreIntensity = 1.35f;
    float bloomThreshold = 0.0f, bloomIntensity = 0.85f;
    tankneon::BladeStyle blade{};
    BulletTrailSettings bulletTrail{};
    int bodySegments = 28;
    cg2::Vector2 bodyScale{1, 1};
    bool separateCurrentBody = false;
    cg2::Vector4 currentPlayerColor{0.5f, 1, 0.35f, 1};
    int currentBodySegments = 28;
    cg2::Vector2 currentBodyScale{1, 1};
};

// A small private scene cache. It owns its vertex/trail buffers and 4 small RTs;
// the only global GPU operation is recording commands on the existing list.
/// @brief 報酬の効果を機体・弾・軌跡の短いデモとして描画する。
class TankRewardPreviewRenderer {
public:
    static constexpr uint32_t kWidth = 512, kHeight = 176;
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(cg2::DirectXCommon* dx, cg2::SrvManager* srv);
    /// @brief 外観を設定する。
    void SetAppearance(const TankRewardPreviewAppearance& appearance);
    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    void Update(const tankreward::DemoSnapshot& before, const tankreward::DemoSnapshot& after, bool compare, bool beforeReflect,
                bool afterReflect, bool melee, bool animate, float elapsed, bool force);
    // Called after the main post effects, before card sprites are drawn. The
    // caller must have one RTV bound (normally the back buffer), not an MRT set.
    // Restores RTV, DSV, viewport and scissor; subsequent draws bind their PSO.
    /// @brief 準備したデモの現在状態を描画する。
    void Render();
    /// @brief SRV添字を返す。
    uint32_t GetSrvIndex() const
    {
        return output_ ? output_->GetSrvIndex() : 0;
    }
    /// @brief Renderedが存在するか判定する。
    bool HasRendered() const
    {
        return hasRendered_;
    }
    /// @brief 描画件数を返す。
    uint64_t GetRenderCount() const
    {
        return renderCount_;
    }
    /// @brief 形状頂点件数を返す。
    uint32_t GetGeometryVertexCount() const
    {
        return neon_ ? neon_->GetVertexCount() : 0;
    }

private:
    /// @brief 形状を組み立てる。
    void BuildGeometry();
    /// @brief Laneを後で処理するために予約する。
    void QueueLane(int lane, const tankreward::DemoSnapshot& state, bool fill);
    /// @brief 指定対象の表示位置を求める。
    cg2::Vector3 Position(int lane, tankreward::Point point) const;
    /// @brief 軌跡を利用前に準備する。
    void PrepareTrails(int lane, const tankreward::DemoSnapshot& state, bool reflects);
    /// @brief GPUリソースを次の利用に必要な状態へ遷移させる。
    void Transition(cg2::RenderTexture& texture, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);
    /// @brief 対象一覧から条件を満たすものを選ぶ。
    void Filter(cg2::RenderTexture& source, cg2::RenderTexture& target, cg2::BlendMode blend);
    cg2::DirectXCommon* dx_ = nullptr;
    cg2::SrvManager* srv_ = nullptr;
    std::unique_ptr<cg2::RtvManager> rtv_;
    std::unique_ptr<cg2::RenderTexture> scene_, blurA_, blurB_, output_;
    std::unique_ptr<cg2::NeonGridRenderer> neon_;
    std::unique_ptr<cg2::TrailManager> trails_;
    std::array<cg2::TrailInstance*, 12> trailInstances_{};
    std::unique_ptr<cg2::BloomConstantBuffer> bloom_;
    std::unique_ptr<cg2::PostEffect> post_;
    TankRewardPreviewAppearance appearance_{};
    tankreward::DemoSnapshot before_{}, after_{};
    cg2::Matrix4x4 projection_{};
    uint32_t backgrounds_ = 0, fills_ = 0;
    std::array<uint32_t, 2> laneStarts_{}, laneCounts_{};
    uint64_t renderCount_ = 0;
    float elapsed_ = 0, previousElapsed_ = -1;
    bool compare_ = false, melee_ = false, beforeReflect_ = false, afterReflect_ = false;
    bool pending_ = false, hasRendered_ = false, appearanceDirty_ = true;
};
