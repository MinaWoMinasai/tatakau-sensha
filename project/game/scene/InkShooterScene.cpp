#include "InkShooterScene.h"
#include "Object3dCommon.h"
#include "ModelManager.h"
#include "WinApp.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <d3d12sdklayers.h>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
Vector3 V(ink::Vec3 v) { return {v.x,v.y,v.z}; }
ink::Vec3 I(Vector3 v) { return {v.x,v.y,v.z}; }
const char* StateName(ink::PlayerState s) {
    return s==ink::PlayerState::WallSwim ? "WALL SWIM" : s==ink::PlayerState::Swim ? "SWIM" : "HUMAN";
}
constexpr Vector4 kInk={0.025f,0.86f,0.68f,1};
}

InkShooterScene::~InkShooterScene() {
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
    camera_->SetFovY(0.92f); camera_->SetNearClip(0.06f); camera_->SetFarClip(150);
    auto common=Object3dCommon::GetInstance();
    common->SetDefaultCamera(camera_.get()); common->SetIsDebugCamera(false);
    previousDebugUi_=common->GetDebugUiEnabled(); common->SetDebugUiEnabled(false);
    common->SetDebugDefaultCamera(nullptr);
    simulation_.Reset();
    paint_.Initialize(common->GetDxCommon(),common->GetSrvManager(),simulation_.Surfaces());
    ModelManager::GetInstance()->CreateBoxModel("__ink_box");
    ModelManager::GetInstance()->CreateUvSphereModel("__ink_sphere",1,12,18);
    for (size_t n=0;n<actor_.size();++n)
        actor_[n]=MakeObject(n==1||n==9 ? "__ink_sphere" : "__ink_box", kInk);
    actor_[0]->SetColor({0.08f,0.12f,0.20f,1});
    actor_[2]->SetColor({0.02f,0.055f,0.09f,1});
    actor_[3]->SetColor({0.94f,0.56f,0.12f,1});
    actor_[4]->SetColor({0.06f,0.17f,0.20f,1});
    actor_[7]->SetColor({0.10f,0.14f,0.19f,1});
    actor_[8]->SetColor({0.10f,0.14f,0.19f,1});
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
    title_->Initialize(SpriteCommon::GetInstance(),"INK / SHOOTER LAB",style);
    title_->SetPosition({28,19});
    style.fontSize=16; style.color={0.85f,0.91f,0.95f,1};
    guide_=std::make_unique<TextLabel>();
    guide_->Initialize(SpriteCommon::GetInstance(),
        "WASD  移動    Mouse  照準    左クリック  射撃    Shift  インクに潜る    Space  ジャンプ\n"
        "塗った壁 + Shift + W/S/A/D  壁を泳ぐ    Tab  マウス解放    F1  調整    R  リセット    F9  デモ    Esc  戻る",style);
    guide_->SetPosition({28,646});
    style.fontSize=19;
    stateText_=std::make_unique<TextLabel>();
    stateText_->Initialize(SpriteCommon::GetInstance(),"HUMAN",style);
    stateText_->SetPosition({29,73});
    lastTime_=std::chrono::steady_clock::now();
    SetCaptured(true); UpdateCamera(1); UpdateModels(); UpdateHud();
    wchar_t automatic[8]{};
    if (GetEnvironmentVariableW(L"CG2_INK_AUTOTEST",automatic,8)>0) StartReplay();
}

void InkShooterScene::SetCaptured(bool captured) {
    if (captured_==captured) return;
    captured_=captured;
    if (captured_) { ShowCursor(FALSE); }
    else { ClipCursor(nullptr); ShowCursor(TRUE); }
}

void InkShooterScene::Reset() {
    simulation_.Reset(); paint_.Clear(); accumulator_=0; jumpPending_=false;
    yaw_=0; pitch_=0.16f; cameraReady_=false;
}

void InkShooterScene::StartReplay() {
    Reset(); replay_=true; replayTime_=0; logTime_=0;
    if (replayLog_.is_open()) replayLog_.close();
    std::filesystem::create_directories("generated");
    replayLog_.open("generated/ink_replay.csv",std::ios::trunc);
    replayLog_ << "time,state,x,y,z,ink,ownInk,grounded,speed,projectiles,stamps,fps\n";
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> queue;
    if (SUCCEEDED(Object3dCommon::GetInstance()->GetDxCommon()->GetDevice().As(&queue)))
        gpuMessageStart_=queue->GetNumStoredMessagesAllowedByRetrievalFilter();
}

void InkShooterScene::WriteGpuDiagnostics() {
    auto dx=Object3dCommon::GetInstance()->GetDxCommon();
    std::ofstream report("generated/ink_gpu_validation.txt",std::ios::trunc);
    report << "D3D12 debug layer: " << dx->IsD3D12DebugLayerEnabled()
        << "\nGPU based validation: " << dx->IsGpuBasedValidationEnabled() << '\n';
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
        replayTime_+=dt; pitch_=0.19f;
        c.fire=replayTime_<3.5f || (replayTime_>7.5f && replayTime_<10);
        c.moveZ=(replayTime_>1 && replayTime_<7.5f)?1.0f:0.0f;
        c.swim=replayTime_>=3.5f && replayTime_<7.5f;
        c.jump=replayTime_>=8 && replayTime_-dt<8;
        if (replayTime_>12) { replay_=false; replayLog_.flush(); WriteGpuDiagnostics(); }
    } else if (captured_) {
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
    const Vector3 target=V(p.position+ink::Vec3{0,1.45f-0.30f*p.formBlend,0});
    if (!cameraReady_) { cameraTarget_=target; cameraReady_=true; }
    const float blend=1.0f-std::exp(-18.0f*dt);
    cameraTarget_=cameraTarget_+(target-cameraTarget_)*blend;
    const ink::Vec3 pivot=I(cameraTarget_);
    const ink::Vec3 desired=pivot-forward*cameraDistance_+right*0.58f;
    const auto offset=desired-pivot;
    const auto collision=simulation_.Raycast(pivot,ink::Normalize(offset),ink::Length(offset),0.18f);
    const ink::Vec3 eye=collision.hit ? pivot+ink::Normalize(offset)*(std::max)(0.0f,collision.distance-0.12f) : desired;
    camera_->SetTranslate(V(eye)); camera_->SetRotate({pitch_,yaw_,0});
    auto window=WinApp::GetInstance();
    camera_->SetAspectRatio(static_cast<float>(window->GetClientWidth())/static_cast<float>((std::max)(1,window->GetClientHeight())));
    camera_->Update();
    const auto hit=simulation_.Raycast(eye,forward,100);
    aimPoint_=hit.hit?hit.position:eye+forward*100;
    controls_.aimPoint=aimPoint_;
}

void InkShooterScene::Update() {
    const auto now=std::chrono::steady_clock::now();
    const float wallDt=std::chrono::duration<float>(now-lastTime_).count(); lastTime_=now;
    const float dt=std::clamp(wallDt,0.0f,0.1f);
    elapsed_+=dt; fps_+=(1.0f/(std::max)(wallDt,0.001f)-fps_)*0.05f;
    auto triggered=[&](int n) { return input_->IsTrigger(input_->GetKey()[n],input_->GetPreKey()[n]); };
    if (triggered(DIK_ESCAPE)) { SetCaptured(false); finished_=true; return; }
    if (triggered(DIK_TAB)) SetCaptured(!captured_);
    if (triggered(DIK_F1)) { debug_=!debug_; SetCaptured(!debug_); }
    if (triggered(DIK_R)&&captured_) { replay_=false; Reset(); }
    if (triggered(DIK_F9)) { if (replay_) replay_=false; else StartReplay(); }
    // This scene owns its camera: Shift+D is a gameplay chord, even in editor builds.
    Object3dCommon::GetInstance()->SetIsDebugCamera(false);
    if (captured_) {
        RECT rect{}; auto hwnd=WinApp::GetInstance()->GetHwnd();
        GetClientRect(hwnd,&rect); POINT corners[2]={{rect.left,rect.top},{rect.right,rect.bottom}};
        MapWindowPoints(hwnd,nullptr,corners,2);
        RECT clip={corners[0].x,corners[0].y,corners[1].x,corners[1].y}; ClipCursor(&clip);
    }
    controls_=ReadControls(dt); UpdateCamera(dt); controls_.aimPoint=aimPoint_;
    jumpPending_=jumpPending_||controls_.jump;
    accumulator_+=dt;
    constexpr float step=1.0f/120.0f;
    while (accumulator_>=step) {
        controls_.jump=jumpPending_; jumpPending_=false;
        simulation_.Step(step,controls_); accumulator_-=step;
    }
    controls_.jump=false;
    paint_.QueueStamps(simulation_.TakePendingStamps());
    // Follow after movement, and update the render camera once before drawing actors.
    UpdateCamera(0); UpdateModels(); UpdateHud(); DrawDebug();
    if (replay_ && replayTime_-logTime_>=0.5f) {
        logTime_=replayTime_; const auto& p=simulation_.Player();
        replayLog_<<replayTime_<<','<<StateName(p.state)<<','<<p.position.x<<','<<p.position.y<<','<<p.position.z<<','
            <<p.ink<<','<<p.onOwnInk<<','<<p.grounded<<','<<p.speed<<','<<simulation_.Projectiles().size()<<','<<simulation_.StampCount()<<','<<fps_<<'\n';
        replayLog_.flush();
    }
}

void InkShooterScene::UpdateModels() {
    const auto& p=simulation_.Player();
    const float f=p.formBlend, human=(std::max)(0.001f,1-f);
    const ink::Vec3 right={std::cos(yaw_),0,-std::sin(yaw_)};
    const ink::Vec3 forward={std::sin(yaw_),0,std::cos(yaw_)};
    auto part=[&](int n,ink::Vec3 offset,Vector3 scale,float pitch=0) {
        actor_[n]->SetTranslate(V(p.position+right*offset.x+ink::Vec3{0,offset.y,0}+forward*offset.z));
        actor_[n]->SetScale(scale); actor_[n]->SetRotate({pitch,yaw_,0}); actor_[n]->Update();
    };
    const float stride=std::sin(elapsed_*13)*(std::min)(1.0f,p.speed/2.0f)*0.13f;
    part(0,{0,0.94f*human,0},{0.49f*human,0.56f*human,0.32f*human});
    part(1,{0,1.42f*human,0},{0.29f*human,0.29f*human,0.29f*human});
    part(2,{0,1.43f*human,0.25f*human},{0.43f*human,0.13f*human,0.05f*human});
    const auto muzzle=simulation_.Muzzle(controls_);
    actor_[3]->SetTranslate(V(muzzle-forward*0.21f));
    actor_[3]->SetScale({0.17f*human,0.20f*human,0.63f*human});
    actor_[3]->SetRotate({pitch_,yaw_,0}); actor_[3]->Update();
    part(4,{0,0.95f*human,-0.30f*human},{0.28f*human,0.55f*human,0.21f*human});
    part(5,{-0.32f*human,0.96f*human,0.20f*human},{0.15f*human,0.33f*human,0.18f*human},-0.65f);
    part(6,{0.32f*human,0.96f*human,0.20f*human},{0.15f*human,0.33f*human,0.18f*human},-0.65f);
    part(7,{-0.16f*human,0.33f*human,stride},{0.19f*human,0.56f*human,0.23f*human});
    part(8,{0.16f*human,0.33f*human,-stride},{0.19f*human,0.56f*human,0.23f*human});
    const float swimScale=(std::max)(0.001f,f);
    part(9,{0,0.13f,0},{0.38f*swimScale,0.13f*swimScale,0.58f*swimScale});
    if (p.state==ink::PlayerState::WallSwim) {
        actor_[9]->SetScale({0.38f,0.5f,0.12f});
        actor_[9]->SetTranslate(V(p.position+ink::Vec3{0,0.3f,0})); actor_[9]->Update();
    }
    effects_->BeginFrame();
    const Vector3 cameraForward={std::sin(yaw_)*std::cos(pitch_),-std::sin(pitch_),std::cos(yaw_)*std::cos(pitch_)};
    for (const auto& shot:simulation_.Projectiles()) {
        const auto direction=ink::Normalize(shot.velocity);
        effects_->QueueCameraFacingLine(V(shot.position-direction*0.44f),V(shot.position),0.12f,kInk,cameraForward);
    }
    for (const auto& drop:simulation_.Droplets()) {
        effects_->QueueCameraFacingLine(V(drop.position+ink::Vec3{0,0.10f,0}),V(drop.position),0.065f,kInk,cameraForward);
    }
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
}

void InkShooterScene::UpdateHud() {
    const auto& p=simulation_.Player();
    const bool swim=p.state!=ink::PlayerState::Human;
    stateText_->SetText(replay_ ? std::string("DEMO / ")+StateName(p.state) : !captured_ ? "PAUSED INPUT / Tab to play" :
        std::string(StateName(p.state))+(p.onOwnInk?" / OWN INK":" / DRY")+(p.ink<0.01f?" / EMPTY":""));
    auto rect=[&](int n,float x,float y,float w,float h,Vector4 color) {
        hud_[n]->SetPosition({x,y}); hud_[n]->SetSize({(std::max)(w,0.01f),h}); hud_[n]->SetColor(color); hud_[n]->Update();
    };
    rect(0,23,69,280,70,{0.015f,0.028f,0.05f,0.85f});
    rect(1,33,112,256,10,{0.11f,0.18f,0.24f,1});
    rect(2,33,112,256*p.ink,10,p.ink<0.12f?Vector4{1,0.32f,0.15f,1}:kInk);
    const float gap=7+p.accuracy*0.8f;
    const Vector4 cross=swim?Vector4{0.04f,0.9f,0.75f,0.65f}:Vector4{1,1,1,0.92f};
    rect(3,640-gap-8,359,8,2,cross); rect(4,640+gap,359,8,2,cross);
    rect(5,639,360-gap-8,2,8,cross); rect(6,639,360+gap,2,8,cross);
    rect(7,639,359,2,2,cross);
}

void InkShooterScene::DrawPostEffect3D() {
    paint_.Draw(camera_->GetViewProjectionMatrix(),camera_->GetTranslate());
    Object3dCommon::GetInstance()->PreDraw(kNone);
    // Bloom applies the current frame's projection jitter just before this pass.
    for (auto& object:actor_) { object->Update(); object->Draw(); }
    effects_->DrawAll(camera_->GetViewProjectionMatrix());
}

void InkShooterScene::DrawSprite() {
    for (auto& sprite:hud_) sprite->Draw();
    title_->Draw(); guide_->Draw(); stateText_->Draw();
}

void InkShooterScene::DrawDebug() {
#ifdef USE_IMGUI
    if (!debug_) return;
    ImGui::SetNextWindowPos({900,25},ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({365,580},ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Ink shooter / F1",&debug_)) {
        const auto& p=simulation_.Player();
        ImGui::Text("FPS %.1f | fixed simulation 120 Hz",fps_);
        ImGui::Text("State: %s",StateName(p.state));
        ImGui::ProgressBar(p.ink,{-1,0},"Ink tank");
        ImGui::Text("Own ink %s | Grounded %s",p.onOwnInk?"yes":"no",p.grounded?"yes":"no");
        ImGui::Text("Speed %.2f | XYZ %.2f %.2f %.2f",p.speed,p.position.x,p.position.y,p.position.z);
        ImGui::Text("Spread cone %.2f degrees",p.accuracy);
        ImGui::Text("Projectiles %zu | Paint stamps %zu",simulation_.Projectiles().size(),simulation_.StampCount());
        ImGui::TextUnformatted("Weapon: Splattershot-inspired / 11.3.0");
        auto& w=simulation_.weapon;
        ImGui::Text("Fire interval %.3fs | %.1f shots/s",w.FireInterval(),1.0f/w.FireInterval());
        if (ImGui::CollapsingHeader("Weapon and movement tuning")) {
            ImGui::SliderFloat("Repeat frames",&w.repeatFrame,3,15,"%.1f");
            ImGui::SliderFloat("Ink / shot",&w.inkConsume,0.001f,0.04f,"%.4f");
            ImGui::SliderFloat("Recovery lock",&w.inkRecoverStop,0,1,"%.3fs");
            ImGui::SliderFloat("Projectile speed",&w.projectileSpeed,20,100);
            ImGui::SliderFloat("Ground spread",&w.groundSpread,0,15);
            ImGui::SliderFloat("Jump spread",&w.jumpSpread,0,30);
            ImGui::SliderFloat("Droplet spacing",&w.paintDropletSpacing,0.3f,3);
            ImGui::SliderFloat("Impact radius",&w.impactPaintRadius,0.2f,2);
            ImGui::SliderFloat("Human speed",&simulation_.movement.humanSpeed,1,6);
            ImGui::SliderFloat("Swim speed",&simulation_.movement.swimSpeed,2,12);
        }
        ImGui::SliderFloat("Mouse sensitivity",&sensitivity_,0.0005f,0.008f,"%.4f");
        ImGui::Checkbox("Invert mouse pitch",&invertPitch_);
        ImGui::SliderFloat("Camera distance",&cameraDistance_,2,7);
        if (ImGui::Button("Reset stage / R")) Reset();
        ImGui::SameLine(); if (ImGui::Button("Run demo / F9")) StartReplay();
        if (ImGui::Button("Paint climb lane (debug)")) {
            const auto& surfaces=simulation_.Surfaces();
            for (uint32_t n=0;n<static_cast<uint32_t>(surfaces.size());++n) {
                const auto& s=surfaces[n];
                if (!s.inkable) continue;
                for (float y=0;y<s.height;y+=0.6f) {
                    for (float x=0;x<s.width;x+=0.6f) {
                        const auto world=s.origin+s.u*x+s.v*y;
                        if (std::abs(world.x)<1.2f && world.z>-9 && world.z<15)
                            simulation_.Paint({n,x,y,0.6f,0.6f,0,1});
                    }
                }
            }
        }
        ImGui::Separator();
        ImGui::TextUnformatted("Gyro unavailable (no sensor backend)");
        ImGui::TextUnformatted("Raw X/Y/Z: -- / -- / --");
        ImGui::TextWrapped("Mouse is fully supported. SDL3 sensor input is documented as the next optional step.");
        if (ImGui::CollapsingHeader("GPU paint atlas")) {
            const auto handle=Object3dCommon::GetInstance()->GetSrvManager()->GetGPUDescriptorHandle(paint_.GetMaskSrvIndex());
            ImGui::Image(static_cast<ImTextureID>(handle.ptr),{300,300});
        }
    }
    ImGui::End();
#endif
}
