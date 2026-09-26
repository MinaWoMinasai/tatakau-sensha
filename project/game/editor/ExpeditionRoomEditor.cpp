#include "ExpeditionRoomEditor.h"
#if defined(USE_IMGUI) || defined(USE_RUNTIME_PROFILER)
#include "externals/imgui/imgui.h"
#endif
#include <cstring>

namespace tankexp {
const std::string& ExpeditionRoomEditor::GetSelectedRoomId() const {
    static const std::string empty;
    return roomIndex_>=0&&roomIndex_<static_cast<int>(draft_.rooms.size())?draft_.rooms[static_cast<size_t>(roomIndex_)].id:empty;
}
bool ExpeditionRoomEditor::Draw(bool* open,RoomCatalog& applied,const std::vector<std::string>& enemyIds,const MapDefinition* map,const MapDefinition* activeMap) {
#if defined(USE_IMGUI) || defined(USE_RUNTIME_PROFILER)
    if(!open||!*open||!ImGui::GetCurrentContext()) return false;
    if(!initialized_) {draft_=applied;initialized_=true;dirty_=false;}
    auto validate=[&](const RoomCatalog& catalog,std::string& error) {
        if(map?!ValidateExpeditionMapRooms(*map,catalog,error,&enemyIds):!ValidateRoomCatalog(catalog,error,&enemyIds))return false;
        return !activeMap||ValidateExpeditionMapRooms(*activeMap,catalog,error,&enemyIds);
    };
    bool committed=false;
    ImGui::SetNextWindowPos({46,38},ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({1188,640},ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("区画エディター / F4で戻る",open)) {ImGui::End();return false;}
    ImGui::TextWrapped("F4: 部屋の敵・壁・達成目標 / F5: 出現する列と部屋の抽選 / F6: 敵の種類・性能。適用した配置は次の部屋入場から有効です。戦闘中の敵は変わりません。");
    if(ImGui::Button("Apply / 適用")) {
        if(validate(draft_,status_)) {applied=draft_;dirty_=false;committed=true;status_="適用しました。次の入場から有効です。保存すると次回起動にも反映します。";}
    }
    ImGui::SameLine();
    if(ImGui::Button("Save / 保存")) {
        if(validate(draft_,status_)&&SaveRoomCatalog(kRoomCatalogPath,draft_,status_)) {
            applied=draft_;dirty_=false;committed=true;status_="保存・適用しました: "+std::string(kRoomCatalogPath);
        }
    }
    ImGui::SameLine();
    if(ImGui::Button("Reload / 再読込")) {
        RoomCatalog loaded;
        const bool loadedFile=LoadRoomCatalog(kRoomCatalogPath,loaded,status_);
        // Legacy catalogs predate the scene-injected introductory room. Reload
        // those files without dropping the reserved room from an active map.
        if(loadedFile&&!FindRoom(loaded,"tutorial_training"))
            if(const auto* tutorial=FindRoom(applied,"tutorial_training"))loaded.rooms.push_back(*tutorial);
        if(loadedFile&&validate(loaded,status_)) {
            draft_=loaded;applied=std::move(loaded);roomIndex_=0;spawnIndex_=-1;dirty_=false;committed=true;status_="保存済みの部屋を再読込・適用しました。";
        }
    }
    ImGui::SameLine();
    if(ImGui::Button("Revert / 編集を破棄")) {draft_=applied;dirty_=false;spawnIndex_=-1;status_="未適用の編集を破棄しました。";}
    ImGui::SameLine();ImGui::TextUnformatted(dirty_?"* 未適用の編集あり":"適用中の内容");
    ImGui::TextWrapped("%s",status_.c_str());
    ImGui::Separator();
    if(draft_.rooms.empty()) {ImGui::TextUnformatted("部屋がありません。保存済みデータを再読込してください。");ImGui::End();return committed;}
    roomIndex_=(std::clamp)(roomIndex_,0,static_cast<int>(draft_.rooms.size())-1);
    if(ImGui::BeginChild("RoomProperties",{310,0},ImGuiChildFlags_Borders)) {
        if(ImGui::BeginCombo("区画",draft_.rooms[static_cast<size_t>(roomIndex_)].name.c_str())) {
            for(size_t i=0;i<draft_.rooms.size();++i) {
                ImGui::PushID(static_cast<int>(i));
                if(ImGui::Selectable(draft_.rooms[i].name.c_str(),roomIndex_==static_cast<int>(i))) {roomIndex_=static_cast<int>(i);spawnIndex_=-1;}
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        auto nextId=[&]() {int number=1;while(FindRoom(draft_,"room_"+std::to_string(number))) ++number;return "room_"+std::to_string(number);};
        if(draft_.rooms.size()<128) {
            if(ImGui::Button("新しい区画")) {
                auto added=MakeEmptyRoom(nextId(),"新しい区画");
                if(!enemyIds.empty())for(auto& spawn:added.spawns){spawn.type=enemyIds.front();spawn.hp=0;}
                draft_.rooms.push_back(std::move(added));roomIndex_=static_cast<int>(draft_.rooms.size())-1;spawnIndex_=-1;dirty_=true;
            }
            ImGui::SameLine();
            if(ImGui::Button("区画を複製")) {auto clone=draft_.rooms[static_cast<size_t>(roomIndex_)];clone.id=nextId();clone.name+=" コピー";draft_.rooms.push_back(std::move(clone));roomIndex_=static_cast<int>(draft_.rooms.size())-1;spawnIndex_=-1;dirty_=true;}
        }
        auto& room=draft_.rooms[static_cast<size_t>(roomIndex_)];
        ImGui::Text("ID: %s",room.id.c_str());
        if(map) {
            bool used=false;
            if(map->procedural)for(const auto& rule:map->generationRooms)if(rule.roomTemplate==room.id) {
                ImGui::Text("抽選: %d～%d列目 / %s / 重み%d",rule.firstColumn+1,rule.lastColumn+1,NodeKindId(rule.kind),rule.weight);used=true;
            }
            if(!map->procedural)for(const auto& node:map->nodes)if(node.roomTemplate==room.id) {
                ImGui::Text("固定配置: %d列目 / %s",node.column+1,node.id.c_str());used=true;
            }
            if(!used)ImGui::TextWrapped("通常経路では未使用。F5でこの部屋を列・抽選候補へ追加してください。");
        }
        std::array<char,193> name{};std::memcpy(name.data(),room.name.data(),(std::min)(room.name.size(),name.size()-1));
        if(ImGui::InputText("名前",name.data(),name.size())) {room.name=name.data();dirty_=true;}
        const char* objectiveNames[]={"配置した敵・資源をすべて破壊","装置3個と敵・資源を破壊","ボスを倒す"};
        int objective=room.objective=="control"?1:room.objective=="boss"?2:0;
        if(ImGui::Combo("達成目標",&objective,objectiveNames,3)) {
            room.objective=objective==1?"control":objective==2?"boss":"eliminate";
            room.objectiveTargets=objective==1?std::vector<RoomPoint>{{44,30},{32,38},{58,20}}:
                objective==2?std::vector<RoomPoint>{{62,30}}:std::vector<RoomPoint>{};
            targetIndex_=0;dirty_=true;
        }
        ImGui::SeparatorText("配置するもの");
        ImGui::RadioButton("床",&tool_,0);ImGui::SameLine();ImGui::RadioButton("通常壁",&tool_,1);ImGui::SameLine();ImGui::RadioButton("危険壁",&tool_,2);
        ImGui::RadioButton("敵・資源",&tool_,3);ImGui::SameLine();ImGui::RadioButton("自機開始",&tool_,4);ImGui::SameLine();ImGui::RadioButton("消しゴム",&tool_,6);
        if(!room.objectiveTargets.empty()) {
            ImGui::RadioButton(room.objective=="boss"?"ボス開始":"目標装置",&tool_,5);
            if(room.objective=="control") {ImGui::SameLine();ImGui::SetNextItemWidth(110);int displayTarget=targetIndex_+1;if(ImGui::SliderInt("番号",&displayTarget,1,3)) targetIndex_=displayTarget-1;}
        }
        if(!enemyIds.empty()) {
            enemyIndex_=(std::clamp)(enemyIndex_,0,static_cast<int>(enemyIds.size())-1);
            if(ImGui::BeginCombo("敵・資源タイプ",enemyIds[static_cast<size_t>(enemyIndex_)].c_str())) {
                for(size_t i=0;i<enemyIds.size();++i) if(ImGui::Selectable(enemyIds[i].c_str(),enemyIndex_==static_cast<int>(i))) {enemyIndex_=static_cast<int>(i);tool_=3;}
                ImGui::EndCombo();
            }
        } else ImGui::TextWrapped("種類エディターで敵・資源タイプを追加してください。");
        ImGui::TextWrapped("左クリック / ドラッグで配置。右クリックで敵・壁を消去。敵の上を左クリックすると詳細を選択。目標は道具を選んで移動。外周は固定です。");
        ImGui::SeparatorText("配置した敵・資源");
        if(ImGui::BeginListBox("##spawnList",{-1,90})) {
            for(size_t i=0;i<room.spawns.size();++i) {
                const auto label=room.spawns[i].id+" : "+room.spawns[i].type;
                if(ImGui::Selectable(label.c_str(),spawnIndex_==static_cast<int>(i))) spawnIndex_=static_cast<int>(i);
            }
            ImGui::EndListBox();
        }
        if(spawnIndex_>=0&&spawnIndex_<static_cast<int>(room.spawns.size())) {
            auto& spawn=room.spawns[static_cast<size_t>(spawnIndex_)];
            ImGui::TextUnformatted("HP上書き (0=種類の標準)");ImGui::SetNextItemWidth(-1);
            if(ImGui::DragInt("##spawnHp",&spawn.hp,1,0,100000)) dirty_=true;
            if(ImGui::BeginCombo("変更する種類",spawn.type.c_str())) {
                for(const auto& id:enemyIds) if(ImGui::Selectable(id.c_str(),id==spawn.type)) {spawn.type=id;dirty_=true;}
                ImGui::EndCombo();
            }
            float xy[2]={spawn.x,spawn.y};
            if(ImGui::DragFloat2("座標",xy,2,0,88,"%.0f")) {spawn.x=xy[0];spawn.y=xy[1];dirty_=true;}
            if(ImGui::Button("選択した敵を削除")) {room.spawns.erase(room.spawns.begin()+spawnIndex_);spawnIndex_=-1;dirty_=true;}
        }
        std::string validation;
        if(!validate(draft_,validation)) {ImGui::PushStyleColor(ImGuiCol_Text,{1.0f,0.48f,0.37f,1});ImGui::TextWrapped("保存できません: %s",validation.c_str());ImGui::PopStyleColor();}
        else ImGui::TextColored({0.35f,0.95f,0.72f,1},"配置チェック OK / 敵 %d体",static_cast<int>(room.spawns.size()));
    }
    ImGui::EndChild();ImGui::SameLine();
    if(ImGui::BeginChild("RoomCanvas",{0,0},ImGuiChildFlags_Borders)) {
        auto& room=draft_.rooms[static_cast<size_t>(roomIndex_)];
        ImGui::TextUnformatted("水色 P=自機 / 赤 E=敵・資源 / 金 1～3=装置 / 金 B=ボス");
        const auto available=ImGui::GetContentRegionAvail();
        const float cell=(std::max)(6.0f,(std::min)(available.x/static_cast<float>(kRoomColumns),(available.y-26)/static_cast<float>(kRoomRows)));
        const ImVec2 origin=ImGui::GetCursorScreenPos();
        const ImVec2 size={cell*kRoomColumns,cell*kRoomRows};
        ImGui::InvisibleButton("RoomPaint",size,ImGuiButtonFlags_MouseButtonLeft|ImGuiButtonFlags_MouseButtonRight);
        const bool canvasHovered=ImGui::IsItemHovered(),canvasActive=ImGui::IsItemActive();
        auto* draw=ImGui::GetWindowDrawList();
        draw->AddRectFilled(origin,{origin.x+size.x,origin.y+size.y},IM_COL32(7,15,28,255));
        for(int row=0;row<kRoomRows;++row) for(int col=0;col<kRoomColumns;++col) {
            const auto index=static_cast<size_t>(row*kRoomColumns+col);
            const int tile=room.grid[index];const bool inside=col>=kRoomLeft&&col<=kRoomRight&&row>=kRoomTop&&row<=kRoomBottom;
            const ImU32 fill=tile==1?IM_COL32(35,145,177,255):tile==2?IM_COL32(202,56,105,255):inside?IM_COL32(18,34,50,255):IM_COL32(10,16,25,255);
            const ImVec2 a={origin.x+cell*col,origin.y+cell*row},b={a.x+cell,a.y+cell};
            draw->AddRectFilled(a,b,fill);draw->AddRect(a,b,IM_COL32(53,80,98,80));
        }
        auto marker=[&](RoomPoint point,const char* text,ImU32 color,bool selected) {
            const ImVec2 p={origin.x+(point.x/2+0.5f)*cell,origin.y+(static_cast<float>(kRoomRows)-0.5f-point.y/2)*cell};
            draw->AddCircleFilled(p,cell*0.43f,color);
            if(selected) draw->AddCircle(p,cell*0.65f,IM_COL32(255,255,255,255),12,2);
            const float fontSize=(std::max)(9.0f,cell*0.75f);
            draw->AddText(nullptr,fontSize,{p.x-fontSize*0.3f,p.y-fontSize*0.5f},IM_COL32(3,10,20,255),text);
        };
        marker(room.playerStart,"P",IM_COL32(53,244,218,255),false);
        for(size_t i=0;i<room.spawns.size();++i) marker({room.spawns[i].x,room.spawns[i].y},"E",IM_COL32(250,107,130,255),spawnIndex_==static_cast<int>(i));
        for(size_t i=0;i<room.objectiveTargets.size();++i) marker(room.objectiveTargets[i],room.objective=="boss"?"B":std::to_string(i+1).c_str(),IM_COL32(255,216,99,255),tool_==5&&targetIndex_==static_cast<int>(i));
        if(canvasHovered) {
            const auto mouse=ImGui::GetIO().MousePos;
            const int col=static_cast<int>((mouse.x-origin.x)/cell),row=static_cast<int>((mouse.y-origin.y)/cell);
            if(col>kRoomLeft&&col<kRoomRight&&row>kRoomTop&&row<kRoomBottom) {
                const RoomPoint point=RoomCellToWorld(col,row);
                const int index=row*kRoomColumns+col;
                draw->AddRect({origin.x+col*cell,origin.y+row*cell},{origin.x+(col+1)*cell,origin.y+(row+1)*cell},IM_COL32(255,255,255,240),0.0f,2.0f);
                ImGui::SetTooltip("セル %d,%d / ワールド %.0f,%.0f",col,row,point.x,point.y);
                const bool right=ImGui::IsMouseClicked(ImGuiMouseButton_Right);
                const bool left=ImGui::IsMouseClicked(ImGuiMouseButton_Left);
                const bool painting=ImGui::IsMouseDown(ImGuiMouseButton_Left)&&canvasActive;
                auto occupant=std::find_if(room.spawns.begin(),room.spawns.end(),[&](const RoomSpawn& spawn){return RoomPointCell({spawn.x,spawn.y})==index;});
                if(right||((left||painting)&&tool_==6)) {
                    if(occupant!=room.spawns.end()) {room.spawns.erase(occupant);spawnIndex_=-1;dirty_=true;}
                    if(room.grid[static_cast<size_t>(index)]!=0) {room.grid[static_cast<size_t>(index)]=0;dirty_=true;}
                    // Required targets are moved, never removed; this keeps the objective count explicit.
                } else if((left||painting)&&tool_<=2) {
                    if(room.grid[static_cast<size_t>(index)]!=tool_) {room.grid[static_cast<size_t>(index)]=tool_;dirty_=true;}
                } else if(left&&tool_==3&&!enemyIds.empty()) {
                    if(occupant!=room.spawns.end()) spawnIndex_=static_cast<int>(std::distance(room.spawns.begin(),occupant));
                    else if(room.spawns.size()<128) {
                        int id=1;while(std::any_of(room.spawns.begin(),room.spawns.end(),[&](const RoomSpawn& s){return s.id=="enemy_"+std::to_string(id);})) ++id;
                        room.spawns.push_back({"enemy_"+std::to_string(id),enemyIds[static_cast<size_t>(enemyIndex_)],point.x,point.y,0});
                        spawnIndex_=static_cast<int>(room.spawns.size())-1;dirty_=true;
                    }
                } else if(left&&tool_==4) {room.playerStart=point;dirty_=true;}
                else if(left&&tool_==5&&!room.objectiveTargets.empty()) {
                    targetIndex_=(std::clamp)(targetIndex_,0,static_cast<int>(room.objectiveTargets.size())-1);room.objectiveTargets[static_cast<size_t>(targetIndex_)]=point;dirty_=true;
                }
            }
        }
        ImGui::TextUnformatted("経路マップ上の戦闘地点に、この区画のIDを割り当てて使用します。");
    }
    ImGui::EndChild();ImGui::End();return committed;
#else
    (void)open;(void)applied;(void)enemyIds;(void)map;(void)activeMap;return false;
#endif
}
} // namespace tankexp
