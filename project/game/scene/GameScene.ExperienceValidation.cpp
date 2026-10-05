#include "GameScene.h"
#include "Enemy.h"
#include "game/run/TankSubmissionValidation.h"
#include <functional>
#include <fstream>
#include <iomanip>

namespace {
constexpr uint32_t kExperienceSeed=20260926;
/// @brief 実行検証の出力先ディレクトリーを返す。
std::string ExperienceDirectory(int variant) {
    if(tanksubmission::Enabled()) return "generated/submission_validation/";
    return std::string("generated/experience_validation/")+(variant==1?"upper/":"lower/");
}
}

void GameScene::InitializeExperienceValidation() {
    wchar_t flag[8]{};
    if(titleDemo_) return;
    const bool submission=tanksubmission::Enabled();
    if(!submission&&(GetEnvironmentVariableW(L"CG2_TANK_EXPERIENCE_AUTOTEST",flag,8)==0||(flag[0]!=L'1'&&flag[0]!=L'2'))) return;
    if(submission) {
        flag[0]=L'1';
        ++tanksubmission::state.runs;
        if(tanksubmission::state.runs==2) {experienceValidationVariant_=1;return;}
    }
    experienceValidationVariant_=flag[0]==L'1'?1:2;
    wchar_t styleFlag[8]{};
    if(GetEnvironmentVariableW(L"CG2_TANK_EXPERIENCE_STYLE",styleFlag,8)>0&&styleFlag[0]>=L'0'&&styleFlag[0]<=L'2') experienceValidationStyle_=styleFlag[0]-L'0';
    expeditionSeed_=kExperienceSeed;
    expeditionMapDefinition_=tankexp::GenerateExpeditionMap(expeditionSeed_,30,8);
    if(submission) {
        // Use the real generated graph. Pick a route with repair/workshop stops,
        // then select existing standard rooms to guarantee all three enemy views.
        std::function<int(const std::string&)> score=[&](const std::string& id) {
            const auto* n=tankexp::FindMapNode(expeditionMapDefinition_,id);if(!n)return 0;
            int next=0;for(const auto& to:n->next)next=(std::max)(next,score(to));
            return next+(n->kind==tankexp::NodeKind::Heal?50:n->kind==tankexp::NodeKind::Upgrade?10:1);
        };
        std::string id="tutorial_combat";int enemyRoom=0;
        const char* rooms[]={"command_post","emp_patrol","reflect_bastion"};
        const int firstColumns[]={7,10,13};
        while(!id.empty()) {
            tanksubmission::state.path.push_back(id);
            auto it=std::find_if(expeditionMapDefinition_.nodes.begin(),expeditionMapDefinition_.nodes.end(),[&](const auto& n){return n.id==id;});
            if(it==expeditionMapDefinition_.nodes.end())break;
            if(enemyRoom<3&&it->kind==tankexp::NodeKind::Combat&&it->column>=firstColumns[enemyRoom])it->roomTemplate=rooms[enemyRoom++];
            std::string next;int best=-1;
            for(const auto& to:it->next)if(const int value=score(to);value>best){best=value;next=to;}
            id=next;
        }
    }
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
    if(tanksubmission::Enabled()) RecordSubmissionUi(name);
}

void GameScene::WriteExperienceValidationReport(bool completed) {
    const auto& map=expeditionMapRun_.GetDefinition();
    nlohmann::json report={
        {"completed",completed},{"testMode",true},{"variant",experienceValidationVariant_},{"seed",expeditionSeed_},
        {"elapsed",experienceValidationElapsed_},{"state",experienceValidationState_},
        {"forcedTutorialClear",false},{"forcedLaterCombat",true},{"forcedLaterClears",experienceForcedLaterClears_},
        {"tutorialInvulnerable",true},{"laterInvulnerable",true},
        {"meleeModuleGrantedForProbe",false},{"meleeFixtureReplacesFirstNormalRoomEnemies",true},
        {"buildStyle",tankbuild::Id(static_cast<tankbuild::Style>(experienceValidationStyle_))},{"buildPreserved",experienceBuildPreserved_},
        {"droneSamples",experienceDroneSamples_},{"rarityScreensAreVisualFixtures",!tanksubmission::Enabled()},
        {"additiveGrowthVerified",experienceEvolutionVerified_},{"growthOfferSeedIsFixture",true},
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
    if(tanksubmission::Enabled()){tanksubmission::state.experience=std::move(report);tanksubmission::Write();}
    else std::ofstream(ExperienceDirectory(experienceValidationVariant_)+"validation.json")<<std::setw(2)<<report<<'\n';
}

void GameScene::BeginExperienceMeleeProbe() {
    experienceMeleeStarted_=true;experienceMeleeAge_=0;
    // A single passive high-HP target isolates real melee collision/knockback.
    // This fixture substitution happens only after the normal introductory flow.
    enemyManager_->ClearRunActors();bulletManager_->ClearAll();
    if(!player_->HasExpeditionCombatStyle()||player_->GetExpeditionCombatStyle()!=static_cast<tankbuild::Style>(experienceValidationStyle_))
        experienceValidationErrors_.push_back("Chosen base style was not equipped after introduction");
    ApplyTankRunCards();
    const cg2::Vector3 position=player_->GetWorldPosition();
    const std::array<cg2::Vector3,4> directions{cg2::Vector3{1,0,0},cg2::Vector3{0,1,0},cg2::Vector3{-1,0,0},cg2::Vector3{0,-1,0}};
    bool spawned=false;
    for(const auto& direction:directions) {
        const cg2::Vector3 target=position+direction*3.1f;
        if(stage_->IsCollisionWithAnyBlock(target,1.0f)||stage_->IsCollisionWithAnyBlock(position+direction*1.6f,0.8f)) continue;
        if(enemyManager_->SpawnLevelEnemy(target,"tutorial_target",500)) {
            experienceMeleeTargetStart_=target;spawned=true;break;
        }
    }
    if(!spawned) experienceValidationErrors_.push_back("No safe melee probe placement");
}

bool GameScene::UpdateExperienceValidation(float dt) {
    if(!experienceValidationVariant_) return false;
    if(tanksubmission::Enabled()) {
        if(tanksubmission::state.runs==2) {UpdateSubmissionValidation(dt);return false;}
        if(UpdateSubmissionValidation(dt))return true;
    }
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
    player_->SetDemoInput(true,{},player_->GetWorldPosition()+cg2::Vector3{1,0,0},false,false);
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
    const float timeLimit=tanksubmission::Enabled()?160.0f:115.0f;
    if(experienceValidationElapsed_>timeLimit||player_->IsDead()||!experienceValidationErrors_.empty()) {
        if(experienceValidationElapsed_>timeLimit) experienceValidationErrors_.push_back("Experience runtime timed out");
        if(player_->IsDead()) experienceValidationErrors_.push_back("Tutorial player died");
        WriteExperienceValidationReport(false);PostQuitMessage(7);return true;
    }
    if(combatFlow_.GetState()==GameFlowState::StageClear) {
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
        if(!experienceEvolutionVerified_) experienceValidationErrors_.push_back("Additive workshop upgrade and preservation not verified");
        if(experienceValidationVariant_==1&&(experienceInitialKills_<2||experiencePlayerBulletSamples_==0||experienceSuccessfulDashes_<3||experienceGroundOrbSamples_==0))
            experienceValidationErrors_.push_back("Real tutorial shooting/pickup/dash evidence incomplete");
        if(player_->GetLevel()!=1||player_->GetExp()!=0) experienceValidationErrors_.push_back("Expedition leaked level/EXP progression");
        WriteExperienceValidationReport(experienceValidationErrors_.empty());
        if(tanksubmission::Enabled()&&experienceValidationErrors_.empty()) {
            auto& check=tanksubmission::state;
            check.tutorialSaved=tanksubmission::TutorialSaved();
            if(!check.tutorialSaved||!check.repairDone||check.newEnemies!=7)check.errors.push_back("Missing persisted tutorial, repair transaction, or standard enemy rooms");
            check.finished=true;resultSelection_=1;ConfirmResultSelection();tanksubmission::Write();return true;
        }
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
        if(tanksubmission::Enabled()) {
            const auto index=expeditionMapRun_.GetVisitedNodeIds().size();
            if(index<tanksubmission::state.path.size())id=tanksubmission::state.path[index];
            RequestExpeditionMapNode(id);return false;
        }
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
            const bool request=experienceValidationStateAge_>0.6f&&!player_->IsDashing();
            const cg2::Vector2 move=(request||player_->IsDashing())?cg2::Vector2{0,1}:cg2::Vector2{};
            player_->SetDemoInput(true,move,player_->GetWorldPosition()+cg2::Vector3{0,1,0},false,request);
            return false;
        }
        if(stage==S::Upgrade) { experienceSuccessfulDashes_=expeditionGuide_.GetCompletedDashes(); if(player_->GetHp()!=player_->GetMaxHp()) experienceValidationErrors_.push_back("Tutorial practice reduced HP"); }
        if(stage==S::Collect) {
            CaptureExperienceValidation("collect");
            if(!expeditionCredits_.empty()) {
                cg2::Vector3 move=expeditionCredits_.front().position-player_->GetWorldPosition();
                if(cg2::Length(move)>0.1f) move=cg2::Normalize(move);
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
            experienceMeleeDisplacement_=(std::max)(experienceMeleeDisplacement_,cg2::Length(target->GetWorldPosition()-experienceMeleeTargetStart_));
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
        if(experienceValidationStateAge_>(tanksubmission::Enabled()?2.0f:0.4f)) {
            bool cleared=false;
            for(auto* actor:enemyManager_->GetEnemyPtrs()) if(actor&&!actor->IsDead()) {actor->TakeDamageFromPlayer(100000);cleared=true;}
            if(node->kind==tankexp::NodeKind::Boss&&!enemy_->IsDead()) {
                CaptureExperienceValidation("boss");
                if(!tanksubmission::Enabled()||tankRunCapturePath_.empty()){enemy_->TakeDamage(100000);cleared=true;}
            }
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
                auto offers=tankcontent::BuildShopOffers(expeditionContent_,expeditionBuildStyle_,tankRun_.GetCardCounts(),expeditionPurchases_,fixtureSeed);
                const bool additive=std::any_of(offers.begin(),offers.end(),[&](const std::string& id){const auto* u=tankcontent::FindUpgrade(expeditionContent_,id);return u&&std::any_of(u->effects.begin(),u->effects.end(),[](auto effect){return effect>=tankrun::CardId::ExtraBarrel1;});});
                if(additive){expeditionServiceOffers_=std::move(offers);RefreshExpeditionBuildCards();break;}
            }
            for(size_t i=0;i<expeditionServiceOffers_.size();++i) {
                const auto* u=tankcontent::FindUpgrade(expeditionContent_,expeditionServiceOffers_[i]);
                if(!u||!std::any_of(u->effects.begin(),u->effects.end(),[](auto effect){return effect>=tankrun::CardId::ExtraBarrel1;}))continue;
                CaptureExperienceValidation("additive_upgrade");
                if(experienceValidationStateAge_>0.8f&&tankRunCapturePath_.empty()&&expeditionMapRun_.CanAfford(ExpeditionServicePrice(u->id))) {
                    SelectExpeditionService(static_cast<int>(i));return false;
                }
            }
        }
        if(experienceValidationStateAge_>0.9f) SelectExpeditionService(3);
    }
    return false;
}

void GameScene::RecordSubmissionUi(const std::string& screen) {
    auto& check=tanksubmission::state;
    auto text=nlohmann::json::array();
    auto add=[&](const cg2::TextLabel* label){if(label&&!label->GetText().empty())text.push_back(label->GetText());};
    const bool mapScreen=expeditionMapRun_.IsChoosing()||IsExpeditionBuildCardScreen()||
        (expeditionMapRun_.GetActiveNode()&&!tankexp::IsCombatNode(expeditionMapRun_.GetActiveNode()->kind));
    add(tankRunHud_.get());add(tankRunObjectiveText_.get());
    if(mapScreen) {
        add(expeditionMapTitle_.get());add(expeditionMapSubtitle_.get());
        add(expeditionMapInfo_.get());add(expeditionMapHelp_.get());
    }
    if(!expeditionMapRun_.IsChoosing()){add(tankRunHeading_.get());add(tankRunDescription_.get());}
    if(tankExpeditionRivalActive_)add(tankRunBossText_.get());
    if(expeditionMapRun_.IsChoosing()) {
        add(expeditionMapLegend_.get());
        for(const auto& n:expeditionMapVisuals_){add(n.label.get());add(n.state.get());}
    }
    const auto* active=expeditionMapRun_.GetActiveNode();
    if(expeditionGuideActive_){add(tutorialInputText_.get());add(tutorialDescriptionText_.get());}
    if(IsExpeditionBuildCardScreen()) {
        if(!expeditionBuildChoice_)add(expeditionSkipText_.get());
        for(int i=0;i<3;++i) if(expeditionBuildChoice_||static_cast<size_t>(expeditionServicePage_*3+i)<expeditionServiceOffers_.size()) {
            const auto& model=expeditionRewardCards_[i]->GetModel();
            text.push_back(model.title);text.push_back(model.description);text.push_back(model.footer);
        }
    } else if(active&&active->kind==tankexp::NodeKind::Heal) {
        add(tankRunCardTitles_[0].get());add(tankRunCardBodies_[0].get());add(expeditionSkipText_.get());
    } else if(combatFlow_.GetState()==GameFlowState::StageClear) {
        add(tankRunFooter_.get());
        for(int i=0;i<2;++i){add(tankRunCardTitles_[i].get());add(tankRunCardBodies_[i].get());}
    }
    if(expeditionTransition_.IsActive()){add(expeditionTransitionTitle_.get());add(expeditionTransitionDetail_.get());}
    for(const auto& entry:text) {
        const auto value=entry.get<std::string>();
        for(const char* forbidden:{"候補なし","進化","換装","キット","DEPLOY","DOCK","INSTALL","REPAIR","ROUTE","システム更新中","AUTOTEST","SPECIAL VALIDATION","F2 数値"})
            if(value.find(forbidden)!=std::string::npos)check.errors.push_back(screen+": visible obsolete/developer text: "+value);
    }
    check.screens[screen]=std::move(text);tanksubmission::Write();
}

bool GameScene::UpdateSubmissionValidation(float dt) {
    auto& check=tanksubmission::state;
    if(check.runs==2) {
        experienceValidationElapsed_+=dt;
        if(experienceValidationElapsed_>0.5f&&!check.secondFresh) {
            check.secondFresh=expeditionMapRun_.GetCurrency()==20&&expeditionMapRun_.GetVisitedNodeIds().empty()&&
                !expeditionBuildChosen_&&std::all_of(tankRun_.GetCardCounts().begin(),tankRun_.GetCardCounts().end(),[](int n){return n==0;});
            if(!check.secondFresh||!tanksubmission::TutorialSaved()||!expeditionTutorialPreviouslyCompleted_)check.errors.push_back("New expedition did not retain completion and reset run progress");
            CaptureExperienceValidation("new_expedition");
        }
        if(experienceValidationElapsed_>1.2f&&expeditionMapRun_.GetVisitedNodeIds().empty()&&
            expeditionMapRun_.IsChoosing()&&!expeditionTransition_.IsActive()&&tankRunCapturePath_.empty())
            RequestExpeditionMapNode("tutorial_skip");
        if(expeditionMapRun_.GetCurrentNodeId()=="tutorial_skip"&&expeditionMapRun_.IsChoosing()&&!expeditionTransition_.IsActive()) {
            check.skipWorks=expeditionMapRun_.GetCurrency()==58&&!expeditionGuideActive_;
            CaptureExperienceValidation("completed_user_skip");
            if(!tankRunCapturePath_.empty())return true;
            if(!check.skipWorks)check.errors.push_back("Completed user skip or equivalent currency grant failed");
            tanksubmission::Write(check.errors.empty()&&check.firstFresh&&check.secondFresh&&check.returned);
            PostQuitMessage(check.errors.empty()?0:9);
        }
        if(experienceValidationElapsed_>20){check.errors.push_back("New expedition verification timeout");tanksubmission::Write();PostQuitMessage(9);}
        return true;
    }
    if(check.finished)return true;
    if(!check.firstFresh&&expeditionMapRun_.IsChoosing()) {
        check.firstFresh=expeditionMapRun_.GetCurrency()==20&&!tanksubmission::TutorialSaved()&&
            expeditionMapSelection_=="tutorial_combat"&&std::all_of(tankRun_.GetCardCounts().begin(),tankRun_.GetCardCounts().end(),[](int n){return n==0;});
        if(!check.firstFresh)check.errors.push_back("First profile did not start empty with tutorial selected");
    }
    if(expeditionTransition_.IsActive())return false;
    if(check.repairStep==4&&!check.repairDone&&expeditionMapRun_.IsChoosing()) {
        check.repairDone=player_->GetHp()==check.repairHp&&expeditionMapRun_.GetCurrency()==check.repairWallet-check.repairCost;
        if(!check.repairDone)check.errors.push_back("Repair did not restore exactly 50% max HP or charge its price");
        tanksubmission::Write();
    }
    const auto* node=expeditionMapRun_.GetActiveNode();
    if(!node)return false;
    if(tankexp::IsCombatNode(node->kind)) {
        for(const auto* actor:enemyManager_->GetEnemyPtrs()) {
            if(!actor||actor->IsDead())continue;
            const auto type=actor->GetType();
            const unsigned bit=type==ExpEnemyType::SummonerCommander?1u:type==ExpEnemyType::EMPJammer?2u:type==ExpEnemyType::ReflectArmor?4u:0u;
            if(bit&&experienceValidationStateAge_>0.8f){check.newEnemies|=bit;CaptureExperienceValidation("enemy_"+std::to_string(bit));}
        }
    }
    if(node->kind!=tankexp::NodeKind::Heal||check.repairDone)return false;
    check.repairAge+=dt;
    if(check.repairStep==0) {
        player_->HealRunPlayer(player_->GetMaxHp());
        check.repairWallet=expeditionMapRun_.GetCurrency();check.repairCost=node->serviceCost;
        SelectExpeditionService(0);
        check.fullBlocked=!expeditionTransition_.IsActive()&&expeditionMapRun_.GetCurrency()==check.repairWallet;
        if(!check.fullBlocked)check.errors.push_back("Full HP repair was purchasable");
        check.repairStep=1;check.repairAge=0;
    } else if(check.repairStep==1&&check.repairAge>0.6f) {
        CaptureExperienceValidation("repair_full");
        if(!tankRunCapturePath_.empty())return true;
        player_->SpendRunHealth((std::max)(1,player_->GetMaxHp()*3/5));
        expeditionMapRun_.TrySpendCurrency(expeditionMapRun_.GetCurrency());
        SelectExpeditionService(0);
        check.poorBlocked=!expeditionTransition_.IsActive()&&expeditionMapRun_.GetCurrency()==0;
        if(!check.poorBlocked)check.errors.push_back("Repair accepted insufficient Cr");
        check.repairStep=2;check.repairAge=0;
    } else if(check.repairStep==2&&check.repairAge>0.6f) {
        CaptureExperienceValidation("repair_insufficient");
        if(!tankRunCapturePath_.empty())return true;
        expeditionMapRun_.EarnCurrency(check.repairWallet);expeditionMapStatus_.clear();
        check.repairStep=3;check.repairAge=0;
    } else if(check.repairStep==3&&check.repairAge>0.6f) {
        CaptureExperienceValidation("repair_available");
        if(!tankRunCapturePath_.empty())return true;
        check.repairHp=(std::min)(player_->GetMaxHp(),player_->GetHp()+(std::max)(1,player_->GetMaxHp()/2));
        SelectExpeditionService(0);check.repairStep=4;
    }
    RefreshTankExpeditionUi();
    return check.repairStep<4;
}
