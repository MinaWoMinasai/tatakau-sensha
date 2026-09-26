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

struct TankRewardPreviewAppearance {
    Vector4 playerColor{0.5f,1,0.35f,1},bodyFill{0.035f,0.055f,0.065f,1};
    Vector4 droneColor{0.24f,1,0.78f,0.95f},meleeColor{0.3f,1.35f,1.6f,0.95f};
    Vector4 gridColor{0.08f,0.18f,0.19f,0.5f},enemyColor{1.25f,0.24f,0.20f,1};
    float playerRadius=0.66f,lineWidth=0.05f,softEdgeRatio=0.42f,coreIntensity=1.35f;
    float bloomThreshold=0.0f,bloomIntensity=0.85f;
    tankneon::BladeStyle blade{};
    BulletTrailSettings bulletTrail{};
    int bodySegments=28;
    Vector2 bodyScale{1,1};
    bool separateCurrentBody=false;
    Vector4 currentPlayerColor{0.5f,1,0.35f,1};
    int currentBodySegments=28;
    Vector2 currentBodyScale{1,1};
};

// A small private scene cache. It owns its vertex/trail buffers and 4 small RTs;
// the only global GPU operation is recording commands on the existing list.
class TankRewardPreviewRenderer {
public:
    static constexpr uint32_t kWidth=512,kHeight=176;
    void Initialize(DirectXCommon* dx,SrvManager* srv);
    void SetAppearance(const TankRewardPreviewAppearance& appearance);
    void Update(const tankreward::DemoSnapshot& before,const tankreward::DemoSnapshot& after,
        bool compare,bool beforeReflect,bool afterReflect,bool melee,bool animate,float elapsed,bool force);
    // Called after the main post effects, before card sprites are drawn. The
    // caller must have one RTV bound (normally the back buffer), not an MRT set.
    // Restores RTV, DSV, viewport and scissor; subsequent draws bind their PSO.
    void Render();
    uint32_t GetSrvIndex()const{return output_?output_->GetSrvIndex():0;}
    bool HasRendered()const{return hasRendered_;}
    uint64_t GetRenderCount()const{return renderCount_;}
    uint32_t GetGeometryVertexCount()const{return neon_?neon_->GetVertexCount():0;}
private:
    void BuildGeometry();
    void QueueLane(int lane,const tankreward::DemoSnapshot& state,bool fill);
    Vector3 Position(int lane,tankreward::Point point)const;
    void PrepareTrails(int lane,const tankreward::DemoSnapshot& state,bool reflects);
    void Transition(RenderTexture& texture,D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after);
    void Filter(RenderTexture& source,RenderTexture& target,BlendMode blend);
    DirectXCommon* dx_=nullptr;SrvManager* srv_=nullptr;
    std::unique_ptr<RtvManager> rtv_;
    std::unique_ptr<RenderTexture> scene_,blurA_,blurB_,output_;
    std::unique_ptr<NeonGridRenderer> neon_;
    std::unique_ptr<TrailManager> trails_;
    std::array<TrailInstance*,12> trailInstances_{};
    std::unique_ptr<BloomConstantBuffer> bloom_;
    std::unique_ptr<PostEffect> post_;
    TankRewardPreviewAppearance appearance_{};
    tankreward::DemoSnapshot before_{},after_{};
    Matrix4x4 projection_{};
    uint32_t backgrounds_=0,fills_=0;
    std::array<uint32_t,2> laneStarts_{},laneCounts_{};
    uint64_t renderCount_=0;
    float elapsed_=0,previousElapsed_=-1;
    bool compare_=false,melee_=false,beforeReflect_=false,afterReflect_=false;
    bool pending_=false,hasRendered_=false,appearanceDirty_=true;
};
