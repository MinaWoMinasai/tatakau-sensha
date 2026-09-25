#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
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
enum class NodeKind { Combat, Elite, Upgrade, Evolution, Heal, Boss };

inline const char* NodeKindId(NodeKind kind) {
    switch(kind) {
    case NodeKind::Combat:return "combat";
    case NodeKind::Elite:return "elite";
    case NodeKind::Upgrade:return "upgrade";
    case NodeKind::Evolution:return "evolution";
    case NodeKind::Heal:return "heal";
    case NodeKind::Boss:return "boss";
    }
    return "invalid";
}
inline bool ParseNodeKind(const std::string& text,NodeKind& output) {
    for(const auto kind:{NodeKind::Combat,NodeKind::Elite,NodeKind::Upgrade,NodeKind::Evolution,NodeKind::Heal,NodeKind::Boss}) {
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
};
struct MapDefinition {
    int startingCurrency=20;
    std::vector<std::string> startNodes;
    std::vector<MapNode> nodes;
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
inline bool ValidateExpeditionMap(const MapDefinition& map,std::string& error) {
    auto fail=[&](const std::string& text){error=text;return false;};
    if(map.startingCurrency<0||map.startingCurrency>kExpeditionCurrencyLimit)
        return fail("Starting currency must be between 0 and 999999.");
    if(map.nodes.empty()||map.nodes.size()>96) return fail("A map needs 1 to 96 nodes.");
    if(map.startNodes.empty()||map.startNodes.size()>5) return fail("A map needs 1 to 5 starting nodes.");
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
        if(node.clearReward<0||node.clearReward>kExpeditionCurrencyLimit||node.serviceCost<0||node.serviceCost>kExpeditionCurrencyLimit)
            return fail("Rewards and prices must be between 0 and 999999: "+node.id);
        if(IsCombatNode(node.kind)) {
            if(node.combatStage<0||node.combatStage>31||!IsMapIdentifier(node.roomTemplate)||node.serviceCost!=0)
                return fail("Combat needs a stage 0..31, room template, and zero service cost: "+node.id);
        } else if(node.combatStage!=-1||!node.roomTemplate.empty()||node.clearReward!=0) {
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
        {"evolution","機体進化",NodeKind::Evolution,4,2,-1,"",0,0,{"ricochet","drone"}},
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
inline nlohmann::json ExpeditionMapJson(const MapDefinition& map) {
    nlohmann::json result={{"schemaVersion",1},{"startingCurrency",map.startingCurrency},{"startNodes",map.startNodes},{"nodes",nlohmann::json::array()}};
    for(const auto& node:map.nodes) result["nodes"].push_back({
        {"id",node.id},{"label",node.label},{"kind",NodeKindId(node.kind)},
        {"column",node.column},{"row",node.row},{"combatStage",node.combatStage},
        {"roomTemplate",node.roomTemplate},{"clearReward",node.clearReward},
        {"serviceCost",node.serviceCost},{"next",node.next}});
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
        if(!input.is_object()||!integer(input,"schemaVersion",1,1,version)||
            !integer(input,"startingCurrency",0,kExpeditionCurrencyLimit,parsed.startingCurrency)||
            !strings(input,"startNodes",parsed.startNodes)||!input.contains("nodes")||
            !input["nodes"].is_array()||input["nodes"].empty()||input["nodes"].size()>96) {
            error="Invalid map header. Expected schemaVersion 1, startingCurrency, startNodes and nodes.";return false;
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
        definition_=definition;currency_=definition_.startingCurrency;phase_=MapRunPhase::Choosing;
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
    bool CompleteCombat() {
        const auto* node=GetActiveNode();
        if(phase_!=MapRunPhase::ActiveNode||!node||!IsCombatNode(node->kind)) return false;
        if(node->clearReward>0) EarnCurrency(node->clearReward);
        const bool boss=node->kind==NodeKind::Boss;
        CompleteActiveNode();if(boss) phase_=MapRunPhase::Clear;return true;
    }
    bool CompleteService(bool purchase=true) {
        const auto* node=GetActiveNode();
        if(phase_!=MapRunPhase::ActiveNode||!node||IsCombatNode(node->kind)) return false;
        if(purchase&&!TrySpendCurrency(node->serviceCost)) return false;
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
