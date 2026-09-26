#include "GameScene.h"
#include "Enemy.h"
#include <fstream>
#include <iomanip>

namespace {
constexpr const char* kSpecialDirectory="generated/special_validation/";
constexpr std::array<const char*,4> kSpecialNames{"RailCannon","DroneLaserLink","SlashWave","ParryBlade"};
}

void GameScene::InitializeSpecialValidationFixture() {
    wchar_t flag[8]{};
    specialValidationEnabled_=!titleDemo_&&GetEnvironmentVariableW(L"CG2_TANK_SPECIAL_AUTOTEST",flag,8)>0&&flag[0]==L'1';
    if(!specialValidationEnabled_)return;
    specialValidation_={{"completed",false},{"testMode",true},{"forcedDamage",false},{"scriptedInputAndTargets",true},
        {"invulnerablePlayer",true},{"elapsed",0.0},{"probe",-1},{"age",0.0},{"requested",false},
        {"errors",nlohmann::json::array()},{"captures",nlohmann::json::array()},{"probes",nlohmann::json::array()}};
    tankRunAutoTest_=expeditionMapAutoTest_=combatValidationEnabled_=false;experienceValidationVariant_=0;
    expeditionGuideActive_=false;expeditionBuildChosen_=true;expeditionBuildChoice_=false;
    tankExpeditionTutorial_.Skip();tutorialConfig_.enabled=false;debugPlayerNoDamage_=true;
    tankExpeditionMusicEnabled_=tankExpeditionEffectsEnabled_=false;
    tankExpeditionAudio_.SetMusicVolume(0);tankExpeditionAudio_.SetEffectsVolume(0);
    expeditionContent_=tankcontent::DefaultCatalog();
    tankcontent::Enemy target;target.id="special_target";target.name="特殊能力・検証標的";
    target.behavior=tankcontent::EnemyBehavior::Square;target.hp=5000;target.contactDamage=1;
    target.moveSpeedScale=.1f;target.creditDrop=0;target.color={1.0f,.40f,.25f,1};
    expeditionContent_.enemies.push_back(target);
    auto room=tankexp::MakeEmptyRoom("special_probe","特殊能力・実動作検証");
    room.objective="boss";room.playerStart={30,29};room.objectiveTargets={{56,29}};room.spawns.clear();
    expeditionRooms_.rooms={room};
    expeditionMapDefinition_.startingCurrency=0;expeditionMapDefinition_.startNodes={"special_probe"};
    expeditionMapDefinition_.nodes={{"special_probe","特殊能力・実動作検証",tankexp::NodeKind::Boss,0,2,0,"special_probe",0,0,{}}};
    std::filesystem::create_directories(kSpecialDirectory);
    std::ofstream(std::string(kSpecialDirectory)+"validation.json")<<std::setw(2)<<specialValidation_<<'\n';
}

bool GameScene::UpdateSpecialValidation(float dt) {
    if(!specialValidationEnabled_)return false;
    auto write=[&] {std::ofstream(std::string(kSpecialDirectory)+"validation.json")<<std::setw(2)<<specialValidation_<<'\n';};
    auto capture=[&](const std::string& name) {
        auto& names=specialValidation_["captures"];
        if(!tankRunCapturePath_.empty()||std::find(names.begin(),names.end(),nlohmann::json(name))!=names.end())return;
        names.push_back(name);tankRunCapturePath_=std::string(kSpecialDirectory)+name+".png";
    };
    auto fail=[&](const std::string& reason) {specialValidation_["errors"].push_back(reason);};
    const double elapsed=specialValidation_.value("elapsed",0.0)+dt;specialValidation_["elapsed"]=elapsed;
    if(elapsed>40) {fail("Special combat runtime timed out");write();PostQuitMessage(8);return true;}
    const bool transitioning=expeditionTransition_.IsActive();UpdateExpeditionPresentation(dt);
    if(transitioning) {RefreshTankExpeditionUi();return true;}
    if(!specialValidation_.value("requested",false)) {
        if(elapsed>.3) {specialValidation_["requested"]=true;RequestExpeditionMapNode("special_probe");}
        RefreshTankExpeditionUi();return true;
    }
    if(!tankExpedition_.IsCombat())return true;
    int index=specialValidation_.value("probe",-1);
    if(index>=4) {
        if(tankRunCapturePath_.empty()) {specialValidation_["completed"]=specialValidation_["errors"].empty();write();PostQuitMessage(specialValidation_["completed"].get<bool>()?0:8);}
        return true;
    }
    if(index<0||specialValidation_.value("beginNext",false)) {
        index=index<0?0:index+1;specialValidation_["probe"]=index;specialValidation_["beginNext"]=false;
        if(index>=4)return true;
        enemyManager_->ClearRunActors();bulletManager_->ClearAll();enemy_->SetRunEncounterEnabled(false);tankExpeditionRivalActive_=false;
        playerMeleeSlashes_.clear();playerLaserBeams_.clear();playerMines_.clear();expeditionHitSparks_.clear();
        if(playerMeleeTrailManager_)playerMeleeTrailManager_->ClearInstances();
        player_->ResetRunRoomState({30,29,0});player_->SetDebugNoDamage(true);
        tankRun_.Reset(20260927);tankRun_.ChooseLoadout(0);tankRun_.ChooseCore(0);
        const std::array<tankrun::CardId,4> effects{tankrun::CardId::RailCannon,tankrun::CardId::DroneLaserLink,tankrun::CardId::SlashWave,tankrun::CardId::ParryBlade};
        if(!tankRun_.GrantExpeditionModules({effects[static_cast<size_t>(index)]}))fail("Could not grant actual expedition module");
        ApplyTankRunCards();
        expeditionBuildStyle_=index==0?tankbuild::Style::Shooter:index==1?tankbuild::Style::Drone:tankbuild::Style::Melee;
        if(!player_->SetExpeditionCombatStyle(expeditionBuildStyle_))fail("Could not equip style");
        player_->SetDemoInput(true,{},Vector3{50,29,0},false,false);
        const auto stats=player_->GetSpecialCombatStats();
        specialValidation_["before"]={{"railShots",stats.railShots},{"linkTicks",stats.linkTicks},{"slashWaves",stats.slashWaves},{"parries",stats.parries},{"perfectParries",stats.perfectParries}};
        specialValidation_["age"]=0.0;specialValidation_["minHp"]=5000;specialValidation_["secondMinHp"]=5000;
        specialValidation_["normalInjected"]=false;specialValidation_["perfectInjected"]=false;specialValidation_["armoredRemaining"]=12.0f;
        specialValidation_["maxCharge"]=0.0f;specialValidation_["maxLinks"]=0;specialValidation_["waveSamples"]=0;specialValidation_["railSamples"]=0;
        if(index<3)enemyManager_->SpawnLevelEnemy(index==0?Vector3{38,29,0}:index==1?Vector3{25,29,0}:Vector3{38.6f,29,0},"special_target",5000);
        if(index==0)enemyManager_->SpawnLevelEnemy({42,29,0},"special_target",5000);
        SetEventCallout(std::string("SPECIAL VALIDATION / ")+kSpecialNames[static_cast<size_t>(index)],1.2f);
        write();
    }
    const float age=static_cast<float>(specialValidation_.value("age",0.0)+dt);specialValidation_["age"]=age;
    // Keep the wave probe outside direct sword reach despite attack follow-through.
    if(index==2) {player_->SetWorldPosition({30,29,0});player_->SetVelocity({});}
    const auto actors=enemyManager_->GetEnemyPtrs();
    // Position fixtures isolate damage from movement/knockback; no HP or damage is injected.
    for(size_t i=0;i<actors.size();++i) {
        const Vector3 position=index==0?Vector3{38+4*static_cast<float>(i),29,0}:index==1?Vector3{age>1.2f?28.25f:25.0f,29,0}:Vector3{38.6f,29,0};
        actors[i]->SetWorldPosition(position);
        const char* field=i==0?"minHp":"secondMinHp";specialValidation_[field]=(std::min)(specialValidation_.value(field,5000),actors[i]->GetHp());
    }
    const auto stats=player_->GetSpecialCombatStats();const auto& before=specialValidation_["before"];
    specialValidation_["maxCharge"]=(std::max)(specialValidation_.value("maxCharge",0.0f),player_->GetRailChargeRatio());
    specialValidation_["maxLinks"]=(std::max)(specialValidation_.value("maxLinks",0),static_cast<int>(player_->GetDroneLaserLinks().size()));
    for(auto* bullet:bulletManager_->GetBulletPtrs()) {
        if(bullet->GetSpecialKind()==Bullet::SpecialKind::Rail) {specialValidation_["railSamples"]=specialValidation_.value("railSamples",0)+1;capture("rail_fire");}
        if(bullet->GetSpecialKind()==Bullet::SpecialKind::SlashWave) {specialValidation_["waveSamples"]=specialValidation_.value("waveSamples",0)+1;if(Length(bullet->GetWorldPosition()-player_->GetWorldPosition())>4.5f)capture("slash_wave");}
        // Latch the first cut: a later, separately timed sword swing may finish it.
        if(index==3&&bullet->GetOwner()==kEnemy&&bullet->GetDamage()==13&&bullet->GetBulletHp()<12&&specialValidation_.value("armoredRemaining",12.0f)==12)
            specialValidation_["armoredRemaining"]=bullet->GetBulletHp();
    }
    bool shoot=false;
    if(index==0) {shoot=age>.3f&&age<1.5f;if(player_->GetRailChargeRatio()>=.999f)capture("rail_charge");}
    if(index==1&&std::any_of(player_->GetDroneLaserLinks().begin(),player_->GetDroneLaserLinks().end(),[](const auto& link){return link.contact;}))capture("drone_link");
    if(index>=2)shoot=age>.35f;
    player_->SetDemoInput(true,{},Vector3{50,29,0},shoot,false);
    if(index==3) {
        const auto spawn=[&](float hp,uint32_t damage,Vector3 offset) {auto b=std::make_unique<Bullet>();b->Initialize(player_->GetWorldPosition()+offset,{-.02f,0,0},damage,kEnemy,false,hp,1);b->ConfigureGrowth(0,0,0);bulletManager_->Add(std::move(b));};
        for(const auto& slash:playerMeleeSlashes_) {
            const float active=slash.elapsed-slash.windupDuration;
            if(!specialValidation_.value("normalInjected",false)&&slash.finisher&&active>.125f&&active<.165f) {
                spawn(tankspecial::kOrdinaryEnemyBulletHp,7,{2.5f,0,0});specialValidation_["normalInjected"]=true;
            }
            if(specialValidation_.value("normalInjected",false)&&!specialValidation_.value("perfectInjected",false)&&stats.parries>before.value("parries",0u)&&active>=0&&active<.04f) {
                spawn(tankspecial::kOrdinaryEnemyBulletHp,9,{2.5f,0,0});spawn(tankspecial::kBossEnemyBulletHp,13,{2.8f,.2f,0});specialValidation_["perfectInjected"]=true;
            }
        }
        if(stats.parries>before.value("parries",0u)&&stats.perfectParries==before.value("perfectParries",0u))capture("parry_normal");
        if(stats.perfectParries>before.value("perfectParries",0u))capture("parry_perfect");
    }
    if(age>(index==3?5.0f:4.0f)&&tankRunCapturePath_.empty()) {
        nlohmann::json result={{"id",kSpecialNames[static_cast<size_t>(index)]},{"damage",5000-specialValidation_.value("minHp",5000)},
            {"secondDamage",5000-specialValidation_.value("secondMinHp",5000)},{"railShots",stats.railShots-before.value("railShots",0u)},
            {"linkTicks",stats.linkTicks-before.value("linkTicks",0u)},{"slashWaves",stats.slashWaves-before.value("slashWaves",0u)},
            {"parries",stats.parries-before.value("parries",0u)},{"perfectParries",stats.perfectParries-before.value("perfectParries",0u)},
            {"maxCharge",specialValidation_["maxCharge"]},{"maxLinks",specialValidation_["maxLinks"]},
            {"waveSamples",specialValidation_["waveSamples"]},{"railSamples",specialValidation_["railSamples"]},{"armoredRemaining",specialValidation_["armoredRemaining"]}};
        if(index==0&&(result["railShots"].get<int>()!=1||result["damage"].get<int>()<25||result["secondDamage"].get<int>()<25||result["maxCharge"].get<float>()<.99f))fail("Rail charge/release/pierce actual target damage incomplete");
        if(index==1&&(result["linkTicks"].get<int>()<2||result["damage"].get<int>()<2||result["maxLinks"].get<int>()!=3))fail("Actual drone laser contact/damage incomplete");
        if(index==2&&(result["slashWaves"].get<int>()<1||result["damage"].get<int>()<1||result["waveSamples"].get<int>()<1))fail("Actual finisher wave failed to hit outside melee reach");
        if(index==3&&(result["parries"].get<int>()<3||result["perfectParries"].get<int>()<2||result["armoredRemaining"].get<float>()<=0||result["armoredRemaining"].get<float>()>=12))fail("Normal/perfect cut and armored bullet survival incomplete");
        specialValidation_["probes"].push_back(result);specialValidation_["beginNext"]=true;write();
    }
    RefreshTankExpeditionUi();return true;
}
