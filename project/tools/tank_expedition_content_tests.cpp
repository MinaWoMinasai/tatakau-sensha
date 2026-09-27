#include "../game/run/TankExpeditionContent.h"
#include "../game/run/TankRunCopy.h"
#include <cassert>
#include <iostream>
#include <limits>

void ShopContracts() {
    using namespace tankcontent;using S=tankbuild::Style;using C=tankrun::CardId;
    auto catalog=DefaultCatalog();tankrun::CardCounts owned{};
    assert(!FindUpgrade(catalog,"MeleeBlade"));
    for(auto style:{S::Shooter,S::Drone,S::Melee})for(int rarity=3;rarity<=3;++rarity) {
        assert(std::any_of(catalog.upgrades.begin(),catalog.upgrades.end(),[&](const Upgrade& item){return item.rarity==rarity&&EligibleUpgrade(item,style,{});}));
    }
    assert(EligibleUpgrade(*FindUpgrade(catalog,"BladeReach"),S::Melee,owned));
    for(auto style:{S::Shooter,S::Drone}) {
        assert(EligibleUpgrade(*FindUpgrade(catalog,"Homing"),style,owned));
        assert(!EligibleUpgrade(*FindUpgrade(catalog,"BladeReach"),style,owned));
    }
    assert(!EligibleUpgrade(*FindUpgrade(catalog,"Pierce"),S::Melee,owned));
    assert(!EligibleUpgrade(*FindUpgrade(catalog,"Drones"),S::Shooter,owned));
    assert(EligibleUpgrade(*FindUpgrade(catalog,"Drones"),S::Drone,owned));
    assert(!EligibleUpgrade(*FindUpgrade(catalog,"Capacitor"),S::Melee,owned));
    owned[static_cast<std::size_t>(C::PerfectDodge)]=1;
    assert(EligibleUpgrade(*FindUpgrade(catalog,"Capacitor"),S::Melee,owned));
    auto composite=*FindUpgrade(catalog,"Ricochet");composite.id="MyComposite";composite.effects={C::Ricochet,C::Pierce};composite.compatibleStyles=tankbuild::AllStyles;
    assert(!EligibleUpgrade(composite,S::Melee,{})); // Author tags cannot make projectile-only effects work on swords.
    owned[static_cast<std::size_t>(C::Ricochet)]=1;
    assert(!EligibleUpgrade(composite,S::Shooter,owned)); // No partly redundant full-price kit.
    composite.effects={C::Homing,C::BladeReach};
    for(auto style:{S::Shooter,S::Drone,S::Melee})assert(!EligibleUpgrade(composite,style,{}));
    std::unordered_map<std::string,int> purchased;
    for(unsigned seed=0;seed<512;++seed)for(auto style:{S::Shooter,S::Drone,S::Melee}) {
        const auto offers=BuildShopOffers(catalog,style,{},purchased,seed);
        assert(offers.size()==3&&offers==BuildShopOffers(catalog,style,{},purchased,seed));
        assert(std::set<std::string>(offers.begin(),offers.end()).size()==3);
        for(std::size_t i=0;i<offers.size();++i) {
            const auto* item=FindUpgrade(catalog,offers[i]);assert(item&&EligibleUpgrade(*item,style,{}));
            if(i==0)assert(IsBehaviorUpgrade(*item)); // One behavioral option is guaranteed; the rest retain normal weights.
        }
    }
    purchased["Heavy"]=1;
    for(unsigned seed=0;seed<32;++seed){const auto ids=BuildShopOffers(catalog,S::Shooter,{},purchased,seed);assert(std::find(ids.begin(),ids.end(),"Heavy")==ids.end());}
    for(const auto& item:catalog.upgrades)purchased[item.id]=1;
    assert(BuildShopOffers(catalog,S::Melee,{},purchased,2).empty());
    owned.fill(1);assert(BuildShopOffers(catalog,S::Drone,owned,{},2).empty());
    auto scarce=catalog;scarce.upgrades={*FindUpgrade(catalog,"Heavy")};
    assert(BuildShopOffers(scarce,S::Shooter,{}, {},5)==std::vector<std::string>{"Heavy"});
    auto weighted=catalog;weighted.upgrades.clear();
    for(int rarity=0;rarity<5;++rarity){auto u=*FindUpgrade(catalog,"Heavy");u.id="Weight"+std::to_string(rarity);u.rarity=rarity;weighted.upgrades.push_back(u);}
    std::array<int,5> selected{};
    for(unsigned seed=1;seed<=12000;++seed){const auto ids=BuildShopOffers(weighted,S::Shooter,{}, {},seed);assert(ids.size()==3);++selected[static_cast<std::size_t>(ids[0].back()-'0')];}
    for(std::size_t i=1;i<selected.size();++i)assert(selected[i-1]>selected[i]&&selected[i]>0);
}

void SpecialAndAdditiveContracts() {
    using namespace tankcontent;using S=tankbuild::Style;using C=tankrun::CardId;
    const auto catalog=DefaultCatalog();
    const std::array<const char*,8> retired{"BankshotKit","DashBomberKit","BreachBladeKit","ArcBladeKit","DroneBastionKit","SeekingWingKit","SeekingRicochetKit","PredatorCircuit"};
    for(const auto* id:retired)assert(!FindUpgrade(catalog,id));
    for(const auto& p:catalog.players)assert(!FindUpgrade(catalog,"Refit_"+p.id));
    for(const auto& u:catalog.upgrades)assert(u.effects.size()==1&&u.refitPlayer.empty());
    std::set<std::string> seen;
    for(const auto style:{S::Shooter,S::Drone,S::Melee}) {
        tankrun::CardCounts prerequisites{};
        if(style==S::Shooter)prerequisites[static_cast<std::size_t>(C::ExtraBarrel1)]=1;
        for(unsigned seed=0;seed<2048;++seed)for(const auto owned:{tankrun::CardCounts{},prerequisites}) {
            const auto offers=BuildShopOffers(catalog,style,owned,{},seed);
            assert(offers.size()==3&&IsBehaviorUpgrade(*FindUpgrade(catalog,offers[0])));
            for(const auto& id:offers) {
                seen.insert(id);const auto* u=FindUpgrade(catalog,id);
                assert(u&&u->refitPlayer.empty()&&!IsRetiredStandardUpgrade(id)&&EligibleUpgrade(*u,style,owned));
            }
        }
    }
    for(std::size_t i=static_cast<std::size_t>(C::RailCannon);i<tankrun::CardCount;++i) {
        const auto* u=FindUpgrade(catalog,kEffectIds[i]);assert(u&&seen.count(u->id));
        const auto effect=static_cast<C>(i);const unsigned style=EffectStyles(effect);
        for(const auto candidate:{S::Shooter,S::Drone,S::Melee})if(!(style&tankbuild::Mask(candidate)))assert(!EligibleUpgrade(*u,candidate,{}));
    }
    tankrun::RunDirector run;assert(run.ChooseLoadout(0)&&run.ChooseCore(0));
    auto check=[&](const char* id,bool eligible){assert(EligibleUpgrade(*FindUpgrade(catalog,id),S::Shooter,run.GetCardCounts())==eligible);};
    check("ExtraBarrel1",true);check("ExtraBarrel2",false);check("FanMount",false);check("AlternatingFire",false);
    assert(run.GrantExpeditionModules({C::ExtraBarrel1}));
    check("ExtraBarrel1",false);check("ExtraBarrel2",true);check("FanMount",true);check("AlternatingFire",true);
    assert(run.GrantExpeditionModules({C::ExtraBarrel2,C::FanMount,C::AlternatingFire}));
    for(auto effect:{C::ExtraBarrel1,C::ExtraBarrel2,C::FanMount,C::AlternatingFire})assert(run.GetCardCount(effect)==1);
    // Every supported legacy schema removes known retired cards before parsing
    // their old payload, preserving unrelated custom composites and designs.
    for(int version=1;version<=5;++version) {
        auto legacy=CatalogToJson(catalog);legacy["schemaVersion"]=version;
        if(version==1) {
            for(auto& u:legacy["upgrades"]){u["rarity"]=u["rarity"].get<int>()/2;u.erase("compatibleStyles");}
            for(auto& p:legacy["players"]){p.erase("style");p.erase("rarity");}
        }
        for(const auto* id:retired)legacy["upgrades"].push_back({{"id",id}});
        for(const auto& p:catalog.players)legacy["upgrades"].push_back({{"id","Refit_"+p.id},{"refitPlayer",p.id}});
        auto custom=legacy["upgrades"][0];custom["id"]="MyCombo";custom["effects"]={"Ricochet","Homing"};custom["refitPlayer"]="OldAuthoredTank";legacy["upgrades"].push_back(custom);
        auto oldRefit=custom;oldRefit["id"]="MyOldRefit";oldRefit["effects"]=nlohmann::json::array();legacy["upgrades"].push_back(oldRefit);
        Catalog migrated;std::string error;assert(CatalogFromJson(legacy,migrated,error));
        assert(migrated.players.size()==catalog.players.size());
        assert(FindUpgrade(migrated,"MyCombo")->effects.size()==2&&FindUpgrade(migrated,"MyCombo")->refitPlayer.empty());
        assert(!FindUpgrade(migrated,"MyOldRefit"));
        for(const auto& u:migrated.upgrades)assert(!IsRetiredStandardUpgrade(u.id)&&u.refitPlayer.empty());
        Catalog roundtrip;assert(CatalogFromJson(CatalogToJson(migrated),roundtrip,error));assert(CatalogToJson(migrated)==CatalogToJson(roundtrip));
    }
}

int main(int argc,char** argv){
    using namespace tankcontent;
    auto original=DefaultCatalog();std::string error;
    assert(ValidateCatalog(original,error));
    assert(original.upgrades.size()==40&&original.players.size()==8&&original.enemies.size()==16);
    for(const auto& upgrade:original.upgrades) {
        assert(upgrade.effects.size()==1);
        const auto copy=tankrun::copy::ExpeditionCardCopy(static_cast<int>(upgrade.effects.front()));
        assert(upgrade.description==copy.body&&upgrade.name==copy.title);
    }
    ShopContracts();
    SpecialAndAdditiveContracts();
    assert(!FindUpgrade(original,"ScatterShot"));
    for(const auto& upgrade:original.upgrades)for(const auto effect:upgrade.effects)assert(tankrun::IsAvailableCard(effect));
    for(const auto& player:original.players)assert(player.bulletCount==1);
    Catalog restored;assert(CatalogFromJson(CatalogToJson(original),restored,error));
    assert(CatalogToJson(original)==CatalogToJson(restored));
    auto powered=original;powered.upgrades[0].effectPower[0]=2.5f;
    assert(CatalogFromJson(CatalogToJson(powered),restored,error));assert(restored.upgrades[0].effectPower[0]==2.5f);
    assert(CatalogFromJson(CatalogToJson(original),restored,error));
    const auto before=CatalogToJson(restored);
    auto rejected=[&](nlohmann::json j){assert(!CatalogFromJson(j,restored,error));assert(CatalogToJson(restored)==before);assert(!error.empty());};
    auto old2=before;old2["schemaVersion"]=2;for(auto& u:old2["upgrades"])u.erase("effectPower");
    Catalog schema2;assert(CatalogFromJson(old2,schema2,error));for(const auto& u:schema2.upgrades)for(float power:u.effectPower)assert(power==1);
    auto badPower=before;badPower["upgrades"][0]["effectPower"]["Ricochet"]=0;rejected(badPower);
    badPower=before;badPower["upgrades"][0]["effectPower"]["Ricochet"]=6;rejected(badPower);
    badPower=before;badPower["upgrades"][0]["effectPower"]["Unknown"]=1;rejected(badPower);
    badPower=before;badPower["upgrades"][0]["effectPower"]["Ricochet"]="bad";rejected(badPower);
    // Older schema-1 catalog files omit the new optional magazine fields.
    auto legacy=before;legacy["schemaVersion"]=1;
    for(auto& e:legacy["enemies"]){e.erase("magazineSize");e.erase("reloadSeconds");}
    for(auto& u:legacy["upgrades"]){u["rarity"]=u["rarity"].get<int>()/2;u.erase("compatibleStyles");}
    for(auto& p:legacy["players"]){p.erase("style");p.erase("rarity");}
    Catalog oldFile;assert(CatalogFromJson(legacy,oldFile,error));
    auto preservedLegacy=legacy;
    preservedLegacy["upgrades"][0]["name"]="自作の反射";preservedLegacy["upgrades"][0]["price"]=73;preservedLegacy["upgrades"][0]["rarity"]=1;
    preservedLegacy["enemies"][0]["hp"]=123;preservedLegacy["players"][0]["damageScale"]=1.7f;
    assert(CatalogFromJson(preservedLegacy,oldFile,error));
    assert(oldFile.upgrades[0].name=="自作の反射"&&oldFile.upgrades[0].price==73&&oldFile.upgrades[0].rarity==2);
    assert(oldFile.upgrades[0].compatibleStyles==3&&oldFile.enemies[0].hp==123&&oldFile.players[0].damageScale==1.7f);
    assert(FindPlayer(oldFile,"DroneCarrier")->style==tankbuild::Style::Drone);
    // Old authored catalogs keep unrelated definitions but retire spread/split.
    auto spread=legacy;spread["upgrades"].push_back({{"id","LegacySpread"},{"name","旧分裂"},{"description","旧仕様"},{"price",42},{"maxPurchases",1},{"rarity",1},{"effects",{"ScatterShot"}}});
    spread["upgrades"][0]["effects"]={"Ricochet","ScatterShot"};spread["players"][0]["bulletCount"]=4;
    assert(CatalogFromJson(spread,oldFile,error));assert(!FindUpgrade(oldFile,"LegacySpread"));
    assert(oldFile.upgrades.front().effects==std::vector<tankrun::CardId>{tankrun::CardId::Ricochet});
    assert(oldFile.players.front().bulletCount==1&&oldFile.enemies.size()==original.enemies.size());
    auto invalidSpread=original;invalidSpread.upgrades[0].effects={tankrun::CardId::ScatterShot};assert(!ValidateCatalog(invalidSpread,error));
    invalidSpread=original;invalidSpread.players[0].bulletCount=2;assert(!ValidateCatalog(invalidSpread,error));
    tankrun::CardCounts owned{};
    assert(!MeetsUpgradeRequirements(*FindUpgrade(original,"BladeReach"),owned));
    assert(!MeetsUpgradeRequirements(*FindUpgrade(original,"Capacitor"),owned));
    assert(EligibleUpgrade(*FindUpgrade(original,"LightBladeActuator"),tankbuild::Style::Melee,owned));
    assert(MeetsUpgradeRequirements(*FindUpgrade(original,"ImpactDrive"),owned));
    owned[static_cast<std::size_t>(tankrun::CardId::MeleeBlade)]=1;
    owned[static_cast<std::size_t>(tankrun::CardId::PerfectDodge)]=1;
    assert(MeetsUpgradeRequirements(*FindUpgrade(original,"BladeReach"),owned));
    assert(MeetsUpgradeRequirements(*FindUpgrade(original,"Capacitor"),owned));
    bool varied=false;const auto first=IntroUpgradeIds(original,1);
    for(std::uint32_t seed=0;seed<64;++seed) {
        const auto upper=IntroUpgradeIds(original,seed),lower=IntroUpgradeIds(original,seed);
        assert(upper==lower&&upper.size()==3);varied|=upper!=first;
        const std::set<std::string> unique(upper.begin(),upper.end());assert(unique.size()==3);
        for(const auto& id:upper) {const auto* offer=FindUpgrade(original,id);assert(offer&&offer->rarity==0&&offer->effects.size()==1&&id!="Pierce");assert(MeetsUpgradeRequirements(*offer,{}));for(auto style:{tankbuild::Style::Shooter,tankbuild::Style::Drone,tankbuild::Style::Melee})assert(EligibleUpgrade(*offer,style,{}));}
    }
    assert(varied);
    auto editedIntro=original;
    for(auto& option:editedIntro.upgrades)if(option.id=="Heavy")option.effects={tankrun::CardId::BladeReach};
    for(std::uint32_t seed=0;seed<64;++seed) {
        const auto ids=IntroUpgradeIds(editedIntro,seed);
        assert(ids.size()==3&&std::find(ids.begin(),ids.end(),"Heavy")==ids.end()); // An edited ID cannot smuggle a dependent effect into the introduction.
    }
    assert(FindEnemy(original,"Skirmisher")->behavior==EnemyBehavior::Skirmisher);
    assert(FindEnemy(original,"Flanker")->bulletDamage<FindEnemy(original,"Skirmisher")->bulletDamage);
    auto j=before;j["enemies"][0]["id"]="../outside";rejected(j);
    j=before;j["enemies"][1]["id"]=j["enemies"][0]["id"];rejected(j);
    j=before;j["enemies"][0]["behavior"]="UnknownBrain";rejected(j);
    j=before;j["enemies"][0]["hp"]="high";rejected(j);
    j=before;j["enemies"][0]["hp"]=2.5;rejected(j);
    j=before;j["enemies"][0]["hp"]=(std::numeric_limits<std::uint64_t>::max)();rejected(j);
    j=before;j["enemies"][0]["moveSpeedScale"]=(std::numeric_limits<double>::infinity)();rejected(j);
    j=before;j["enemies"][0]["magazineSize"]=9;rejected(j);
    j=before;j["enemies"][0]["magazineSize"]=1.5;rejected(j);
    j=before;j["enemies"][0]["reloadSeconds"]=0.2;rejected(j);
    j=before;j["enemies"][0]["reloadSeconds"]=(std::numeric_limits<double>::quiet_NaN)();rejected(j);
    j=before;j["upgrades"][0]["effects"]={"NotImplemented"};rejected(j);
    j=before;j["upgrades"][0]["effects"]={"Rapid","Rapid"};rejected(j);
    j=before;j["upgrades"][0]["effects"]=nlohmann::json::array();rejected(j);
    j=before;j["upgrades"][0]["effects"]={"ScatterShot","ScatterShot"};rejected(j);
    j=before;j["upgrades"][0]["maxPurchases"]=3;rejected(j);
    j=before;j["players"][0]["barrels"]=10000;rejected(j);
    j=before;j["players"][0]["bulletCount"]=0;rejected(j);
    j=before;j["players"][0]["bulletCount"]=6;rejected(j);
    j=before;j["players"][0]["baseClass"]="NoSuchPlayer";rejected(j);
    j=before;j["players"][0]["id"]="exp_twin_fortress";rejected(j);
    j=before;j["players"]=nlohmann::json::object();rejected(j);
    j=before;j["players"]=nlohmann::json::array();rejected(j);
    j=before;j["enemies"][0]["color"]={1,1};rejected(j);
    j=before;j["schemaVersion"]=6;rejected(j);
    j=before;j["upgrades"][0]["rarity"]=5;rejected(j);
    j=before;j["upgrades"][0]["compatibleStyles"]={"shooter","shooter"};rejected(j);
    j=before;j["upgrades"][0]["compatibleStyles"]={"invalid"};rejected(j);
    j=before;j["upgrades"][0]["compatibleStyles"]=nlohmann::json::array();rejected(j);
    j=before;j["players"][0]["style"]="invalid";rejected(j);
    j=before;j["players"][0]["rarity"]=4.5;rejected(j);
    j=before;j["players"][0]["style"]="drone";j["players"][0]["drones"]=0;rejected(j);
    j=before;j["players"][0]["style"]="melee";j["players"][0]["reflect"]=true;rejected(j);
    auto added=original;auto enemy=added.enemies.back();enemy.id="MyNewSniper";enemy.name="新しい狙撃兵";enemy.hp=71;enemy.creditDrop=19;added.enemies.push_back(enemy);
    assert(ValidateCatalog(added,error));assert(FindEnemy(added,"MyNewSniper")->hp==71);
    auto player=added.players.front();player.id="MyNewTank";player.barrels=6;player.fanAngle=12;added.players.push_back(player);
    auto upgrade=added.upgrades.front();upgrade.id="MyCombo";upgrade.effects={tankrun::CardId::Homing,tankrun::CardId::Pierce};added.upgrades.push_back(upgrade);
    assert(ValidateCatalog(added,error));
    const auto testFile=std::filesystem::path("content_roundtrip.json");
    assert(SaveCatalog(testFile.string(),added,error));Catalog disk;assert(LoadCatalog(testFile.string(),disk,error));assert(CatalogToJson(disk)==CatalogToJson(added));
    auto invalid=added;invalid.enemies[0].hp=-4;assert(!SaveCatalog(testFile.string(),invalid,error));assert(LoadCatalog(testFile.string(),disk,error));assert(CatalogToJson(disk)==CatalogToJson(added));
    assert(!LoadCatalog("missing-catalog-file.json",disk,error));assert(CatalogToJson(disk)==CatalogToJson(added));
    assert(CreditsFromExperience(-1)==0&&CreditsFromExperience(0)==0&&CreditsFromExperience(4)==1&&CreditsFromExperience(20)==4);
    for(int credits=0;credits<=999;++credits)assert(CreditsFromExperience(credits*5)==credits);
    const std::string shipped="../../project/resources/configs/expedition_content.json";
    if(std::filesystem::exists(shipped)) {
        Catalog production;assert(LoadCatalog(shipped,production,error));
        for(const auto& shippedUpgrade:production.upgrades)if(const auto* standard=FindUpgrade(original,shippedUpgrade.id))
            assert(shippedUpgrade.description==standard->description);
        for(const auto* id:{"BladeReach","ImpactDrive","PerfectDodge","LightBladeActuator","HeavyBladeEdge"})assert(FindUpgrade(production,id));
        assert(!FindUpgrade(production,"ScatterShot")&&IntroUpgradeIds(production,1).size()==3);
        for(const auto* id:{"DroneFocus","DroneGuard","MeleeTempo","FinisherCharge","HeavyDroneCore","ChainLightning"})assert(FindUpgrade(production,id));
        for(auto style:{tankbuild::Style::Shooter,tankbuild::Style::Drone,tankbuild::Style::Melee}) {
            const auto offers=BuildShopOffers(production,style,{}, {},23);assert(offers.size()==3);
            assert(std::count_if(production.players.begin(),production.players.end(),[&](const PlayerVariant& p){return p.style==style;})>=2);
        }
        for(const auto& playerType:production.players)assert(playerType.bulletCount==1);
    }
    if(argc>1){assert(SaveCatalog(argv[1],original,error));}
    std::cout<<"Content catalog: schema1-5 migration, no standard kits/refits, additive prerequisites, 18 new effects, weighted eligibility, custom composites and atomic save passed.\n";
}
