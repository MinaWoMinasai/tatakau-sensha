#include "GameScene.h"
#include "game/run/TankTutorialCopy.h"
#include <cmath>

namespace {
constexpr cg2::Vector2 kWallet{304,32};
std::unique_ptr<cg2::Sprite> ExperienceSprite(const char* texture,cg2::Vector2 p,cg2::Vector2 size,const cg2::Vector4& tint) {
    auto sprite=std::make_unique<cg2::Sprite>();sprite->Initialize(cg2::SpriteCommon::GetInstance(),texture);
    sprite->SetPosition(p);sprite->SetSize(size);sprite->SetColor(tint);sprite->Update();return sprite;
}
std::unique_ptr<cg2::TextLabel> ExperienceText(float size,cg2::Vector2 p,const char* text,const cg2::Vector4& color) {
    cg2::TextStyle style{};style.fontFamily="Meiryo";style.fontSize=size;style.color=color;style.padding=4;style.outlineThickness=0;
    auto label=std::make_unique<cg2::TextLabel>();label->Initialize(cg2::SpriteCommon::GetInstance(),text,style);label->SetPosition(p);return label;
}
}

void GameScene::InitializeExpeditionExperience() {
    expeditionCompleteGlow_=std::make_unique<NeonTextEffect>();expeditionCompleteGlow_->Initialize(cg2::Object3dCommon::GetInstance()->GetDxCommon(),cg2::Object3dCommon::GetInstance()->GetSrvManager());
    NeonTextEffectStyle completeStyle;completeStyle.glowColor={0.15f,1,0.38f,1};completeStyle.innerIntensity=0.65f;completeStyle.outerIntensity=0.42f;expeditionCompleteGlow_->SetStyle(completeStyle);
    expeditionCreditIcon_=ExperienceSprite("resources/ui/salvage_orb.png",kWallet,{54,54},{1.5f,1.15f,0.34f,1});
    expeditionCreditPulse_=ExperienceSprite("resources/ui/salvage_orb.png",kWallet,{70,70},{1.4f,1.05f,0.2f,0});
    expeditionCreditIcon_->SetAnchorPoint({0.5f,0.5f});expeditionCreditPulse_->SetAnchorPoint({0.5f,0.5f});
    expeditionCreditText_=ExperienceText(25,{328,12},"20",{1,0.91f,0.57f,1});
    expeditionStaminaTrack_=ExperienceSprite("resources/white512x512.png",{24,66},{230,5},{0.17f,0.14f,0.05f,0.9f});
    expeditionStaminaFill_=ExperienceSprite("resources/white512x512.png",{24,66},{230,5},{1,0.80f,0.16f,1});
    for(auto& shade:expeditionSpotlight_) shade=ExperienceSprite("resources/white512x512.png",{0,0},{1,1},{0.002f,0.006f,0.014f,0.82f});
    EnsureExpeditionPointers(6);
    expeditionPriceIcon_=ExperienceSprite("resources/ui/salvage_orb.png",{0,0},{30,30},{1.5f,1.15f,0.34f,1});
    expeditionPriceIcon_->SetAnchorPoint({0.5f,0.5f});
    expeditionContinueButton_=ExperienceSprite("resources/white512x512.png",{502,530},{276,48},{0.035f,0.20f,0.22f,1});
    expeditionContinueText_=ExperienceText(18,{640,542},"左クリックで続ける",{0.76f,1,0.94f,1});expeditionContinueText_->SetAnchorPoint({0.5f,0});
    expeditionSkipButton_=ExperienceSprite("resources/white512x512.png",{966,620},{242,44},{0.045f,0.09f,0.13f,1});
    expeditionSkipText_=ExperienceText(18,{1087,629},"購入せず進む",{0.68f,0.85f,0.92f,1});expeditionSkipText_->SetAnchorPoint({0.5f,0});
    expeditionCredits_.reserve(96);
}

void GameScene::SpawnExpeditionCredits(const cg2::Vector3& position,int amount,bool flyImmediately) {
    if(!expeditionMapEnabled_||amount<=0) return;
    // A bounded number of visible balls preserves the exact sum for large rewards.
    const int count=(std::min)(5,(std::max)(1,amount/3));
    for(int i=0;i<count;++i) {
        const int value=amount/count+(i<amount%count?1:0);
        if(expeditionCredits_.size()>=96) {expeditionCredits_.back().value+=value;continue;}
        ExpeditionCreditOrb orb;orb.position=position;orb.value=value;
        const float angle=static_cast<float>(i)*2.399963f+expeditionPresentationClock_*1.7f;
        orb.velocity={std::cos(angle)*4.5f,std::sin(angle)*4.5f,0};
        orb.flying=flyImmediately;orb.launch=WorldToScreen(position);
        orb.sprite=ExperienceSprite("resources/ui/salvage_orb.png",orb.launch,{32,32},{1.45f,1.05f,0.25f,1});
        orb.sprite->SetAnchorPoint({0.5f,0.5f});
        expeditionCredits_.push_back(std::move(orb));
    }
}

void GameScene::UpdateExpeditionCredits(float dt,bool collectAll) {
    expeditionCreditPulseAge_=(std::max)(0.0f,expeditionCreditPulseAge_-dt);
    if(!player_||tankRunPaused_||expeditionMapPreview_||IsGuidedExpeditionPaused()) return;
    const auto playerPosition=player_->GetWorldPosition();
    for(auto& orb:expeditionCredits_) {
        orb.age+=dt;
        if(!orb.flying) {
            const auto next=orb.position+orb.velocity*dt;
            if(!stage_->IsCollisionWithAnyBlock(next,0.18f)) orb.position=next;
            else orb.velocity=orb.velocity*-0.3f;
            orb.velocity=orb.velocity*std::exp(-dt*5.5f);
            if(collectAll||(orb.age>0.25f&&cg2::Length(orb.position-playerPosition)<3.0f)) {
                orb.flying=true;orb.launch=WorldToScreen(orb.position);orb.flight=0;
            }
        } else {
            orb.flight+=dt;
            if(orb.flight>=0.55f) {
                expeditionMapRun_.EarnCurrency(orb.value);expeditionCreditsCollected_+=orb.value;
                if(expeditionGuideActive_) expeditionGuide_.RecordCurrencyCollected(orb.value);
                tankExpeditionAudio_.Collect(orb.value);expeditionCreditPulseAge_=0.25f;orb.value=0;tankRunHudTimer_=0;
            }
        }
    }
    std::erase_if(expeditionCredits_,[](const ExpeditionCreditOrb& orb){return orb.value==0;});
}

void GameScene::DrawExpeditionVitals() {
    if(!expeditionCreditIcon_) return;
    cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
    const auto& stats=player_->GetStats();
    const float stamina=stats.maxStamina>0?(std::clamp)(stats.stamina/stats.maxStamina,0.0f,1.0f):0;
    expeditionStaminaTrack_->Draw();
    if(stamina>0) {expeditionStaminaFill_->SetSize({230*stamina,5});expeditionStaminaFill_->Update();expeditionStaminaFill_->Draw();}
    expeditionCreditIcon_->Update();expeditionCreditIcon_->Draw();
    const float pulse=expeditionCreditPulseAge_/0.25f;
    expeditionCreditPulse_->SetSize({60+(1-pulse)*42,60+(1-pulse)*42});expeditionCreditPulse_->SetColor({1.4f,1.08f,0.3f,pulse*0.75f});
    expeditionCreditPulse_->Update();expeditionCreditPulse_->Draw();
    expeditionCreditText_->Draw();
}

void GameScene::DrawExpeditionCredits() {
    if(!expeditionMapEnabled_) return;
    cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
    for(auto& orb:expeditionCredits_) {
        cg2::Vector2 point=WorldToScreen(orb.position);
        if(orb.flying) {
            const float t=(std::clamp)(orb.flight/0.55f,0.0f,1.0f),ease=t*t*(3-2*t);
            point={orb.launch.x+(kWallet.x-orb.launch.x)*ease,orb.launch.y+(kWallet.y-orb.launch.y)*ease-std::sin(t*3.14159265f)*80};
        } else point.y-=std::sin(orb.age*5.5f)*3;
        const float size=orb.flying?30.0f:34+std::sin(orb.age*4.5f)*3;
        orb.sprite->SetPosition(point);orb.sprite->SetSize({size,size});orb.sprite->Update();orb.sprite->Draw();
    }
}

void GameScene::DrawExpeditionPointer(cg2::Vector2 target,bool right) {
    target.x=(std::clamp)(target.x,64.0f,1216.0f);
    const float sign=right?1.0f:-1.0f;
    const float motion=std::sin(expeditionPresentationClock_*5.0f)*5;
    for(int i=0;i<3;++i) for(int side=0;side<2;++side) {
        if(expeditionPointerCursor_>=expeditionPointer_.size()||!expeditionPointer_[expeditionPointerCursor_]) return;
        auto& line=expeditionPointer_[expeditionPointerCursor_];
        auto& halo=expeditionPointerGlow_[expeditionPointerCursor_++];
        line->SetPosition({target.x-sign*(24+i*14-motion)+(side?sign*13.0f:0),target.y+(side?0:-11)});
        line->SetRotation(side?(right?2.44f:0.70f):(right?0.70f:2.44f));
        // UI is drawn after post processing. Reuse the cached soft neon orb as
        // an additive halo, without allocating a render target for each arrow.
        const auto p=line->GetPosition();const float angle=side?(right?2.44f:0.70f):(right?0.70f:2.44f);
        halo->SetPosition({p.x+std::cos(angle)*8.5f,p.y+std::sin(angle)*8.5f});
        halo->SetSize({42,22});halo->SetRotation(angle);
        halo->SetColor({0.15f,0.8f,0.85f,0.45f-i*0.08f});halo->Update();
        cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kAdd);halo->Draw();
        cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
        line->SetColor({0.65f,1.15f,1.1f,0.95f-i*0.18f});line->Update();line->Draw();
    }
}

void GameScene::DrawCurrencyIcon(cg2::Vector2 center,float size) {
    expeditionPriceIcon_->SetPosition(center);expeditionPriceIcon_->SetSize({size,size});expeditionPriceIcon_->Update();expeditionPriceIcon_->Draw();
}

void GameScene::EnsureExpeditionPointers(size_t count) {
    for(size_t i=0;i<(std::min)(count,expeditionPointer_.size());++i) if(!expeditionPointer_[i]) {
        expeditionPointer_[i]=ExperienceSprite("resources/white512x512.png",{0,0},{17,3},{1,1,1,1});
        expeditionPointer_[i]->SetAnchorPoint({0,0.5f});
        expeditionPointerGlow_[i]=ExperienceSprite("resources/ui/salvage_orb.png",{0,0},{42,22},{1,1,1,1});
        expeditionPointerGlow_[i]->SetAnchorPoint({0.5f,0.5f});
    }
}

void GameScene::QueueExpeditionImpact(const cg2::Vector3& position,const cg2::Vector3& direction,bool finisher) {
    if(!expeditionRun_) return;
    cg2::ParticleManager::GetInstance()->EmitNeonImpactEffect(position,direction,{1.45f,0.95f,0.28f,1},finisher?16:10);
    if(tankRunBursts_.size()<24) tankRunBursts_.push_back({position,0,false});
    cameraShakeDuration_=finisher?0.15f:0.10f;cameraShakeTimer_=cameraShakeDuration_;cameraShakePower_=finisher?0.20f:0.11f;
    // Only committed melee/body hits get a short, bounded impact hold.
    expeditionImpactHold_=(std::max)(expeditionImpactHold_,finisher?0.065f:0.04f);
    tankExpeditionAudio_.Slam(finisher);
}

bool GameScene::IsGuidedExpeditionPaused() const {
    if(!expeditionMapEnabled_||!expeditionGuideActive_||expeditionTransition_.IsActive()) return false;
    using S=tankexp::GuidedCombatTutorial::Stage;
    return expeditionGuide_.GetStage()==S::Briefing||expeditionGuide_.GetStage()==S::Vitals;
}

void GameScene::AcknowledgeGuidedExpedition() {
    using S=tankexp::GuidedCombatTutorial::Stage;
    const auto before=expeditionGuide_.GetStage();
    if(!expeditionGuide_.Acknowledge()) return;
    expeditionGuideAge_=0;tankExpeditionAudio_.UiConfirm();
    if(before==S::Vitals&&!expeditionGuideShooterSpawned_) {
        enemyManager_->SpawnLevelEnemy({52,30,0},"tutorial_shooter",24);expeditionGuideShooterSpawned_=true;
    }
}

void GameScene::UpdateGuidedExpedition(float dt) {
    if(!expeditionGuideActive_||expeditionTransition_.IsActive()) return;
    expeditionGuideAge_+=dt;
    using S=tankexp::GuidedCombatTutorial::Stage;
    const auto before=expeditionGuide_.GetStage();
    const auto mouse=input_->GetMouseState(),previous=input_->GetPreMouseState();
    const bool click=input_->IsTrigger(mouse.rgbButtons[0],previous.rgbButtons[0]);
    if(IsGuidedExpeditionPaused()) {
        if(click&&expeditionGuideAge_>0.25f) {
            AcknowledgeGuidedExpedition();
        }
        return;
    }
    if(tankExpedition_.IsCombat()) {
        if(player_->GetPrimaryAttackCount()>expeditionGuideAttackCount_) expeditionGuide_.RecordShot();
        while(expeditionGuideLastKills_<defeatedEnemies_) {++expeditionGuideLastKills_;expeditionGuide_.RecordEnemyDefeat();}
        if(player_->GetDamageTakenCount()!=expeditionGuideDamageCount_) expeditionGuide_.RecordDamage();
        expeditionGuideDamageCount_=player_->GetDamageTakenCount();
        expeditionGuide_.ObserveDash(player_->IsDashing(),player_->GetHp());
        // A destroyed practice turret is replaced until the dodge is actually completed.
        if(expeditionGuide_.GetStage()==S::Dash&&expeditionGuideShooterSpawned_) {
            bool alive=false;for(auto* e:enemyManager_->GetEnemyPtrs()) if(e&&!e->IsDead()) alive=true;
            if(!alive) enemyManager_->SpawnLevelEnemy({52,30,0},"tutorial_retry",24);
        }
    }
    if(before!=expeditionGuide_.GetStage()) {expeditionGuideAge_=0;tankExpeditionAudio_.UiConfirm();}
}

void GameScene::DrawGuidedExpedition() {
    if(!expeditionMapEnabled_||!expeditionGuideActive_||expeditionTransition_.IsActive()||tankRunPaused_) return;
    using S=tankexp::GuidedCombatTutorial::Stage;
    const auto step=expeditionGuide_.GetStage();
    if(step==S::Complete) return;
    const auto* active=expeditionMapRun_.GetActiveNode();
    if(!active) return;
    if(step==S::Upgrade&&active->kind!=tankexp::NodeKind::Upgrade) return;
    cg2::SpriteCommon::GetInstance()->PreDraw(cg2::kNormal);
    cg2::Vector2 target=WorldToScreen(player_->GetWorldPosition());
    if(step==S::ShootKill||step==S::Briefing) {
        for(auto* e:enemyManager_->GetEnemyPtrs()) if(e&&!e->IsDead()) {target=WorldToScreen(e->GetWorldPosition());break;}
    } else if(step==S::Collect&&!expeditionCredits_.empty()) target=WorldToScreen(expeditionCredits_.front().position);
    else if(step==S::Vitals) target={264,72};
    else if(step==S::Upgrade) target={70,405};
    if(IsGuidedExpeditionPaused()) {
        const bool vitals=step==S::Vitals;
        const float x=vitals?12:(std::clamp)(target.x-74,12.0f,1110.0f),y=vitals?8:(std::clamp)(target.y-74,90.0f,480.0f);
        const float w=vitals?252.0f:148.0f,h=vitals?78.0f:148.0f;
        const cg2::Vector2 positions[]={{0,0},{0,y},{x+w,y},{0,y+h}};
        const cg2::Vector2 sizes[]={{1280,y},{x,h},{1280-x-w,h},{1280,720-y-h}};
        for(int i=0;i<4;++i) {expeditionSpotlight_[i]->SetPosition(positions[i]);expeditionSpotlight_[i]->SetSize(sizes[i]);expeditionSpotlight_[i]->Update();expeditionSpotlight_[i]->Draw();}
    }
    if(step!=S::Upgrade) DrawExpeditionPointer(target,step!=S::Vitals);
    tutorialPanel_->Draw();tutorialTitleText_->Draw();tutorialInputText_->Draw();tutorialDescriptionText_->Draw();
    if(IsGuidedExpeditionPaused()) {expeditionContinueButton_->Draw();expeditionContinueText_->Draw();}
}

void GameScene::RefreshGuidedExpeditionUi() {
    if(expeditionCreditText_) expeditionCreditText_->SetText(std::to_string(expeditionMapRun_.GetCurrency()));
    if(!expeditionGuideActive_) return;
    using S=tankexp::GuidedCombatTutorial::Stage;
    const auto step=expeditionGuide_.GetStage();
    const auto copy=tankexp::GuidedTutorialCopy(step,expeditionGuide_.GetFailedDashAttempts()>0);
    const float y=IsGuidedExpeditionPaused()?422.0f:step==S::Upgrade?124.0f:594.0f;
    tutorialPanel_->SetPosition({260,y});tutorialPanel_->SetSize({760,104});tutorialPanel_->SetColor({0.007f,0.025f,0.04f,0.94f});tutorialPanel_->Update();
    tutorialTitleText_->SetText("チュートリアル");tutorialTitleText_->SetPosition({640,y+5});
    tutorialInputText_->SetText(copy.heading);tutorialInputText_->SetPosition({640,y+28});
    tutorialDescriptionText_->SetText(step==S::Dash?std::to_string(expeditionGuide_.GetCompletedDashes())+" / 3回  ・  "+copy.detail:copy.detail);tutorialDescriptionText_->SetPosition({640,y+67});
    tutorialTitleText_->PrepareForDraw();tutorialInputText_->PrepareForDraw();tutorialDescriptionText_->PrepareForDraw();
}
