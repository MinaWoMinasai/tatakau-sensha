#include "TitleScene.h"
#include "GameScene.h"
#include "GameStartMode.h"
#include "SceneManager.h"
#include "StartupTrace.h"
#include "game/run/TankSubmissionValidation.h"
#include <algorithm>
#include <cmath>
#include <fstream>

namespace {
void Fit(TextLabel& label,const Vector2& bounds) {
    label.PrepareForDraw();
    if(auto* sprite=label.GetSprite()) {
        const auto size=sprite->GetSize();
        const float scale=(std::min)(bounds.x/(std::max)(size.x,1.0f),bounds.y/(std::max)(size.y,1.0f));
        sprite->SetSize({size.x*scale,size.y*scale});
    }
}
}
TitleScene::TitleScene()=default;
TitleScene::~TitleScene()=default;
void TitleScene::Initialize() {
    StartupTrace::Scope startupScope("TitleScene.Initialize");
    input_=Input::GetInstance();
    finished_=false;demoFrozen_=false;nextSceneName_.clear();
    phase_=Phase::kFadeIn;blinkTimer_=0;capturedStages_=0;
    previousMousePosition_=input_->GetMousePosition();
    wchar_t test[8]{};
    autoTest_=GetEnvironmentVariableW(L"CG2_TITLE_AUTOTEST",test,8)>0&&test[0]==L'1';
    startupAutoTest_=GetEnvironmentVariableW(L"CG2_STARTUP_AUTOTEST",test,8)>0&&test[0]==L'1';
    if(autoTest_) {
        std::filesystem::create_directories("generated/title_demo");
        std::ofstream("generated/title_demo/validation.json")<<"{\"completed\":false}\n";
    }
    // The title owns a real game scene with explicit demo controls. Process
    // input and environment remain untouched throughout the attract sequence.
    demo_=std::make_unique<GameScene>(false,true);
    demo_->EnableTitleDemo();
    { StartupTrace::Scope scope("TitleScene.GameplayDemo"); demo_->Initialize(); }
    stageSamples_={};stageSamples_[0].build=demo_->GetTitleDemoBuild().dump();
    fade_=std::make_unique<Fade>();fade_->Initialize();fade_->Start(Fade::Status::FadeIn,0.65f);
    const float w=static_cast<float>(WinApp::GetInstance()->GetClientWidth());
    const float h=static_cast<float>(WinApp::GetInstance()->GetClientHeight());
    backgroundVeil_=std::make_unique<Sprite>();
    backgroundVeil_->Initialize(SpriteCommon::GetInstance(),"resources/white512x512.png");
    backgroundVeil_->SetPosition({0,0});backgroundVeil_->SetSize({w,h});
    backgroundVeil_->SetColor({0.004f,0.009f,0.016f,0.42f});backgroundVeil_->Update();
    const auto label=[](const std::string& text,float size,Vector2 position,Vector4 color,bool rounded=false) {
        TextStyle style{};style.fontFamily=rounded?"Zen Maru Gothic":"Meiryo";
        if(rounded) style.fontPath="resources/fonts/ZenMaruGothic-Bold.ttf";
        style.fontSize=size;style.fontWeight=rounded?700:400;
        style.color=color;style.outlineThickness=0;style.padding=8;
        auto result=std::make_unique<TextLabel>();
        result->Initialize(SpriteCommon::GetInstance(),text,style);
        result->SetAnchorPoint({0.5f,0.5f});result->SetPosition(position);return result;
    };
    title_=label("たたかうせんしゃ",100,{w*0.5f,h*0.32f},{0.91f,1,0.96f,1},true);
    Fit(*title_,{w*0.76f,118});
    subtitle_=label("強化を重ねて最深部を目指す、ローグライトアクション",23,{w*0.5f,h*0.44f},{0.73f,0.91f,0.94f,1});
    Fit(*subtitle_,{w*0.78f,42});
    menu_[0]=label("遠征をはじめる",31,{w*0.5f,h*0.60f},{0.47f,1,0.76f,1});
    for(auto& item:menu_) Fit(*item,{w*0.72f,58});
    hint_=label("クリックで決定",15,{w*0.5f,h-36},{0.57f,0.71f,0.77f,1});
    demoCaption_=label(" ",13,{w-130,30},{0.47f,0.62f,0.67f,1});
    menuSelection_=0;menuHovered_=HitTestMenu(input_->GetMousePosition())==0;UpdateMenuVisuals();
    if(tanksubmission::Enabled()) {
        ++tanksubmission::state.titles;
        tanksubmission::state.returned=tanksubmission::state.titles==2;
        tanksubmission::state.screens["title"]=nlohmann::json::array({title_->GetText(),subtitle_->GetText(),menu_[0]->GetText(),hint_->GetText()});
        tanksubmission::Write();
    }
    NeonTextEffectStyle neon{};neon.enabled=true;neon.glowColor={0.18f,1,0.56f,1};
    neon.sourceBrightness=1.55f;neon.threshold=0;neon.innerIntensity=0.48f;neon.outerIntensity=0.22f;
    titleTextNeonEffect_=std::make_unique<NeonTextEffect>();
    titleTextNeonEffect_->Initialize(Object3dCommon::GetInstance()->GetDxCommon(),Object3dCommon::GetInstance()->GetSrvManager());
    titleTextNeonEffect_->SetStyle(neon);
}
void TitleScene::Update() {
    constexpr float dt=1.0f/60.0f;
    demo_->FlushTitleDemoCapture();blinkTimer_+=dt;
    const auto triggered=[this](int key){return input_->IsKeyTriggered(static_cast<uint8_t>(key));};
    // Confirmation is evaluated before the background simulation. The first
    // input therefore cannot shoot, move, or select a reward inside the demo.
    if(phase_!=Phase::kFadeOut) {
        const Vector2 mouse=input_->GetMousePosition();
        previousMousePosition_=mouse;
        const bool click=input_->IsTrigger(input_->GetMouseState().rgbButtons[0],input_->GetPreMouseState().rgbButtons[0]);
        const int hovered=HitTestMenu(mouse);
        if(menuHovered_!=(hovered==0)) {menuHovered_=hovered==0;UpdateMenuVisuals();}
        if(cg2::kDeveloperTools&&triggered(DIK_F9)) StartTransitionIfAvailable("TANK_EXPEDITION",0.5f);
        else if(triggered(DIK_RETURN)||triggered(DIK_SPACE)||(click&&hovered>=0)) {
            if(menuSelection_==0) StartTransitionIfAvailable("TANK_EXPEDITION",0.65f);
        }
        if(autoTest_&&capturedStages_==15&&demo_->GetTitleDemoStatus().totalSeconds>=81.0f)
            StartTransitionIfAvailable("TANK_EXPEDITION",0.65f);
        if(startupAutoTest_&&blinkTimer_>=1.0f)
            StartTransitionIfAvailable("TANK_EXPEDITION",0.65f);
        if(tanksubmission::Enabled()) {
            if(!submissionCaptured_&&blinkTimer_>=0.8f) {
                demo_->RequestTitleDemoCapture("submission_title_"+std::to_string(tanksubmission::state.titles));
                submissionCaptured_=true;
            }
            if(blinkTimer_>=1.5f) StartTransitionIfAvailable("TANK_EXPEDITION",0.65f);
        }
    }
    const auto before=demo_->GetTitleDemoStatus();
    // Capture transient effects before the game can restore the next fixed
    // scene. A single still at six seconds cannot prove an Overdrive trigger.
    if(autoTest_) {
        const auto combat=demo_->GetTitleDemoBuild();
        maxObservedBurstAge_=(std::max)(maxObservedBurstAge_,combat.value("oldestBurstAge",0.0f));
        auto& observed=stageSamples_[before.stage];
        observed.maxHomingTurnRate=(std::max)(observed.maxHomingTurnRate,combat.value("homingTurnRate",0.0f));
        observed.maxDashExplosionsEmitted=(std::max)(observed.maxDashExplosionsEmitted,combat.value("dashExplosionsEmitted",0u));
        observed.wallBouncesObserved=(std::max)(observed.wallBouncesObserved,combat.value("wallBouncesObserved",uint64_t{0}));
        observed.actorPiercesObserved=(std::max)(observed.actorPiercesObserved,combat.value("actorPiercesObserved",uint64_t{0}));
        observed.splitChildrenObserved=(std::max)(observed.splitChildrenObserved,combat.value("splitChildrenObserved",uint64_t{0}));
    }
    if(!demoFrozen_) demo_->Update();
    const auto& status=demo_->GetTitleDemoStatus();
    if(autoTest_&&status.stage==0&&status.stageSeconds>19.85f&&!capturedSceneFadeOut_) {
        demo_->RequestTitleDemoCapture("scene_fade_out");capturedSceneFadeOut_=true;
    }
    if(autoTest_&&status.stage==1&&status.stageSeconds<0.10f&&!capturedSceneFadeIn_) {
        demo_->RequestTitleDemoCapture("scene_fade_in");capturedSceneFadeIn_=true;
    }
    auto& sample=stageSamples_[before.stage];
    sample.shots+=(std::max)(0,status.shots-before.shots);
    sample.kills+=(std::max)(0,status.kills-before.kills);
    sample.dashes+=(std::max)(0,status.dashes-before.dashes);
    if(stageSamples_[status.stage].build.empty()) stageSamples_[status.stage].build=demo_->GetTitleDemoBuild().dump();
    const char* captions[]={"プレイ例 / 序盤","プレイ例 / 反射・分裂","プレイ例 / 強化した戦車","プレイ例 / ボス戦"};
    demoCaption_->SetText(captions[status.stage]);
    if(autoTest_&&status.stageSeconds>=6&&!(capturedStages_&(1u<<status.stage))) {
        demo_->RequestTitleDemoCapture("stage_"+std::to_string(status.stage));capturedStages_|=1u<<status.stage;WriteDemoValidation(false);
    }
    menu_[menuSelection_]->SetAlpha(menuHovered_?0.90f+std::sin(blinkTimer_*3.2f)*0.10f:1.0f);
    if(phase_==Phase::kFadeIn||phase_==Phase::kFadeOut) {
        fade_->Update();
        if(fade_->IsFinished()) {
            if(phase_==Phase::kFadeIn) phase_=Phase::kMain;
            else {finished_=true;if(autoTest_) WriteDemoValidation(true);}
        }
    }
}
void TitleScene::Draw(){demo_->Draw();}
void TitleScene::DrawShadow(){demo_->DrawShadow();}
void TitleScene::DrawPostEffect3D(){demo_->DrawPostEffect3D();}
void TitleScene::DrawAfterPostEffect3D(){
    // Dim only the game background; both text glow and sharp source glyphs
    // follow this veil, so moving walls/volleys cannot wash out menu labels.
    SpriteCommon::GetInstance()->PreDraw(kNormal);
    backgroundVeil_->SetColor({0.004f,0.009f,0.016f,0.42f+0.58f*demo_->GetTitleDemoFade()});backgroundVeil_->Update();
    backgroundVeil_->Draw();
    if(titleTextNeonEffect_) {
        if(menuHovered_) titleTextNeonEffect_->DrawBloom({title_.get(),menu_[0].get()});
        else titleTextNeonEffect_->DrawBloom({title_.get()});
    }
}
void TitleScene::DrawSprite(){
    title_->Draw();subtitle_->Draw();
    for(int i=0;i<1;++i) if(IsMenuAvailable(i)) menu_[i]->Draw();
    hint_->Draw();demoCaption_->Draw();fade_->Draw();demo_->CopyTitleDemoCapture();
}
IScene::ScreenEffectState TitleScene::GetScreenEffectState()const {
    ScreenEffectState state{};state.active=true;state.suppressOutlines=true;state.suppressPostEffectDebugUi=true;
    state.param.gaussianIntensity=0.88f;state.param.isGrayscale=0.28f;state.param.exposure=-0.22f;
    state.param.vignetteIntensity=0.30f;state.param.vignetteScale=1.2f;return state;
}
void TitleScene::UpdateMenuVisuals(){
    for(int i=0;i<1;++i){
        const auto size=menu_[i]->GetSprite()->GetSize();auto style=menu_[i]->GetStyle();
        style.color=menuHovered_?Vector4{0.47f,1,0.76f,1}:Vector4{0.48f,0.52f,0.55f,1};
        menu_[i]->SetStyle(style);menu_[i]->PrepareForDraw();menu_[i]->GetSprite()->SetSize(size);
    }
}
bool TitleScene::IsSceneAvailable(std::string_view name)const{return SceneManager::GetInstance()->ContainsScene(name);}
bool TitleScene::IsMenuAvailable(int selection)const{return selection==0&&IsSceneAvailable("TANK_EXPEDITION");}
int TitleScene::HitTestMenu(const Vector2& mouse)const{
    for(int i=0;i<1;++i)if(IsMenuAvailable(i)){
        auto* sprite=menu_[i]->GetSprite();const auto center=sprite->GetPosition(),size=sprite->GetSize();
        if(std::abs(mouse.x-center.x)<=(std::max)(150.0f,size.x*0.5f)&&std::abs(mouse.y-center.y)<=28)return i;
    }return -1;
}
bool TitleScene::StartTransitionIfAvailable(std::string_view name,float duration){
    if(!IsSceneAvailable(name)||phase_==Phase::kFadeOut)return false;
    StartupTrace::Mark("transition.request."+std::string(name));
    demoFrozen_=true;frozenAt_=demo_->GetTitleDemoStatus().totalSeconds;nextSceneName_=name;
    if(name=="TANK_EXPEDITION")GameStartSession::SetMode(GameStartMode::Normal);
    fade_->Start(Fade::Status::FadeOut,duration);phase_=Phase::kFadeOut;return true;
}
void TitleScene::WriteDemoValidation(bool fadeComplete){
    const auto& s=demo_->GetTitleDemoStatus();std::ofstream report("generated/title_demo/validation.json");
    report<<"{\"completed\":false,\"testMode\":true,\"fixedSeed\":20260925,\"stagesVisited\":"<<s.stagesVisitedMask
        <<",\"sceneFadeCaptured\":"<<(capturedSceneFadeOut_&&capturedSceneFadeIn_?"true":"false")<<",\"maxBurstAge\":"<<maxObservedBurstAge_
        <<",\"capturedStages\":"<<capturedStages_<<",\"seconds\":"<<s.totalSeconds
        <<",\"projectileEmissionSamples\":"<<s.shots<<",\"dashes\":"<<s.dashes<<",\"kills\":"<<s.kills
        <<",\"rewards\":"<<s.rewards<<",\"routes\":"<<s.routes<<",\"maxPlayerBullets\":"<<s.maxPlayerBullets
        <<",\"fadeComplete\":"<<(fadeComplete?"true":"false")
        <<",\"demoFrozenDuringFade\":"<<(demoFrozen_&&s.totalSeconds==frozenAt_?"true":"false")
        <<",\"transitionRequested\":\""<<nextSceneName_<<"\",\"stageSamples\":[";
    for(size_t i=0;i<stageSamples_.size();++i) {
        const auto& sample=stageSamples_[i];
        report<<(i?",":"")<<"{\"build\":"<<(sample.build.empty()?"{}":sample.build)
            <<",\"projectileEmissionSamples\":"<<sample.shots<<",\"kills\":"<<sample.kills<<",\"dashes\":"<<sample.dashes
            <<",\"maxHomingTurnRate\":"<<sample.maxHomingTurnRate<<",\"maxDashExplosionsEmitted\":"<<sample.maxDashExplosionsEmitted
            <<",\"wallBouncesObserved\":"<<sample.wallBouncesObserved<<",\"actorPiercesObserved\":"<<sample.actorPiercesObserved
            <<",\"splitChildrenObserved\":"<<sample.splitChildrenObserved<<"}";
    }
    report<<"]}\n";
}
