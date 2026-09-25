#pragma once
#include "game/run/TankExpeditionRooms.h"

namespace tankexp {
class ExpeditionRoomEditor {
public:
    // An accepted Apply/Save/Reload returns true. Invalid edits never replace applied.
    bool Draw(bool* open,RoomCatalog& applied,const std::vector<std::string>& enemyIds);
    const std::string& GetSelectedRoomId() const;
    void ResetDraft() { initialized_=false; }
private:
    RoomCatalog draft_;
    bool initialized_=false;
    int roomIndex_=0,tool_=1,enemyIndex_=0,spawnIndex_=-1,targetIndex_=0;
    bool dirty_=false;
    std::string status_="部屋を選び、左の道具で配置します。";
};
}
