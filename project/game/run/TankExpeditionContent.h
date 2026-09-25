#pragma once
#include "TankRunDirector.h"
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
enum class EnemyBehavior { Square,Triangle,Pentagon,Shooter,Charger,Sniper };
struct Upgrade {
    std::string id,name,description;
    int price=25,maxPurchases=1,rarity=0;
    std::vector<tankrun::CardId> effects{tankrun::CardId::Rapid};
};
struct Enemy {
    std::string id,name;
    EnemyBehavior behavior=EnemyBehavior::Charger;
    int hp=24,contactDamage=8,bulletDamage=10,creditDrop=5;
    float moveSpeedScale=1,fireIntervalScale=1;
    std::array<float,4> color{1.6f,0.36f,0.1f,1};
};
struct PlayerVariant {
    std::string id,name,description,baseClass="Twin";
    int price=40,barrels=2,bulletCount=1,bodyShape=0,drones=0;
    float damageScale=1,reloadScale=1,bulletSpeedScale=1,fanAngle=0;
    bool alternate=false,reflect=false,penetrate=false;
    std::array<float,4> color{0.3f,1.5f,1.3f,1};
};
struct Catalog { std::vector<Upgrade> upgrades;std::vector<Enemy> enemies;std::vector<PlayerVariant> players; };
inline constexpr std::array<const char*,tankrun::CardCount> kEffectIds{
    "Ricochet","Heavy","Rapid","Thrusters","Capacitor","Repair","Drones","Pierce",
    "ScatterShot","Homing","DashBurst","Overdrive"};
inline constexpr std::array<const char*,tankrun::CardCount> kEffectNames{
    "壁反射","重弾","速射","推進器","蓄電池","装甲修復","支援ドローン","貫通",
    "分裂弾","追尾","ダッシュ攻撃","過負荷"};
inline constexpr std::array<const char*,6> kBehaviorIds{"Square","Triangle","Pentagon","Shooter","Charger","Sniper"};
inline constexpr std::array<const char*,6> kBehaviorNames{"四角資源","三角資源","五角資源","射撃砲台","突進兵","狙撃兵"};
inline int CreditsFromExperience(int amount) { return amount<=0?0:(std::clamp)(amount/5,1,100000); }
template<class T> inline const T* FindDefinition(const std::vector<T>& items,const std::string& id) {
    const auto it=std::find_if(items.begin(),items.end(),[&](const T& item){return item.id==id;});
    return it==items.end()?nullptr:&*it;
}
inline const Upgrade* FindUpgrade(const Catalog& c,const std::string& id){return FindDefinition(c.upgrades,id);}
inline const Enemy* FindEnemy(const Catalog& c,const std::string& id){return FindDefinition(c.enemies,id);}
inline const PlayerVariant* FindPlayer(const Catalog& c,const std::string& id){return FindDefinition(c.players,id);}
inline Catalog DefaultCatalog() {
    Catalog c;
    const std::array<const char*,tankrun::CardCount> descriptions{
        "壁で弾が反射する。角度を使って敵を狙う。","威力を増やす。速射や貫通との組み合わせに。",
        "発射間隔を短縮する。","移動とダッシュを強化する。","スタミナの余裕を増やす。",
        "最大HPを増やし、その増加分を修復する。","支援ドローンを追加する。","弾が敵を貫く。",
        "弾数が増え、最初の命中で小弾に分裂。","弾が近くの敵を追う。","ダッシュ中に攻撃。重弾と組み合わせると爆発。",
        "連続攻撃を強化。追尾との組み合わせで旋回も強化。"};
    for(std::size_t i=0;i<kEffectIds.size();++i) {
        Upgrade u;u.id=kEffectIds[i];u.name=kEffectNames[i];u.description=descriptions[i];
        u.price=i>=8?42:24;u.rarity=i>=8?1:0;u.effects={static_cast<tankrun::CardId>(i)};c.upgrades.push_back(u);
    }
    c.upgrades.push_back({"BankshotKit","反射散弾キット","反射と分裂をまとめた複合改造。",58,1,1,{tankrun::CardId::Ricochet,tankrun::CardId::ScatterShot}});
    c.upgrades.push_back({"DashBomberKit","突撃爆破キット","重弾とダッシュ攻撃を組み合わせ、突進先で爆発。",58,1,1,{tankrun::CardId::Heavy,tankrun::CardId::DashBurst}});
    const int hp[]={6,10,24,14,30,24};const int drops[]={2,3,7,5,6,6};
    const std::array<std::array<float,4>,6> colors{{{1,0.86f,0.2f,1},{1,0.3f,0.35f,1},{0.35f,0.48f,1,1},{0.95f,0.25f,0.18f,1},{1.6f,0.36f,0.1f,1},{1.4f,0.16f,0.72f,1}}};
    for(std::size_t i=0;i<kBehaviorIds.size();++i) {
        Enemy e;e.id=kBehaviorIds[i];e.name=kBehaviorNames[i];e.behavior=static_cast<EnemyBehavior>(i);
        e.hp=hp[i];e.creditDrop=drops[i];e.color=colors[i];c.enemies.push_back(e);
    }
    auto armored=c.enemies[4];armored.id="ArmoredCharger";armored.name="重装突進兵";armored.hp=65;armored.moveSpeedScale=0.75f;armored.creditDrop=12;armored.color={1.4f,0.8f,0.1f,1};c.enemies.push_back(armored);
    auto rapid=c.enemies[5];rapid.id="RapidSniper";rapid.name="高速狙撃兵";rapid.hp=32;rapid.fireIntervalScale=0.72f;rapid.creditDrop=10;rapid.color={0.5f,0.3f,1.7f,1};c.enemies.push_back(rapid);
    PlayerVariant p;p.id="TwinBattery";p.name="ツイン砲台";p.description="2門を同時発射。直線火力を伸ばす。";c.players.push_back(p);
    p.id="FanBattery";p.name="扇形砲台";p.description="3門を扇状に発射。広い範囲を制圧。";p.price=48;p.barrels=3;p.fanAngle=14;p.damageScale=0.78f;c.players.push_back(p);
    p={};p.id="RapidBattery";p.name="速射機体";p.description="交互射撃で切れ目なく攻撃。";p.baseClass="MachineGun";p.barrels=2;p.alternate=true;p.reloadScale=0.65f;p.damageScale=0.8f;p.color={0.3f,1.1f,1.8f,1};c.players.push_back(p);
    p={};p.id="BankshotBattery";p.name="反射機体";p.description="3門から反射弾を発射。";p.barrels=3;p.reflect=true;p.damageScale=0.85f;p.price=48;p.bodyShape=1;p.color={1.6f,0.4f,1.2f,1};c.players.push_back(p);
    p={};p.id="DroneCarrier";p.name="ドローン母艦";p.description="砲撃に支援ドローンを組み合わせる。";p.baseClass="Overseer";p.barrels=1;p.drones=3;p.bodyShape=3;p.color={0.5f,1.6f,0.7f,1};c.players.push_back(p);
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
        if(u.description.size()>768||u.price<0||u.price>9999||u.maxPurchases!=1||u.rarity<0||u.rarity>2||u.effects.empty()||u.effects.size()>4)return fail("強化: 価格・効果数・レア度が範囲外です。購入上限は1です。");
        std::set<int> effects;for(const auto effect:u.effects)if(effect< tankrun::CardId::Ricochet||effect>=tankrun::CardId::Count||!effects.insert(static_cast<int>(effect)).second)return fail("強化: 効果が不正または重複しています。");
    }
    for(const auto& e:c.enemies) if(e.behavior<EnemyBehavior::Square||e.behavior>EnemyBehavior::Sniper||e.hp<1||e.hp>9999||e.contactDamage<0||e.contactDamage>999||e.bulletDamage<1||e.bulletDamage>999||e.creditDrop<0||e.creditDrop>999||!bounded(e.moveSpeedScale,0.1f,3)||!bounded(e.fireIntervalScale,0.3f,4)||!colorValid(e.color))return fail("敵: HP・速度・ダメージ・色が範囲外です。");
    for(const auto& p:c.players) {
        if(p.id.rfind("exp_",0)==0)return fail("機体: exp_ で始まるIDは標準進化用に予約されています。");
        if((p.baseClass!="Basic"&&p.baseClass!="Twin"&&p.baseClass!="MachineGun"&&p.baseClass!="Overseer")||p.description.size()>768||p.price<0||p.price>9999||p.barrels<1||p.barrels>6||p.bulletCount<1||p.bulletCount>5||p.bodyShape<0||p.bodyShape>3||p.drones<0||p.drones>7||!bounded(p.damageScale,0.2f,4)||!bounded(p.reloadScale,0.25f,4)||!bounded(p.bulletSpeedScale,0.3f,3)||!bounded(p.fanAngle,0,45)||!colorValid(p.color))return fail("機体: 基本型・砲数・性能・色が範囲外です。");
    }
    error.clear();return true;
}
inline nlohmann::json CatalogToJson(const Catalog& c) {
    nlohmann::json j={{"schemaVersion",1},{"upgrades",nlohmann::json::array()},{"enemies",nlohmann::json::array()},{"players",nlohmann::json::array()}};
    for(const auto& u:c.upgrades){auto effects=nlohmann::json::array();for(auto e:u.effects)effects.push_back(kEffectIds[static_cast<std::size_t>(e)]);j["upgrades"].push_back({{"id",u.id},{"name",u.name},{"description",u.description},{"price",u.price},{"maxPurchases",u.maxPurchases},{"rarity",u.rarity},{"effects",effects}});}
    for(const auto& e:c.enemies)j["enemies"].push_back({{"id",e.id},{"name",e.name},{"behavior",kBehaviorIds[static_cast<std::size_t>(e.behavior)]},{"hp",e.hp},{"contactDamage",e.contactDamage},{"bulletDamage",e.bulletDamage},{"creditDrop",e.creditDrop},{"moveSpeedScale",e.moveSpeedScale},{"fireIntervalScale",e.fireIntervalScale},{"color",e.color}});
    for(const auto& p:c.players)j["players"].push_back({{"id",p.id},{"name",p.name},{"description",p.description},{"baseClass",p.baseClass},{"price",p.price},{"barrels",p.barrels},{"bulletCount",p.bulletCount},{"bodyShape",p.bodyShape},{"drones",p.drones},{"damageScale",p.damageScale},{"reloadScale",p.reloadScale},{"bulletSpeedScale",p.bulletSpeedScale},{"fanAngle",p.fanAngle},{"alternate",p.alternate},{"reflect",p.reflect},{"penetrate",p.penetrate},{"color",p.color}});
    return j;
}
inline bool CatalogFromJson(const nlohmann::json& j,Catalog& output,std::string& error) {
    try {
        auto integers=[](const nlohmann::json& object,std::initializer_list<const char*> keys){for(const auto* key:keys){const auto& value=object.at(key);if(!value.is_number_integer()||value.get<double>()<static_cast<double>((std::numeric_limits<int>::min)())||value.get<double>()>static_cast<double>((std::numeric_limits<int>::max)()))throw std::runtime_error(std::string("Integer field required: ")+key);}};
        if(!j.is_object())throw std::runtime_error("Catalog object required");
        integers(j,{"schemaVersion"});
        if(j.at("schemaVersion").get<int>()!=1)throw std::runtime_error("Unsupported catalog version");
        for(const char* key:{"upgrades","enemies","players"})if(!j.at(key).is_array()||j.at(key).size()>kMaxDefinitions)throw std::runtime_error("Invalid catalog collection");
        for(const auto& value:j.at("upgrades"))integers(value,{"price","maxPurchases","rarity"});
        for(const auto& value:j.at("enemies"))integers(value,{"hp","contactDamage","bulletDamage","creditDrop"});
        for(const auto& value:j.at("players"))integers(value,{"price","barrels","bulletCount","bodyShape","drones"});
        Catalog c;
        for(const auto& v:j.at("upgrades")){Upgrade u;u.id=v.at("id").get<std::string>();u.name=v.at("name").get<std::string>();u.description=v.at("description").get<std::string>();u.price=v.at("price").get<int>();u.maxPurchases=v.at("maxPurchases").get<int>();u.rarity=v.at("rarity").get<int>();u.effects.clear();if(!v.at("effects").is_array())throw std::runtime_error("Invalid effects");for(const auto& effect:v.at("effects")){const auto id=effect.get<std::string>();const auto it=std::find(kEffectIds.begin(),kEffectIds.end(),id);if(it==kEffectIds.end())throw std::runtime_error("Unknown effect: "+id);u.effects.push_back(static_cast<tankrun::CardId>(it-kEffectIds.begin()));}c.upgrades.push_back(std::move(u));}
        for(const auto& v:j.at("enemies")){Enemy e;e.id=v.at("id").get<std::string>();e.name=v.at("name").get<std::string>();const auto behavior=v.at("behavior").get<std::string>();const auto it=std::find(kBehaviorIds.begin(),kBehaviorIds.end(),behavior);if(it==kBehaviorIds.end())throw std::runtime_error("Unknown behavior: "+behavior);e.behavior=static_cast<EnemyBehavior>(it-kBehaviorIds.begin());e.hp=v.at("hp").get<int>();e.contactDamage=v.at("contactDamage").get<int>();e.bulletDamage=v.at("bulletDamage").get<int>();e.creditDrop=v.at("creditDrop").get<int>();e.moveSpeedScale=v.at("moveSpeedScale").get<float>();e.fireIntervalScale=v.at("fireIntervalScale").get<float>();e.color=v.at("color").get<std::array<float,4>>();c.enemies.push_back(std::move(e));}
        for(const auto& v:j.at("players")){PlayerVariant p;p.id=v.at("id").get<std::string>();p.name=v.at("name").get<std::string>();p.description=v.at("description").get<std::string>();p.baseClass=v.at("baseClass").get<std::string>();p.price=v.at("price").get<int>();p.barrels=v.at("barrels").get<int>();p.bulletCount=v.at("bulletCount").get<int>();p.bodyShape=v.at("bodyShape").get<int>();p.drones=v.at("drones").get<int>();p.damageScale=v.at("damageScale").get<float>();p.reloadScale=v.at("reloadScale").get<float>();p.bulletSpeedScale=v.at("bulletSpeedScale").get<float>();p.fanAngle=v.at("fanAngle").get<float>();p.alternate=v.at("alternate").get<bool>();p.reflect=v.at("reflect").get<bool>();p.penetrate=v.at("penetrate").get<bool>();p.color=v.at("color").get<std::array<float,4>>();c.players.push_back(std::move(p));}
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
