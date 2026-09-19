#include "InkShooterScene.h"
#include "Object3dCommon.h"
#include "ModelManager.h"
#include "WinApp.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <cstdio>
#include <d3d12sdklayers.h>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
Vector3 V(ink::Vec3 v) { return {v.x,v.y,v.z}; }
ink::Vec3 I(Vector3 v) { return {v.x,v.y,v.z}; }
const char* StateName(ink::PlayerState s) {
    return s==ink::PlayerState::WallSwim ? "WALL SWIM" : s==ink::PlayerState::Swim ? "SWIM" : s==ink::PlayerState::Squid ? "SQUID" : "HUMAN";
}
constexpr Vector4 kInk={0.025f,0.86f,0.68f,1};
constexpr Vector4 kLiquid={0.004f,0.39f,0.235f,1};
const char* JapaneseState(ink::PlayerState state) {
    return state==ink::PlayerState::WallSwim ? "壁を遊泳" : state==ink::PlayerState::Swim ? "インクに潜伏" : state==ink::PlayerState::Squid ? "イカ状態" : "人型";
}
}

InkShooterScene::~InkShooterScene() {
    audio_.Shutdown();
    SetCaptured(false);
    auto common=Object3dCommon::GetInstance();
    if (common->GetDefaultCamera()==camera_.get()) common->SetDefaultCamera(nullptr);
    common->SetDebugUiEnabled(previousDebugUi_);
}

std::unique_ptr<Object3d> InkShooterScene::MakeObject(const char* model, Vector4 color) {
    auto object=std::make_unique<Object3d>();
    object->Initialize(); object->SetModel(model); object->SetColor(color);
    object->SetCamera(camera_.get()); object->SetLighting(true);
    object->SetDirectionalLightDirection({0.3f,-1.0f,0.5f});
    object->SetRoughness(0.32f); object->SetMetallic(0.05f);
    return object;
}

void InkShooterScene::Initialize() {
    input_=Input::GetInstance(); camera_=std::make_unique<Camera>();
    camera_->SetFovY(fovY_); camera_->SetNearClip(0.06f); camera_->SetFarClip(150);
    auto common=Object3dCommon::GetInstance();
    common->SetDefaultCamera(camera_.get()); common->SetIsDebugCamera(false);
    previousDebugUi_=common->GetDebugUiEnabled(); common->SetDebugUiEnabled(false);
    common->SetDebugDefaultCamera(nullptr);
    simulation_.Reset();
    InitializeWeapons();
    paint_.Initialize(common->GetDxCommon(),common->GetSrvManager(),simulation_.Surfaces());
    liquid_.Initialize(common->GetDxCommon(),common->GetSrvManager());
    reticle_.Initialize(common->GetDxCommon());
    ModelManager::GetInstance()->CreateBoxModel("__ink_box");
    ModelManager::GetInstance()->CreateUvSphereModel("__ink_sphere",1,12,18);
    for (size_t n=0;n<actor_.size();++n)
        actor_[n]=MakeObject(n==1||n>=9 ? "__ink_sphere" : "__ink_box", kInk);
    actor_[0]->SetColor({0.08f,0.12f,0.20f,1});
    actor_[2]->SetColor({0.02f,0.055f,0.09f,1});
    actor_[3]->SetColor({0.94f,0.56f,0.12f,1});
    actor_[4]->SetColor({0.06f,0.17f,0.20f,1});
    actor_[7]->SetColor({0.10f,0.14f,0.19f,1});
    actor_[8]->SetColor({0.10f,0.14f,0.19f,1});
    actor_[9]->SetColor(kLiquid);
    for (int n=10;n<12;++n) actor_[n]->SetColor({0.07f,0.11f,0.10f,1});
    for(auto& part:bow_) part=MakeObject("__ink_box",{0.95f,0.57f,0.15f,1});
    dummy_[0]=MakeObject("__ink_sphere",{0.94f,0.48f,0.12f,1});
    dummy_[1]=MakeObject("__ink_box",{0.14f,0.19f,0.22f,1});
    dummy_[2]=MakeObject("__ink_box",{0.97f,0.86f,0.51f,1});
    effects_=std::make_unique<NeonGridRenderer>();
    effects_->Initialize(common->GetDxCommon(),"resources/white512x512.png");
    effects_->SetLineStyle(0.2f,1.0f);
    for (auto& sprite:hud_) {
        sprite=std::make_unique<Sprite>();
        sprite->Initialize(SpriteCommon::GetInstance(),"resources/white512x512.png");
    }
    TextStyle style{}; style.fontFamily="Meiryo"; style.fontSize=27;
    style.color={0.80f,1,0.95f,1}; style.outlineThickness=2; style.padding=5;
    title_=std::make_unique<TextLabel>();
    title_->Initialize(SpriteCommon::GetInstance(),"インクシューター / 試射場",style);
    title_->SetPosition({28,19});
    style.fontSize=16; style.color={0.85f,0.91f,0.95f,1};
    guide_=std::make_unique<TextLabel>();
    guide_->Initialize(SpriteCommon::GetInstance(),
        "WASD  移動    Mouse  照準    左クリック  射撃 / 長押しで弓をためる\n"
        "Shift  イカ変身 / チャージ中断    Space  ジャンプ    1 / 2  ブキ切替    Q  次のブキ\n"
        "塗った壁 + Shift + WASD  壁を泳ぐ    Tab  マウス解放    F1  日本語設定    R  初期化    F10  写真",style);
    guide_->SetPosition({28,621});
    style.fontSize=19;
    stateText_=std::make_unique<TextLabel>();
    stateText_->Initialize(SpriteCommon::GetInstance(),"人型",style);
    stateText_->SetPosition({29,73});
    style.fontSize=16; style.color={1,0.89f,0.64f,1};
    dummyText_=std::make_unique<TextLabel>();
    dummyText_->Initialize(SpriteCommon::GetInstance(),"試射ダミー  HP 100 / 100",style);
    dummyText_->SetPosition({830,25});
    style.color={0.85f,0.95f,1,1};
    weaponText_=std::make_unique<TextLabel>();
    weaponText_->Initialize(SpriteCommon::GetInstance(),simulation_.ActiveWeaponName(),style);
    weaponText_->SetPosition({29,144});
    lastTime_=std::chrono::steady_clock::now();
    SetCaptured(true); UpdateCamera(1); UpdateModels(); UpdateHud();
    wchar_t automatic[8]{};
    if (GetEnvironmentVariableW(L"CG2_INK_AUTOTEST",automatic,8)>0) {
        extendedReplay_=automatic[0]==L'2'; fidelityReplay_=automatic[0]==L'3'; weaponsReplay_=automatic[0]==L'4'; feelReplay_=automatic[0]==L'5'; StartReplay();
    }
    if (GetEnvironmentVariableW(L"CG2_INK_SETTINGS",automatic,8)>0) { debug_=true; SetCaptured(false); }
}

void InkShooterScene::SetCaptured(bool captured) {
    if (captured_==captured) return;
    captured_=captured;
    if (captured_) { ShowCursor(FALSE); }
    else { ClipCursor(nullptr); ShowCursor(TRUE); }
}

void InkShooterScene::Reset() {
    audio_.Stop();
    simulation_.Reset(); paint_.Clear(); accumulator_=0; jumpPending_=false;
    yaw_=0; pitch_=0.16f; cameraReady_=false;
    bursts_.clear(); visualShotCount_=0; visualState_=ink::PlayerState::Human;
    formAge_=10; visualKick_=0; wakeTime_=0;
}

void InkShooterScene::StartReplay() {
    debug_=false; SetCaptured(true);
    if(!weaponsReplay_) EquipCatalogWeapon("splattershot");
    Reset(); replay_=true; replayTime_=0; logTime_=0; replaySection_=-1;
    if (replayLog_.is_open()) replayLog_.close();
    std::filesystem::create_directories("generated");
    replayLog_.open("generated/ink_replay.csv",std::ios::trunc);
    replayLog_ << "time,state,x,y,z,ink,ownInk,grounded,speed,projectiles,stamps,fps,dummyHp,damage,section,paintGpuMs,surfaceGpuMs,smoothing,particles,enemyInk,droplets,shots,weapon,charge,embedded,carry\n";
    // Include pipeline/resource creation diagnostics from before this replay.
    gpuMessageStart_=0;
}

void InkShooterScene::WriteGpuDiagnostics() {
    audio_.WriteDiagnostics("generated/ink_audio_validation.txt");
    auto dx=Object3dCommon::GetInstance()->GetDxCommon();
    std::ofstream report("generated/ink_gpu_validation.txt",std::ios::trunc);
    report << "D3D12 debug layer: " << dx->IsD3D12DebugLayerEnabled()
        << "\nGPU based validation: " << dx->IsGpuBasedValidationEnabled()
        << "\nIncludes startup messages: 1\n";
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> queue;
    if (FAILED(dx->GetDevice().As(&queue))) { report << "InfoQueue unavailable\n"; return; }
    uint64_t errors=0,warnings=0;
    const auto count=queue->GetNumStoredMessagesAllowedByRetrievalFilter();
    for (uint64_t n=gpuMessageStart_;n<count;++n) {
        SIZE_T size=0; if (FAILED(queue->GetMessage(n,nullptr,&size))) continue;
        std::vector<unsigned char> bytes(size);
        auto message=reinterpret_cast<D3D12_MESSAGE*>(bytes.data());
        if (FAILED(queue->GetMessage(n,message,&size))) continue;
        if (message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR) ++errors;
        if (message->Severity==D3D12_MESSAGE_SEVERITY_WARNING) ++warnings;
        if (message->Severity<=D3D12_MESSAGE_SEVERITY_WARNING && errors+warnings<=50)
            report << message->pDescription << '\n';
    }
    report << "Replay errors: " << errors << "\nReplay warnings: " << warnings << '\n';
}

ink::Controls InkShooterScene::ReadControls(float dt) {
    ink::Controls c{};
    if (replay_) {
        if (feelReplay_) return ReadFeelValidationControls(dt);
        if (weaponsReplay_) return ReadWeaponsValidationControls(dt);
        if (fidelityReplay_) return ReadFidelityControls(dt);
        if (extendedReplay_) return ReadValidationControls(dt);
        replayTime_+=dt; pitch_=0.19f;
        c.fire=replayTime_<3.5f || (replayTime_>7.5f && replayTime_<10);
        c.moveZ=(replayTime_>1 && replayTime_<7.5f)?1.0f:0.0f;
        c.swim=replayTime_>=3.5f && replayTime_<7.5f;
        c.jump=replayTime_>=8 && replayTime_-dt<8;
        if (replayTime_>12) { replay_=false; replayLog_.flush(); WriteGpuDiagnostics(); }
    } else if (captured_ && !debug_ && GetForegroundWindow()==WinApp::GetInstance()->GetHwnd()) {
        const auto key=input_->GetKey();
        auto held=[&](int n) { return input_->IsPress(key[n]); };
        c.moveX=(held(DIK_D)?1.0f:0)-(held(DIK_A)?1.0f:0);
        c.moveZ=(held(DIK_W)?1.0f:0)-(held(DIK_S)?1.0f:0);
        c.swim=held(DIK_LSHIFT)||held(DIK_RSHIFT);
        c.jump=input_->IsTrigger(key[DIK_SPACE],input_->GetPreKey()[DIK_SPACE]);
        const auto mouse=input_->GetMouseState();
        c.fire=(mouse.rgbButtons[0]&0x80)!=0;
        yaw_+=static_cast<float>(mouse.lX)*sensitivity_;
        pitch_+=static_cast<float>(mouse.lY)*sensitivity_*(invertPitch_?-1.0f:1.0f);
    }
    pitch_=std::clamp(pitch_,-1.1f,1.25f);
    yaw_=std::remainder(yaw_,6.2831853f);
    c.yaw=yaw_; c.pitch=pitch_; c.aimPoint=aimPoint_;
    return c;
}

void InkShooterScene::UpdateCamera(float dt) {
    const auto& p=simulation_.Player();
    const ink::Vec3 forward={std::sin(yaw_)*std::cos(pitch_),-std::sin(pitch_),std::cos(yaw_)*std::cos(pitch_)};
    const ink::Vec3 right={std::cos(yaw_),0,-std::sin(yaw_)};
    const Vector3 target=V(p.position+ink::Vec3{0,cameraHeight_-0.16f*p.formBlend,0});
    if (!cameraReady_) { cameraTarget_=target; cameraReady_=true; }
    const float blend=1.0f-std::exp(-followSharpness_*dt);
    cameraTarget_=cameraTarget_+(target-cameraTarget_)*blend;
    const ink::Vec3 pivot=I(cameraTarget_);
    const ink::Vec3 desired=pivot-forward*cameraDistance_+right*shoulderOffset_;
    const auto offset=desired-pivot;
    const auto collision=simulation_.Raycast(pivot,ink::Normalize(offset),ink::Length(offset),0.18f);
    const ink::Vec3 eye=collision.hit ? pivot+ink::Normalize(offset)*(std::max)(0.0f,collision.distance-0.12f) : desired;
    camera_->SetTranslate(V(eye)); camera_->SetRotate({pitch_,yaw_,0});
    camera_->SetFovY(fovY_);
    auto window=WinApp::GetInstance();
    camera_->SetAspectRatio(static_cast<float>(window->GetClientWidth())/static_cast<float>((std::max)(1,window->GetClientHeight())));
    camera_->Update();
    const auto hit=simulation_.Raycast(eye,forward,100);
    aimPoint_=hit.hit?hit.position:eye+forward*100;
    controls_.aimPoint=aimPoint_;
}

void InkShooterScene::Update() {
    FinishCapture();
    const auto now=std::chrono::steady_clock::now();
    const float wallDt=std::chrono::duration<float>(now-lastTime_).count(); lastTime_=now;
    const float dt=std::clamp(wallDt,0.0f,0.1f);
    elapsed_+=dt; fps_+=(1.0f/(std::max)(wallDt,0.001f)-fps_)*0.05f;
    auto triggered=[&](int n) { return input_->IsTrigger(input_->GetKey()[n],input_->GetPreKey()[n]); };
    if (triggered(DIK_ESCAPE)) { SetCaptured(false); finished_=true; return; }
    if (triggered(DIK_TAB)) SetCaptured(!captured_);
    if (triggered(DIK_F1) && !input_->GetKey()[DIK_LSHIFT] && !input_->GetKey()[DIK_RSHIFT]) { debug_=!debug_; SetCaptured(!debug_); }
    if (triggered(DIK_R)&&captured_) { replay_=false; Reset(); }
    if (triggered(DIK_F9)) { if (replay_) replay_=false; else StartReplay(); }
    if (triggered(DIK_F10)) RequestCapture("manual");
    if(captured_ && !replay_) {
        if(triggered(DIK_1)) EquipCatalogWeapon("splattershot");
        if(triggered(DIK_2)) EquipCatalogWeapon("tri_stringer");
        if(triggered(DIK_Q)) CycleWeapon();
    }
    const bool inputFocused=replay_ || (captured_ && !debug_ && GetForegroundWindow()==WinApp::GetInstance()->GetHwnd());
    if(!inputFocused) simulation_.CancelCharge();
    // This scene owns its camera: Shift+D is a gameplay chord, even in editor builds.
    Object3dCommon::GetInstance()->SetIsDebugCamera(false);
    if (captured_) {
        RECT rect{}; auto hwnd=WinApp::GetInstance()->GetHwnd();
        GetClientRect(hwnd,&rect); POINT corners[2]={{rect.left,rect.top},{rect.right,rect.bottom}};
        MapWindowPoints(hwnd,nullptr,corners,2);
        RECT clip={corners[0].x,corners[0].y,corners[1].x,corners[1].y}; ClipCursor(&clip);
    }
    controls_=ReadControls(dt); UpdateCamera(dt); controls_.aimPoint=aimPoint_;
    // Repeatable validation scenes use explicit world targets; ordinary input
    // always keeps the camera-ray to muzzle correction above.
    if(replay_ && extendedReplay_ && replaySection_==3)
        controls_.aimPoint={0,0.25f+(replayTime_-21)*1.48f,14};
    if(replay_ && extendedReplay_ && replaySection_==5)
        controls_.aimPoint=simulation_.Dummy().position;
    if(replay_ && weaponsReplay_ && replaySection_==6)
        controls_.aimPoint=simulation_.Dummy().position;
    if(replay_ && weaponsReplay_ && replaySection_==7)
        controls_.aimPoint={0,2,14};
    if(replay_ && feelReplay_ && replaySection_==5)
        controls_.aimPoint={0,2,14};
    jumpPending_=jumpPending_||controls_.jump;
    accumulator_+=dt;
    constexpr float step=1.0f/120.0f;
    while (accumulator_>=step) {
        controls_.jump=jumpPending_; jumpPending_=false;
        simulation_.Step(step,controls_); accumulator_-=step;
    }
    controls_.jump=false;
    paint_.QueueStamps(simulation_.TakePendingStamps());
    paint_.SetEdgeSmoothing(wetEdges_);
    // Follow after movement, and update the render camera once before drawing actors.
    UpdateCamera(0); audio_.Update(dt,simulation_,inputFocused && (replay_ || !debug_)); UpdateVisuals(dt); UpdateModels(); UpdateHud(); DrawDebug();
    if (replay_ && replayTime_-logTime_>=(feelReplay_?0.05f:0.5f)) {
        logTime_=replayTime_; const auto& p=simulation_.Player();
        replayLog_<<replayTime_<<','<<StateName(p.state)<<','<<p.position.x<<','<<p.position.y<<','<<p.position.z<<','
            <<p.ink<<','<<p.onOwnInk<<','<<p.grounded<<','<<p.speed<<','<<simulation_.Projectiles().size()<<','<<simulation_.StampCount()<<','<<fps_<<','
            <<simulation_.Dummy().hp<<','<<simulation_.Dummy().lastDamage<<','<<replaySection_<<','
            <<paint_.GetLastPaintGpuMs()<<','<<paint_.GetLastSurfaceGpuMs()<<','<<wetEdges_<<','<<liquid_.GetParticleCount()<<','
            <<p.onEnemyInk<<','<<simulation_.Droplets().size()<<','<<simulation_.ShotsFired()<<','
            <<simulation_.ActiveWeaponId()<<','<<simulation_.ChargeTime()<<','<<simulation_.EmbeddedArrows().size()<<','<<simulation_.DrySquidCarryRemaining()<<'\n';
        replayLog_.flush();
    }
}

void InkShooterScene::UpdateModels() {
    const auto& p=simulation_.Player();
    const float f=p.formBlend;
    const float emerge=visualState_==ink::PlayerState::Human ? std::sin((std::min)(formAge_,0.20f)*15.7f)*std::exp(-formAge_*14)*0.16f : 0;
    const float human=(std::max)(0.001f,(1-f)*(1+emerge));
    const float humanWidth=f>0.98f ? 0.001f : 1-f*0.55f;
    const ink::Vec3 right={std::cos(yaw_),0,-std::sin(yaw_)};
    const ink::Vec3 forward={std::sin(yaw_),0,std::cos(yaw_)};
    auto part=[&](int n,ink::Vec3 offset,Vector3 scale,float pitch=0) {
        actor_[n]->SetTranslate(V(p.position+right*offset.x+ink::Vec3{0,offset.y,0}+forward*offset.z));
        actor_[n]->SetScale(scale); actor_[n]->SetRotate({pitch,yaw_,0}); actor_[n]->Update();
    };
    const float stride=std::sin(elapsed_*13)*(std::min)(1.0f,p.speed/2.0f)*0.13f;
    part(0,{0,0.94f*human-0.09f*f,0},{0.49f*humanWidth,0.56f*human,0.32f*humanWidth});
    part(1,{0,1.42f*human-0.09f*f,0},{0.29f*humanWidth,0.29f*human,0.29f*humanWidth});
    part(2,{0,1.43f*human-0.09f*f,0.25f*humanWidth},{0.43f*humanWidth,0.13f*human,0.05f*humanWidth});
    const auto muzzle=simulation_.Muzzle(controls_);
    actor_[3]->SetTranslate(V(muzzle-forward*(0.21f+0.10f*visualKick_)-ink::Vec3{0,f*0.9f,0}));
    actor_[3]->SetScale({0.17f*human,0.20f*human,0.63f*human});
    actor_[3]->SetRotate({pitch_-0.08f*visualKick_,yaw_,0}); actor_[3]->Update();
    part(4,{0,0.95f*human,-0.30f*human},{0.28f*human,0.55f*human,0.21f*human});
    part(5,{-0.32f*human,0.96f*human,0.20f*human},{0.15f*human,0.33f*human,0.18f*human},-0.65f);
    part(6,{0.32f*human,0.96f*human,0.20f*human},{0.15f*human,0.33f*human,0.18f*human},-0.65f);
    part(7,{-0.16f*human,0.33f*human,stride},{0.19f*human,0.56f*human,0.23f*human});
    part(8,{0.16f*human,0.33f*human,-stride},{0.19f*human,0.56f*human,0.23f*human});
    const float swimScale=(std::max)(0.001f,f);
    const bool exposed=p.state==ink::PlayerState::Squid;
    const float bodyHeight=exposed?0.21f:0.045f;
    const float bodyThickness=exposed?0.20f:0.085f;
    const float airPitch=exposed && !p.grounded?std::clamp(-p.velocity.y*0.045f,-0.30f,0.30f):0;
    part(9,{0,bodyHeight,0},{0.29f*swimScale,bodyThickness*swimScale,(exposed?0.43f:0.47f)*swimScale},airPitch);
    part(10,{-0.105f*swimScale,bodyHeight+(exposed?0.09f:0.03f),0.30f*swimScale},{0.045f*swimScale,0.032f*swimScale,0.07f*swimScale});
    part(11,{0.105f*swimScale,bodyHeight+(exposed?0.09f:0.03f),0.30f*swimScale},{0.045f*swimScale,0.032f*swimScale,0.07f*swimScale});
    if (p.state==ink::PlayerState::WallSwim) {
        const float wallYaw=std::atan2(p.wallNormal.x,p.wallNormal.z);
        actor_[9]->SetScale({0.29f,0.43f,0.08f}); actor_[9]->SetRotate({0,wallYaw,0});
        actor_[9]->SetTranslate(V(p.position-p.wallNormal*0.12f+ink::Vec3{0,0.3f,0})); actor_[9]->Update();
        for(int n=10;n<12;++n) { actor_[n]->SetScale({0.001f,0.001f,0.001f}); actor_[n]->Update(); }
    }
    const auto& target=simulation_.Dummy();
    const float targetScale=target.hp>0?target.radius:target.radius*0.12f;
    dummy_[0]->SetTranslate(V(target.position)); dummy_[0]->SetScale({targetScale,targetScale,targetScale});
    dummy_[0]->SetColor(target.hp>0?Vector4{0.94f,0.48f,0.12f,1}:Vector4{0.16f,0.21f,0.23f,1});
    dummy_[1]->SetTranslate(V(target.position+ink::Vec3{0,-0.6f,0})); dummy_[1]->SetScale({0.16f,0.7f,0.16f});
    dummy_[2]->SetTranslate(V(target.position+ink::Vec3{0,0,-target.radius-0.008f})); dummy_[2]->SetScale({0.5f,0.08f,0.03f});
    for(auto& object:dummy_) object->Update();
    effects_->BeginFrame();
    liquid_.BeginFrame();
    const Vector3 cameraForward={std::sin(yaw_)*std::cos(pitch_),-std::sin(pitch_),std::cos(yaw_)*std::cos(pitch_)};
    for (const auto& shot:simulation_.Projectiles()) {
        const auto direction=ink::Normalize(shot.velocity);
        const float speed=ink::Length(shot.velocity);
        const bool arrow=shot.projectileKind==ink::ProjectileKind::StringerArrow;
        const auto color=arrow && shot.explosive?Vector4{0.20f,0.95f,0.74f,1}:kLiquid;
        liquid_.QueueBlob(V(shot.position),V(shot.velocity),arrow?0.065f:0.115f,color,arrow?3.4f:1.0f+(std::min)(1.15f,speed/65.0f));
        liquid_.QueueBlob(V(shot.position-direction*(arrow?0.32f:0.24f)),V(shot.velocity),arrow?0.028f:0.052f,kLiquid,arrow?2.8f:1.7f,0,0,InkLiquidKind::Droplet);
    }
    for (const auto& drop:simulation_.Droplets()) {
        const float widthRatio=drop.paintRadius/(std::max)(0.01f,drop.tuning.paintDropletRadius);
        liquid_.QueueBlob(V(drop.position),V(drop.velocity),0.047f*std::clamp(widthRatio,0.65f,1.4f),kLiquid,1.55f,0,0,InkLiquidKind::Droplet);
    }
    for(const auto& burst:bursts_) {
        if(burst.ripple) liquid_.QueueRipple(V(burst.position),V(burst.normal),burst.radius+burst.age*0.65f,
            {0.015f,0.48f,0.29f,0.62f},burst.age,burst.lifetime,0.09f);
        else liquid_.QueueBlob(V(burst.position),V(burst.velocity),burst.radius,kLiquid,1.55f,burst.age,burst.lifetime,InkLiquidKind::Splash);
    }
    if(visualKick_>0.12f && human>0.3f)
        liquid_.QueueBlob(V(muzzle),V(ink::Normalize(aimPoint_-muzzle)),0.15f*visualKick_,kLiquid,1.1f,0,0,InkLiquidKind::Muzzle);
    // World-space impact indicator supplements the screen crosshair at close obstructions.
    const auto muzzleHit=simulation_.Raycast(muzzle,ink::Normalize(aimPoint_-muzzle),ink::Length(aimPoint_-muzzle));
    if (muzzleHit.hit) {
        const auto rightAxis=ink::Normalize(ink::Cross(muzzleHit.normal,{0,1,0}));
        const auto tangent=ink::Length(rightAxis)>0.1f?rightAxis:ink::Vec3{1,0,0};
        const auto up=ink::Normalize(ink::Cross(muzzleHit.normal,tangent));
        const auto center=muzzleHit.position+muzzleHit.normal*0.018f;
        effects_->QueueCameraFacingLine(V(center-tangent*0.07f),V(center+tangent*0.07f),0.018f,{1,0.85f,0.3f,1},cameraForward);
        effects_->QueueCameraFacingLine(V(center-up*0.07f),V(center+up*0.07f),0.018f,{1,0.85f,0.3f,1},cameraForward);
    }
    UpdateWeaponVisuals();
}

void InkShooterScene::UpdateHud() {
    const auto& p=simulation_.Player();
    stateText_->SetText(replay_ ? std::string("動作デモ / ")+JapaneseState(p.state) : !captured_ ? "設定中 / Tabで操作に戻る" :
        std::string(JapaneseState(p.state))+(p.onOwnInk?" / 自分のインク":p.onEnemyInk?" / 相手のインク":!p.grounded?" / 空中":" / 未塗装")+(p.ink<0.01f?" / インク切れ":""));
    if(elapsed_>=dummyTextTime_) {
        dummyTextTime_=elapsed_+0.20f;
        const auto& target=simulation_.Dummy(); char text[256]{};
        std::snprintf(text,sizeof(text),"試射ダミー  HP %.0f / 100\n直前のダメージ %.1f   命中距離 %.2f\n撃破まで %u 発 / 撃破数 %u",
            target.hp,target.lastDamage,target.lastHitDistance,target.lastShotsToKill,target.kills);
        dummyText_->SetText(text);
        std::string weaponLine=simulation_.ActiveWeaponName();
        if(simulation_.ActiveWeaponClass()==ink::WeaponClass::Stringer) {
            const auto charge=simulation_.ChargeProfile(); char chargeText[160]{};
            std::snprintf(chargeText,sizeof(chargeText),"\n%s%d本 / チャージ %.0f%% / %s",
                p.grounded?"横":"縦",simulation_.stringer.arrowCount,charge.normalizedCharge*100,
                charge.explosive?"着弾後に爆発":"離すと発射");
            weaponLine+=chargeText;
        } else weaponLine+="\n中央：照準 / 外側：左右の拡散範囲";
        weaponText_->SetText(weaponLine);
    }
    auto rect=[&](int n,float x,float y,float w,float h,Vector4 color) {
        hud_[n]->SetPosition({x,y}); hud_[n]->SetSize({(std::max)(w,0.01f),h}); hud_[n]->SetColor(color); hud_[n]->Update();
    };
    rect(0,23,69,280,70,{0.015f,0.028f,0.05f,0.85f});
    rect(1,33,112,256,10,{0.11f,0.18f,0.24f,1});
    rect(2,33,112,256*p.ink,10,p.ink<0.12f?Vector4{1,0.32f,0.15f,1}:kInk);
    // The reticle has its own antialiased procedural pass. The old cross-shaped
    // HUD sprites remain transparent so no rectangle approximates the new ring.
    for(int n=3;n<8;++n) rect(n,0,0,1,1,{0,0,0,0});
}

void InkShooterScene::DrawPostEffect3D() {
    paint_.Draw(camera_->GetViewProjectionMatrix(),camera_->GetTranslate());
    Object3dCommon::GetInstance()->PreDraw(kNone);
    // Bloom applies the current frame's projection jitter just before this pass.
    for (auto& object:actor_) { object->Update(); object->Draw(); }
    for (auto& object:bow_) { object->Update(); object->Draw(); }
    for (auto& object:dummy_) { object->Update(); object->Draw(); }
    const Vector3 right={std::cos(yaw_),0,-std::sin(yaw_)};
    const Vector3 up={std::sin(yaw_)*std::sin(pitch_),std::cos(pitch_),std::cos(yaw_)*std::sin(pitch_)};
    liquid_.Draw(camera_->GetViewProjectionMatrix(),camera_->GetTranslate(),right,up);
    effects_->DrawAll(camera_->GetViewProjectionMatrix());
}

void InkShooterScene::DrawSprite() {
    for (auto& sprite:hud_) sprite->Draw();
    title_->Draw(); guide_->Draw(); stateText_->Draw(); dummyText_->Draw(); weaponText_->Draw();
    InkReticleState state;
    state.stringer=simulation_.ActiveWeaponClass()==ink::WeaponClass::Stringer;
    state.projectileCount=simulation_.stringer.arrowCount;
    state.spreadDegrees=simulation_.Player().accuracy;
    state.submerged=ink::IsSubmerged(simulation_.Player().state);
    state.vertical=!simulation_.Player().grounded;
    const auto profile=simulation_.ChargeProfile();
    state.charge=simulation_.IsCharging()?profile.normalizedCharge:0;
    state.firstChargeRatio=simulation_.stringer.midChargeTime/(std::max)(0.001f,simulation_.stringer.fullChargeTime);
    state.outOfInk=simulation_.Player().ink<(state.stringer?profile.inkConsume:simulation_.weapon.inkConsume);
    const auto viewport=Object3dCommon::GetInstance()->GetDxCommon()->GetViewportRect();
    reticle_.Draw(state,viewport.Width,viewport.Height,fovY_);
    CopyCapture();
}
