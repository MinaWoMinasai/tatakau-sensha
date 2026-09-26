#include "GameScene.h"
#include "Enemy.h"
#include <fstream>
#include <iomanip>

namespace {
constexpr uint32_t kExperienceSeed=20260926;
std::string ExperienceDirectory(int variant) {
    return std::string("generated/experience_validation/")+(variant==1?"upper/":"lower/");
}
}

void GameScene::InitializeExperienceValidation() {
    wchar_t flag[8]{};
    if(titleDemo_||GetEnvironmentVariableW(L"CG2_TANK_EXPERIENCE_AUTOTEST",flag,8)==0||(flag[0]!=L'1'&&flag[0]!=L'2')) return;
    experienceValidationVariant_=flag[0]==L'1'?1:2;
    wchar_t styleFlag[8]{};
    if(GetEnvironmentVariableW(L"CG2_TANK_EXPERIENCE_STYLE",styleFlag,8)>0&&styleFlag[0]>=L'0'&&styleFlag[0]<=L'2') experienceValidationStyle_=styleFlag[0]-L'0';
    expeditionSeed_=kExperienceSeed;
    expeditionMapDefinition_=tankexp::GenerateExpeditionMap(expeditionSeed_,30,8);
    expeditionIntroOffers_=tankcontent::IntroUpgradeIds(expeditionContent_,expeditionSeed_);
    expeditionMapAutoTest_=tankRunAutoTest_=combatValidationEnabled_=false;
    tutorialConfig_.enabled=false;
    debugPlayerNoDamage_=false;
    tankExpeditionMusicEnabled_=tankExpeditionEffectsEnabled_=false;
    tankExpeditionAudio_.SetMusicVolume(0);tankExpeditionAudio_.SetEffectsVolume(0);
    std::filesystem::create_directories(ExperienceDirectory(experienceValidationVariant_));
    WriteExperienceValidationReport(false);
}

void GameScene::CaptureExperienceValidation(const std::string& name) {
    if(!tankRunCapturePath_.empty()||std::find(experienceValidationCaptures_.begin(),experienceValidationCaptures_.end(),name)!=experienceValidationCaptures_.end()) return;
    experienceValidationCaptures_.push_back(name);
    tankRunCapturePath_=ExperienceDirectory(experienceValidationVariant_)+name+".png";
}

void GameScene::WriteExperienceValidationReport(bool completed) {
    const auto& map=expeditionMapRun_.GetDefinition();
    nlohmann::json report={
        {"completed",completed},{"testMode",true},{"variant",experienceValidationVariant_},{"seed",expeditionSeed_},
        {"elapsed",experienceValidationElapsed_},{"state",experienceValidationState_},
        {"forcedTutorialClear",false},{"forcedLaterCombat",true},{"forcedLaterClears",experienceForcedLaterClears_},
        {"tutorialInvulnerable",false},{"laterInvulnerable",true},
        {"meleeModuleGrantedForProbe",false},{"meleeFixtureReplacesFirstNormalRoomEnemies",true},
        {"buildStyle",tankbuild::Id(static_cast<tankbuild::Style>(experienceValidationStyle_))},{"buildPreserved",experienceBuildPreserved_},
        {"droneSamples",experienceDroneSamples_},{"rarityScreensAreVisualFixtures",true},
        {"refitVerified",experienceEvolutionVerified_},{"refitOfferSeedIsFixture",true},
        {"initialKills",experienceInitialKills_},{"initialPlayerBulletSamples",experiencePlayerBulletSamples_},
        {"guideStageMask",experienceGuideStageMask_},{"successfulDashes",experienceSuccessfulDashes_},
        {"introWallet",experienceIntroWallet_},{"afterIntroWallet",experienceAfterIntroWallet_},
        {"introOffers",experienceValidationIntroOffers_},{"introOfferDetails",experienceIntroOfferDetails_},
        {"earlyFlightSamples",experienceEarlyFlightSamples_},{"prematureCredits",experiencePrematureCredits_},
        {"groundOrbSamples",experienceGroundOrbSamples_},{"creditsCollected",expeditionCreditsCollected_},
        {"meleeProbeCompleted",experienceMeleeDone_},{"meleeTargetHp",experienceMeleeMinHp_},
        {"meleeSlashSamples",experienceMeleeSlashSamples_},{"meleePlayerBulletSamples",experienceMeleeBulletSamples_},
        {"meleeTargetDisplacement",experienceMeleeDisplacement_},
        {"visited",expeditionMapRun_.GetVisitedNodeIds()},{"mapNodeCount",map.nodes.size()},
        {"credits",expeditionMapRun_.GetCurrency()},{"level",player_->GetLevel()},{"experience",player_->GetExp()},
        {"errors",experienceValidationErrors_},{"captures",experienceValidationCaptures_}
    };
    std::ofstream(ExperienceDirectory(experienceValidationVariant_)+"validation.json")<<std::setw(2)<<report<<'\n';
}

void GameScene::BeginExperienceMeleeProbe() {
    experienceMeleeStarted_=true;experienceMeleeAge_=0;
    // A single passive high-HP target isolates real melee collision/knockback.
    // This fixture substitution happens only after the normal introductory flow.
    enemyManager_->ClearRunActors();bulletManager_->ClearAll();
    if(!player_->HasExpeditionCombatStyle()||player_->GetExpeditionCombatStyle()!=static_cast<tankbuild::Style>(experienceValidationStyle_))
        experienceValidationErrors_.push_back("Chosen base style was not equipped after introduction");
    ApplyTankRunCards();
    const Vector3 position=player_->GetWorldPosition();
    const std::array<Vector3,4> directions{Vector3{1,0,0},Vector3{0,1,0},Vector3{-1,0,0},Vector3{0,-1,0}};
    bool spawned=false;
    for(const auto& direction:directions) {
        const Vector3 target=position+direction*3.1f;
        if(stage_->IsCollisionWithAnyBlock(target,1.0f)||stage_->IsCollisionWithAnyBlock(position+direction*1.6f,0.8f)) continue;
        if(enemyManager_->SpawnLevelEnemy(target,"tutorial_target",500)) {
            experienceMeleeTargetStart_=target;spawned=true;break;
        }
    }
    if(!spawned) experienceValidationErrors_.push_back("No safe melee probe placement");
}

bool GameScene::UpdateExperienceValidation(float dt) {
    if(!experienceValidationVariant_) return false;
    using S=tankexp::GuidedCombatTutorial::Stage;
    experienceValidationElapsed_+=dt;
    const auto* node=expeditionMapRun_.GetActiveNode();
    const auto stage=expeditionGuide_.GetStage();
    const std::string state=(expeditionBuildChoice_?"build_choice":expeditionMapRun_.IsChoosing()?"map_"+expeditionMapRun_.GetCurrentNodeId():expeditionMapRun_.GetActiveNodeId())+
        (expeditionGuideActive_?"/guide_"+std::to_string(static_cast<int>(stage)):"");
    if(state!=experienceValidationState_) {
        experienceValidationState_=state;experienceValidationStateAge_=0;
        WriteExperienceValidationReport(false);
    }
    experienceValidationStateAge_+=dt;
    player_->SetDemoInput(true,{},player_->GetWorldPosition()+Vector3{1,0,0},false,false);
    if(expeditionGuideActive_) experienceGuideStageMask_|=1u<<static_cast<unsigned>(stage);
    for(const auto& orb:expeditionCredits_) {
        if(!orb.flying) ++experienceGroundOrbSamples_;
        if(orb.flying&&orb.flight>=0.50f) experienceCreditArrivalEligible_=true;
        if(orb.flying&&orb.flight>0.08f&&orb.flight<0.45f) {
            ++experienceEarlyFlightSamples_;CaptureExperienceValidation("credits_flight");
        }
    }
    if(!experienceCreditDelivered_&&expeditionMapRun_.GetCurrency()>20) {
        if(!experienceCreditArrivalEligible_) ++experiencePrematureCredits_;
        experienceCreditDelivered_=true;
    }
    if(experienceValidationElapsed_>115.0f||player_->IsDead()||!experienceValidationErrors_.empty()) {
        if(experienceValidationElapsed_>115.0f) experienceValidationErrors_.push_back("Experience runtime timed out");
        if(player_->IsDead()) experienceValidationErrors_.push_back("Tutorial player died");
        WriteExperienceValidationReport(false);PostQuitMessage(7);return true;
    }
    if(gameFlowState_==GameFlowState::StageClear) {
        CaptureExperienceValidation("complete");
        if(experienceValidationStateAge_<1.0f||!tankRunCapturePath_.empty()) return false;
        const size_t visits=expeditionMapRun_.GetVisitedNodeIds().size();
        if(!expeditionMapRun_.IsComplete()||visits<18||visits>22) experienceValidationErrors_.push_back("Procedural route did not reach 18..22 visited nodes and final boss");
        if(experienceIntroWallet_!=58||experienceAfterIntroWallet_!=(experienceValidationVariant_==1?43:58)) experienceValidationErrors_.push_back("Intro paths have incorrect grant or purchase/skip economy");
        if(experienceValidationIntroOffers_.size()!=3||experienceIntroOfferDetails_.size()!=3) experienceValidationErrors_.push_back("Intro did not offer exactly three modules");
        if(experiencePrematureCredits_||experienceEarlyFlightSamples_==0) experienceValidationErrors_.push_back("Credits did not wait for flight arrival");
        if(experienceValidationStyle_==2&&(!experienceMeleeDone_||experienceMeleeMinHp_>=500||experienceMeleeSlashSamples_==0||experienceMeleeBulletSamples_!=0||experienceMeleeDisplacement_<0.10f))
            experienceValidationErrors_.push_back("Actual melee hit/knockback evidence incomplete");
        if(experienceValidationStyle_!=2&&(!experienceMeleeDone_||experienceMeleeMinHp_>=500||experienceMeleeSlashSamples_!=0||experienceMeleeBulletSamples_==0))
            experienceValidationErrors_.push_back("Actual projectile attack evidence incomplete");
        if(experienceValidationStyle_==1&&experienceDroneSamples_==0) experienceValidationErrors_.push_back("Drone style did not create immediate companions");
        if(!experienceBuildPreserved_) experienceValidationErrors_.push_back("Build selection preservation not verified");
        if(!experienceEvolutionVerified_) experienceValidationErrors_.push_back("Same-style workshop refit purchase and preservation not verified");
        if(experienceValidationVariant_==1&&(experienceInitialKills_<2||experiencePlayerBulletSamples_==0||experienceSuccessfulDashes_==0||experienceGroundOrbSamples_==0))
            experienceValidationErrors_.push_back("Real tutorial shooting/pickup/dash evidence incomplete");
        if(player_->GetLevel()!=1||player_->GetExp()!=0) experienceValidationErrors_.push_back("Expedition leaked level/EXP progression");
        WriteExperienceValidationReport(experienceValidationErrors_.empty());
        PostQuitMessage(experienceValidationErrors_.empty()?0:7);return true;
    }
    if(expeditionTransition_.IsActive()) {
        if(expeditionTransition_.Cover()>0.65f) CaptureExperienceValidation("transition");
        if(expeditionBuildChoice_) experienceValidationStateAge_=0;
        return false;
    }
    if(expeditionBuildChoice_) {
        if(experienceAfterIntroWallet_<0) experienceAfterIntroWallet_=expeditionMapRun_.GetCurrency();
        experiencePreviewRarity_=(std::min)(4,static_cast<int>(experienceValidationStateAge_));
        if(experienceValidationStateAge_>0.30f) CaptureExperienceValidation("build_choice");
        if(std::fmod(experienceValidationStateAge_,1.0f)>0.65f) CaptureExperienceValidation("rarity_"+std::to_string(experiencePreviewRarity_));
        if(experienceValidationStateAge_>5.1f&&tankRunCapturePath_.empty()) SelectExpeditionBuildStyle(experienceValidationStyle_);
        return false;
    }
    if(expeditionMapRun_.IsChoosing()) {
        if(expeditionMapRun_.GetVisitedNodeIds().empty()) CaptureExperienceValidation("map");
        if(expeditionMapRun_.GetVisitedNodeIds().size()==2&&experienceAfterIntroWallet_<0) {
            experienceAfterIntroWallet_=expeditionMapRun_.GetCurrency();
            WriteExperienceValidationReport(false);
        }
        if(experienceValidationStateAge_<0.45f||!tankRunCapturePath_.empty()) return false;
        auto options=expeditionMapRun_.GetAvailableNodeIds();if(options.empty()) return false;
        std::string id=options.front();
        if(expeditionMapRun_.GetVisitedNodeIds().empty()) id=experienceValidationVariant_==1?"tutorial_combat":"tutorial_skip";
        else for(const auto& option:options) {
            const auto* candidate=tankexp::FindMapNode(expeditionMapRun_.GetDefinition(),option);
            if(candidate&&candidate->kind==tankexp::NodeKind::Upgrade&&!experienceEvolutionVerified_) {id=option;break;}
            if(candidate) id=option;
        }
        RequestExpeditionMapNode(id);return false;
    }
    if(!node) return false;
    if(node->role==tankexp::NodeRole::TutorialCombat) {
        experiencePlayerBulletSamples_+=static_cast<int>(bulletManager_->GetBulletCounts().player);
        if(stage==S::Briefing||stage==S::Vitals) {
            CaptureExperienceValidation(stage==S::Briefing?"briefing":"hp_stamina");
            if(experienceValidationStateAge_>0.75f&&tankRunCapturePath_.empty()) AcknowledgeGuidedExpedition();
            return false;
        }
        if(stage==S::Dash) {
            if(player_->IsDashing()) CaptureExperienceValidation("dash");
            const auto bullets=bulletManager_->GetBulletCounts();
            const bool request=experienceValidationStateAge_>0.6f&&!player_->IsDashing()&&bullets.enemy+bullets.hostileExpEnemy>0;
            const Vector2 move=(request||player_->IsDashing())?Vector2{0,1}:Vector2{};
            player_->SetDemoInput(true,move,player_->GetWorldPosition()+Vector3{0,1,0},false,request);
            return false;
        }
        if(stage==S::Upgrade) experienceSuccessfulDashes_=player_->GetDashStartedCount();
        if(stage==S::Collect) {
            CaptureExperienceValidation("collect");
            if(!expeditionCredits_.empty()) {
                Vector3 move=expeditionCredits_.front().position-player_->GetWorldPosition();
                if(Length(move)>0.1f) move=Normalize(move);
                player_->SetDemoInput(true,{move.x,move.y},expeditionCredits_.front().position,false,false);
            }
            return false;
        }
        const auto actors=enemyManager_->GetEnemyPtrs();
        if(!actors.empty()&&actors.front()&&!actors.front()->IsDead())
            player_->SetDemoInput(true,{},actors.front()->GetWorldPosition(),true,false);
        return false;
    }
    if(tankexp::IsIntroUpgrade(node->role)) {
        CaptureExperienceValidation("intro_upgrades");
        if(experienceIntroWallet_<0) {
            experienceIntroWallet_=expeditionMapRun_.GetCurrency();
            experienceInitialKills_=defeatedEnemies_;
            experienceValidationIntroOffers_=expeditionServiceOffers_;
            for(const auto& id:experienceValidationIntroOffers_) {
                const auto* offer=tankcontent::FindUpgrade(expeditionContent_,id);
                const int price=ExpeditionServicePrice(id);
                if(!offer||offer->rarity!=0||offer->effects.size()!=1||price!=15)
                    experienceValidationErrors_.push_back("Intro offers are not weak single-effect modules priced 15");
                experienceIntroOfferDetails_.push_back({{"id",id},{"price",price},{"rarity",offer?offer->rarity:-1}});
            }
            WriteExperienceValidationReport(false);
        }
        if(experienceValidationStateAge_>1.0f&&tankRunCapturePath_.empty())
            SelectExpeditionService(experienceValidationVariant_==1?0:3);
        return false;
    }
    if(node->kind==tankexp::NodeKind::Currency) return false;
    debugPlayerNoDamage_=true;
    if(tankexp::IsCombatNode(node->kind)) {
        if(!experienceMeleeDone_) {
            if(!experienceMeleeStarted_) BeginExperienceMeleeProbe();
            experienceMeleeAge_+=dt;
            const auto actors=enemyManager_->GetEnemyPtrs();
            if(actors.empty()) {experienceValidationErrors_.push_back("Melee target disappeared");return false;}
            auto* target=actors.front();
            experienceMeleeMinHp_=(std::min)(experienceMeleeMinHp_,target->GetHp());
            experienceMeleeDisplacement_=(std::max)(experienceMeleeDisplacement_,Length(target->GetWorldPosition()-experienceMeleeTargetStart_));
            experienceMeleeSlashSamples_+=static_cast<int>(playerMeleeSlashes_.size());
            experienceMeleeBulletSamples_+=static_cast<int>(bulletManager_->GetBulletCounts().player);
            experienceDroneSamples_+=static_cast<int>(player_->GetDronePtrs().size());
            player_->SetDemoInput(true,{},target->GetWorldPosition(),experienceMeleeAge_>0.45f,false);
            if(experienceValidationStyle_!=2&&experienceMeleeAge_>1.0f) CaptureExperienceValidation("build_attack");
            for(const auto& slash:playerMeleeSlashes_)
                if(slash.elapsed>=slash.windupDuration+slash.swingDuration*0.35f&&slash.elapsed<slash.windupDuration+slash.swingDuration)
                    CaptureExperienceValidation(slash.finisher?"melee_finisher":"melee");
            if(experienceMeleeAge_>3.0f) {
                experienceMeleeDone_=true;
                player_->SetDemoInput(true,{},target->GetWorldPosition(),false,false);
                WriteExperienceValidationReport(false);
            }
            return false;
        }
        if(experienceValidationStateAge_>0.4f) {
            bool cleared=false;
            for(auto* actor:enemyManager_->GetEnemyPtrs()) if(actor&&!actor->IsDead()) {actor->TakeDamageFromPlayer(100000);cleared=true;}
            if(node->kind==tankexp::NodeKind::Boss&&!enemy_->IsDead()) {CaptureExperienceValidation("boss");enemy_->TakeDamage(100000);cleared=true;}
            if(cleared) ++experienceForcedLaterClears_;
        }
    } else {
        if(node->kind==tankexp::NodeKind::Upgrade) for(const auto& id:expeditionServiceOffers_) {
            const auto* offer=tankcontent::FindUpgrade(expeditionContent_,id);
            if(!offer||!tankcontent::EligibleUpgrade(*offer,expeditionBuildStyle_,tankRun_.GetCardCounts()))
                experienceValidationErrors_.push_back("Incompatible upgrade leaked into runtime shop: "+id);
        }
        if(node->kind==tankexp::NodeKind::Upgrade&&!experienceEvolutionVerified_&&node->role==tankexp::NodeRole::None) {
            // Deterministic offer seed isolates purchase preservation from luck.
            // The real weighted generator, eligibility and card transaction run.
            if(experienceValidationStateAge_<0.7f)for(uint32_t fixtureSeed=1;fixtureSeed<512;++fixtureSeed) {
                auto offers=tankcontent::BuildShopOffers(expeditionContent_,expeditionBuildStyle_,tankRun_.GetCardCounts(),expeditionPurchases_,fixtureSeed,expeditionRefitPurchased_);
                const bool refit=std::any_of(offers.begin(),offers.end(),[&](const std::string& id){const auto* u=tankcontent::FindUpgrade(expeditionContent_,id);return u&&!u->refitPlayer.empty();});
                if(refit){expeditionServiceOffers_=std::move(offers);RefreshExpeditionBuildCards();break;}
            }
            for(size_t i=0;i<expeditionServiceOffers_.size();++i) {
                const auto* u=tankcontent::FindUpgrade(expeditionContent_,expeditionServiceOffers_[i]);
                if(!u||u->refitPlayer.empty())continue;
                const auto* p=tankcontent::FindPlayer(expeditionContent_,u->refitPlayer);
                if(!p||p->style!=expeditionBuildStyle_)experienceValidationErrors_.push_back("Cross-style refit leaked into workshop");
                CaptureExperienceValidation("refit");
                if(experienceValidationStateAge_>0.8f&&tankRunCapturePath_.empty()&&expeditionMapRun_.CanAfford(ExpeditionServicePrice(u->id))) {
                    SelectExpeditionService(static_cast<int>(i));return false;
                }
            }
        }
        if(experienceValidationStateAge_>0.9f) SelectExpeditionService(3);
    }
    return false;
}
