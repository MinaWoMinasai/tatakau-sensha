#include "GameScene.h"
#include "Enemy.h"
#include <fstream>
#include <iomanip>

namespace {
constexpr const char* kSpecialDirectory="generated/special_validation/";
constexpr std::array<const char*,15> kSpecialNames{"RailCannon","DroneLaserLink","SlashWave","ParryBlade",
    "ChainLightning","MarkDetonation","BoomerangShell","KillBurst","DroneCharge","DroneRebuildBomb",
    "TargetPainter","AutonomousSpread","DashSlash","SpinBlade","WallSmash"};
constexpr std::array<tankrun::CardId,15> kSpecialEffects{tankrun::CardId::RailCannon,tankrun::CardId::DroneLaserLink,
    tankrun::CardId::SlashWave,tankrun::CardId::ParryBlade,tankrun::CardId::ChainLightning,tankrun::CardId::MarkDetonation,
    tankrun::CardId::BoomerangShell,tankrun::CardId::KillBurst,tankrun::CardId::DroneCharge,tankrun::CardId::DroneRebuildBomb,
    tankrun::CardId::TargetPainter,tankrun::CardId::AutonomousSpread,tankrun::CardId::DashSlash,tankrun::CardId::SpinBlade,tankrun::CardId::WallSmash};
/// @brief 検証用の対象配置を計算する。
cg2::Vector3 ProbePosition(int index,size_t actor,float age) {
    switch(index) {
    case 0:return {38+4*static_cast<float>(actor),29,0};
    case 1:return {age>1.2f?28.25f:25.0f,29,0};
    case 2:return {38.6f,29,0};
    case 4:return actor==0?cg2::Vector3{38,29,0}:actor==1?cg2::Vector3{41,32,0}:cg2::Vector3{44,31,0};
    case 7:return actor==0?cg2::Vector3{38,29,0}:actor==1?cg2::Vector3{38,32,0}:cg2::Vector3{38,26,0};
    case 11:return {38,25+4*static_cast<float>(actor),0};
    case 13:return actor==0?cg2::Vector3{33,29,0}:cg2::Vector3{27,29,0};
    case 14:return actor==0?cg2::Vector3{65.5f,29,0}:cg2::Vector3{65,31.5f,0};
    default:return {38,29,0};
    }
}
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
    if(elapsed>140) {fail("Special combat runtime timed out");write();PostQuitMessage(8);return true;}
    const bool transitioning=expeditionTransition_.IsActive();UpdateExpeditionPresentation(dt);
    if(transitioning) {RefreshTankExpeditionUi();return true;}
    if(!specialValidation_.value("requested",false)) {
        if(elapsed>.3) {specialValidation_["requested"]=true;RequestExpeditionMapNode("special_probe");}
        RefreshTankExpeditionUi();return true;
    }
    if(!tankExpedition_.IsCombat())return true;
    int index=specialValidation_.value("probe",-1);
    if(index>=static_cast<int>(kSpecialNames.size())) {
        if(tankRunCapturePath_.empty()) {specialValidation_["completed"]=specialValidation_["errors"].empty();write();PostQuitMessage(specialValidation_["completed"].get<bool>()?0:8);}
        return true;
    }
    if(index<0||specialValidation_.value("beginNext",false)) {
        index=index<0?0:index+1;specialValidation_["probe"]=index;specialValidation_["beginNext"]=false;
        if(index>=static_cast<int>(kSpecialNames.size()))return true;
        enemyManager_->ClearRunActors();bulletManager_->ClearAll();enemy_->SetRunEncounterEnabled(false);tankExpeditionRivalActive_=false;
        playerMeleeSlashes_.clear();playerLaserBeams_.clear();playerMines_.clear();expeditionHitSparks_.clear();
        if(playerMeleeTrailManager_)playerMeleeTrailManager_->ClearInstances();
        player_->ResetRunRoomState(index==14?cg2::Vector3{62,29,0}:cg2::Vector3{30,29,0});player_->SetDebugNoDamage(true);
        tankRun_.Reset(20260927);tankRun_.ChooseLoadout(0);tankRun_.ChooseCore(0);
        if(!tankRun_.GrantExpeditionModules({kSpecialEffects[static_cast<size_t>(index)]}))fail("Could not grant actual expedition module");
        if(index==12)tankRun_.GrantExpeditionModules({tankrun::CardId::SlashWave});
        if(index==13)tankRun_.GrantExpeditionModules({tankrun::CardId::ParryBlade});
        if(index==14)tankRun_.GrantExpeditionModules({tankrun::CardId::ImpactDrive});
        ApplyTankRunCards();
        expeditionBuildStyle_=index==0||(index>=4&&index<=7)?tankbuild::Style::Shooter:
            index==1||(index>=8&&index<=11)?tankbuild::Style::Drone:tankbuild::Style::Melee;
        if(!player_->SetExpeditionCombatStyle(expeditionBuildStyle_))fail("Could not equip style");
        player_->SetDemoInput(true,{},index==14?cg2::Vector3{68,29,0}:cg2::Vector3{50,29,0},false,false);
        const auto stats=player_->GetSpecialCombatStats();
        specialValidation_["before"]={{"railShots",stats.railShots},{"linkTicks",stats.linkTicks},{"slashWaves",stats.slashWaves},{"parries",stats.parries},{"perfectParries",stats.perfectParries},
            {"droneCharges",stats.droneCharges},{"droneChargeHits",stats.droneChargeHits},{"droneBombs",stats.droneBombs},{"droneRebuilds",stats.droneRebuilds},
            {"targetLocks",stats.targetLocks},{"dashSlashes",stats.dashSlashes},{"dashSlashHits",stats.dashSlashHits},{"spinTicks",stats.spinTicks},{"wallSmashes",stats.wallSmashes}};
        specialValidation_["age"]=0.0;specialValidation_["minHp"]=index==7?6:5000;specialValidation_["secondMinHp"]=5000;
        specialValidation_["normalInjected"]=false;specialValidation_["perfectInjected"]=false;specialValidation_["armoredRemaining"]=12.0f;
        specialValidation_["maxCharge"]=0.0f;specialValidation_["maxLinks"]=0;specialValidation_["waveSamples"]=0;specialValidation_["railSamples"]=0;
        specialValidation_["dashInjected"]=false;specialValidation_["minAvailable"]=99;specialValidation_["returnedEscort"]=false;
        specialValidation_["thirdMinHp"]=5000;specialValidation_["spinCutInjected"]=false;
        const int count=index==3?0:index==0||index==13||index==14?2:index==4||index==7||index==11?3:1;
        for(int n=0;n<count;++n)enemyManager_->SpawnLevelEnemy(ProbePosition(index,static_cast<size_t>(n),0),"special_target",index==7&&n==0?6:5000);
        SetEventCallout(std::string("SPECIAL VALIDATION / ")+kSpecialNames[static_cast<size_t>(index)],1.2f);
        write();
    }
    const float age=static_cast<float>(specialValidation_.value("age",0.0)+dt);specialValidation_["age"]=age;
    // Keep the wave probe outside direct sword reach despite attack follow-through.
    if(index==2||index==6||index==13) {player_->SetWorldPosition({30,29,0});player_->SetVelocity({});}
    const auto actors=enemyManager_->GetEnemyPtrs();
    // Position fixtures isolate damage from movement/knockback; no HP or damage is injected.
    for(size_t i=0;i<actors.size();++i) {
        if(index!=14&&index!=7)actors[i]->SetWorldPosition(ProbePosition(index,i,age));
        const char* field=i==0?"minHp":i==1?"secondMinHp":"thirdMinHp";specialValidation_[field]=(std::min)(specialValidation_.value(field,5000),actors[i]->GetHp());
    }
    const auto stats=player_->GetSpecialCombatStats();const auto& before=specialValidation_["before"];
    specialValidation_["maxCharge"]=(std::max)(specialValidation_.value("maxCharge",0.0f),player_->GetRailChargeRatio());
    specialValidation_["maxLinks"]=(std::max)(specialValidation_.value("maxLinks",0),static_cast<int>(player_->GetDroneLaserLinks().size()));
    for(auto* bullet:bulletManager_->GetBulletPtrs()) {
        if(bullet->GetSpecialKind()==Bullet::SpecialKind::Rail) {specialValidation_["railSamples"]=specialValidation_.value("railSamples",0)+1;capture("rail_fire");}
        if(bullet->GetSpecialKind()==Bullet::SpecialKind::SlashWave) {specialValidation_["waveSamples"]=specialValidation_.value("waveSamples",0)+1;if(cg2::Length(bullet->GetWorldPosition()-player_->GetWorldPosition())>4.5f)capture("slash_wave");}
        // Latch the first cut: a later, separately timed sword swing may finish it.
        if(index==3&&bullet->GetOwner()==kEnemy&&bullet->GetDamage()==13&&bullet->GetBulletHp()<12&&specialValidation_.value("armoredRemaining",12.0f)==12)
            specialValidation_["armoredRemaining"]=bullet->GetBulletHp();
    }
    bool shoot=false;
    if(index==0) {shoot=age>.3f&&age<1.5f;if(player_->GetRailChargeRatio()>=.999f)capture("rail_charge");}
    if(index==1&&std::any_of(player_->GetDroneLaserLinks().begin(),player_->GetDroneLaserLinks().end(),[](const auto& link){return link.contact;}))capture("drone_link");
    if(index>=2)shoot=age>.35f;
    if(index==12)shoot=age>.42f;
    if(index==4||index==6||index==7)shoot=age>.35f&&age<.46f;
    bool dash=false;
    if((index==12||index==14)&&age>.42f&&!specialValidation_.value("dashInjected",false)) {dash=true;specialValidation_["dashInjected"]=true;}
    player_->SetDemoInput(true,{},index==14?cg2::Vector3{68,29,0}:index>=8&&index<=11?cg2::Vector3{38,29,0}:cg2::Vector3{50,29,0},shoot,dash);
    if(index>=8&&index<=11) {
        int available=0;bool escort=true;
        for(auto* drone:player_->GetDronePtrs()) {if(drone->IsRunAvailable())++available;if(drone->GetRunMission().GetPhase()!=tankspecial::DronePhase::Escort)escort=false;}
        specialValidation_["minAvailable"]=(std::min)(specialValidation_.value("minAvailable",99),available);
        if(escort&&stats.droneChargeHits>before.value("droneChargeHits",0u))specialValidation_["returnedEscort"]=true;
    }
    if(index==13&&player_->GetSpinBladeRatio()>.35f&&!specialValidation_.value("spinCutInjected",false)) {
        auto shot=std::make_unique<Bullet>();shot->Initialize(player_->GetWorldPosition()+cg2::Vector3{-2.5f,0,0},{.01f,0,0},7,kEnemy,false,6,1);
        shot->ConfigureGrowth(0,0,0);bulletManager_->Add(std::move(shot));specialValidation_["spinCutInjected"]=true;
        specialValidation_["perfectAtSpin"]=stats.perfectParries;specialValidation_["parryAtSpin"]=stats.parries;
    }
    const auto shooterStats=bulletManager_->GetShooterStats();
    if(index==4&&shooterStats.chainHits>0)capture("chain_lightning");
    if(index==5&&shooterStats.detonations>0)capture("mark_detonation");
    if(index==6&&shooterStats.returns>0)capture("boomerang_shell");
    if(index==7&&shooterStats.burstTriggers>0)capture("kill_burst");
    if(index==8&&stats.droneChargeHits>before.value("droneChargeHits",0u))capture("drone_charge");
    if(index==9&&stats.droneBombs>before.value("droneBombs",0u))capture("drone_bomb");
    if(index==9&&stats.droneRebuilds>before.value("droneRebuilds",0u))capture("drone_rebuild");
    if(index==10&&stats.targetLocks>before.value("targetLocks",0u))capture("target_painter");
    if(index==11&&age>1.8f)capture("autonomous_spread");
    if(index==12&&stats.dashSlashes>before.value("dashSlashes",0u))capture("dash_slash");
    if(index==13&&player_->GetSpinBladeRatio()>.3f)capture("spin_blade");
    if(index==14&&stats.wallSmashes>before.value("wallSmashes",0u))capture("wall_smash");
    if(index==3) {
        const auto spawn=[&](float hp,uint32_t damage,const cg2::Vector3& offset) {auto b=std::make_unique<Bullet>();b->Initialize(player_->GetWorldPosition()+offset,{-.02f,0,0},damage,kEnemy,false,hp,1);b->ConfigureGrowth(0,0,0);bulletManager_->Add(std::move(b));};
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
    const float duration=index==9?11.5f:index==8?5.4f:index==3||index==10||index==12||index==13||index==14?5.0f:4.0f;
    if(age>duration&&tankRunCapturePath_.empty()) {
        nlohmann::json result={{"id",kSpecialNames[static_cast<size_t>(index)]},{"damage",(index==7?6:5000)-specialValidation_.value("minHp",5000)},
            {"secondDamage",5000-specialValidation_.value("secondMinHp",5000)},{"railShots",stats.railShots-before.value("railShots",0u)},
            {"linkTicks",stats.linkTicks-before.value("linkTicks",0u)},{"slashWaves",stats.slashWaves-before.value("slashWaves",0u)},
            {"parries",stats.parries-before.value("parries",0u)},{"perfectParries",stats.perfectParries-before.value("perfectParries",0u)},
            {"maxCharge",specialValidation_["maxCharge"]},{"maxLinks",specialValidation_["maxLinks"]},
            {"waveSamples",specialValidation_["waveSamples"]},{"railSamples",specialValidation_["railSamples"]},{"armoredRemaining",specialValidation_["armoredRemaining"]},
            {"chainHits",shooterStats.chainHits},{"detonations",shooterStats.detonations},{"returns",shooterStats.returns},
            {"burstTriggers",shooterStats.burstTriggers},{"burstChildren",shooterStats.burstChildren},
            {"droneCharges",stats.droneCharges-before.value("droneCharges",0u)},{"droneChargeHits",stats.droneChargeHits-before.value("droneChargeHits",0u)},
            {"droneBombs",stats.droneBombs-before.value("droneBombs",0u)},{"droneRebuilds",stats.droneRebuilds-before.value("droneRebuilds",0u)},
            {"targetLocks",stats.targetLocks-before.value("targetLocks",0u)},{"minAvailable",specialValidation_["minAvailable"]},{"returnedEscort",specialValidation_["returnedEscort"]},
            {"dashSlashes",stats.dashSlashes-before.value("dashSlashes",0u)},{"dashSlashHits",stats.dashSlashHits-before.value("dashSlashHits",0u)},
            {"spinTicks",stats.spinTicks-before.value("spinTicks",0u)},{"wallSmashes",stats.wallSmashes-before.value("wallSmashes",0u)},
            {"thirdDamage",5000-specialValidation_.value("thirdMinHp",5000)}};
        if(index==0&&(result["railShots"].get<int>()!=1||result["damage"].get<int>()<25||result["secondDamage"].get<int>()<25||result["maxCharge"].get<float>()<.99f))fail("Rail charge/release/pierce actual target damage incomplete");
        if(index==1&&(result["linkTicks"].get<int>()<2||result["damage"].get<int>()<2||result["maxLinks"].get<int>()!=3))fail("Actual drone laser contact/damage incomplete");
        if(index==2&&(result["slashWaves"].get<int>()<1||result["damage"].get<int>()<1||result["waveSamples"].get<int>()<1))fail("Actual finisher wave failed to hit outside melee reach");
        if(index==3&&(result["parries"].get<int>()<3||result["perfectParries"].get<int>()<2||result["armoredRemaining"].get<float>()<=0||result["armoredRemaining"].get<float>()>=12))fail("Normal/perfect cut and armored bullet survival incomplete");
        if(index==4&&(shooterStats.chainHits!=2||result["secondDamage"].get<int>()<1||result["thirdDamage"].get<int>()<1))fail("Chain lightning did not hit exactly two additional targets");
        if(index==5&&shooterStats.detonations<1)fail("Actual repeated projectile hits did not detonate mark");
        if(index==6&&(shooterStats.returns!=1||result["damage"].get<int>()<12))fail("One boomerang did not return and hit outward/inward");
        if(index==7&&(shooterStats.burstTriggers!=1||shooterStats.burstChildren!=6))fail("Kill burst missing or recursively triggered");
        if(index==8&&(result["droneCharges"].get<int>()<1||result["droneChargeHits"].get<int>()<1||!specialValidation_.value("returnedEscort",false)))fail("Drone charge hit/return incomplete");
        if(index==9&&(result["droneBombs"].get<int>()<1||result["droneRebuilds"].get<int>()<1||specialValidation_.value("minAvailable",99)!=2))fail("Drone bomb offline/rebuild incomplete");
        if(index==10&&result["targetLocks"].get<int>()<1)fail("Distinct drone attacks did not build target LOCK");
        if(index==11&&(result["damage"].get<int>()<1||result["secondDamage"].get<int>()<1||result["thirdDamage"].get<int>()<1))fail("Autonomous drones did not damage distinct targets");
        if(index==12&&(result["dashSlashes"].get<int>()<1||result["dashSlashHits"].get<int>()<1||result["slashWaves"].get<int>()<1))fail("Dash slash hit/combo finisher connection incomplete");
        if(index==13&&(result["spinTicks"].get<int>()<4||result["secondDamage"].get<int>()<1||stats.parries<=specialValidation_.value("parryAtSpin",stats.parries)||stats.perfectParries!=specialValidation_.value("perfectAtSpin",stats.perfectParries)))fail("Spin damage or non-perfect bullet cut incomplete");
        if(index==14&&result["wallSmashes"].get<int>()<1)fail("Actual knockback/wall collision did not create WallSmash");
        specialValidation_["probes"].push_back(result);specialValidation_["beginNext"]=true;write();
    }
    RefreshTankExpeditionUi();return true;
}
