#include "../game/run/TankExpeditionRooms.h"
#include "../game/run/TankExpeditionMap.h"
#include "../game/run/TankExpeditionContent.h"
#include <cassert>
#include <iostream>
#include <limits>

int main(int argc,char** argv) {
    using namespace tankexp;
    std::string error;
    const auto original=DefaultRoomCatalog();
    if(!ValidateRoomCatalog(original,error)) {std::cerr<<error<<'\n';return 1;}
    assert(original.rooms.size()==6);
    assert(FindRoom(original,"gatekeeper")->objectiveTargets.size()==3);
    assert(FindRoom(original,"final_duel")->objective=="boss");
    assert(FindRoom(original,"hazard_lane")->spawns[2].type=="RapidSniper"&&FindRoom(original,"hazard_lane")->spawns[2].hp==0);
    assert(FindRoom(original,"gatekeeper")->spawns[0].type=="ArmoredCharger"&&FindRoom(original,"gatekeeper")->spawns[0].hp==0);
    const auto content=tankcontent::DefaultCatalog();
    std::vector<std::string> enemyIds;
    for(const auto& enemy:content.enemies) enemyIds.push_back(enemy.id);
    assert(ValidateRoomCatalog(original,error,&enemyIds));
    const auto route=DefaultExpeditionMap();
    assert(ValidateExpeditionMap(route,error));
    assert(ValidateExpeditionMapRooms(route,original,error,&enemyIds));
    auto invalidRoute=route;invalidRoute.nodes.front().roomTemplate="missing_room";
    assert(!ValidateExpeditionMapRooms(invalidRoute,original,error,&enemyIds)&&error.find("missing_room")!=std::string::npos);
    invalidRoute=route;invalidRoute.nodes.front().roomTemplate="final_duel";
    assert(!ValidateExpeditionMapRooms(invalidRoute,original,error,&enemyIds));
    invalidRoute=route;invalidRoute.nodes.back().roomTemplate="outskirts";
    assert(!ValidateExpeditionMapRooms(invalidRoute,original,error,&enemyIds));
    invalidRoute=route;invalidRoute.procedural=true;invalidRoute.generationRooms[0].roomTemplate="missing_candidate";
    assert(!ValidateExpeditionMapRooms(invalidRoute,original,error,&enemyIds)&&error.find("missing_candidate")!=std::string::npos);
    invalidRoute.generationRooms[0].roomTemplate="final_duel";
    assert(!ValidateExpeditionMapRooms(invalidRoute,original,error,&enemyIds));
    invalidRoute=route;invalidRoute.procedural=true;invalidRoute.generationRooms.back().roomTemplate="outskirts";
    assert(!ValidateExpeditionMapRooms(invalidRoute,original,error,&enemyIds));
    auto unknownEnemy=original;unknownEnemy.rooms[0].spawns[0].type="missing_enemy";
    assert(!ValidateExpeditionMapRooms(route,unknownEnemy,error,&enemyIds)&&error.find("missing_enemy")!=std::string::npos);
    auto tutorialRooms=original;auto tutorial=MakeEmptyRoom("tutorial_training","訓練");
    tutorial.spawns={{"target","Square",44,30,12}};tutorialRooms.rooms.push_back(tutorial);
    const auto generated=GenerateExpeditionMap(17);
    assert(ValidateExpeditionMapRooms(generated,tutorialRooms,error,&enemyIds));
    for(const auto& node:route.nodes) if(IsCombatNode(node.kind)) {
        const auto* room=FindRoom(original,node.roomTemplate);assert(room);
        assert((node.kind==NodeKind::Boss)==(room->objective=="boss"));
        for(const auto& spawn:room->spawns) assert(tankcontent::FindEnemy(content,spawn.type));
    }
    const auto json=RoomCatalogToJson(original);
    auto parsed=original;
    assert(RoomCatalogFromJson(json,parsed,error));
    assert(RoomCatalogToJson(parsed)==json);
    assert(SaveRoomCatalog("rooms_roundtrip.json",original,error));
    auto changed=original;changed.rooms[0].name="編集した外縁";changed.rooms[0].spawns[0].hp=27;
    assert(SaveRoomCatalog("rooms_roundtrip.json",changed,error)); // Replace existing file atomically.
    assert(LoadRoomCatalog("rooms_roundtrip.json",parsed,error));
    assert(RoomCatalogToJson(parsed)==RoomCatalogToJson(changed));
    auto bad=changed;bad.rooms[0].spawns[0].x=20;
    assert(!ValidateRoomCatalog(bad,error));
    assert(!SaveRoomCatalog("rooms_roundtrip.json",bad,error));
    assert(LoadRoomCatalog("rooms_roundtrip.json",parsed,error));
    assert(RoomCatalogToJson(parsed)==RoomCatalogToJson(changed)); // Failed write preserves disk.
    bad=original;bad.rooms[0].grid[7*kRoomColumns+20]=0;
    assert(!ValidateRoomCatalog(bad,error)); // Enclosure mandatory.
    bad=original;bad.rooms[0].spawns[0].x=std::numeric_limits<float>::quiet_NaN();
    assert(!ValidateRoomCatalog(bad,error));
    bad=original;bad.rooms[0].spawns[0].x=26;bad.rooms[0].spawns[0].y=28;
    assert(!ValidateRoomCatalog(bad,error)); // Player/enemy overlap.
    bad=original;bad.rooms[0].spawns[0].x=28;bad.rooms[0].spawns[0].y=28;
    bad.rooms[0].grid[15*kRoomColumns+14]=1;
    assert(!ValidateRoomCatalog(bad,error)); // Enemy inside wall.
    bad=original;
    for(int r=8;r<22;++r) bad.rooms[0].grid[static_cast<size_t>(r*kRoomColumns+16)]=1;
    assert(!ValidateRoomCatalog(bad,error)); // Enemies across disconnected partition.
    bad=original;bad.rooms[0].spawns.clear();assert(!ValidateRoomCatalog(bad,error));
    bad=original;for(auto& spawn:bad.rooms[0].spawns) spawn.type="Square";
    assert(ValidateRoomCatalog(bad,error)); // Authored eliminate counts every placed actor, including shapes.
    bad=original;bad.rooms[5].objectiveTargets[0]=bad.rooms[5].objectiveTargets[1];
    assert(!ValidateRoomCatalog(bad,error));
    bad=original;bad.rooms[4].objectiveTargets[0]={66,30};
    assert(!ValidateRoomCatalog(bad,error)); // Radius2 boss cannot touch outer wall.
    bad=original;bad.rooms[5].objectiveTargets[0]={40,36};
    assert(!ValidateRoomCatalog(bad,error)); // Radius1.2 resource overlaps neighboring wall.
    bad=original;bad.rooms.push_back(bad.rooms[0]);assert(!ValidateRoomCatalog(bad,error));
    bad=original;bad.rooms[0].spawns[1].id=bad.rooms[0].spawns[0].id;assert(!ValidateRoomCatalog(bad,error));
    const std::vector<std::string> known=enemyIds;
    bad=original;bad.rooms[0].spawns[0].type="ArmoredCharger";assert(ValidateRoomCatalog(bad,error,&known));
    bad.rooms[0].spawns[0].type="Unknown";assert(!ValidateRoomCatalog(bad,error,&known));
    const auto before=RoomCatalogToJson(parsed);
    auto badJson=json;badJson["rooms"][0]["spawns"][0]["hp"]=4294967296ULL;
    assert(!RoomCatalogFromJson(badJson,parsed,error));assert(RoomCatalogToJson(parsed)==before);
    badJson=json;badJson["rooms"][0]["tiles"][3]="0";
    assert(!RoomCatalogFromJson(badJson,parsed,error));assert(RoomCatalogToJson(parsed)==before);
    std::ofstream("rooms_bad.json")<<"{broken";
    assert(!LoadRoomCatalog("rooms_bad.json",parsed,error));assert(RoomCatalogToJson(parsed)==before);
    const auto csv=RoomToCsv(original.rooms[0]);
    assert(std::count(csv.begin(),csv.end(),'\n')==kRoomRows);
    assert(std::count(csv.begin(),csv.end(),',')==kRoomRows*(kRoomColumns-1));
    if(argc>1) {assert(SaveRoomCatalog(argv[1],original,error));}
    std::cout<<"Room authoring: six defaults, map/content contracts, roundtrip, atomic replace, bounds, overlaps, reachability, reference validation passed.\n";
}
