#pragma once
#include "TankExpeditionMap.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace tankexp {
inline constexpr const char* kRoomCatalogPath="resources/maps/expedition_layouts.json";
inline constexpr int kRoomColumns=45, kRoomRows=30;
inline constexpr int kRoomLeft=10, kRoomRight=34, kRoomTop=7, kRoomBottom=22;
struct RoomPoint { float x=26, y=28; };
struct RoomSpawn { std::string id, type; float x=44, y=30; int hp=0; };
struct RoomDefinition {
    std::string id, name, objective="eliminate";
    std::vector<int> grid;
    std::vector<RoomSpawn> spawns;
    RoomPoint playerStart;
    std::vector<RoomPoint> objectiveTargets;
};
struct RoomCatalog { std::vector<RoomDefinition> rooms; };
inline RoomPoint RoomCellToWorld(int column,int row) {
    return {static_cast<float>(column*2),static_cast<float>((kRoomRows-1-row)*2)};
}
inline int RoomPointCell(RoomPoint point) {
    if(!std::isfinite(point.x)||!std::isfinite(point.y)||point.x<0||point.x>88||point.y<0||point.y>58) return -1;
    const int column=static_cast<int>(std::round(point.x/2));
    const int row=kRoomRows-1-static_cast<int>(std::round(point.y/2));
    return row*kRoomColumns+column;
}
inline bool IsRoomPointClear(const RoomDefinition& room,RoomPoint point,float radius=0.8f) {
    if(room.grid.size()!=static_cast<size_t>(kRoomColumns*kRoomRows)||!std::isfinite(point.x)||!std::isfinite(point.y)||
       point.x<22||point.x>66||point.y<16||point.y>42) return false;
    for(int row=kRoomTop;row<=kRoomBottom;++row) for(int col=kRoomLeft;col<=kRoomRight;++col) {
        if(room.grid[static_cast<size_t>(row*kRoomColumns+col)]==0) continue;
        const auto center=RoomCellToWorld(col,row);
        const float dx=(std::max)(std::abs(point.x-center.x)-1.0f,0.0f);
        const float dy=(std::max)(std::abs(point.y-center.y)-1.0f,0.0f);
        if(dx*dx+dy*dy<=radius*radius) return false;
    }
    return true;
}
inline bool RoomIdentifierValid(const std::string& id) {
    if(id.empty()||id.size()>64) return false;
    for(const unsigned char c:id) if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-')) return false;
    return true;
}
inline bool ValidateRoomCatalog(const RoomCatalog& catalog,std::string& error,const std::vector<std::string>* enemyIds=nullptr) {
    auto fail=[&](const std::string& message){error=message;return false;};
    if(catalog.rooms.empty()||catalog.rooms.size()>128) return fail("部屋数は1～128件です。");
    std::set<std::string> ids;
    for(const auto& room:catalog.rooms) {
        const std::string prefix=room.name+" ["+room.id+"]: ";
        if(!RoomIdentifierValid(room.id)||!ids.insert(room.id).second) return fail(prefix+"部屋IDは重複のない英数字・_-で指定してください。");
        if(room.name.empty()||room.name.size()>192) return fail(prefix+"名前は1～192バイトです。");
        if(room.objective!="eliminate"&&room.objective!="control"&&room.objective!="boss") return fail(prefix+"未対応の目標です。");
        if(room.grid.size()!=static_cast<size_t>(kRoomColumns*kRoomRows)) return fail(prefix+"グリッドは45×30です。");
        for(int row=0;row<kRoomRows;++row) for(int col=0;col<kRoomColumns;++col) {
            const int tile=room.grid[static_cast<size_t>(row*kRoomColumns+col)];
            if(tile<0||tile>2) return fail(prefix+"ブロックは0/1/2で指定してください。");
            const bool inside=col>=kRoomLeft&&col<=kRoomRight&&row>=kRoomTop&&row<=kRoomBottom;
            const bool border=inside&&(col==kRoomLeft||col==kRoomRight||row==kRoomTop||row==kRoomBottom);
            if((border&&tile!=1)||(!inside&&tile!=0)) return fail(prefix+"外周は通常壁で囲み、外側は空にしてください。");
        }
        if(!IsRoomPointClear(room,room.playerStart)) return fail(prefix+"自機開始位置が壁または範囲外です。");
        if(room.spawns.size()>128) return fail(prefix+"敵は128体まで配置できます。");
        if(room.objectiveTargets.size()!=(room.objective=="control"?3u:room.objective=="boss"?1u:0u))
            return fail(prefix+"制圧は目標3個、ボスは開始位置1個、殲滅は目標なしです。");
        std::array<bool,kRoomColumns*kRoomRows> reached{};
        std::queue<int> open;const int start=RoomPointCell(room.playerStart);open.push(start);reached[static_cast<size_t>(start)]=true;
        while(!open.empty()) {
            const int cell=open.front();open.pop();
            const int row=cell/kRoomColumns,col=cell%kRoomColumns;
            const std::array<int,4> next={cell-1,cell+1,cell-kRoomColumns,cell+kRoomColumns};
            for(const int n:next) {
                const int nr=n/kRoomColumns,nc=n%kRoomColumns;
                if(nr<=kRoomTop||nr>=kRoomBottom||nc<=kRoomLeft||nc>=kRoomRight||std::abs(nr-row)+std::abs(nc-col)!=1) continue;
                if(reached[static_cast<size_t>(n)]||room.grid[static_cast<size_t>(n)]!=0) continue;
                reached[static_cast<size_t>(n)]=true;open.push(n);
            }
        }
        std::set<std::string> spawnIds;
        std::vector<std::pair<RoomPoint,float>> occupied{{room.playerStart,0.8f}};
        auto available=[&](RoomPoint point,float radius) {
            if(!IsRoomPointClear(room,point,radius)) return false;
            const int cell=RoomPointCell(point);
            if(cell<0||!reached[static_cast<size_t>(cell)]) return false;
            for(const auto& other:occupied) {
                const float dx=point.x-other.first.x,dy=point.y-other.first.y;
                const float clearance=radius+other.second;
                if(dx*dx+dy*dy<clearance*clearance) return false;
            }
            occupied.push_back({point,radius});return true;
        };
        for(const auto& spawn:room.spawns) {
            if(!RoomIdentifierValid(spawn.id)||!spawnIds.insert(spawn.id).second) return fail(prefix+"配置IDが不正または重複しています。");
            if(!RoomIdentifierValid(spawn.type)||spawn.hp<0||spawn.hp>100000) return fail(prefix+"敵の種類またはHPが不正です。");
            if(enemyIds&&std::find(enemyIds->begin(),enemyIds->end(),spawn.type)==enemyIds->end()) return fail(prefix+"敵の種類が見つかりません: "+spawn.type);
            if(!available({spawn.x,spawn.y},0.8f)) return fail(prefix+"敵 "+spawn.id+" が壁・範囲外・孤立区画・他の配置に重なっています。");
        }
        if(room.objective=="eliminate"&&room.spawns.empty()) return fail(prefix+"殲滅目標には敵または資源タイプを1体以上配置してください。");
        for(const auto point:room.objectiveTargets) if(!available(point,room.objective=="boss"?2.0f:1.2f)) return fail(prefix+"目標位置が壁・範囲外・孤立区画・他の配置に重なっています。");
    }
    error.clear();return true;
}
inline const RoomDefinition* FindRoom(const RoomCatalog& catalog,const std::string& id) {
    for(const auto& room:catalog.rooms) if(room.id==id) return &room;
    return nullptr;
}
inline RoomDefinition* FindRoom(RoomCatalog& catalog,const std::string& id) {
    for(auto& room:catalog.rooms) if(room.id==id) return &room;
    return nullptr;
}
inline bool ValidateExpeditionMapRooms(const MapDefinition& map,const RoomCatalog& rooms,std::string& error,
    const std::vector<std::string>* enemyIds=nullptr) {
    if(!ValidateExpeditionMap(map,error)||!ValidateRoomCatalog(rooms,error,enemyIds))return false;
    auto check=[&](const std::string& id,NodeKind kind,const std::string& context) {
        const auto* room=FindRoom(rooms,id);
        if(!room) {error=context+": 部屋が見つかりません: "+id;return false;}
        if((kind==NodeKind::Boss)!=(room->objective=="boss")) {
            error=context+": ボス地点にはボス目標の部屋、通常/精鋭には殲滅または制圧の部屋が必要です: "+id;return false;
        }
        return true;
    };
    for(const auto& node:map.nodes)if(IsCombatNode(node.kind)&&!check(node.roomTemplate,node.kind,"地点 "+node.id))return false;
    if(map.procedural)for(const auto& rule:map.generationRooms)
        if(!check(rule.roomTemplate,rule.kind,"生成候補 "+std::to_string(rule.firstColumn+1)+"～"+std::to_string(rule.lastColumn+1)+"列目"))return false;
    error.clear();return true;
}
inline RoomDefinition MakeEmptyRoom(const std::string& id,const std::string& name) {
    RoomDefinition room;room.id=id;room.name=name;room.grid.resize(kRoomColumns*kRoomRows);
    for(int row=kRoomTop;row<=kRoomBottom;++row) for(int col=kRoomLeft;col<=kRoomRight;++col)
        if(row==kRoomTop||row==kRoomBottom||col==kRoomLeft||col==kRoomRight) room.grid[static_cast<size_t>(row*kRoomColumns+col)]=1;
    room.spawns={{"enemy_1","Charger",38,22,24},{"enemy_2","Skirmisher",50,34,24},{"enemy_3","Skirmisher",58,22,24}};
    return room;
}
inline RoomCatalog DefaultRoomCatalog() {
    RoomCatalog catalog;
    for(const auto& entry:std::array<std::pair<const char*,const char*>,5>{{{"outskirts","外縁区画"},{"crossfire","交差射線"},{"resource_fork","中継拠点"},{"hazard_lane","危険回廊"},{"final_duel","最終決戦"}}})
        catalog.rooms.push_back(MakeEmptyRoom(entry.first,entry.second));
    auto set=[&](int index,int col,int row,int tile){catalog.rooms[static_cast<size_t>(index)].grid[static_cast<size_t>(row*kRoomColumns+col)]=tile;};
    for(int r=10;r<=11;++r) for(int c=18;c<=19;++c) set(1,c,r,1);
    for(int r=18;r<=19;++r) for(int c=25;c<=26;++c) set(1,c,r,1);
    for(int r:{9,10,11,17,18,19}) for(int c:{19,25}) set(2,c,r,1);
    for(int r:{11,17}) for(int c:{19,20,21,23,24,25}) set(3,c,r,2);
    for(int r:{10,11,18,19}) for(int c:{16,17,27,28}) set(4,c,r,1);
    catalog.rooms[1].spawns={{"enemy_1","Flanker",42,24,0},{"enemy_2","Skirmisher",58,34,0},{"enemy_3","Sniper",60,20,0},{"enemy_4","Skirmisher",46,38,0}};
    catalog.rooms[2].objective="control";catalog.rooms[2].objectiveTargets={{44,30},{32,38},{58,20}};
    catalog.rooms[2].spawns={{"enemy_1","Charger",34,24,0},{"enemy_2","Flanker",56,36,0},{"enemy_3","Suppressor",62,28,0},{"enemy_4","Skirmisher",44,22,0}};
    catalog.rooms[3].spawns={{"enemy_1","Flanker",34,22,0},{"enemy_2","Skirmisher",46,28,0},{"enemy_3","RapidSniper",58,38,0},{"enemy_4","Suppressor",60,24,0}};
    catalog.rooms[4].objective="boss";catalog.rooms[4].objectiveTargets={{62,30}};
    catalog.rooms[4].spawns={{"enemy_1","Flanker",42,22,0},{"enemy_2","Skirmisher",46,38,0}};
    catalog.rooms.push_back(catalog.rooms[2]);catalog.rooms.back().id="gatekeeper";catalog.rooms.back().name="防衛装置区画";
    catalog.rooms.back().spawns[0].type="ArmoredCharger";catalog.rooms.back().spawns[0].hp=0;
    catalog.rooms[2].objective="eliminate";catalog.rooms[2].objectiveTargets.clear();
    return catalog;
}
inline nlohmann::json RoomCatalogToJson(const RoomCatalog& catalog) {
    nlohmann::json data={{"schemaVersion",1},{"columns",kRoomColumns},{"rows",kRoomRows},{"rooms",nlohmann::json::array()}};
    for(const auto& room:catalog.rooms) {
        nlohmann::json row={{"id",room.id},{"name",room.name},{"objective",room.objective},{"playerStart",{room.playerStart.x,room.playerStart.y}},
            {"tiles",nlohmann::json::array()},{"spawns",nlohmann::json::array()},{"objectiveTargets",nlohmann::json::array()}};
        for(int y=0;y<kRoomRows;++y) {
            std::string line;line.reserve(kRoomColumns);
            for(int x=0;x<kRoomColumns;++x) line.push_back(static_cast<char>('0'+room.grid[static_cast<size_t>(y*kRoomColumns+x)]));
            row["tiles"].push_back(line);
        }
        for(const auto& spawn:room.spawns) row["spawns"].push_back({{"id",spawn.id},{"type",spawn.type},{"x",spawn.x},{"y",spawn.y},{"hp",spawn.hp}});
        for(const auto target:room.objectiveTargets) row["objectiveTargets"].push_back({target.x,target.y});
        data["rooms"].push_back(std::move(row));
    }
    return data;
}
inline bool RoomCatalogFromJson(const nlohmann::json& data,RoomCatalog& output,std::string& error) {
    try {
        if(!data.is_object()||data.at("schemaVersion")!=1||data.at("columns")!=kRoomColumns||data.at("rows")!=kRoomRows||!data.at("rooms").is_array()||data.at("rooms").size()>128)
            {error="部屋ファイルの形式が異なります。";return false;}
        RoomCatalog loaded;
        for(const auto& value:data.at("rooms")) {
            RoomDefinition room;room.id=value.at("id").get<std::string>();room.name=value.at("name").get<std::string>();room.objective=value.at("objective").get<std::string>();
            const auto& start=value.at("playerStart");
            if(!start.is_array()||start.size()!=2) throw std::runtime_error("playerStart needs [x,y]");
            room.playerStart={start[0].get<float>(),start[1].get<float>()};
            const auto& tiles=value.at("tiles");
            if(!tiles.is_array()||tiles.size()!=kRoomRows) throw std::runtime_error("tiles needs 30 rows");
            for(const auto& lineValue:tiles) {
                const auto line=lineValue.get<std::string>();if(line.size()!=kRoomColumns) throw std::runtime_error("tile row needs 45 cells");
                for(const char tile:line) {if(tile<'0'||tile>'2') throw std::runtime_error("tile must be 0,1,2");room.grid.push_back(tile-'0');}
            }
            const auto& spawns=value.at("spawns");
            if(!spawns.is_array()||spawns.size()>128) throw std::runtime_error("spawns needs an array (max128)");
            for(const auto& spawn:spawns) {
                if(!spawn.at("hp").is_number_integer()) throw std::runtime_error("enemy hp must be an integer");
                const auto hp=spawn.at("hp").get<int64_t>();if(hp<0||hp>100000) throw std::runtime_error("enemy hp out of range");
                room.spawns.push_back({spawn.at("id").get<std::string>(),spawn.at("type").get<std::string>(),spawn.at("x").get<float>(),spawn.at("y").get<float>(),static_cast<int>(hp)});
            }
            const auto& targets=value.at("objectiveTargets");
            if(!targets.is_array()||targets.size()>3) throw std::runtime_error("objectiveTargets needs an array (max3)");
            for(const auto& target:targets) {
                if(!target.is_array()||target.size()!=2) throw std::runtime_error("target needs [x,y]");
                room.objectiveTargets.push_back({target[0].get<float>(),target[1].get<float>()});
            }
            loaded.rooms.push_back(std::move(room));
        }
        if(!ValidateRoomCatalog(loaded,error)) return false;
        output=std::move(loaded);error.clear();return true;
    } catch(const std::exception& e) {error=std::string("部屋ファイル読込失敗: ")+e.what();return false;}
}
inline bool LoadRoomCatalog(const std::string& path,RoomCatalog& output,std::string& error) {
    try {
        std::ifstream file(ExpeditionMapPath(path),std::ios::binary|std::ios::ate);
        if(!file) {error="部屋ファイルを開けません: "+path;return false;}
        if(file.tellg()>8*1024*1024) {error="部屋ファイルが8MBを超えています。";return false;}
        file.seekg(0);nlohmann::json data;file>>data;return RoomCatalogFromJson(data,output,error);
    } catch(const std::exception& e) {error=e.what();return false;}
}
inline bool SaveRoomCatalog(const std::string& path,const RoomCatalog& catalog,std::string& error) {
    if(!ValidateRoomCatalog(catalog,error)) return false;
    const std::filesystem::path target=ExpeditionMapPath(path);
    auto temporary=target;temporary+=".tmp";
    try {
        {std::ofstream file(temporary,std::ios::binary|std::ios::trunc);if(!file) {error="保存先を開けません。";return false;}
         file<<RoomCatalogToJson(catalog).dump(2)<<'\n';file.flush();if(!file) {error="部屋ファイルを書き込めません。";return false;}}
#ifdef _WIN32
        if(!MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
            error="部屋ファイルを置き換えられません (Windows "+std::to_string(GetLastError())+")。";return false;
        }
#else
        std::filesystem::rename(temporary,target);
#endif
        error.clear();return true;
    } catch(const std::exception& e) {error=e.what();return false;}
}
inline std::string RoomToCsv(const RoomDefinition& room) {
    if(room.grid.size()!=static_cast<size_t>(kRoomColumns*kRoomRows)) return {};
    std::ostringstream text;
    for(int y=0;y<kRoomRows;++y) {for(int x=0;x<kRoomColumns;++x) {if(x) text<<',';text<<room.grid[static_cast<size_t>(y*kRoomColumns+x)];}text<<'\n';}
    return text.str();
}
} // namespace tankexp
