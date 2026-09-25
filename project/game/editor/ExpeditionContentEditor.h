#pragma once
#include "game/run/TankExpeditionContent.h"
namespace tankcontent {
// Draw only after ImGui::NewFrame. A true result means the scene should inject
// the updated catalog into its player/enemy systems. Unapplied edits stay local.
class ContentEditor {
public:
    void Open(const Catalog& live){draft_=live;selectedUpgrade_=selectedEnemy_=selectedPlayer_=0;status_.clear();}
    bool Draw(bool& open,Catalog& live);
private:
    Catalog draft_=DefaultCatalog();
    int selectedUpgrade_=0,selectedEnemy_=0,selectedPlayer_=0;
    std::string status_;
};
}
