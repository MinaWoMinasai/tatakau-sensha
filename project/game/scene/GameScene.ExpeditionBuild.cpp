#include "GameScene.h"
#include "StartupTrace.h"
#include "game/run/TankSubmissionValidation.h"

bool GameScene::IsExpeditionBuildCardScreen() const {
    if(!expeditionMapEnabled_||tankRunPaused_||expeditionMapPreview_) return false;
    if(expeditionBuildChoice_) return true;
    const auto* node=expeditionMapRun_.GetActiveNode();
    return node&&(node->kind==tankexp::NodeKind::Upgrade||node->kind==tankexp::NodeKind::Evolution);
}

void GameScene::InitializeExpeditionBuildCards() {
    cg2::StartupTrace::Scope scope("Expedition.BuildCards");
    for(auto& card:expeditionRewardCards_) {
        card=std::make_unique<TankRewardCard>();card->Initialize(cg2::SpriteCommon::GetInstance());
        card->InitializePreview(cg2::Object3dCommon::GetInstance()->GetSrvManager());
    }
    // Authored maps without the two introductory branches still offer a style
    // before their first encounter. The isolated AI fixture has no map input.
    const auto& nodes=expeditionMapRun_.GetDefinition().nodes;
    expeditionBuildChoice_=!combatValidationEnabled_&&!specialValidationEnabled_&&std::none_of(nodes.begin(),nodes.end(),[](const auto& n){return tankexp::IsIntroUpgrade(n.role);});
    RefreshExpeditionBuildCards();
    UpdateExpeditionBuildCards(0);
}

void GameScene::SelectExpeditionBuildStyle(int index) {
    if(!expeditionBuildChoice_||expeditionBuildChosen_||index<0||index>2) return;
    const auto style=static_cast<tankbuild::Style>(index);
    if(!expeditionTransition_.IsActive()) {
        expeditionPendingBuild_=index;expeditionRewardCards_[index]->PlayAcquire();
        BeginExpeditionPresentation(4,std::string(tankbuild::Name(style))+" / 機体準備",
            "取得した強化・現在HP・通貨を引き継ぎます",{0.65f,0.94f,1,1});
        return;
    }
    const int hp=player_->GetHp(),maxHp=player_->GetMaxHp(),credits=expeditionMapRun_.GetCurrency();
    const auto cards=tankRun_.GetCardCounts();
    if(!player_->SetExpeditionCombatStyle(style)) return;
    expeditionBuildStyle_=style;expeditionBuildChosen_=true;expeditionBuildChoice_=false;
    ApplyTankRunCards();
    if(experienceValidationVariant_) {
        experienceBuildPreserved_=hp==player_->GetHp()&&maxHp==player_->GetMaxHp()&&credits==expeditionMapRun_.GetCurrency()&&cards==tankRun_.GetCardCounts();
        if(!experienceBuildPreserved_) experienceValidationErrors_.push_back("Style choice changed health, currency or purchased modules");
        if(style==tankbuild::Style::Drone&&player_->GetDronePtrs().size()!=3) experienceValidationErrors_.push_back("Drone style did not equip three companions immediately");
    }
    expeditionGuideActive_=false;tankRunMenuAge_=0;tankRunSelection_=0;
    expeditionLastFocus_.clear();expeditionMapStatus_.clear();
    tankExpeditionAudio_.Upgrade();RefreshTankExpeditionUi();
}

void GameScene::RefreshExpeditionBuildCards() {
    if(!IsExpeditionBuildCardScreen()||!expeditionRewardCards_[0]) return;
    const auto current=player_->GetRunCombatSnapshot();
    for(int i=0;i<3;++i) {
        const size_t offerIndex=static_cast<size_t>(expeditionServicePage_*3+i);
        if(!expeditionBuildChoice_&&(offerIndex>=expeditionServiceOffers_.size()||
            !tankcontent::FindUpgrade(expeditionContent_,expeditionServiceOffers_[offerIndex]))) continue;
        TankRewardCardModel model;model.style=expeditionBuildStyle_;model.ownedEffects=tankRun_.GetCardCounts();
        model.profile=player_->GetCombatStyleProfile(model.style);model.ownedEffectPower=ExpeditionEffectPowers();
        const auto& growth=tankExpeditionBalance_["playerUpgrades"];
        model.growth={growth.value("maxHp",.15f),growth.value("bulletDamage",.25f),growth.value("bulletSpeed",.2f),growth.value("reloadSpeed",.25f),growth.value("moveSpeed",.12f)};
        model.drones=model.style==tankbuild::Style::Drone?(std::clamp)(current.baseDroneCount+(current.isAuthored?current.classDroneCount-3:0),1,12):0;
        model.currentDamageScale=model.damageScale=current.classDamageScale;
        model.currentReloadScale=model.reloadScale=current.classReloadScale;
        model.currentBulletSpeedScale=model.bulletSpeedScale=current.classBulletSpeedScale;
        model.barrels=(std::max)(1,current.barrels);model.reflect=current.classReflects;model.penetrate=current.classPenetrates;
        model.currentBarrels=model.barrels;model.currentDrones=model.drones;
        model.currentReflect=current.classReflects;model.currentPenetrate=current.classPenetrates;
        if(const auto* variant=tankcontent::FindPlayer(expeditionContent_,current.classId)) {
            model.fanAngle=model.currentFanAngle=variant->fanAngle;
            model.alternate=model.currentAlternate=variant->alternate;
        }
        if(expeditionBuildChoice_) {
            model.style=static_cast<tankbuild::Style>(i);model.profile=player_->GetCombatStyleProfile(model.style);model.styleChoice=true;model.rarity=0;
            model.id=std::string("style_")+tankbuild::Id(model.style);model.title=tankbuild::Name(model.style);
            model.description=i==0?"狙って撃つ。追尾・跳弾・貫通で射撃を育てる。":
                i==1?"追従するドローンを左クリックで指揮。集中射撃や迎撃で支える。":
                     "左クリックで3段斬り。間合い・連撃・フィニッシュを育てる。";
            model.footer="左クリックで選択";model.drones=i==1?model.profile.droneCount:0;
            model.currentDrones=model.drones;model.barrels=model.currentBarrels=1;
            model.fanAngle=model.currentFanAngle=0;model.alternate=model.currentAlternate=false;
            model.currentDamageScale=model.damageScale=model.currentReloadScale=model.reloadScale=model.currentBulletSpeedScale=model.bulletSpeedScale=1.0f;
        } else {
            model.id=expeditionServiceOffers_[offerIndex];
            const int price=ExpeditionServicePrice(model.id);
            model.footer=std::to_string(price)+(expeditionMapRun_.CanAfford(price)?"  / 左クリックで装備":"  / 通貨が足りません");
            const auto* u=tankcontent::FindUpgrade(expeditionContent_,model.id);
            model.title=u->name;model.description=u->description;model.rarity=u->rarity;model.effects=u->effects;model.effectPower=u->effectPower;
        }
        if(experienceValidationVariant_&&!tanksubmission::Enabled()&&expeditionBuildChoice_&&experienceValidationStateAge_>0.55f) {
            model.rarity=experiencePreviewRarity_;model.styleChoice=false;model.authoredVariant="visual_fixture";
        }
        TankRewardPreviewAppearance appearance;
        appearance.playerRadius=playerNeonBillboardRadius_;
        appearance.lineWidth=actorNeonBillboardLineWidth_;
        appearance.softEdgeRatio=neonLineSoftEdgeRatio_;appearance.coreIntensity=neonLineCoreIntensity_;
        appearance.bodyFill=actorNeonBodyFillColor_;appearance.gridColor=worldGridColor_;
        const auto body=player_->GetNeonBodyLayout();
        appearance.playerColor=body.outlineColor;appearance.bodyScale=body.scale;
        const int shapes[]={28,4,3,5};appearance.bodySegments=shapes[(std::clamp)(static_cast<int>(body.shape),0,3)];
        appearance.separateCurrentBody=!expeditionBuildChoice_;
        appearance.currentPlayerColor=appearance.playerColor;appearance.currentBodySegments=appearance.bodySegments;appearance.currentBodyScale=appearance.bodyScale;
        appearance.currentPlayerColor.x*=playerNeonEmission_;appearance.currentPlayerColor.y*=playerNeonEmission_;appearance.currentPlayerColor.z*=playerNeonEmission_;
        if(expeditionBuildChoice_) {appearance.playerColor={0.5f,1,0.35f,1};appearance.bodySegments=28;appearance.bodyScale={1,1};}
        if(const auto* variant=tankcontent::FindPlayer(expeditionContent_,model.authoredVariant)) {
            appearance.playerColor={variant->color[0],variant->color[1],variant->color[2],variant->color[3]};
            appearance.bodySegments=shapes[variant->bodyShape];
        }
        appearance.playerColor.x*=playerNeonEmission_;appearance.playerColor.y*=playerNeonEmission_;appearance.playerColor.z*=playerNeonEmission_;
        appearance.meleeColor.x*=playerNeonEmission_;appearance.meleeColor.y*=playerNeonEmission_;appearance.meleeColor.z*=playerNeonEmission_;
        appearance.bloomThreshold=neonGridPostEffect_->GetParam().threshold;
        appearance.bloomIntensity=enableNeonGridPostEffect_?neonGridPostEffect_->GetParam().intensity:0;
        appearance.blade={playerMeleeBladeOuterWidthScale_,playerMeleeBladeHaloWidthScale_,playerMeleeBladeCoreWidthScale_};
        appearance.bulletTrail=bulletManager_->GetTrailSettings();
        expeditionRewardCards_[i]->SetPreviewAppearance(appearance);
        expeditionRewardCards_[i]->SetModel(model);
    }
}

void GameScene::UpdateExpeditionBuildCards(float dt) {
    if(!IsExpeditionBuildCardScreen()||!expeditionRewardCards_[0]) return;
    const auto mouse=input_->GetMousePosition();int hovered=-1;
    for(int i=0;i<3;++i) {
        const size_t index=static_cast<size_t>(expeditionServicePage_*3+i);
        const bool present=expeditionBuildChoice_||(index<expeditionServiceOffers_.size()&&tankcontent::FindUpgrade(expeditionContent_,expeditionServiceOffers_[index]));
        if(present&&mouse.x>=64+i*388&&mouse.x<=432+i*388&&mouse.y>=260&&mouse.y<=590) hovered=i;
    }
    if(experienceValidationVariant_) hovered=expeditionBuildChoice_?experienceValidationStyle_:tankRunSelection_;
    for(int i=0;i<3;++i) {
        const size_t index=static_cast<size_t>(expeditionServicePage_*3+i);
        const bool present=expeditionBuildChoice_||(index<expeditionServiceOffers_.size()&&tankcontent::FindUpgrade(expeditionContent_,expeditionServiceOffers_[index]));
        if(!present) continue;
        expeditionRewardCards_[i]->Update({248.0f+i*388.0f,425},{368,330},dt,i==hovered&&!expeditionTransition_.IsActive(),true);
    }
    const std::string focus="card_"+std::to_string(hovered);
    if(expeditionBuildChoice_&&focus!=expeditionLastFocus_) {
        if(hovered>=0&&!expeditionLastFocus_.empty()&&!expeditionTransition_.IsActive()) tankExpeditionAudio_.UiHover();
        expeditionLastFocus_=focus;
    }
}
