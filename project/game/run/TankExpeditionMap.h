#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace tankexp {
inline constexpr const char* kExpeditionMapPath="resources/configs/expedition_map.json";
inline constexpr int kExpeditionCurrencyLimit=999999;
inline std::filesystem::path ExpeditionMapPath(const std::string& path) {
#if defined(__cpp_char8_t)
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(path.data()),path.size()));
#else
    return std::filesystem::u8path(path);
#endif
}
enum class NodeKind { Combat, Elite, Upgrade, Evolution, Heal, Boss, Currency };
enum class NodeRole { None, TutorialCombat, TutorialSkip, TutorialUpgrade, IntroUpgrade };
inline const char* NodeRoleId(NodeRole role) {
    switch(role) {
    case NodeRole::None:return "none";
    case NodeRole::TutorialCombat:return "tutorial_combat";
    case NodeRole::TutorialSkip:return "tutorial_skip";
    case NodeRole::TutorialUpgrade:return "tutorial_upgrade";
    case NodeRole::IntroUpgrade:return "intro_upgrade";
    }
    return "invalid";
}
inline bool ParseNodeRole(const std::string& text,NodeRole& output) {
    for(const auto role:{NodeRole::None,NodeRole::TutorialCombat,NodeRole::TutorialSkip,NodeRole::TutorialUpgrade,NodeRole::IntroUpgrade})
        if(text==NodeRoleId(role)) {output=role;return true;}
    return false;
}
inline bool IsIntroUpgrade(NodeRole role) {return role==NodeRole::TutorialUpgrade||role==NodeRole::IntroUpgrade;}

inline const char* NodeKindId(NodeKind kind) {
    switch(kind) {
    case NodeKind::Combat:return "combat";
    case NodeKind::Elite:return "elite";
    case NodeKind::Upgrade:return "upgrade";
    case NodeKind::Evolution:return "evolution";
    case NodeKind::Heal:return "heal";
    case NodeKind::Boss:return "boss";
    case NodeKind::Currency:return "currency";
    }
    return "invalid";
}
inline bool ParseNodeKind(const std::string& text,NodeKind& output) {
    for(const auto kind:{NodeKind::Combat,NodeKind::Elite,NodeKind::Upgrade,NodeKind::Evolution,NodeKind::Heal,NodeKind::Boss,NodeKind::Currency}) {
        if(text==NodeKindId(kind)) {output=kind;return true;}
    }
    return false;
}
inline bool IsCombatNode(NodeKind kind) {
    return kind==NodeKind::Combat||kind==NodeKind::Elite||kind==NodeKind::Boss;
}
struct MapNode {
    std::string id;
    std::string label;
    NodeKind kind=NodeKind::Combat;
    int column=0;
    int row=0;
    int combatStage=-1;
    std::string roomTemplate;
    int clearReward=0;
    int serviceCost=0;
    std::vector<std::string> next;
    NodeRole role=NodeRole::None;
};
struct GenerationRoomRule {
    NodeKind kind=NodeKind::Combat;
    int firstColumn=2,lastColumn=31,weight=1;
    std::string roomTemplate;
};
inline std::vector<GenerationRoomRule> DefaultGenerationRooms() {
    return {
        {NodeKind::Combat,2,7,4,"outskirts"},{NodeKind::Combat,2,7,1,"crossfire"},
        {NodeKind::Combat,8,31,2,"guard_patrol"},{NodeKind::Combat,8,13,2,"crossfire"},{NodeKind::Combat,8,13,2,"resource_fork"},
        {NodeKind::Combat,14,31,2,"hazard_lane"},{NodeKind::Combat,14,31,1,"crossfire"},
        {NodeKind::Elite,2,31,1,"gatekeeper"},{NodeKind::Boss,2,31,1,"final_duel"}};
}
struct MapDefinition {
    int startingCurrency=20;
    std::vector<std::string> startNodes;
    std::vector<MapNode> nodes;
    // Old files remain authored. Only an explicit opt-in regenerates next run.
    bool procedural=false;
    uint32_t generationSeed=0;
    int generationMinColumns=18,generationMaxColumns=22;
    std::vector<GenerationRoomRule> generationRooms=DefaultGenerationRooms();
};
inline const MapNode* FindMapNode(const MapDefinition& map,const std::string& id) {
    const auto it=std::find_if(map.nodes.begin(),map.nodes.end(),[&](const MapNode& node){return node.id==id;});
    return it==map.nodes.end()?nullptr:&*it;
}
inline bool IsMapIdentifier(const std::string& text) {
    if(text.empty()||text.size()>64) return false;
    return std::all_of(text.begin(),text.end(),[](unsigned char c) {
        return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-';
    });
}
inline bool HasThreeConsecutiveNodeKinds(const MapDefinition& map) {
    for(const auto& first:map.nodes) for(const auto& secondId:first.next) {
        const auto* second=FindMapNode(map,secondId);
        if(!second||second->kind!=first.kind) continue;
        for(const auto& thirdId:second->next) {
            const auto* third=FindMapNode(map,thirdId);
            if(third&&third->kind==first.kind) return true;
        }
    }
    return false;
}
inline bool ValidateGenerationSettings(const MapDefinition& map,std::string& error) {
    auto fail=[&](const std::string& text){error=text;return false;};
    if(map.generationMinColumns<8||map.generationMaxColumns>32||map.generationMinColumns>map.generationMaxColumns)
        return fail("生成する列数は8～32、最小列数は最大列数以下にしてください。");
    if(map.generationRooms.empty()||map.generationRooms.size()>128)
        return fail("生成用の部屋候補は1～128件必要です。");
    std::set<std::tuple<NodeKind,int,int,std::string>> duplicates;
    for(const auto& rule:map.generationRooms) {
        if(!IsCombatNode(rule.kind)||!IsMapIdentifier(rule.roomTemplate)||rule.firstColumn<2||rule.lastColumn>31||
            rule.firstColumn>rule.lastColumn||rule.weight<1||rule.weight>1000)
            return fail("生成候補「"+rule.roomTemplate+"」: 戦闘/精鋭/ボス、左から3～32列目、開始≦終了、重み1～1000で指定してください。");
        if(!duplicates.emplace(rule.kind,rule.firstColumn,rule.lastColumn,rule.roomTemplate).second)
            return fail("同じ範囲・種類・部屋の生成候補が重複しています: "+rule.roomTemplate);
    }
    // Every possible generated combat column must have an explicit candidate.
    // The two introductory columns never use these rules.
    for(const auto kind:{NodeKind::Combat,NodeKind::Elite,NodeKind::Boss}) {
        const int first=kind==NodeKind::Boss?map.generationMinColumns-1:kind==NodeKind::Elite?3:2;
        const int last=kind==NodeKind::Boss?map.generationMaxColumns-1:map.generationMaxColumns-2;
        for(int column=first;column<=last;++column) {
            const bool covered=std::any_of(map.generationRooms.begin(),map.generationRooms.end(),[&](const auto& rule){
                return rule.kind==kind&&rule.firstColumn<=column&&column<=rule.lastColumn;
            });
            if(!covered) return fail("左から "+std::to_string(column+1)+" 列目の "+NodeKindId(kind)+" に部屋候補がありません。範囲をつないでください。");
        }
    }
    error.clear();return true;
}
inline bool ValidateExpeditionMap(const MapDefinition& map,std::string& error) {
    auto fail=[&](const std::string& text){error=text;return false;};
    if(map.startingCurrency<0||map.startingCurrency>kExpeditionCurrencyLimit)
        return fail("Starting currency must be between 0 and 999999.");
    if(map.nodes.empty()||map.nodes.size()>96) return fail("A map needs 1 to 96 nodes.");
    if(map.startNodes.empty()||map.startNodes.size()>5) return fail("A map needs 1 to 5 starting nodes.");
    if(!ValidateGenerationSettings(map,error))return false;
    std::set<std::string> identifiers;
    std::set<std::pair<int,int>> positions;
    int bosses=0;
    for(const auto& node:map.nodes) {
        if(!IsMapIdentifier(node.id)||!identifiers.insert(node.id).second)
            return fail("Node IDs must be unique ASCII identifiers: "+node.id);
        if(node.label.empty()||node.label.size()>160) return fail("Node labels need 1 to 160 UTF-8 bytes: "+node.id);
        if(node.column<0||node.column>31||node.row<0||node.row>4)
            return fail("Node positions must use column 0..31 and row 0..4: "+node.id);
        if(!positions.insert({node.column,node.row}).second) return fail("Two nodes share the same position: "+node.id);
        NodeKind validKind=NodeKind::Combat;
        if(!ParseNodeKind(NodeKindId(node.kind),validKind)) return fail("Unknown node kind: "+node.id);
        NodeRole validRole=NodeRole::None;
        if(!ParseNodeRole(NodeRoleId(node.role),validRole)) return fail("Unknown node role: "+node.id);
        if((node.role==NodeRole::TutorialCombat&&node.kind!=NodeKind::Combat)||
           (node.role==NodeRole::TutorialSkip&&node.kind!=NodeKind::Currency)||
           (IsIntroUpgrade(node.role)&&node.kind!=NodeKind::Upgrade))
            return fail("The introductory role does not match this node kind: "+node.id);
        if(node.clearReward<0||node.clearReward>kExpeditionCurrencyLimit||node.serviceCost<0||node.serviceCost>kExpeditionCurrencyLimit)
            return fail("Rewards and prices must be between 0 and 999999: "+node.id);
        if(IsCombatNode(node.kind)) {
            if(node.combatStage<0||node.combatStage>31||!IsMapIdentifier(node.roomTemplate)||node.serviceCost!=0)
                return fail("Combat needs a stage 0..31, room template, and zero service cost: "+node.id);
        } else if(node.combatStage!=-1||!node.roomTemplate.empty()||
            (node.kind!=NodeKind::Currency&&node.clearReward!=0)||(node.kind==NodeKind::Currency&&node.serviceCost!=0)) {
            return fail("Service nodes cannot contain a combat stage, room, or clear reward: "+node.id);
        }
        if(node.kind==NodeKind::Boss) {
            ++bosses;
            if(!node.next.empty()) return fail("The boss must be a terminal node: "+node.id);
        } else if(node.next.empty()) return fail("A non-boss node has no onward route: "+node.id);
        if(node.next.size()>5) return fail("A node can connect to at most 5 next nodes: "+node.id);
    }
    if(bosses!=1) return fail("A map must have exactly one final boss node.");
    std::set<std::string> starts;
    for(const auto& id:map.startNodes) {
        const auto* node=FindMapNode(map,id);
        if(!node||!starts.insert(id).second||node->column!=0)
            return fail("Starting nodes must be unique known IDs in column 0: "+id);
    }
    for(const auto& node:map.nodes) {
        std::set<std::string> destinations;
        for(const auto& id:node.next) {
            const auto* next=FindMapNode(map,id);
            if(!next||!destinations.insert(id).second) return fail("Unknown or duplicate connection from "+node.id+" to "+id);
            // Strictly increasing columns rule out both cycles and backward travel.
            if(next->column<=node.column) return fail("Connections must move to a later column: "+node.id+" to "+id);
        }
    }
    std::set<std::string> reached;
    std::vector<std::string> pending=map.startNodes;
    while(!pending.empty()) {
        const auto id=pending.back();pending.pop_back();
        if(!reached.insert(id).second) continue;
        const auto* node=FindMapNode(map,id);
        pending.insert(pending.end(),node->next.begin(),node->next.end());
    }
    if(reached.size()!=map.nodes.size()) return fail("Every node, including the boss, must be reachable from the start.");
    if(map.procedural&&HasThreeConsecutiveNodeKinds(map)) return fail("Procedural routes cannot contain three consecutive nodes of the same kind.");
    // With a finite forward graph and no other terminal, every route reaches the boss.
    error.clear();return true;
}
inline MapDefinition DefaultExpeditionMap() {
    MapDefinition map;
    map.startNodes={"outskirts"};
    map.nodes={
        {"outskirts","外縁戦闘",NodeKind::Combat,0,2,0,"outskirts",30,0,{"first_upgrade"}},
        {"first_upgrade","最初の改造",NodeKind::Upgrade,1,2,-1,"",0,30,{"salvage","elite"}},
        {"salvage","資源争奪",NodeKind::Combat,2,1,1,"resource_fork",35,0,{"field_repair","workshop"}},
        {"elite","精鋭部隊",NodeKind::Elite,2,3,1,"crossfire",55,0,{"field_repair","workshop"}},
        {"field_repair","応急修理",NodeKind::Heal,3,1,-1,"",0,0,{"evolution"}},
        {"workshop","改造工房",NodeKind::Upgrade,3,3,-1,"",0,45,{"evolution"}},
        {"evolution","換装工房",NodeKind::Upgrade,4,2,-1,"",0,0,{"ricochet","drone"}},
        {"ricochet","反射回廊",NodeKind::Combat,5,1,2,"hazard_lane",40,0,{"arsenal","repair"}},
        {"drone","無人機部隊",NodeKind::Combat,5,3,2,"crossfire",40,0,{"arsenal","repair"}},
        {"arsenal","武装強化",NodeKind::Upgrade,6,1,-1,"",0,65,{"gatekeeper"}},
        {"repair","修理ドック",NodeKind::Heal,6,3,-1,"",0,40,{"gatekeeper"}},
        {"gatekeeper","中枢防衛隊",NodeKind::Elite,7,2,3,"gatekeeper",50,0,{"final_upgrade","final_repair"}},
        {"final_upgrade","最終改造",NodeKind::Upgrade,8,1,-1,"",0,85,{"core"}},
        {"final_repair","最終修理",NodeKind::Heal,8,3,-1,"",0,50,{"core"}},
        {"core","中枢ボス",NodeKind::Boss,9,2,4,"final_duel",0,0,{}}
    };
    return map;
}
// Stable integer PRNG makes a captured seed reproduce identically across builds.
inline MapDefinition GenerateExpeditionMapUnchecked(uint32_t seed,const MapDefinition& settings,int tutorialCombatClearReward,int tutorialEnemyCredits) {
    uint32_t random=seed^0x9e3779b9u;
    auto draw=[&](uint32_t count) {
        random+=0x9e3779b9u;
        uint32_t mixed=random;
        mixed=(mixed^(mixed>>16))*0x85ebca6bu;
        mixed=(mixed^(mixed>>13))*0xc2b2ae35u;
        mixed^=mixed>>16;
        return count?mixed%count:0u;
    };
    MapDefinition map;map.procedural=true;map.generationSeed=seed;
    map.startingCurrency=settings.startingCurrency;
    map.generationMinColumns=settings.generationMinColumns;map.generationMaxColumns=settings.generationMaxColumns;
    map.generationRooms=settings.generationRooms;
    const int reward=(std::clamp)(tutorialCombatClearReward,0,kExpeditionCurrencyLimit);
    const int drops=(std::clamp)(tutorialEnemyCredits,0,kExpeditionCurrencyLimit-reward);
    map.startNodes={"tutorial_combat","tutorial_skip"};
    map.nodes={
        {"tutorial_combat","操作訓練",NodeKind::Combat,0,1,0,"tutorial_training",reward,0,{"tutorial_upgrade"},NodeRole::TutorialCombat},
        {"tutorial_skip","訓練を省略 / 資材支給",NodeKind::Currency,0,3,-1,"",reward+drops,0,{"skip_upgrade"},NodeRole::TutorialSkip},
        {"tutorial_upgrade","訓練後の改造",NodeKind::Upgrade,1,1,-1,"",0,15,{},NodeRole::TutorialUpgrade},
        {"skip_upgrade","初期改造",NodeKind::Upgrade,1,3,-1,"",0,15,{},NodeRole::IntroUpgrade}
    };
    const int length=map.generationMinColumns+static_cast<int>(draw(static_cast<uint32_t>(map.generationMaxColumns-map.generationMinColumns+1)));
    std::vector<size_t> previous{2,3};
    auto pickRoom=[&](NodeKind kind,int column) {
        unsigned total=0;
        for(const auto& rule:map.generationRooms) if(rule.kind==kind&&rule.firstColumn<=column&&column<=rule.lastColumn)total+=static_cast<unsigned>(rule.weight);
        unsigned value=draw(total);
        for(const auto& rule:map.generationRooms) if(rule.kind==kind&&rule.firstColumn<=column&&column<=rule.lastColumn) {
            if(value<static_cast<unsigned>(rule.weight))return rule.roomTemplate;
            value-=static_cast<unsigned>(rule.weight);
        }
        return std::string{};
    };
    for(int column=2;column<length;++column) {
        const bool final=column==length-1;
        const bool workshop=column==5||column==length-5;
        int width=column==2||final?1:workshop||column==length-2?2:1+static_cast<int>(draw(3));
        const size_t first=map.nodes.size();
        for(int lane=0;lane<width;++lane) {
            MapNode node;
            node.id=final?"core":"route_"+std::to_string(column)+"_"+std::to_string(lane);
            node.column=column;node.row=width==1?2:width==2?1+lane*2:lane*2;
            map.nodes.push_back(std::move(node));
        }
        // Connect neighbouring lanes, then repair uncovered lanes. Every source
        // gets an exit and every destination is reachable without dense crossings.
        auto connect=[&](size_t source,int lane) {
            auto& edges=map.nodes[source].next;const auto& id=map.nodes[first+static_cast<size_t>(lane)].id;
            if(std::find(edges.begin(),edges.end(),id)==edges.end()) edges.push_back(id);
        };
        for(size_t p=0;p<previous.size();++p) {
            const int nearest=previous.size()==1?width/2:static_cast<int>((p*static_cast<size_t>(width-1)+(previous.size()-1)/2)/(previous.size()-1));
            connect(previous[p],nearest);
            if(workshop) {connect(previous[p],0);connect(previous[p],1);}
            else if(width>1&&draw(100)<65) connect(previous[p],nearest==width-1?nearest-1:nearest+1);
        }
        for(int lane=0;lane<width;++lane) {
            const auto& id=map.nodes[first+static_cast<size_t>(lane)].id;
            bool incoming=false;for(const auto p:previous)
                incoming|=std::find(map.nodes[p].next.begin(),map.nodes[p].next.end(),id)!=map.nodes[p].next.end();
            if(!incoming) connect(previous[static_cast<size_t>(lane)*previous.size()/static_cast<size_t>(width)],lane);
        }
        for(int lane=0;lane<width;++lane) {
            auto& node=map.nodes[first+static_cast<size_t>(lane)];
            auto allowed=[&](NodeKind kind) {
                for(const auto p:previous) {
                    const auto& parent=map.nodes[p];
                    if(parent.kind!=kind||std::find(parent.next.begin(),parent.next.end(),node.id)==parent.next.end()) continue;
                    for(size_t g=0;g<first;++g) {
                        const auto& grand=map.nodes[g];
                        if(grand.kind==kind&&std::find(grand.next.begin(),grand.next.end(),parent.id)!=grand.next.end()) return false;
                    }
                }
                return true;
            };
            if(final) node.kind=NodeKind::Boss;
            else if(column==2) node.kind=NodeKind::Combat;
            else if(workshop&&lane==0&&allowed(NodeKind::Upgrade)) node.kind=NodeKind::Upgrade;
            else {
                std::vector<NodeKind> choices;
                if(workshop||column==length-2) choices={NodeKind::Upgrade,NodeKind::Heal};
                else if(column%3==2) choices={NodeKind::Combat,NodeKind::Combat,NodeKind::Elite};
                else if(column%3==0) choices={NodeKind::Upgrade,NodeKind::Upgrade,NodeKind::Heal};
                else choices={NodeKind::Combat,NodeKind::Elite,NodeKind::Upgrade,NodeKind::Heal};
                choices.erase(std::remove_if(choices.begin(),choices.end(),[&](NodeKind kind){return !allowed(kind);}),choices.end());
                if(choices.empty()) for(const auto kind:{NodeKind::Combat,NodeKind::Elite,NodeKind::Upgrade,NodeKind::Heal})
                    if(allowed(kind)) choices.push_back(kind);
                node.kind=choices[draw(static_cast<uint32_t>(choices.size()))];
            }
            if(IsCombatNode(node.kind)) {
                node.combatStage=final?4:(std::min)(4,1+(column-2)/4);
                node.roomTemplate=pickRoom(node.kind,column);
                node.clearReward=final?0:(node.kind==NodeKind::Elite?45:28)+column*3;
            } else {
                node.combatStage=-1;
                node.serviceCost=node.kind==NodeKind::Upgrade?30+column*2:node.kind==NodeKind::Heal?20+column:0;
            }
            const char* label=node.kind==NodeKind::Boss?"中枢ボス":node.kind==NodeKind::Combat?"戦闘区画":
                node.kind==NodeKind::Elite?"精鋭部隊":node.kind==NodeKind::Upgrade?"改造工房":"修理ドック";
            node.label=final?label:std::string(label)+" "+std::to_string(column-1);
        }
        previous.clear();for(int lane=0;lane<width;++lane) previous.push_back(first+static_cast<size_t>(lane));
    }
    return map;
}
inline bool GenerateExpeditionMap(const MapDefinition& settings,uint32_t seed,MapDefinition& output,std::string& error,
    int tutorialCombatClearReward=30,int tutorialEnemyCredits=8) {
    if(!ValidateGenerationSettings(settings,error))return false;
    auto generated=GenerateExpeditionMapUnchecked(seed,settings,tutorialCombatClearReward,tutorialEnemyCredits);
    if(!ValidateExpeditionMap(generated,error))return false;
    output=std::move(generated);error.clear();return true;
}
inline MapDefinition GenerateExpeditionMap(uint32_t seed,int tutorialCombatClearReward=30,int tutorialEnemyCredits=8) {
    return GenerateExpeditionMapUnchecked(seed,MapDefinition{},tutorialCombatClearReward,tutorialEnemyCredits);
}
inline nlohmann::json ExpeditionMapJson(const MapDefinition& map) {
    nlohmann::json result={{"schemaVersion",3},{"startingCurrency",map.startingCurrency},{"startNodes",map.startNodes},
        {"procedural",map.procedural},{"generationSeed",map.generationSeed},{"nodes",nlohmann::json::array()}};
    result["generation"]={{"minColumns",map.generationMinColumns},{"maxColumns",map.generationMaxColumns},{"rooms",nlohmann::json::array()}};
    for(const auto& rule:map.generationRooms)result["generation"]["rooms"].push_back({{"kind",NodeKindId(rule.kind)},
        {"firstColumn",rule.firstColumn},{"lastColumn",rule.lastColumn},{"weight",rule.weight},{"roomTemplate",rule.roomTemplate}});
    for(const auto& node:map.nodes) result["nodes"].push_back({
        {"id",node.id},{"label",node.label},{"kind",NodeKindId(node.kind)},
        {"column",node.column},{"row",node.row},{"combatStage",node.combatStage},
        {"roomTemplate",node.roomTemplate},{"clearReward",node.clearReward},
        {"serviceCost",node.serviceCost},{"next",node.next},{"role",NodeRoleId(node.role)}});
    return result;
}
inline bool ParseExpeditionMap(const nlohmann::json& input,MapDefinition& output,std::string& error) {
    // Parse into a temporary definition so all failures preserve the live authoring data.
    auto integer=[](const nlohmann::json& owner,const char* key,int low,int high,int& value) {
        if(!owner.contains(key)||!owner[key].is_number_integer()) return false;
        const double parsed=owner[key].get<double>();
        if(parsed<static_cast<double>(low)||parsed>static_cast<double>(high)) return false;
        value=static_cast<int>(parsed);return true;
    };
    auto strings=[](const nlohmann::json& owner,const char* key,std::vector<std::string>& values) {
        if(!owner.contains(key)||!owner[key].is_array()||owner[key].size()>5) return false;
        for(const auto& item:owner[key]) {
            if(!item.is_string()) return false;
            values.push_back(item.get<std::string>());
        }
        return true;
    };
    try {
        MapDefinition parsed;
        int version=0;
        if(!input.is_object()||!integer(input,"schemaVersion",1,3,version)||
            !integer(input,"startingCurrency",0,kExpeditionCurrencyLimit,parsed.startingCurrency)||
            !strings(input,"startNodes",parsed.startNodes)||!input.contains("nodes")||
            !input["nodes"].is_array()||input["nodes"].empty()||input["nodes"].size()>96) {
            error="Invalid map header. Expected schemaVersion 1, 2 or 3, startingCurrency, startNodes and nodes.";return false;
        }
        if(input.contains("procedural")) {
            if(!input["procedural"].is_boolean()) {error="procedural must be boolean.";return false;}
            parsed.procedural=input["procedural"].get<bool>();
        }
        if(input.contains("generationSeed")) {
            const auto& value=input["generationSeed"];
            if(!value.is_number_integer()||value.get<double>()<0||value.get<double>()>4294967295.0) {error="generationSeed must be uint32.";return false;}
            parsed.generationSeed=value.get<uint32_t>();
        }
        if(version>=3||input.contains("generation")) {
            const auto& generation=input.at("generation");
            if(!generation.is_object()||!integer(generation,"minColumns",8,32,parsed.generationMinColumns)||
                !integer(generation,"maxColumns",8,32,parsed.generationMaxColumns)||!generation.contains("rooms")||
                !generation["rooms"].is_array()||generation["rooms"].empty()||generation["rooms"].size()>128)
                {error="生成設定の列数または部屋候補一覧が不正です。";return false;}
            parsed.generationRooms.clear();
            for(const auto& value:generation["rooms"]) {
                GenerationRoomRule rule;
                if(!value.is_object()||!value.contains("kind")||!value["kind"].is_string()||!ParseNodeKind(value["kind"].get<std::string>(),rule.kind)||
                    !value.contains("roomTemplate")||!value["roomTemplate"].is_string()||
                    !integer(value,"firstColumn",2,31,rule.firstColumn)||!integer(value,"lastColumn",2,31,rule.lastColumn)||!integer(value,"weight",1,1000,rule.weight))
                    {error="生成候補の種類・部屋ID・列範囲・重みが不正です。";return false;}
                rule.roomTemplate=value["roomTemplate"].get<std::string>();parsed.generationRooms.push_back(std::move(rule));
            }
            if(!ValidateGenerationSettings(parsed,error))return false;
        }
        for(const auto& value:input["nodes"]) {
            MapNode node;
            if(!value.is_object()||!value.contains("id")||!value["id"].is_string()||
                !value.contains("label")||!value["label"].is_string()||
                !value.contains("kind")||!value["kind"].is_string()||
                !value.contains("roomTemplate")||!value["roomTemplate"].is_string()||
                !integer(value,"column",0,31,node.column)||!integer(value,"row",0,4,node.row)||
                !integer(value,"combatStage",-1,31,node.combatStage)||
                !integer(value,"clearReward",0,kExpeditionCurrencyLimit,node.clearReward)||
                !integer(value,"serviceCost",0,kExpeditionCurrencyLimit,node.serviceCost)||
                !strings(value,"next",node.next)) {
                error="Invalid node fields or field types.";return false;
            }
            node.id=value["id"].get<std::string>();node.label=value["label"].get<std::string>();
            node.roomTemplate=value["roomTemplate"].get<std::string>();
            if(value.contains("role")&&(!value["role"].is_string()||!ParseNodeRole(value["role"].get<std::string>(),node.role))) {
                error="Unknown node role: "+node.id;return false;
            }
            if(!ParseNodeKind(value["kind"].get<std::string>(),node.kind)) {error="Unknown node kind: "+node.id;return false;}
            parsed.nodes.push_back(std::move(node));
        }
        if(!ValidateExpeditionMap(parsed,error)) return false;
        output=std::move(parsed);error.clear();return true;
    } catch(const std::exception& e) {error=e.what();return false;}
}
inline bool LoadExpeditionMap(const std::string& path,MapDefinition& output,std::string& error) {
    try {
        std::ifstream file(ExpeditionMapPath(path));
        if(!file) {error="Cannot open "+path;return false;}
        nlohmann::json input;file>>input;
        return ParseExpeditionMap(input,output,error);
    } catch(const std::exception& e) {error=e.what();return false;}
}
inline bool SaveExpeditionMap(const std::string& path,const MapDefinition& map,std::string& error) {
    if(!ValidateExpeditionMap(map,error)) return false;
    const auto target=ExpeditionMapPath(path);
    auto temporary=target;temporary+=".tmp";
    try {
        std::ofstream file(temporary,std::ios::binary|std::ios::trunc);
        if(!file) {error="Cannot write "+path;return false;}
        file<<ExpeditionMapJson(map).dump(2)<<'\n';file.close();
        if(!file) {error="Writing expedition map failed.";return false;}
        // rename replaces the old regular file atomically; a failed write leaves it intact.
        std::filesystem::rename(temporary,target);
        error.clear();return true;
    } catch(const std::exception& e) {
        std::error_code ignored;std::filesystem::remove(temporary,ignored);
        error=e.what();return false;
    }
}

enum class MapRunPhase { Choosing, ActiveNode, Clear, Dead };
class ExpeditionMapRun {
public:
    ExpeditionMapRun():definition_(DefaultExpeditionMap()),currency_(definition_.startingCurrency) {}
    bool Reset(const MapDefinition& definition,std::string& error) {
        if(!ValidateExpeditionMap(definition,error)) return false;
        definition_=definition;for(auto& node:definition_.nodes)if(node.kind==NodeKind::Evolution)node.kind=NodeKind::Upgrade;
        currency_=definition_.startingCurrency;phase_=MapRunPhase::Choosing;
        activeId_.clear();currentId_.clear();visited_.clear();chosen_.clear();return true;
    }
    void Reset() {std::string ignored;Reset(DefaultExpeditionMap(),ignored);}
    const MapDefinition& GetDefinition() const {return definition_;}
    MapRunPhase GetPhase() const {return phase_;}
    bool IsChoosing() const {return phase_==MapRunPhase::Choosing;}
    bool IsComplete() const {return phase_==MapRunPhase::Clear;}
    bool IsDead() const {return phase_==MapRunPhase::Dead;}
    int GetCurrency() const {return currency_;}
    const std::string& GetCurrentNodeId() const {return currentId_;}
    const std::string& GetActiveNodeId() const {return activeId_;}
    const MapNode* GetActiveNode() const {return activeId_.empty()?nullptr:FindMapNode(definition_,activeId_);}
    const std::vector<std::string>& GetVisitedNodeIds() const {return visited_;}
    const std::vector<std::string>& GetChosenNodeIds() const {return chosen_;}
    bool HasVisited(const std::string& id) const {return std::find(visited_.begin(),visited_.end(),id)!=visited_.end();}
    std::vector<std::string> GetAvailableNodeIds() const {
        if(!IsChoosing()) return {};
        if(currentId_.empty()) return definition_.startNodes;
        const auto* current=FindMapNode(definition_,currentId_);
        return current?current->next:std::vector<std::string>{};
    }
    bool CanSelectNode(const std::string& id) const {
        const auto available=GetAvailableNodeIds();
        return std::find(available.begin(),available.end(),id)!=available.end();
    }
    bool SelectNode(const std::string& id) {
        if(!CanSelectNode(id)) return false;
        activeId_=id;chosen_.push_back(id);phase_=MapRunPhase::ActiveNode;return true;
    }
    bool CanAfford(int amount) const {return amount>=0&&amount<=currency_;}
    bool EarnCurrency(int amount) {
        if(IsComplete()||IsDead()||amount<=0||amount>kExpeditionCurrencyLimit) return false;
        currency_+=(std::min)(amount,kExpeditionCurrencyLimit-currency_);return true;
    }
    bool TrySpendCurrency(int amount) {
        if(IsComplete()||IsDead()||!CanAfford(amount)) return false;
        currency_-=amount;return true;
    }
    bool CompleteCombat(bool grantReward=true) {
        const auto* node=GetActiveNode();
        if(phase_!=MapRunPhase::ActiveNode||!node||!IsCombatNode(node->kind)) return false;
        if(grantReward&&node->clearReward>0) EarnCurrency(node->clearReward);
        const bool boss=node->kind==NodeKind::Boss;
        CompleteActiveNode();if(boss) phase_=MapRunPhase::Clear;return true;
    }
    bool CompleteService(bool purchase=true) {
        const auto* node=GetActiveNode();
        if(phase_!=MapRunPhase::ActiveNode||!node||IsCombatNode(node->kind)||node->kind==NodeKind::Currency) return false;
        if(purchase&&!TrySpendCurrency(node->serviceCost)) return false;
        CompleteActiveNode();return true;
    }
    bool CompleteCurrencyGrant(bool grantReward=true) {
        const auto* node=GetActiveNode();
        if(phase_!=MapRunPhase::ActiveNode||!node||node->kind!=NodeKind::Currency) return false;
        if(grantReward&&node->clearReward>0) EarnCurrency(node->clearReward);
        CompleteActiveNode();return true;
    }
    bool MarkDead() {
        const auto* node=GetActiveNode();
        if(phase_!=MapRunPhase::ActiveNode||!node||!IsCombatNode(node->kind)) return false;
        phase_=MapRunPhase::Dead;return true;
    }
private:
    void CompleteActiveNode() {
        visited_.push_back(activeId_);currentId_=activeId_;activeId_.clear();phase_=MapRunPhase::Choosing;
    }
    MapDefinition definition_;
    int currency_=0;
    MapRunPhase phase_=MapRunPhase::Choosing;
    std::string activeId_;
    std::string currentId_;
    std::vector<std::string> visited_;
    std::vector<std::string> chosen_;
};
} // namespace tankexp
