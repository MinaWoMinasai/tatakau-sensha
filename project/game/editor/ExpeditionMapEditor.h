#pragma once
#include "game/run/TankExpeditionMap.h"
#include "game/run/TankExpeditionRooms.h"

namespace tankexp {
// The edited definition is committed separately from the active ExpeditionMapRun.
// Applying a graph cannot discard the player's current path, purchases, or wallet.
class MapEditor {
public:
    void Open(const MapDefinition& live) {draft_=live;selected_=0;initialized_=true;status_.clear();}
    bool Draw(bool& open,MapDefinition& live,const RoomCatalog& rooms,const std::vector<std::string>* enemyIds=nullptr);
private:
    MapDefinition draft_;
    int selected_=0,ruleSelected_=0;
    bool initialized_=false;
    std::string status_;
};
} // namespace tankexp
