#pragma once
#include "game/run/TankExpeditionMap.h"

namespace tankexp {
// The edited definition is committed separately from the active ExpeditionMapRun.
// Applying a graph cannot discard the player's current path, purchases, or wallet.
class MapEditor {
public:
    void Open(const MapDefinition& live) {draft_=live;selected_=0;initialized_=true;status_.clear();}
    bool Draw(bool& open,MapDefinition& live,const std::vector<std::string>& roomIds);
private:
    MapDefinition draft_;
    int selected_=0;
    bool initialized_=false;
    std::string status_;
};
} // namespace tankexp
