#pragma once
#include "TankRunDirector.h"
#include "TankBuildStyle.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <string>
#include <vector>
#include <unordered_map>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

// Authoring data is independent of renderer / scene. Unknown behavior names are
// rejected rather than silently producing an enemy with a different attack.
namespace tankcontent {
inline constexpr const char* kCatalogPath="resources/configs/expedition_content.json";
inline constexpr std::size_t kMaxDefinitions=128;
inline std::filesystem::path Utf8Path(const std::string& path){return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(path.data()),path.size()));}
enum class EnemyBehavior { Square,Triangle,Pentagon,Shooter,Charger,Sniper,Skirmisher,Flanker,Suppressor,ShieldGuard,BladeGuard,SummonerCommander,EMPJammer,ReflectArmor };
struct Upgrade {
    std::string id,name,description;
    int price=25,maxPurchases=1,rarity=0;
    std::vector<tankrun::CardId> effects{tankrun::CardId::Rapid};
    unsigned compatibleStyles=tankbuild::AllStyles;
    std::array<float,tankrun::CardCount> effectPower=[] {std::array<float,tankrun::CardCount> p{};p.fill(1);return p;}();
    std::string refitPlayer; // Legacy parsing only; no expedition card may use replacement growth.
};
struct Enemy {
    std::string id,name;
    EnemyBehavior behavior=EnemyBehavior::Charger;
    int hp=24,contactDamage=8,bulletDamage=10,creditDrop=5;
    float moveSpeedScale=1,fireIntervalScale=1;
    int magazineSize=0; // Zero selects the behavior default.
    float reloadSeconds=0;
    std::array<float,4> color{1.6f,0.36f,0.1f,1};
};
struct PlayerVariant {
    std::string id,name,description,baseClass="Twin";
    int price=40,barrels=2,bulletCount=1,bodyShape=0,drones=0;
    float damageScale=1,reloadScale=1,bulletSpeedScale=1,fanAngle=0;
    bool alternate=false,reflect=false,penetrate=false;
    std::array<float,4> color{0.3f,1.5f,1.3f,1};
    tankbuild::Style style=tankbuild::Style::Shooter;
    int rarity=0;
};
struct Catalog { std::vector<Upgrade> upgrades;std::vector<Enemy> enemies;std::vector<PlayerVariant> players; };
inline constexpr std::array<const char*,tankrun::CardCount> kEffectIds{
    "Ricochet","Heavy","Rapid","Thrusters","Capacitor","Repair","Drones","Pierce",
    "ScatterShot","Homing","DashBurst","Overdrive","MeleeBlade","BladeReach","ImpactDrive","PerfectDodge",
    "DroneFocus","DroneGuard","MeleeTempo","FinisherCharge","RailCannon","DroneLaserLink","SlashWave","ParryBlade",
    "ExtraBarrel1","ExtraBarrel2","FanMount","AlternatingFire","HeavyDroneCore","LightBladeActuator","HeavyBladeEdge","ChainLightning","MarkDetonation","BoomerangShell","KillBurst","DroneCharge","DroneRebuildBomb","TargetPainter","AutonomousSpread","DashSlash","SpinBlade","WallSmash"};
inline constexpr std::array<const char*,tankrun::CardCount> kEffectNames{
    "壁反射","攻撃力","攻撃速度","推進器","蓄電池","装甲修復","支援ドローン","貫通",
    "廃止済み","追尾","衝撃放電","過負荷","ネオンブレード","刃先延長","衝突増幅","ジャスト回避",
    "精密ドローン","防衛ドローン","連撃加速","終撃増幅","レールキャノン","レーザーリンク","斬撃波","パリィブレード",
    "追加砲門","追加砲門II","扇形砲架","交互射撃機構","重装ドローン","軽量ブレード機構","重装ブレード","連鎖放電","爆裂マーカー","往復弾","撃破バースト","突撃ドローン","自爆再構築","ターゲットペインター","自律分散モード","ダッシュ斬り","回転ブレード","壁砕き"};
inline constexpr std::array<const char*,14> kBehaviorIds{"Square","Triangle","Pentagon","Shooter","Charger","Sniper","Skirmisher","Flanker","Suppressor","ShieldGuard","BladeGuard","SummonerCommander","EMPJammer","ReflectArmor"};
inline constexpr std::array<const char*,14> kBehaviorNames{"四角資源","三角資源","五角資源","射撃砲台","突進兵","狙撃兵","機動射撃兵","接近散弾兵","制圧射撃兵","シールド兵","ブレード兵","召喚指揮兵","EMP妨害兵","反射装甲兵"};
inline int CreditsFromExperience(int amount) { return amount<=0?0:(std::clamp)(amount/5,1,100000); }
template<class T> inline const T* FindDefinition(const std::vector<T>& items,const std::string& id) {
    const auto it=std::find_if(items.begin(),items.end(),[&](const T& item){return item.id==id;});
    return it==items.end()?nullptr:&*it;
}
inline const Upgrade* FindUpgrade(const Catalog& c,const std::string& id){return FindDefinition(c.upgrades,id);}
inline const Enemy* FindEnemy(const Catalog& c,const std::string& id){return FindDefinition(c.enemies,id);}
inline const PlayerVariant* FindPlayer(const Catalog& c,const std::string& id){return FindDefinition(c.players,id);}
// Only known retired standard IDs are removed. User-created composite effects
// remain supported; legacy player designs remain available as authoring data.
inline bool IsRetiredStandardUpgrade(const std::string& id) {
    constexpr std::array<const char*,8> retired{"BankshotKit","DashBomberKit","BreachBladeKit","ArcBladeKit","DroneBastionKit","SeekingWingKit","SeekingRicochetKit","PredatorCircuit"};
    return id.rfind("Refit_",0)==0||std::find(retired.begin(),retired.end(),id)!=retired.end();
}
inline bool MeetsUpgradeRequirements(const Upgrade& upgrade,const tankrun::CardCounts& owned) {
    auto has=[&](tankrun::CardId id){return owned[static_cast<std::size_t>(id)]>0||std::find(upgrade.effects.begin(),upgrade.effects.end(),id)!=upgrade.effects.end();};
    for(const auto effect:upgrade.effects) {
        if(!tankrun::IsAvailableCard(effect))return false;
        if(effect==tankrun::CardId::BladeReach&&!has(tankrun::CardId::MeleeBlade))return false;
        if(effect==tankrun::CardId::Capacitor&&!has(tankrun::CardId::PerfectDodge))return false;
        if(effect==tankrun::CardId::ExtraBarrel2&&!has(tankrun::CardId::ExtraBarrel1))return false;
        if((effect==tankrun::CardId::FanMount||effect==tankrun::CardId::AlternatingFire)&&!has(tankrun::CardId::ExtraBarrel1))return false;
    }
    return true;
}
inline unsigned EffectStyles(tankrun::CardId effect) {
    using C=tankrun::CardId;
    if(!tankrun::IsAvailableCard(effect))return 0;
    switch(effect) {
    case C::RailCannon:case C::ExtraBarrel1:case C::ExtraBarrel2:case C::FanMount:case C::AlternatingFire:
    case C::ChainLightning:case C::MarkDetonation:case C::BoomerangShell:case C::KillBurst:return tankbuild::Mask(tankbuild::Style::Shooter);
    case C::Ricochet:case C::Pierce:case C::Homing:return 3;
    case C::Drones:case C::DroneFocus:case C::DroneGuard:case C::DroneLaserLink:
    case C::HeavyDroneCore:case C::DroneCharge:case C::DroneRebuildBomb:case C::TargetPainter:case C::AutonomousSpread:return tankbuild::Mask(tankbuild::Style::Drone);
    case C::MeleeBlade:case C::BladeReach:case C::MeleeTempo:case C::FinisherCharge:case C::SlashWave:case C::ParryBlade:
    case C::LightBladeActuator:case C::HeavyBladeEdge:case C::DashSlash:case C::SpinBlade:case C::WallSmash:return tankbuild::Mask(tankbuild::Style::Melee);
    default:return tankbuild::AllStyles;
    }
}
inline unsigned CompatibleEffectStyles(const Upgrade& upgrade) {
    if(!upgrade.refitPlayer.empty())return 0;
    unsigned mask=tankbuild::AllStyles;
    for(const auto effect:upgrade.effects)mask&=EffectStyles(effect);
    return upgrade.effects.empty()?0:mask;
}
inline bool EligibleUpgrade(const Upgrade& upgrade,tankbuild::Style style,const tankrun::CardCounts& owned) {
    if(IsRetiredStandardUpgrade(upgrade.id)||!upgrade.refitPlayer.empty()||!tankbuild::Valid(style)||!tankbuild::ValidRarity(upgrade.rarity)||upgrade.effects.empty()||
       !(upgrade.compatibleStyles&tankbuild::Mask(style))||!(CompatibleEffectStyles(upgrade)&tankbuild::Mask(style)))return false;
    auto effectiveOwned=owned;
    if(style==tankbuild::Style::Melee)effectiveOwned[static_cast<std::size_t>(tankrun::CardId::MeleeBlade)]=1;
    std::set<tankrun::CardId> distinct;
    for(const auto effect:upgrade.effects) {
        if(!tankrun::IsAvailableCard(effect)||effectiveOwned[static_cast<std::size_t>(effect)]>0||!distinct.insert(effect).second)return false;
    }
    return MeetsUpgradeRequirements(upgrade,effectiveOwned);
}
inline bool IsBehaviorUpgrade(const Upgrade& upgrade) {
    if(!upgrade.refitPlayer.empty()||IsRetiredStandardUpgrade(upgrade.id))return false;
    using C=tankrun::CardId;
    for(const auto effect:upgrade.effects)switch(effect) {
    case C::RailCannon:case C::DroneLaserLink:case C::SlashWave:case C::ParryBlade:
    case C::Homing:case C::Ricochet:case C::Pierce:case C::Overdrive:case C::Drones:
    case C::DroneGuard:case C::DroneFocus:case C::BladeReach:case C::ImpactDrive:
    case C::FinisherCharge:case C::DashBurst:case C::PerfectDodge:
    case C::ExtraBarrel1:case C::ExtraBarrel2:case C::FanMount:case C::AlternatingFire:case C::HeavyDroneCore:case C::LightBladeActuator:case C::HeavyBladeEdge:case C::ChainLightning:case C::MarkDetonation:case C::BoomerangShell:case C::KillBurst:case C::DroneCharge:case C::DroneRebuildBomb:case C::TargetPainter:case C::AutonomousSpread:case C::DashSlash:case C::SpinBlade:case C::WallSmash:return true;
    default:break;
    }
    return false;
}
inline std::vector<std::string> BuildShopOffers(const Catalog& catalog,tankbuild::Style style,const tankrun::CardCounts& owned,
    const std::unordered_map<std::string,int>& purchased,std::uint32_t seed) {
    std::vector<const Upgrade*> eligible,special;
    for(const auto& upgrade:catalog.upgrades) {
        const auto purchase=purchased.find(upgrade.id);
        if((purchase!=purchased.end()&&purchase->second>=upgrade.maxPurchases)||!EligibleUpgrade(upgrade,style,owned))continue;
        eligible.push_back(&upgrade);if(IsBehaviorUpgrade(upgrade))special.push_back(&upgrade);
    }
    std::uint32_t state=seed?seed:0x9e3779b9u;
    auto draw=[&](unsigned limit){state^=state<<13;state^=state>>17;state^=state<<5;return state%limit;};
    std::vector<std::string> result;
    auto pick=[&](std::vector<const Upgrade*>& pool) {
        if(pool.empty())return;
        unsigned total=0;for(const auto* option:pool)total+=tankbuild::RarityWeights[static_cast<unsigned>(option->rarity)];
        unsigned value=draw(total);std::size_t chosen=0;
        while(value>=tankbuild::RarityWeights[static_cast<unsigned>(pool[chosen]->rarity)])value-=tankbuild::RarityWeights[static_cast<unsigned>(pool[chosen++]->rarity)];
        result.push_back(pool[chosen]->id);pool.erase(pool.begin()+static_cast<std::ptrdiff_t>(chosen));
    };
    // Exactly one priority draw, then two ordinary weighted draws. Remove the
    // chosen entry from both pools; no duplicate or already owned card appears.
    pick(special);
    if(!result.empty())std::erase_if(eligible,[&](const Upgrade* u){return u->id==result.front();});
    while(result.size()<3&&!eligible.empty())pick(eligible);
    return result;
}
// Both introduction paths share one run seed and the same three foundations.
// This RNG is local: hovering, combat, or opening the other path cannot reroll it.
inline std::vector<std::string> IntroUpgradeIds(const Catalog& catalog,std::uint32_t seed) {
    constexpr std::array<tankrun::CardId,4> pool{tankrun::CardId::Heavy,tankrun::CardId::Rapid,tankrun::CardId::Thrusters,tankrun::CardId::Repair};
    std::vector<std::string> ids;
    for(const auto effect:pool) {
        const auto* id=kEffectIds[static_cast<std::size_t>(effect)];
        if(const auto* upgrade=FindUpgrade(catalog,id);upgrade&&upgrade->effects.size()==1&&upgrade->effects.front()==effect&&upgrade->rarity==0&&upgrade->compatibleStyles==tankbuild::AllStyles)ids.emplace_back(id);
    }
    std::uint32_t state=seed?seed:0x9e3779b9u;
    for(std::size_t n=ids.size();n>1;--n) {
        state^=state<<13;state^=state>>17;state^=state<<5;
        std::swap(ids[n-1],ids[state%static_cast<std::uint32_t>(n)]);
    }
    if(ids.size()>3)ids.resize(3);
    return ids;
}
inline Catalog DefaultCatalog() {
    Catalog c;
    const std::array<const char*,tankrun::CardCount> descriptions{
        "壁で弾が反射する。1砲門1発のまま、角度を使って敵を狙う。","射撃とブレードの威力を強化。重い一撃と衝撃放電の組み合わせに。",
        "攻撃間隔を短縮。射撃・ドローン・近接のすべてに有効。","移動とダッシュを強化する。","ジャスト回避成功でHPとスタミナを回復。ジャスト回避の取得が必要。",
        "最大HPを増やし、その増加分を修復する。","支援ドローンを追加する。","弾が敵を貫く。",
        "廃止済み。","弾が近くの敵を追う。","ダッシュ開始時に近くの敵へ衝撃波。攻撃力の改造と組み合わせると衝撃を強化。",
        "ダッシュ後の攻撃を加速。追尾との組み合わせで旋回も強化。",
        "主砲を近接ブレードへ交換。左クリックで3段斬り。3段目は強い薙ぎ払い。",
        "ブレードの射程+30%、威力+20%。近接系統専用。",
        "ダッシュ体当たりの威力と押し出し+50%。ブレードの押し出しも+50%。",
        "敵弾をダッシュ開始直後に避けるとジャスト回避。短いスローと反撃強化を解放。体当たりはSLAM。",
        "ドローンの精度・照準追従を強化。威力+15%、弾速+15%。集中射撃を当てやすくする。",
        "ドローン弾の耐久と敵弾を消す力を3へ強化。弾幕を切り開く。",
        "近接連撃の各動作を18%短縮。攻撃速度の改造と組み合わせ可能。",
        "近接3段目の威力+50%、押し出し+20%。連撃の締めを強化する。",
        "左クリック長押しで約1秒チャージ、離して貫通レール弾。短押しも可能。チャージ中は移動が少し遅くなる。",
        "ドローンを輪状のレーザーで接続。隊形と位置取りで敵へ重ねて継続ダメージ。壁越しには届かない。",
        "近接3段目から斬撃波を放つ。敵を貫き、弱い敵弾を削る。刃先延長・終撃増幅と組み合わせ可能。",
        "斬撃で低耐久の敵弾を斬る。振り始めはPERFECT PARRYとなり、弾を跳ね返す。高耐久弾は耐久を削る。",
        "主砲を1門追加して2門に。各弾の威力は65%。砲門そのものを増設する。",
        "追加砲門が必要。さらに1門追加して3門に。各弾の威力は50%。取得済みの部品は残る。",
        "2門以上が必要。2門は左右8度、3門は左右14度へ主砲を開く。射撃範囲を広げる。",
        "2門以上が必要。砲門を順番に発射。同時射撃と同じ総火力で切れ目を減らす。",
        "全ドローンの威力+45%、攻撃間隔+20%。大きな弾を撃つ。機数は減らない。",
        "近接の攻撃動作を15%短縮、威力5%減。重装ブレードとも積み重なる。",
        "近接威力+45%、攻撃動作18%増。軽量ブレード機構とも積み重なる。",
        "射撃命中から半径7以内の別敵へ最大2回放電。元弾の威力の48%。同じ敵へは戻らない。",
        "射撃を4回当てると起爆。マークは4秒持続、威力は基礎弾の1.8倍。ボスは6回で起爆。",
        "弾が飛翔後に自機へ折り返す。往路・復路でそれぞれ同じ敵へ1回命中。戻ると消える。",
        "射撃で撃破すると6方向へ威力35%の小型弾。小型弾から追加バーストは起こらない。",
        "準備できたドローンが発光して標的へ突撃。命中で押し出し、自機へ帰還する。",
        "ドローン1機が点滅して突撃・自爆。約5.5秒使えなくなり、自機の近くで再構築する。",
        "複数のドローンで同じ敵を攻撃するとLOCK。4秒間ドローンの威力+35%、ボスは+18%。",
        "左クリック中、それぞれのドローンが別の近い敵を狙う。各機の威力は18%減。",
        "ダッシュ中の攻撃が突進斬りに。斬り抜けから通常2段目、3段目へつながる。",
        "3段目後も左クリックを押し続けると約0.8秒回転斬り。移動が低下。パリィで通常弾を斬れる。",
        "強く吹き飛ばした敵が壁へ当たると基礎近接威力の1.75倍の追加ダメージと半径2.8の衝撃波。"};
    for(std::size_t i=0;i<kEffectIds.size();++i) {
        if(!tankrun::IsAvailableCard(static_cast<tankrun::CardId>(i))||static_cast<tankrun::CardId>(i)==tankrun::CardId::MeleeBlade)continue;
        Upgrade u;u.id=kEffectIds[i];u.name=kEffectNames[i];u.description=descriptions[i];
        const auto effect=static_cast<tankrun::CardId>(i);
        u.rarity=tankrun::IsRare(effect)?2:0;
        if(effect==tankrun::CardId::Ricochet||effect==tankrun::CardId::Drones||effect==tankrun::CardId::DashBurst||effect==tankrun::CardId::BladeReach||effect==tankrun::CardId::DroneGuard||effect==tankrun::CardId::MeleeTempo)u.rarity=1;
        if(effect==tankrun::CardId::ExtraBarrel1||effect==tankrun::CardId::FanMount||effect==tankrun::CardId::LightBladeActuator)u.rarity=1;
        if(effect==tankrun::CardId::MarkDetonation||effect==tankrun::CardId::BoomerangShell||effect==tankrun::CardId::SpinBlade)u.rarity=3;
        if(effect==tankrun::CardId::DroneRebuildBomb||effect==tankrun::CardId::AutonomousSpread||effect==tankrun::CardId::WallSmash)u.rarity=4;
        u.price=u.rarity==4?78:u.rarity==3?58:u.rarity==2?42:u.rarity==1?28:24;u.compatibleStyles=EffectStyles(effect);u.effects={effect};c.upgrades.push_back(u);
        if(effect==tankrun::CardId::RailCannon||effect==tankrun::CardId::DroneLaserLink||effect==tankrun::CardId::ParryBlade){c.upgrades.back().rarity=3;c.upgrades.back().price=58;}
    }
    const int hp[]={6,10,24,14,30,24,28,32,42,80,65,75,60,90};const int drops[]={2,3,7,5,6,6,6,7,9,12,12,12,12,14};
    const std::array<std::array<float,4>,14> colors{{{1,0.86f,0.2f,1},{1,0.3f,0.35f,1},{0.35f,0.48f,1,1},{0.95f,0.25f,0.18f,1},{1.6f,0.36f,0.1f,1},{1.4f,0.16f,0.72f,1},{0.2f,1.45f,1.75f,1},{1.75f,0.25f,0.8f,1},{1.6f,0.95f,0.16f,1},{.18f,1.2f,1.6f,1},{1.6f,.38f,.12f,1},{.85f,.45f,1.7f,1},{.3f,.9f,1.7f,1},{1.5f,.25f,.9f,1}}};
    for(std::size_t i=0;i<kBehaviorIds.size();++i) {
        Enemy e;e.id=kBehaviorIds[i];e.name=kBehaviorNames[i];e.behavior=static_cast<EnemyBehavior>(i);
        e.hp=hp[i];e.creditDrop=drops[i];e.color=colors[i];
        if(e.behavior==EnemyBehavior::Flanker)e.bulletDamage=6;
        if(e.behavior==EnemyBehavior::Suppressor)e.bulletDamage=7;
        if(e.behavior==EnemyBehavior::ShieldGuard)e.bulletDamage=8;
        if(e.behavior==EnemyBehavior::BladeGuard)e.contactDamage=18;
        if(e.behavior==EnemyBehavior::SummonerCommander)e.bulletDamage=5;
        if(e.behavior==EnemyBehavior::ReflectArmor){e.contactDamage=10;e.bulletDamage=8;}
        c.enemies.push_back(e);
    }
    auto armored=c.enemies[4];armored.id="ArmoredCharger";armored.name="重装突進兵";armored.hp=65;armored.moveSpeedScale=0.75f;armored.creditDrop=12;armored.color={1.4f,0.8f,0.1f,1};c.enemies.push_back(armored);
    auto rapid=c.enemies[5];rapid.id="RapidSniper";rapid.name="高速狙撃兵";rapid.hp=32;rapid.fireIntervalScale=0.72f;rapid.creditDrop=10;rapid.color={0.5f,0.3f,1.7f,1};c.enemies.push_back(rapid);
    PlayerVariant p;p.id="TwinBattery";p.name="ツイン砲台";p.description="2門を同時発射。直線火力を伸ばす。";c.players.push_back(p);
    p.id="FanBattery";p.name="扇形砲台";p.description="3門を扇状に発射。広い範囲を制圧。";p.price=48;p.barrels=3;p.fanAngle=14;p.damageScale=0.78f;c.players.push_back(p);
    p={};p.id="RapidBattery";p.name="速射機体";p.description="交互射撃で切れ目なく攻撃。";p.baseClass="MachineGun";p.barrels=2;p.alternate=true;p.reloadScale=0.65f;p.damageScale=0.8f;p.color={0.3f,1.1f,1.8f,1};c.players.push_back(p);
    p={};p.id="BankshotBattery";p.name="反射機体";p.description="3門から反射弾を発射。";p.barrels=3;p.reflect=true;p.damageScale=0.85f;p.price=48;p.bodyShape=1;p.color={1.6f,0.4f,1.2f,1};c.players.push_back(p);
    for(auto& variant:c.players)variant.rarity=variant.id=="BankshotBattery"?2:1;
    p={};p.id="DroneCarrier";p.name="ドローン母艦";p.description="3機のドローンで継続攻撃。各機の発射間隔15%短縮。";p.baseClass="Overseer";p.barrels=1;p.drones=3;p.reloadScale=0.85f;p.bodyShape=3;p.color={0.5f,1.6f,0.7f,1};p.style=tankbuild::Style::Drone;p.rarity=1;c.players.push_back(p);
    p.id="DroneArtillery";p.name="重射撃編隊";p.description="2機の重ドローン。威力45%増、発射間隔20%増。";p.price=56;p.drones=2;p.damageScale=1.45f;p.reloadScale=1.2f;p.rarity=2;c.players.push_back(p);
    p={};p.id="MeleeSweeper";p.name="旋回ブレード";p.description="近接連撃の動作を15%短縮。威力5%減。";p.baseClass="Basic";p.barrels=1;p.damageScale=0.95f;p.reloadScale=0.85f;p.style=tankbuild::Style::Melee;p.rarity=1;p.color={0.3f,1.4f,1.7f,1};c.players.push_back(p);
    p.id="MeleeBreaker";p.name="重撃ブレード";p.description="近接威力45%増、各動作18%増。重い一撃を狙う。";p.price=56;p.damageScale=1.45f;p.reloadScale=1.18f;p.rarity=2;p.color={1.65f,0.55f,0.15f,1};c.players.push_back(p);
    return c;
}
inline bool ValidId(const std::string& id) {
    return !id.empty()&&id.size()<=64&&std::all_of(id.begin(),id.end(),[](unsigned char ch){return (ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='_'||ch=='-';});
}
inline bool ValidateCatalog(const Catalog& c,std::string& error) {
    auto fail=[&](const std::string& s){error=s;return false;};
    auto common=[&](const auto& items,const char* group) {
        if(items.empty()||items.size()>kMaxDefinitions)return fail(std::string(group)+": 件数は1〜128です。");
        std::set<std::string> ids;
        for(const auto& item:items) if(!ValidId(item.id)||!ids.insert(item.id).second||item.name.empty()||item.name.size()>192)
            return fail(std::string(group)+": IDの重複・不正文字、または空の名前があります。");
        return true;
    };
    if(!common(c.upgrades,"強化")||!common(c.enemies,"敵")||!common(c.players,"機体"))return false;
    auto bounded=[](float v,float lo,float hi){return std::isfinite(v)&&v>=lo&&v<=hi;};
    auto colorValid=[&](const auto& color){return bounded(color[0],0,4)&&bounded(color[1],0,4)&&bounded(color[2],0,4)&&bounded(color[3],0.05f,1);};
    for(const auto& u:c.upgrades) {
        if(IsRetiredStandardUpgrade(u.id)||!u.refitPlayer.empty())return fail("強化: 換装・廃止済み標準カードは遠征では使用できません。");
        if(u.description.size()>768||u.price<0||u.price>9999||u.maxPurchases!=1||!tankbuild::ValidRarity(u.rarity)||u.compatibleStyles==0||(u.compatibleStyles&~tankbuild::AllStyles)||(u.effects.empty()&&u.refitPlayer.empty())||u.effects.size()>4)return fail("強化: 価格・効果数・系統・レア度が範囲外です。購入上限は1です。");
        std::set<int> effects;for(const auto effect:u.effects)if(!tankrun::IsAvailableCard(effect)||!effects.insert(static_cast<int>(effect)).second)return fail("強化: 効果が不正・廃止済み、または重複しています。");
        for(float power:u.effectPower)if(!bounded(power,0.1f,5))return fail("強化: 効果倍率は0.1〜5.0の有限値です。");
    }
    for(const auto& e:c.enemies) if(e.behavior<EnemyBehavior::Square||e.behavior>EnemyBehavior::ReflectArmor||e.hp<1||e.hp>9999||e.contactDamage<0||e.contactDamage>999||e.bulletDamage<1||e.bulletDamage>999||e.creditDrop<0||e.creditDrop>999||!bounded(e.moveSpeedScale,0.1f,3)||!bounded(e.fireIntervalScale,0.3f,4)||e.magazineSize<0||e.magazineSize>8||!bounded(e.reloadSeconds,0,8)||(e.reloadSeconds>0&&e.reloadSeconds<0.8f)||!colorValid(e.color))return fail("敵: HP・速度・ダメージ・色が範囲外です。");
    for(const auto& p:c.players) {
        if(p.id.rfind("exp_",0)==0)return fail("機体: exp_ で始まるIDは標準進化用に予約されています。");
        if(!tankbuild::Valid(p.style)||!tankbuild::ValidRarity(p.rarity)||(p.style==tankbuild::Style::Drone&&p.drones<1)||
           (p.style==tankbuild::Style::Melee&&(p.drones!=0||p.reflect||p.penetrate)))return fail("機体: 系統・レア度が不正です。ドローン型は1機以上、近接型は投射物専用効果を持てません。");
        if((p.baseClass!="Basic"&&p.baseClass!="Twin"&&p.baseClass!="MachineGun"&&p.baseClass!="Overseer")||p.description.size()>768||p.price<0||p.price>9999||p.barrels<1||p.barrels>6||p.bulletCount!=1||p.bodyShape<0||p.bodyShape>3||p.drones<0||p.drones>12||!bounded(p.damageScale,0.2f,4)||!bounded(p.reloadScale,0.25f,4)||!bounded(p.bulletSpeedScale,0.3f,3)||!bounded(p.fanAngle,0,45)||!colorValid(p.color))return fail("機体: 基本型・砲数・性能・色が範囲外です。1砲門につき弾数は1です。");
    }
    error.clear();return true;
}
inline nlohmann::json CatalogToJson(const Catalog& c) {
    nlohmann::json j={{"schemaVersion",5},{"upgrades",nlohmann::json::array()},{"enemies",nlohmann::json::array()},{"players",nlohmann::json::array()}};
    for(const auto& u:c.upgrades){auto powers=nlohmann::json::object();for(std::size_t i=0;i<kEffectIds.size();++i)powers[kEffectIds[i]]=u.effectPower[i];auto effects=nlohmann::json::array(),styles=nlohmann::json::array();for(auto e:u.effects)effects.push_back(kEffectIds[static_cast<std::size_t>(e)]);for(const auto style:{tankbuild::Style::Shooter,tankbuild::Style::Drone,tankbuild::Style::Melee})if(u.compatibleStyles&tankbuild::Mask(style))styles.push_back(tankbuild::Id(style));j["upgrades"].push_back({{"id",u.id},{"name",u.name},{"description",u.description},{"price",u.price},{"maxPurchases",u.maxPurchases},{"rarity",u.rarity},{"effects",effects},{"compatibleStyles",styles},{"effectPower",powers}});}
    for(const auto& e:c.enemies)j["enemies"].push_back({{"id",e.id},{"name",e.name},{"behavior",kBehaviorIds[static_cast<std::size_t>(e.behavior)]},{"hp",e.hp},{"contactDamage",e.contactDamage},{"bulletDamage",e.bulletDamage},{"creditDrop",e.creditDrop},{"moveSpeedScale",e.moveSpeedScale},{"fireIntervalScale",e.fireIntervalScale},{"magazineSize",e.magazineSize},{"reloadSeconds",e.reloadSeconds},{"color",e.color}});
    for(const auto& p:c.players)j["players"].push_back({{"id",p.id},{"name",p.name},{"description",p.description},{"baseClass",p.baseClass},{"price",p.price},{"barrels",p.barrels},{"bulletCount",p.bulletCount},{"bodyShape",p.bodyShape},{"drones",p.drones},{"damageScale",p.damageScale},{"reloadScale",p.reloadScale},{"bulletSpeedScale",p.bulletSpeedScale},{"fanAngle",p.fanAngle},{"alternate",p.alternate},{"reflect",p.reflect},{"penetrate",p.penetrate},{"color",p.color},{"style",tankbuild::Id(p.style)},{"rarity",p.rarity}});
    return j;
}
inline bool CatalogFromJson(const nlohmann::json& sourceJson,Catalog& output,std::string& error) {
    try {
        auto j=sourceJson; // Normalize retired growth records before validating live entries.
        auto integers=[](const nlohmann::json& object,std::initializer_list<const char*> keys){for(const auto* key:keys){const auto& value=object.at(key);if(!value.is_number_integer()||value.get<double>()<static_cast<double>((std::numeric_limits<int>::min)())||value.get<double>()>static_cast<double>((std::numeric_limits<int>::max)()))throw std::runtime_error(std::string("Integer field required: ")+key);}};
        if(!j.is_object())throw std::runtime_error("Catalog object required");
        integers(j,{"schemaVersion"});
        const int version=j.at("schemaVersion").get<int>();
        if(version!=1&&version!=2&&version!=3&&version!=4&&version!=5)throw std::runtime_error("Unsupported catalog version");
        for(const char* key:{"upgrades","enemies","players"})if(!j.at(key).is_array()||j.at(key).size()>kMaxDefinitions)throw std::runtime_error("Invalid catalog collection");
        auto& legacyUpgrades=j.at("upgrades");
        std::erase_if(legacyUpgrades.get_ref<nlohmann::json::array_t&>(),[](const nlohmann::json& u){
            const auto id=u.at("id").get<std::string>();
            if(IsRetiredStandardUpgrade(id))return true;
            const auto refit=u.value("refitPlayer",std::string{});
            return !refit.empty()&&u.at("effects").is_array()&&u.at("effects").empty();
        });
        for(auto& u:legacyUpgrades)u.erase("refitPlayer");
        for(const auto& value:legacyUpgrades)integers(value,{"price","maxPurchases","rarity"});
        for(const auto& value:j.at("enemies")){integers(value,{"hp","contactDamage","bulletDamage","creditDrop"});if(value.contains("magazineSize"))integers(value,{"magazineSize"});}
        for(const auto& value:j.at("players")){integers(value,{"price","barrels","bulletCount","bodyShape","drones"});if(version>=2)integers(value,{"rarity"});}
        Catalog c;
        for(const auto& v:j.at("upgrades")){Upgrade u;u.id=v.at("id").get<std::string>();u.name=v.at("name").get<std::string>();u.description=v.at("description").get<std::string>();u.price=v.at("price").get<int>();u.maxPurchases=v.at("maxPurchases").get<int>();u.rarity=v.at("rarity").get<int>();u.refitPlayer=v.value("refitPlayer",std::string{});u.effects.clear();if(!v.at("effects").is_array())throw std::runtime_error("Invalid effects");for(const auto& effect:v.at("effects")){const auto id=effect.get<std::string>();const auto it=std::find(kEffectIds.begin(),kEffectIds.end(),id);if(it==kEffectIds.end())throw std::runtime_error("Unknown effect: "+id);u.effects.push_back(static_cast<tankrun::CardId>(it-kEffectIds.begin()));}c.upgrades.push_back(std::move(u));}
        for(const auto& v:j.at("enemies")){Enemy e;e.id=v.at("id").get<std::string>();e.name=v.at("name").get<std::string>();const auto behavior=v.at("behavior").get<std::string>();const auto it=std::find(kBehaviorIds.begin(),kBehaviorIds.end(),behavior);if(it==kBehaviorIds.end())throw std::runtime_error("Unknown behavior: "+behavior);e.behavior=static_cast<EnemyBehavior>(it-kBehaviorIds.begin());e.hp=v.at("hp").get<int>();e.contactDamage=v.at("contactDamage").get<int>();e.bulletDamage=v.at("bulletDamage").get<int>();e.creditDrop=v.at("creditDrop").get<int>();e.moveSpeedScale=v.at("moveSpeedScale").get<float>();e.fireIntervalScale=v.at("fireIntervalScale").get<float>();e.magazineSize=v.value("magazineSize",0);e.reloadSeconds=v.value("reloadSeconds",0.0f);e.color=v.at("color").get<std::array<float,4>>();c.enemies.push_back(std::move(e));}
        for(const auto& v:j.at("players")){PlayerVariant p;p.id=v.at("id").get<std::string>();p.name=v.at("name").get<std::string>();p.description=v.at("description").get<std::string>();p.baseClass=v.at("baseClass").get<std::string>();p.price=v.at("price").get<int>();p.barrels=v.at("barrels").get<int>();p.bulletCount=v.at("bulletCount").get<int>();p.bodyShape=v.at("bodyShape").get<int>();p.drones=v.at("drones").get<int>();p.damageScale=v.at("damageScale").get<float>();p.reloadScale=v.at("reloadScale").get<float>();p.bulletSpeedScale=v.at("bulletSpeedScale").get<float>();p.fanAngle=v.at("fanAngle").get<float>();p.alternate=v.at("alternate").get<bool>();p.reflect=v.at("reflect").get<bool>();p.penetrate=v.at("penetrate").get<bool>();p.color=v.at("color").get<std::array<float,4>>();c.players.push_back(std::move(p));}
        for(std::size_t i=0;i<c.upgrades.size();++i) {
            auto& upgrade=c.upgrades[i];const auto& source=j.at("upgrades")[i];
            if(source.contains("effectPower")) {
                const auto& powers=source.at("effectPower");
                if(!powers.is_object())throw std::runtime_error("effectPower must be an object");
                for(auto it=powers.begin();it!=powers.end();++it) {
                    const auto effect=std::find(kEffectIds.begin(),kEffectIds.end(),it.key());
                    if(effect==kEffectIds.end()||!it.value().is_number())throw std::runtime_error("Invalid effect power: "+it.key());
                    upgrade.effectPower[static_cast<std::size_t>(effect-kEffectIds.begin())]=it.value().get<float>();
                }
            }
            if(version==1) {
                if(upgrade.rarity<0||upgrade.rarity>2)throw std::runtime_error("Invalid legacy rarity");
                upgrade.rarity*=2;
            } else {
                const auto& styles=source.at("compatibleStyles");
                if(!styles.is_array()||styles.empty()||styles.size()>3)throw std::runtime_error("Invalid compatible styles");
                upgrade.compatibleStyles=0;
                for(const auto& value:styles) {tankbuild::Style style{};if(!value.is_string()||!tankbuild::ParseStyle(value.get<std::string>(),style)||(upgrade.compatibleStyles&tankbuild::Mask(style)))throw std::runtime_error("Unknown or duplicate style");upgrade.compatibleStyles|=tankbuild::Mask(style);}
            }
        }
        for(std::size_t i=0;i<c.players.size();++i) {
            auto& player=c.players[i];const auto& source=j.at("players")[i];
            if(version==1) {player.style=player.drones>0?tankbuild::Style::Drone:tankbuild::Style::Shooter;player.rarity=0;}
            else {if(!tankbuild::ParseStyle(source.at("style").get<std::string>(),player.style))throw std::runtime_error("Unknown player style");player.rarity=source.at("rarity").get<int>();}
        }
        // Schema 1 remains readable: retire legacy spread effects without
        // silently discarding the user's unrelated enemies or authored tanks.
        for(auto& upgrade:c.upgrades) {
            const auto oldSize=upgrade.effects.size();
            const std::set<tankrun::CardId> uniqueEffects(upgrade.effects.begin(),upgrade.effects.end());
            if((oldSize==0&&upgrade.refitPlayer.empty())||oldSize>4||uniqueEffects.size()!=oldSize) {error="強化: 効果数が不正、または重複しています。";return false;}
            if(version==1)std::erase(upgrade.effects,tankrun::CardId::ScatterShot);
            if(oldSize==upgrade.effects.size()||upgrade.effects.empty())continue;
            if(upgrade.id=="BankshotKit") {
                upgrade.effects={tankrun::CardId::Ricochet,tankrun::CardId::Pierce};
                upgrade.name="反射貫通キット";upgrade.description="弾数を増やさず反射と貫通を組み合わせる。";
            } else {
                upgrade.description="装備効果: ";
                for(std::size_t i=0;i<upgrade.effects.size();++i) {
                    if(i)upgrade.description+=" / ";
                    upgrade.description+=kEffectNames[static_cast<std::size_t>(upgrade.effects[i])];
                }
            }
        }
        std::erase_if(c.upgrades,[](const Upgrade& upgrade){return upgrade.effects.empty()&&upgrade.refitPlayer.empty();});
        if(version==1) {
            for(auto& upgrade:c.upgrades) {const unsigned mask=CompatibleEffectStyles(upgrade);upgrade.compatibleStyles=mask?mask:tankbuild::AllStyles;}
            for(auto& player:c.players)if(player.bulletCount>=2&&player.bulletCount<=5)player.bulletCount=1;
        }
        if(version<5) {
            const auto defaults=DefaultCatalog();
            for(const auto& added:defaults.upgrades)if(added.effects.front()>=tankrun::CardId::ExtraBarrel1&&!FindUpgrade(c,added.id)) {
                if(c.upgrades.size()>=kMaxDefinitions)break;
                c.upgrades.push_back(added);
            }
        }
        if(!ValidateCatalog(c,error))return false;output=std::move(c);error.clear();return true;
    }catch(const std::exception& ex){error=ex.what();return false;}
}
inline bool LoadCatalog(const std::string& path,Catalog& output,std::string& error) {
    try {std::ifstream input(Utf8Path(path));if(!input){error="読込できません: "+path;return false;}nlohmann::json j;input>>j;return CatalogFromJson(j,output,error);}catch(const std::exception& ex){error=ex.what();return false;}
}
inline bool SaveCatalog(const std::string& path,const Catalog& c,std::string& error) {
    if(!ValidateCatalog(c,error))return false;
    try {
        const auto target=Utf8Path(path),temp=Utf8Path(path+".tmp");
        {std::ofstream out(temp,std::ios::trunc|std::ios::binary);if(!out){error="保存できません: "+path;return false;}out<<CatalogToJson(c).dump(2)<<'\n';out.close();if(!out){error="保存中の書き込みに失敗しました。";return false;}}
#ifdef _WIN32
        if(!MoveFileExW(temp.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){error="保存先の置き換えに失敗しました。元のファイルは維持されています。";return false;}
#else
        std::filesystem::rename(temp,target);
#endif
        error.clear();return true;
    }catch(const std::exception& ex){error=ex.what();return false;}
}
}
