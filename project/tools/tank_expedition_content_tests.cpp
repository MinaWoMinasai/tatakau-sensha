#include "../game/run/TankExpeditionContent.h"
#include <cassert>
#include <iostream>
#include <limits>

int main(int argc,char** argv){
    using namespace tankcontent;
    auto original=DefaultCatalog();std::string error;
    assert(ValidateCatalog(original,error));
    assert(original.upgrades.size()==14&&original.players.size()==5&&original.enemies.size()==8);
    Catalog restored;assert(CatalogFromJson(CatalogToJson(original),restored,error));
    assert(CatalogToJson(original)==CatalogToJson(restored));
    const auto before=CatalogToJson(restored);
    auto rejected=[&](nlohmann::json j){assert(!CatalogFromJson(j,restored,error));assert(CatalogToJson(restored)==before);assert(!error.empty());};
    auto j=before;j["enemies"][0]["id"]="../outside";rejected(j);
    j=before;j["enemies"][1]["id"]=j["enemies"][0]["id"];rejected(j);
    j=before;j["enemies"][0]["behavior"]="UnknownBrain";rejected(j);
    j=before;j["enemies"][0]["hp"]="high";rejected(j);
    j=before;j["enemies"][0]["hp"]=2.5;rejected(j);
    j=before;j["enemies"][0]["hp"]=(std::numeric_limits<std::uint64_t>::max)();rejected(j);
    j=before;j["enemies"][0]["moveSpeedScale"]=(std::numeric_limits<double>::infinity)();rejected(j);
    j=before;j["upgrades"][0]["effects"]={"NotImplemented"};rejected(j);
    j=before;j["upgrades"][0]["effects"]={"Rapid","Rapid"};rejected(j);
    j=before;j["upgrades"][0]["maxPurchases"]=3;rejected(j);
    j=before;j["players"][0]["barrels"]=10000;rejected(j);
    j=before;j["players"][0]["baseClass"]="NoSuchPlayer";rejected(j);
    j=before;j["players"][0]["id"]="exp_twin_fortress";rejected(j);
    j=before;j["players"]=nlohmann::json::object();rejected(j);
    j=before;j["players"]=nlohmann::json::array();rejected(j);
    j=before;j["enemies"][0]["color"]={1,1};rejected(j);
    j=before;j["schemaVersion"]=2;rejected(j);
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
    if(argc>1){assert(SaveCatalog(argv[1],original,error));}
    std::cout<<"Content catalog: variants, composites, strict validation, non-destructive load, atomic save and currency conversion passed.\n";
}
