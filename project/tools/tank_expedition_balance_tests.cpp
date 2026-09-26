#include "../game/run/TankExpeditionBalance.h"
#include "../game/run/TankExpeditionEncounters.h"
#include "../game/player/TankRunModifiers.h"
#include "../game/exp/ExpEnemyCombatCycle.h"
#include "../game/enemy/actor/PrototypeBossCombat.h"
#include <cassert>
#include <iostream>
#include <limits>

void PersistenceAndValidation() {
    const auto defaults=tankexp::DefaultBalance();
    auto changed=defaults;
    changed["player"]["maxHp"]=175;
    changed["player"]["bulletDamage"]=7.5f;
    changed["playerUpgrades"]["reloadSpeed"]=0.55f;
    changed["bossMaxHp"]=1200;
    changed["bossAttackDefault"]["bulletCount"]=9;
    changed["damage"]["shooterFireInterval"]=2.4f;
    changed["combatStyles"]["melee"]["attackDamage"]=45.0f;
    changed["combatStyles"]["melee"]["attackIntervalSeconds"]=0.75f;
    changed["combatStyles"]["drone"]["droneCount"]=9;
    std::string error;
    assert(tankexp::SaveBalance("balance_roundtrip.json",changed,error));
    nlohmann::json loaded;
    assert(tankexp::LoadBalance("balance_roundtrip.json",loaded,error));
    assert(loaded==tankexp::SanitizeBalance(changed));
    assert(loaded["player"]["maxHp"]==175 && loaded["bossMaxHp"]==1200);
    assert(loaded["bossAttackDefault"]["bulletCount"]==9);
    const auto profiles=tankexp::ReadCombatStyleBalances(loaded);
    assert(profiles[2].attackDamage==45&&profiles[2].attackIntervalSeconds==.75f&&profiles[1].droneCount==9);
    assert(profiles[0].attackDamage==4); // Editing a family never leaks into others.
    std::ofstream("balance_bad.json")<<"{broken";
    const auto before=loaded;
    assert(!tankexp::LoadBalance("balance_bad.json",loaded,error)&&loaded==before);
    assert(!tankexp::LoadBalance("missing-balance-file.json",loaded,error)&&loaded==before);
    changed["player"]["maxHp"]=-100;
    changed["player"]["bulletDamage"]=std::numeric_limits<double>::infinity();
    changed["player"]["moveSpeed"]="broken";
    changed["playerUpgrades"]["reloadSpeed"]=2000;
    changed["damage"]=false;
    changed["bossAttackDefault"]["bulletCount"]=100000;
    changed["combatStyles"]["melee"]["attackIntervalSeconds"]=-10;
    changed["combatStyles"]["drone"]["droneResponse"]="broken";
    changed["combatStyles"]["drone"]["droneCount"]=1000;
    const auto safe=tankexp::SanitizeBalance(changed);
    assert(safe["player"]["maxHp"]==1);
    assert(safe["player"]["bulletDamage"]==defaults["player"]["bulletDamage"]);
    assert(safe["player"]["moveSpeed"]==defaults["player"]["moveSpeed"]);
    assert(safe["playerUpgrades"]["reloadSpeed"].get<float>()<=0.9f);
    assert(safe["damage"]==defaults["damage"]);
    assert(safe["bossAttackDefault"]["bulletCount"]==32);
    const auto sanitized=tankexp::ReadCombatStyleBalances(safe);
    assert(sanitized[2].attackIntervalSeconds==.05f&&sanitized[1].droneCount==12&&sanitized[1].droneResponse==5);
    auto legacy=defaults;legacy.erase("combatStyles");legacy["schemaVersion"]=1;
    legacy["player"]["bulletDamage"]=7;legacy["player"]["maxHp"]=175;legacy["playerUpgrades"]["bulletDamage"]=.8f;
    const auto migrated=tankexp::SanitizeBalance(legacy);
    const auto migratedProfiles=tankexp::ReadCombatStyleBalances(migrated);
    assert(migratedProfiles[0].attackDamage==7&&migratedProfiles[2].maxHp==175);
    assert(migrated["playerUpgrades"]["bulletDamage"].get<float>()==.25f);
    assert(tankexp::SanitizeBalance(nullptr)==defaults);
    assert(tankexp::SanitizeBalance(safe)==safe); // Apply/Save/Reload never stack.
}
void GrowthAndEncounterRamp() {
    TankRunModifiers m{};m.enabled=true;m.expedition=true;
    const auto start=MakeTankRunTuning(m);
    assert(start.extraProjectiles==0&&!start.reflects&&start.damage==1&&start.reloadInterval==1);
    for(unsigned seed=0;seed<512;++seed) {
        tankrun::RunDirector cards(seed);
        assert(cards.ChooseLoadout(0)&&cards.ChooseCore(0));
        assert(cards.OpenExpeditionRewardDraft(0,false));
        const auto offers=cards.GetOffers();
        assert(offers[0]==tankrun::CardId::Rapid&&offers[1]==tankrun::CardId::Heavy&&offers[2]==tankrun::CardId::Thrusters);
        assert(cards.ChooseCard(seed%3));
        assert(cards.GetDraftCount()==1);
    }
    m.rapid=true;const auto rapid=MakeTankRunTuning(m);
    assert(rapid.reloadInterval==0.75f&&rapid.damage==1);
    m.rapid=false;m.heavy=true;const auto heavy=MakeTankRunTuning(m);
    assert(heavy.damage==1.25f&&heavy.bulletSpeed>1.15f);
    m.heavy=false;m.thrusters=true;
    assert(MakeTankRunTuning(m).moveSpeed==1.12f);
    m.repair=true;
    assert(MakeTankRunTuning(m).maxHp==1.15f);
    m.core=TankRunCore::Ricochet;m.rapid=true;m.scatterShot=true;
    const auto late=MakeTankRunTuning(m);
    assert(late.extraProjectiles==0&&late.impactSplitCount==0&&late.reflects&&late.reloadInterval<1);
    TankRunGrowth edited{};edited.reload=0.6f;
    assert(MakeTankRunTuning(m,edited).reloadInterval<late.reloadInterval); // Authored growth is no longer silently capped.
    edited.reload=0.10f;edited.move=0.05f;edited.hp=0.08f;edited.damage=0.10f;m.heavy=true;
    const auto modest=MakeTankRunTuning(m,edited);
    assert(modest.reloadInterval>late.reloadInterval&&modest.moveSpeed==1.05f&&modest.maxHp==1.08f);
    assert(modest.damage<MakeTankRunTuning(m).damage); // Smaller authored growth remains editable.
    const auto early=tankexp::GetEncounter(tankexp::RoomKind::Skirmish,0);
    const auto middle=tankexp::GetEncounter(tankexp::RoomKind::Reflection,2);
    const auto lateEnemies=tankexp::GetEncounter(tankexp::RoomKind::Guard,3);
    assert(early.count==2&&middle.count>early.count&&lateEnemies.count>=middle.count);
    int earlyHp=0,lateHp=0;
    for(int i=0;i<early.count;++i) {earlyHp+=early.units[i].hp;assert(std::string(early.units[i].prefab)=="Charger");}
    bool shooter=false,sniper=false,charger=false;
    for(int i=0;i<lateEnemies.count;++i) {
        lateHp+=lateEnemies.units[i].hp;
        const std::string type=lateEnemies.units[i].prefab;
        shooter|=type=="Shooter";sniper|=type=="Sniper";charger|=type=="Charger";
    }
    assert(lateHp>earlyHp*3&&shooter&&sniper&&charger);
    for(int i=0;i<4;++i) {
        const auto a=tankexp::GetRoomBalance(i),b=tankexp::GetRoomBalance(i+1);
        assert(a.hpScale<b.hpScale&&a.fireIntervalScale>b.fireIntervalScale&&a.bulletSpeedScale<b.bulletSpeedScale);
    }
}
int CountBossAttacks(float scale) {
    PrototypeBossCombat boss;int shots=0;
    for(int i=0;i<3600;++i) if(boss.Step(1.0f/60.0f,true,0,1,0,false,scale).fire) ++shots;
    return shots;
}
int main() {
    PersistenceAndValidation();GrowthAndEncounterRamp();
    assert(CountBossAttacks(0.5f)>CountBossAttacks(2.0f));
    ExpEnemyCombatCycle cycle;cycle.Reset({},0);cycle.SetRecoveryScale(3);
    assert(!cycle.Advance(0.01f,true));
    for(int i=0;i<30;++i) cycle.Advance(1.0f/60.0f,true);
    assert(cycle.GetPhase()==ExpEnemyCombatPhase::Tracking); // Editing cadence keeps warning visible.
    std::cout<<"Expedition balance PASS: bounded JSON, real disk roundtrip, safe failed reload, 512 guaranteed first drafts, visible growth, room threat ramp, adjustable boss cadence.\n";
}
