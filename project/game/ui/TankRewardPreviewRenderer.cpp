#include "TankRewardPreviewRenderer.h"
#include "Calculation.h"
#include "StartupTrace.h"
#include <cmath>
#include <tuple>

namespace {
constexpr float kWorldWidth=18.0f;
constexpr float kWorldHeight=kWorldWidth*static_cast<float>(TankRewardPreviewRenderer::kHeight)/TankRewardPreviewRenderer::kWidth;
constexpr Vector3 kRight{1,0,0},kUp{0,1,0},kForward{0,0,1};
Vector4 Alpha(Vector4 c,float a){c.w*=a;return c;}
Vector3 Direction(float angle){return {std::cos(angle),std::sin(angle),0};}
bool Equal(Vector2 a,Vector2 b){return a.x==b.x&&a.y==b.y;}
bool Equal(Vector4 a,Vector4 b){return a.x==b.x&&a.y==b.y&&a.z==b.z&&a.w==b.w;}
bool Equal(const BulletTrailSettings& a,const BulletTrailSettings& b) {
    return std::tie(a.playerHalfWidth,a.enemyHalfWidth,a.lifetime,a.maxPoints,a.interpolationSteps,
        a.headWidthScale,a.tailWidthScale,a.widthCurvePower,a.colorCurvePower,a.useObjectColorForTrail,
        a.trailHeadIntensity,a.trailTailIntensity,a.trailHeadAlpha,a.trailTailAlpha,
        a.playerTrailLifetimeScale,a.playerTrailAlphaScale)==
        std::tie(b.playerHalfWidth,b.enemyHalfWidth,b.lifetime,b.maxPoints,b.interpolationSteps,
        b.headWidthScale,b.tailWidthScale,b.widthCurvePower,b.colorCurvePower,b.useObjectColorForTrail,
        b.trailHeadIntensity,b.trailTailIntensity,b.trailHeadAlpha,b.trailTailAlpha,
        b.playerTrailLifetimeScale,b.playerTrailAlphaScale)&&
        Equal(a.playerObjectColor,b.playerObjectColor)&&Equal(a.enemyObjectColor,b.enemyObjectColor)&&
        Equal(a.reflectableObjectColor,b.reflectableObjectColor)&&Equal(a.startColor,b.startColor)&&
        Equal(a.playerEndColor,b.playerEndColor)&&Equal(a.enemyEndColor,b.enemyEndColor)&&
        Equal(a.reflectableEndColor,b.reflectableEndColor);
}
bool Equal(const TankRewardPreviewAppearance& a,const TankRewardPreviewAppearance& b) {
    return Equal(a.playerColor,b.playerColor)&&Equal(a.bodyFill,b.bodyFill)&&Equal(a.droneColor,b.droneColor)&&
        Equal(a.meleeColor,b.meleeColor)&&Equal(a.gridColor,b.gridColor)&&Equal(a.enemyColor,b.enemyColor)&&
        Equal(a.bodyScale,b.bodyScale)&&Equal(a.currentPlayerColor,b.currentPlayerColor)&&
        Equal(a.currentBodyScale,b.currentBodyScale)&&Equal(a.bulletTrail,b.bulletTrail)&&
        std::tie(a.playerRadius,a.lineWidth,a.softEdgeRatio,a.coreIntensity,a.bloomThreshold,a.bloomIntensity,
        a.blade.outerWidth,a.blade.haloWidth,a.blade.coreWidth,a.bodySegments,a.separateCurrentBody,a.currentBodySegments)==
        std::tie(b.playerRadius,b.lineWidth,b.softEdgeRatio,b.coreIntensity,b.bloomThreshold,b.bloomIntensity,
        b.blade.outerWidth,b.blade.haloWidth,b.blade.coreWidth,b.bodySegments,b.separateCurrentBody,b.currentBodySegments);
}
}
void TankRewardPreviewRenderer::Initialize(DirectXCommon* dx,SrvManager* srv) {
    if(dx_)return;dx_=dx;srv_=srv;
    StartupTrace::Scope scope("RewardPreview.Initialize");
    rtv_=std::make_unique<RtvManager>();rtv_->Initialize(dx);
    auto make=[&](std::unique_ptr<RenderTexture>& rt,uint32_t w,uint32_t h,DXGI_FORMAT format) {
        rt=std::make_unique<RenderTexture>();rt->Initialize(dx,srv,rtv_.get(),w,h,{0,0,0,1},false,format);
    };
    make(scene_,kWidth,kHeight,DirectXCommon::kSceneRenderTargetFormat);
    make(blurA_,kWidth/2,kHeight/2,DirectXCommon::kSceneRenderTargetFormat);
    make(blurB_,kWidth/2,kHeight/2,DirectXCommon::kSceneRenderTargetFormat);
    make(output_,kWidth,kHeight,DirectXCommon::kBackBufferRenderTargetFormat);
    neon_=std::make_unique<NeonGridRenderer>();neon_->Initialize(dx,"resources/white512x512.png");
    trails_=std::make_unique<TrailManager>();trails_->Initialize(dx,Object3dCommon::GetInstance(),"resources/white512x512.png");
    for(auto& trail:trailInstances_){trail=trails_->CreateInstance();trail->SetIsPermanent(true);}
    bloom_=std::make_unique<BloomConstantBuffer>();bloom_->Initialize(dx);
    post_=std::make_unique<PostEffect>();post_->Initialize(dx,bloom_.get());
    projection_=MakeIdentity4x4();projection_.m[0][0]=2/kWorldWidth;projection_.m[1][1]=2/kWorldHeight;
    projection_.m[3][0]=-1;projection_.m[3][1]=-1;
}
void TankRewardPreviewRenderer::SetAppearance(const TankRewardPreviewAppearance& appearance) {
    if(Equal(appearance_,appearance))return;
    appearance_=appearance;appearanceDirty_=true;
}
void TankRewardPreviewRenderer::Update(const tankreward::DemoSnapshot& before,const tankreward::DemoSnapshot& after,
    bool compare,bool beforeReflect,bool afterReflect,bool melee,bool animate,float elapsed,bool force) {
    if(!dx_||(!animate&&!force&&!appearanceDirty_&&hasRendered_))return;
    before_=before;after_=after;compare_=compare;beforeReflect_=beforeReflect;afterReflect_=afterReflect;
    melee_=melee;elapsed_=elapsed;
    // Update owns all CPU-side geometry and configuration writes. Render only
    // records commands using this prepared, private scene snapshot.
    BuildGeometry();previousElapsed_=elapsed;pending_=true;appearanceDirty_=false;
    BloomParam param{};param.threshold=appearance_.bloomThreshold;param.intensity=appearance_.bloomIntensity;
    param.exposure=1;param.toneMappingMode=1;param.hdrWhitePoint=11.2f;
    bloom_->Update(param);
}
Vector3 TankRewardPreviewRenderer::Position(int lane,tankreward::Point p)const {
    const float width=compare_?kWorldWidth*0.5f:kWorldWidth;
    const float offset=compare_?static_cast<float>(lane)*width:0;
    return {offset+0.25f+p.x*(width-0.5f),0.3f+(1-p.y)*(kWorldHeight-0.6f),0.35f};
}
void TankRewardPreviewRenderer::BuildGeometry() {
    neon_->BeginFrame();neon_->SetLineStyle(appearance_.softEdgeRatio,appearance_.coreIntensity);
    for(auto* trail:trailInstances_)trail->Clear();
    const int lanes=compare_?2:1;
    const float width=kWorldWidth/static_cast<float>(lanes);
    for(int n=0;n<lanes;++n) {
        const float left=static_cast<float>(n)*width;
        neon_->QueueWorldGrid(left+0.15f,left+width-0.15f,0.15f,kWorldHeight-0.15f,0.90f,0.012f,appearance_.gridColor);
        neon_->QueueRectangle({left+width*0.5f,kWorldHeight*0.5f,0.35f},{width-0.22f,kWorldHeight-0.22f,0},0.04f,{0.1f,0.3f,0.31f,0.7f});
    }
    backgrounds_=neon_->GetVertexCount();
    if(compare_)QueueLane(0,before_,true);
    QueueLane(compare_?1:0,after_,true);fills_=neon_->GetVertexCount()-backgrounds_;
    laneStarts_.fill(0);laneCounts_.fill(0);
    if(compare_) {
        laneStarts_[0]=neon_->GetVertexCount();QueueLane(0,before_,false);
        laneCounts_[0]=neon_->GetVertexCount()-laneStarts_[0];PrepareTrails(0,before_,beforeReflect_);
    }
    const int last=compare_?1:0;
    laneStarts_[last]=neon_->GetVertexCount();QueueLane(last,after_,false);
    laneCounts_[last]=neon_->GetVertexCount()-laneStarts_[last];PrepareTrails(last,after_,afterReflect_);
}
void TankRewardPreviewRenderer::QueueLane(int lane,const tankreward::DemoSnapshot& s,bool fill) {
    const float radius=appearance_.playerRadius;
    const bool current=compare_&&lane==0&&appearance_.separateCurrentBody;
    const auto playerColor=current?appearance_.currentPlayerColor:appearance_.playerColor;
    const int bodySegments=current?appearance_.currentBodySegments:appearance_.bodySegments;
    const auto bodyScale=current?appearance_.currentBodyScale:appearance_.bodyScale;
    const auto player=Position(lane,s.player),cursor=Position(lane,s.cursor);
    const float aim=std::atan2(cursor.y-player.y,cursor.x-player.x);
    const float muzzle=s.muzzleFlash;
    auto tank=[&](Vector3 at,float r,float angle,Vector4 color,int barrels,int segments,Vector2 bodyScale) {
        if(fill) {neon_->QueueBillboardRegularPolygonFill(at,segments,r*0.96f,angle,bodyScale,appearance_.bodyFill,kRight,kUp);return;}
        tankneon::QueueBodyOutline(*neon_,at,r,appearance_.lineWidth,segments,angle,bodyScale,color,kRight,kUp);
        const auto forward=Direction(angle),right=Vector3{-forward.y,forward.x,0};
        for(int b=0;b<barrels;++b) {
            const float side=(static_cast<float>(b)-static_cast<float>(barrels-1)*0.5f)*0.48f*r;
            const auto center=at+forward*(0.86f*r)+right*side;
            const float length=r*0.90f,half=r*0.18f;
            const auto base=center-forward*(length*0.20f),tip=center+forward*(length*0.80f);
            tankneon::QueueBarrelOutline(*neon_,base,tip,right,half,appearance_.lineWidth,
                color,false);
            if(muzzle>0) {
                tankneon::QueueBodyOutline(*neon_,tip,r*0.18f,appearance_.lineWidth*0.7f,14,0,{1,1},
                    {1.35f,1.55f,0.75f,muzzle*0.42f},kRight,kUp);
                neon_->QueueLine(tip,tip+forward*r*0.24f,appearance_.lineWidth*1.2f,{2.2f,2.1f,1.65f,muzzle});
            }
        }
    };
    tank(player,radius,aim,playerColor,s.barrelCount,bodySegments,bodyScale);
    for(int i=0;i<s.droneCount;++i) {
        const auto pos=Position(lane,s.drones[i]);
        tank(pos,radius*0.61f,std::atan2(cursor.y-pos.y,cursor.x-pos.x),appearance_.droneColor,1,28,{1,1});
    }
    if(s.wall) {
        const auto start=Position(lane,{.35f,.18f}),end=Position(lane,{.71f,.18f});
        const auto center=(start+end)*0.5f;const float length=Length(end-start);
        if(fill)neon_->QueueBillboardRegularPolygonFill(center,4,1,.78539816f,{length*.70710678f,.32f},{.025f,.055f,.064f,1},kRight,kUp);
        else neon_->QueueBillboardRectangle(center,{length,.45f},0,.055f,{.2f,.65f,.75f,1},kRight,kUp,kForward);
    }
    for(int i=0;i<s.targetCount;++i) {
        const auto center=Position(lane,s.targets[i]);const float r=radius*0.72f;
        auto color=appearance_.enemyColor;color.y+=s.hit[i]*0.5f;color.z+=s.hit[i]*0.4f;
        if(fill)neon_->QueueBillboardRegularPolygonFill(center,4,r,0.78539816f,{1,1},{.13f,.035f,.025f,1},kRight,kUp);
        else {
            tankneon::QueueBodyOutline(*neon_,center,r,.055f,4,.78539816f,{1,1},color,kRight,kUp);
            neon_->QueueBillboardRectangle(center+Vector3{-.08f,.08f,0},{r*1.42f,r*1.42f},0,.024f,Alpha(color,.35f),kRight,kUp,kForward);
            const auto bar=center+Vector3{-r*.7f,r+0.2f,0};
            neon_->QueueLine(bar,bar+Vector3{r*1.4f,0,0},.065f,{.16f,.1f,.1f,1});
            neon_->QueueLine(bar,bar+Vector3{r*1.4f*(1-tankreward::Saturate(s.damage[i])),0,0},.05f,{1,.35f,.2f,1});
            if(s.hit[i]>0.01f)for(int part=0;part<5;++part) {
                const float a=static_cast<float>(part)*1.256637f;
                const auto pos=center+Direction(a)*(r+0.3f+(1-s.hit[i])*.25f);
                neon_->QueueBillboardTriangle(pos,.12f,a+elapsed_*3,.025f,Alpha(color,s.hit[i]),kRight,kUp,kForward);
            }
        }
    }
    if(fill)return;
    // The same idle blade and layered swing blade as the gameplay pass. These
    // are world-space neon vertices, not a sprite substitute for the actor.
    if(melee_&&s.barrelCount==0) {
        const float width=compare_?kWorldWidth*.5f:kWorldWidth;
        const float length=s.slashing?s.slashReach*(width-.5f):radius*1.45f;
        const float angle=s.slashing?-s.slashAngle:aim+0.75f;
        const auto hilt=player+Direction(aim)*radius*.42f;
        const auto color=appearance_.meleeColor;
        if(s.slashing) {
            for(int band=0;band<3;++band) {
                const float range=length*(1-static_cast<float>(band)*.09f);
                const float span=(std::min)(s.slashArc*.32f,.85f);
                Vector3 previous=hilt+Direction(angle-span)*range;
                for(int segment=1;segment<=12;++segment) {
                    const float p=static_cast<float>(segment)/12;
                    const auto current=hilt+Direction(angle-span+span*p)*range;
                    neon_->QueueLine(previous,current,.055f*(1-static_cast<float>(band)*.15f),Alpha(color,p*(.72f-static_cast<float>(band)*.18f)));
                    previous=current;
                }
            }
            for(int copy=3;copy>0;--copy) tankneon::QueueMeleeBlade(*neon_,hilt,Direction(angle-static_cast<float>(copy)*.12f),
                length*(1-.03f*copy),.065f,Alpha(color,.08f*(4-copy)),.20f,appearance_.blade);
        }
        tankneon::QueueMeleeBlade(*neon_,hilt,Direction(angle),length,s.comboStep==2?.13f:.10f,color,.94f,appearance_.blade);
    }
    if(s.dashing)for(int i=1;i<4;++i)tankneon::QueueBodyOutline(*neon_,player-Vector3{static_cast<float>(i)*.3f,0,0},
        radius,.035f,bodySegments,0,bodyScale,Alpha(playerColor,.13f*(4-i)),kRight,kUp);
    // A small aiming cross uses the same scene scale; no real cursor is moved.
    neon_->QueueLine(cursor-Vector3{.09f,0,0},cursor+Vector3{.09f,0,0},.018f,{.5f,.7f,.7f,.45f});
    neon_->QueueLine(cursor-Vector3{0,.09f,0},cursor+Vector3{0,.09f,0},.018f,{.5f,.7f,.7f,.45f});
}
void TankRewardPreviewRenderer::PrepareTrails(int lane,const tankreward::DemoSnapshot& s,bool reflects) {
    const auto& source=appearance_.bulletTrail;
    const auto color=reflects?source.reflectableObjectColor:source.playerObjectColor;
    TrailConfig config{};
    config.startColor=source.useObjectColorForTrail?Vector4{color.x*source.trailHeadIntensity,color.y*source.trailHeadIntensity,color.z*source.trailHeadIntensity,source.trailHeadAlpha*source.playerTrailAlphaScale}:source.startColor;
    config.endColor=source.useObjectColorForTrail?Vector4{color.x*source.trailTailIntensity,color.y*source.trailTailIntensity,color.z*source.trailTailIntensity,source.trailTailAlpha}:reflects?source.reflectableEndColor:source.playerEndColor;
    config.startWidthScale=source.headWidthScale;config.endWidthScale=source.tailWidthScale;
    config.widthCurvePower=source.widthCurvePower;config.colorCurvePower=source.colorCurvePower;
    config.interpolationSteps=static_cast<uint32_t>((std::max)(1,source.interpolationSteps));config.maxPoints=12;config.lifetime=1;
    for(std::size_t i=0;i<s.bullets.size();++i) {
        const auto& bullet=s.bullets[i];auto* trail=trailInstances_[static_cast<std::size_t>(lane)*6+i];
        if(!bullet.visible)continue;
        const auto right=Vector3{std::sin(bullet.angle),std::cos(bullet.angle),0}*source.playerHalfWidth;
        for(std::size_t p=0;p<bullet.trailCount;++p) {
            const auto pos=Position(lane,bullet.trail[p]);trail->Update(0,pos+right,pos-right,config);
        }
        const auto head=Position(lane,bullet.position);trail->Update(0,head+right,head-right,config);
    }
}
void TankRewardPreviewRenderer::Transition(RenderTexture& rt,D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource=rt.GetResource();barrier.Transition.StateBefore=before;barrier.Transition.StateAfter=after;
    barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;dx_->GetList()->ResourceBarrier(1,&barrier);
}
void TankRewardPreviewRenderer::Filter(RenderTexture& source,RenderTexture& target,BlendMode blend) {
    Transition(target,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);
    dx_->SetRenderTargetNoDepth(target.GetRTVHandle());dx_->SetViewport(kWidth/2,kHeight/2);
    post_->Draw(source.GetGPUHandle(),blend,true);
    Transition(target,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}
void TankRewardPreviewRenderer::Render() {
    if(!pending_||!dx_)return;
    const auto savedRtv=dx_->GetCurrentRTVHandle(),savedDsv=dx_->GetCurrentDSVHandle();const bool hadDepth=dx_->HasCurrentDSV();
    const auto savedViewport=dx_->GetViewportRect();const auto savedScissor=dx_->GetSissorRect();
    srv_->PreDraw();
    Transition(*scene_,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);
    dx_->SetRenderTargetNoDepth(scene_->GetRTVHandle());dx_->SetViewport(kWidth,kHeight);
    const float clear[]{.010f,.018f,.024f,1};dx_->ClearRenderTarget(scene_->GetRTVHandle(),clear);
    neon_->DrawRange(0,backgrounds_,projection_);
    // DrawRangeSolid binds the engine's LDR HUD PSO. This target is HDR, so
    // fills use the scene-compatible neon PSO, like the outlines and trails.
    neon_->DrawRange(backgrounds_,fills_,projection_);
    for(int lane=0;lane<(compare_?2:1);++lane) {
        const LONG width=static_cast<LONG>(compare_?kWidth/2:kWidth);
        const D3D12_RECT crop{lane*width,0,(lane+1)*width,static_cast<LONG>(kHeight)};
        dx_->GetList()->RSSetScissorRects(1,&crop);
        neon_->DrawRange(laneStarts_[lane],laneCounts_[lane],projection_);
    }
    const D3D12_RECT full{0,0,static_cast<LONG>(kWidth),static_cast<LONG>(kHeight)};
    dx_->GetList()->RSSetScissorRects(1,&full);
    trails_->DrawAll(projection_);
    Transition(*scene_,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    Filter(*scene_,*blurA_,kAdd_Bloom_Extract);Filter(*blurA_,*blurB_,kAdd_Bloom_BlurH);Filter(*blurB_,*blurA_,kAdd_Bloom_BlurV);
    Transition(*output_,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);
    dx_->SetRenderTargetNoDepth(output_->GetRTVHandle());dx_->SetViewport(kWidth,kHeight);
    dx_->ClearRenderTarget(output_->GetRTVHandle(),clear);
    post_->DrawObjectComposite(scene_->GetGPUHandle(),blurA_->GetGPUHandle(),false);
    post_->DrawObjectBloomAdd(blurA_->GetGPUHandle(),false);
    Transition(*output_,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    if(hadDepth)dx_->SetRenderTarget(savedRtv,savedDsv);else dx_->SetRenderTargetNoDepth(savedRtv);
    // SetViewport restores the engine's tracked values; explicit bindings also
    // preserve a caller's non-default crop rectangle and viewport origin.
    dx_->SetViewport(static_cast<uint32_t>(savedViewport.Width),static_cast<uint32_t>(savedViewport.Height));
    dx_->GetList()->RSSetViewports(1,&savedViewport);dx_->GetList()->RSSetScissorRects(1,&savedScissor);
    pending_=false;hasRendered_=true;++renderCount_;
}
