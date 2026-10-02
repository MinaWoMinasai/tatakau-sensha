#include "ExpeditionMapEditor.h"
#if defined(USE_IMGUI) || defined(USE_RUNTIME_PROFILER)
#include "externals/imgui/imgui.h"
#include <array>
#include <cstdio>
#include <map>

namespace {
constexpr auto kNodeNames=[] {
    std::array<const char*,tankexp::kNodeKindDefinitions.size()> names{};
    for(std::size_t i=0;i<names.size();++i)names[i]=tankexp::kNodeKindDefinitions[i].editorName;
    return names;
}();
constexpr auto kRoleNames=[] {
    std::array<const char*,tankexp::kNodeRoleDefinitions.size()> names{};
    for(std::size_t i=0;i<names.size();++i)names[i]=tankexp::kNodeRoleDefinitions[i].editorName;
    return names;
}();
ImU32 MapColor(tankexp::NodeKind kind,int alpha=255) {
    const auto& color=tankexp::GetNodeKindDefinition(kind).editorColor;
    return IM_COL32(color[0],color[1],color[2],alpha);
}
void TextField(const char* label,std::string& value) {
    std::array<char,512> text{};std::snprintf(text.data(),text.size(),"%s",value.c_str());
    if(ImGui::InputText(label,text.data(),text.size())) value=text.data();
}
std::string NewId(const tankexp::MapDefinition& map) {
    for(int suffix=1;;++suffix) {
        const auto id="node_"+std::to_string(suffix);
        if(!tankexp::FindMapNode(map,id)) return id;
    }
}
bool RebuildPreview(tankexp::MapDefinition& map,int& selected,std::string& error) {
    tankexp::MapDefinition preview;
    if(!tankexp::GenerateExpeditionMap(map,map.generationSeed,preview,error))return false;
    map=std::move(preview);selected=0;return true;
}
bool GenerationRules(tankexp::MapDefinition& map,int& selected,const tankexp::RoomCatalog& rooms) {
    bool changed=false;
    ImGui::SeparatorText("出現する区画と敵編成 / 左からの列で指定");
    ImGui::TextWrapped("左から1列目は訓練/スキップ、2列目は初期改造に固定。3列目以降の戦闘を設定します。同じ種類・列に重なる候補は重みで抽選されます。敵の配置はF4、敵の種類・性能はF6で編集します。");
    ImGui::SetNextItemWidth(150);changed|=ImGui::InputInt("最小列数",&map.generationMinColumns);
    ImGui::SameLine();ImGui::SetNextItemWidth(150);changed|=ImGui::InputInt("最大列数",&map.generationMaxColumns);
    ImGui::TextWrapped("最後の列が最終ボスです。通常戦闘の候補は3列目～最大列数−1、精鋭は4列目～最大列数−1を覆うように設定してください。重み4と1なら、その列での抽選はおよそ80%と20%です。");
    if(ImGui::BeginListBox("生成候補",{-1,130})) {
        for(size_t i=0;i<map.generationRooms.size();++i) {
            const auto& rule=map.generationRooms[i];ImGui::PushID(static_cast<int>(i));
            const auto* room=tankexp::FindRoom(rooms,rule.roomTemplate);
            const std::string text=std::to_string(rule.firstColumn+1)+"～"+std::to_string(rule.lastColumn+1)+"列目 / "+kNodeNames[static_cast<int>(rule.kind)]+
                " / "+(room?room->name:rule.roomTemplate)+" / 重み"+std::to_string(rule.weight);
            if(ImGui::Selectable(text.c_str(),selected==static_cast<int>(i)))selected=static_cast<int>(i);
            ImGui::PopID();
        }
        ImGui::EndListBox();
    }
    ImGui::BeginDisabled(map.generationRooms.size()>=128);
    if(ImGui::Button("部屋候補を追加")) {
        tankexp::GenerationRoomRule rule;
        for(const auto& room:rooms.rooms)if(room.objective!="boss") {rule.roomTemplate=room.id;break;}
        map.generationRooms.push_back(std::move(rule));selected=static_cast<int>(map.generationRooms.size())-1;changed=true;
    }
    ImGui::EndDisabled();ImGui::SameLine();
    ImGui::BeginDisabled(map.generationRooms.empty());
    if(ImGui::Button("選択した候補を削除")) {
        selected=(std::clamp)(selected,0,static_cast<int>(map.generationRooms.size())-1);
        map.generationRooms.erase(map.generationRooms.begin()+selected);changed=true;
    }
    ImGui::EndDisabled();
    if(map.generationRooms.empty())return changed;
    selected=(std::clamp)(selected,0,static_cast<int>(map.generationRooms.size())-1);
    auto& rule=map.generationRooms[static_cast<size_t>(selected)];
    const char* kinds[]={"通常戦闘","精鋭戦闘","最終ボス"};int kind=rule.kind==tankexp::NodeKind::Boss?2:rule.kind==tankexp::NodeKind::Elite?1:0;
    if(ImGui::Combo("配置先の種類",&kind,kinds,3)) {rule.kind=kind==2?tankexp::NodeKind::Boss:kind==1?tankexp::NodeKind::Elite:tankexp::NodeKind::Combat;changed=true;}
    int firstColumn=rule.firstColumn+1,lastColumn=rule.lastColumn+1;
    ImGui::SetNextItemWidth(135);if(ImGui::InputInt("開始列目",&firstColumn)){rule.firstColumn=firstColumn-1;changed=true;}ImGui::SameLine();
    ImGui::SetNextItemWidth(135);if(ImGui::InputInt("終了列目",&lastColumn)){rule.lastColumn=lastColumn-1;changed=true;}ImGui::SameLine();
    ImGui::SetNextItemWidth(135);changed|=ImGui::InputInt("重み",&rule.weight);
    if(ImGui::BeginCombo("部屋 / 敵編成",rule.roomTemplate.c_str())) {
        for(const auto& room:rooms.rooms) {
            if((rule.kind==tankexp::NodeKind::Boss)!=(room.objective=="boss"))continue;
            const auto text=room.name+" / "+room.id+" / 敵"+std::to_string(room.spawns.size())+"体";
            if(ImGui::Selectable(text.c_str(),rule.roomTemplate==room.id)){rule.roomTemplate=room.id;changed=true;}
        }
        ImGui::EndCombo();
    }
    if(const auto* room=tankexp::FindRoom(rooms,rule.roomTemplate)) {
        std::map<std::string,int> counts;for(const auto& spawn:room->spawns)++counts[spawn.type];
        std::string composition;for(const auto& entry:counts)composition+=entry.first+"×"+std::to_string(entry.second)+"  ";
        ImGui::TextWrapped("編成: %s",composition.empty()?"配置なし":composition.c_str());
    }
    unsigned total=0,roomWeight=0;for(const auto& candidate:map.generationRooms)
        if(candidate.kind==rule.kind&&candidate.firstColumn<=rule.firstColumn&&rule.firstColumn<=candidate.lastColumn&&candidate.weight>0) {
            total+=static_cast<unsigned>(candidate.weight);
            if(candidate.roomTemplate==rule.roomTemplate)roomWeight+=static_cast<unsigned>(candidate.weight);
        }
    if(total)ImGui::Text("開始列でこの部屋が選ばれる割合: %.1f%% (同種戦闘が選ばれた場合)",100.0*roomWeight/total);
    return changed;
}
void Preview(tankexp::MapDefinition& map,int& selected) {
    ImGui::TextUnformatted("経路プレビュー / ノードをクリックして編集・横スクロール");
    if(ImGui::BeginChild("MapPreview",{0,215},ImGuiChildFlags_Borders,ImGuiWindowFlags_HorizontalScrollbar)) {
        auto* draw=ImGui::GetWindowDrawList();const auto origin=ImGui::GetCursorScreenPos();
        auto point=[&](const tankexp::MapNode& node) {
            return ImVec2(origin.x+20+static_cast<float>((std::clamp)(node.column,0,31))*130,
                origin.y+30+static_cast<float>((std::clamp)(node.row,0,4))*34);
        };
        int columns=1;
        for(const auto& node:map.nodes) {
            columns=(std::max)(columns,node.column+1);const auto from=point(node);
            for(const auto& id:node.next) if(const auto* target=tankexp::FindMapNode(map,id)) {
                const auto to=point(*target);
                draw->AddLine({from.x+96,from.y+14},{to.x,to.y+14},IM_COL32(45,119,143,180),2);
            }
        }
        for(int column=0;column<columns;++column) {
            const auto text=std::to_string(column+1)+"列目";
            draw->AddText({origin.x+20+column*130.0f,origin.y+4},IM_COL32(146,186,202,255),text.c_str());
        }
        for(std::size_t i=0;i<map.nodes.size();++i) {
            const auto& node=map.nodes[i];const auto p=point(node);const bool active=selected==static_cast<int>(i);
            ImGui::SetCursorScreenPos(p);ImGui::PushID(static_cast<int>(i));
            if(ImGui::InvisibleButton("node",{96,28})) selected=static_cast<int>(i);
            draw->AddRectFilled(p,{p.x+96,p.y+28},active?IM_COL32(32,64,80,255):IM_COL32(12,25,37,255),5);
            draw->AddRect(p,{p.x+96,p.y+28},MapColor(node.kind,active?255:130),5,0,active?2.5f:1.0f);
            const auto& label=node.label;
            draw->PushClipRect(p,{p.x+96,p.y+28},true);
            draw->AddText({p.x+6,p.y+5},IM_COL32(222,244,250,255),label.c_str());draw->PopClipRect();
            if(ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();ImGui::TextUnformatted(node.label.c_str());
                ImGui::Text("ID: %s / %s / %d列目",node.id.c_str(),tankexp::NodeKindId(node.kind),node.column+1);
                if(tankexp::IsCombatNode(node.kind))ImGui::Text("部屋: %s",node.roomTemplate.c_str());
                ImGui::EndTooltip();
            }
            ImGui::PopID();
        }
        ImGui::SetCursorScreenPos(origin);
        ImGui::Dummy({static_cast<float>((std::clamp)(columns,1,32))*130+20,208});
    }
    ImGui::EndChild();
}
}
#endif

bool tankexp::MapEditor::Draw(bool& open,MapDefinition& live,const RoomCatalog& rooms,const std::vector<std::string>* enemyIds) {
#if defined(USE_IMGUI) || defined(USE_RUNTIME_PROFILER)
    if(!open||!ImGui::GetCurrentContext()) return false;
    if(!initialized_) Open(live);
    std::vector<std::string> roomIds;for(const auto& room:rooms.rooms)roomIds.push_back(room.id);
    bool committed=false;
    ImGui::SetNextWindowPos({120,40},ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({1040,650},ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("遠征の経路エディター / F5で再開",&open)) {ImGui::End();return false;}
    ImGui::TextWrapped("戦闘・工房・回復のつながりを編集します。適用・保存した経路は次の遠征開始から反映され、現在の所持金や進行は保持します。");
    if(ImGui::Button("適用 / Apply")) {
        if((!draft_.procedural||RebuildPreview(draft_,selected_,status_))&&ValidateExpeditionMapRooms(draft_,rooms,status_,enemyIds)) {live=draft_;committed=true;status_="適用しました。次の遠征開始から反映します。保存前でも次の遠征に使えます。";}
    }
    ImGui::SameLine();if(ImGui::Button("保存 / Save")) {
        if((!draft_.procedural||RebuildPreview(draft_,selected_,status_))&&ValidateExpeditionMapRooms(draft_,rooms,status_,enemyIds)&&SaveExpeditionMap(kExpeditionMapPath,draft_,status_)) {
            live=draft_;committed=true;status_="経路を保存しました。次の遠征開始から反映します。";
        }
    }
    ImGui::SameLine();if(ImGui::Button("再読込 / Reload")) {
        MapDefinition loaded;
        if(LoadExpeditionMap(kExpeditionMapPath,loaded,status_)&&ValidateExpeditionMapRooms(loaded,rooms,status_,enemyIds)) {
            draft_=loaded;live=loaded;selected_=0;committed=true;status_="保存済みの経路を読込みました。次の遠征開始から反映します。";
        }
    }
    ImGui::SameLine();if(ImGui::Button("標準へ戻す...")) ImGui::OpenPopup("経路を標準へ戻す");
    if(ImGui::BeginPopupModal("経路を標準へ戻す",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("編集中の経路を標準へ戻します。適用・保存は別操作です。");
        if(ImGui::Button("標準へ戻す")) {draft_=DefaultExpeditionMap();selected_=0;ImGui::CloseCurrentPopup();}
        ImGui::SameLine();if(ImGui::Button("キャンセル")) ImGui::CloseCurrentPopup();ImGui::EndPopup();
    }
    ImGui::SameLine();if(ImGui::Button("検証 / Validate")) {
        if(ValidateExpeditionMapRooms(draft_,rooms,status_,enemyIds)) status_="有効な経路です。部屋参照とボス目標の一致を確認しました。どの分岐も最終ボスへ到達します。";
    }
    ImGui::TextWrapped("%s",status_.c_str());
    bool procedural=draft_.procedural;
    if(ImGui::Checkbox("新規遠征ごとにランダム生成",&procedural)) {
        draft_.procedural=procedural;
        if(procedural)RebuildPreview(draft_,selected_,status_);
    }
    if(draft_.procedural) {
        ImGui::TextWrapped("同種3連続を避け、工房と回復を挟んで進みます。以下は確認用シードのプレビューです。チェックを外すと、この経路を固定して編集できます。");
        ImGui::SetNextItemWidth(180);
        if(ImGui::InputScalar("プレビュー用シード",ImGuiDataType_U32,&draft_.generationSeed)) {
            RebuildPreview(draft_,selected_,status_);
        }
        ImGui::SameLine();if(ImGui::Button("別の経路をプレビュー")) {
            draft_.generationSeed+=0x9e3779b9u;RebuildPreview(draft_,selected_,status_);
        }
        if(GenerationRules(draft_,ruleSelected_,rooms))RebuildPreview(draft_,selected_,status_);
        std::string validation;
        if(!ValidateExpeditionMapRooms(draft_,rooms,validation,enemyIds))ImGui::TextColored({1,.45f,.35f,1},"保存できません: %s",validation.c_str());
    }
    ImGui::SetNextItemWidth(180);ImGui::InputInt("開始時の所持金 (CR)",&draft_.startingCurrency);
    Preview(draft_,selected_);
    if(draft_.procedural) {ImGui::End();return committed;}
    if(draft_.nodes.empty()) {ImGui::TextUnformatted("ノードがありません。標準へ戻してください。");ImGui::End();return committed;}
    selected_=(std::clamp)(selected_,0,static_cast<int>(draft_.nodes.size())-1);
    if(ImGui::BeginCombo("編集するノード",draft_.nodes[static_cast<std::size_t>(selected_)].label.c_str())) {
        for(std::size_t i=0;i<draft_.nodes.size();++i) {
            ImGui::PushID(static_cast<int>(i));
            const auto text=draft_.nodes[i].label+" / "+draft_.nodes[i].id;
            if(ImGui::Selectable(text.c_str(),selected_==static_cast<int>(i))) selected_=static_cast<int>(i);
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    int maxColumn=0;for(const auto& node:draft_.nodes) maxColumn=(std::max)(maxColumn,node.column);
    ImGui::BeginDisabled(draft_.nodes.size()>=96||maxColumn>=31);
    if(ImGui::Button("施設を追加")) {
        const auto source=draft_.nodes[static_cast<std::size_t>(selected_)];
        MapNode added;added.id=NewId(draft_);added.label="新しい強化施設";added.kind=NodeKind::Upgrade;
        added.column=source.column+(source.kind==NodeKind::Boss?0:1);added.row=source.row;added.serviceCost=40;
        for(auto& item:draft_.nodes) if(item.column>=added.column) ++item.column;
        if(source.kind==NodeKind::Boss) {
            added.next={source.id};
            for(auto& item:draft_.nodes) for(auto& edge:item.next) if(edge==source.id) edge=added.id;
            for(auto& id:draft_.startNodes) if(id==source.id) id=added.id;
        } else {
            added.next=source.next;draft_.nodes[static_cast<std::size_t>(selected_)].next={added.id};
        }
        draft_.nodes.push_back(std::move(added));selected_=static_cast<int>(draft_.nodes.size())-1;
        status_="経路に施設を追加しました。種類を変えると戦闘などにもできます。";
    }
    ImGui::EndDisabled();ImGui::SameLine();
    const auto source=draft_.nodes[static_cast<std::size_t>(selected_)];int freeRow=-1;
    for(int row=0;row<5;++row) {
        const bool used=std::any_of(draft_.nodes.begin(),draft_.nodes.end(),[&](const MapNode& node){return node.column==source.column&&node.row==row;});
        if(!used) {freeRow=row;break;}
    }
    ImGui::BeginDisabled(draft_.nodes.size()>=96||source.kind==NodeKind::Boss||freeRow<0||
        (source.column==0&&draft_.startNodes.size()>=5));
    if(ImGui::Button("分岐として複製")) {
        auto copied=source;copied.id=NewId(draft_);copied.label+=" コピー";copied.row=freeRow;
        bool allowed=true;
        for(const auto& node:draft_.nodes) if(std::find(node.next.begin(),node.next.end(),source.id)!=node.next.end()&&node.next.size()>=5) allowed=false;
        if(allowed) {
            for(auto& node:draft_.nodes) if(std::find(node.next.begin(),node.next.end(),source.id)!=node.next.end()) node.next.push_back(copied.id);
            if(std::find(draft_.startNodes.begin(),draft_.startNodes.end(),source.id)!=draft_.startNodes.end()) draft_.startNodes.push_back(copied.id);
            draft_.nodes.push_back(std::move(copied));selected_=static_cast<int>(draft_.nodes.size())-1;
            status_="同じ列に分岐を追加しました。種類・部屋・価格を変更できます。";
        } else status_="接続元の分岐が上限の5個です。先に接続を減らしてください。";
    }
    ImGui::EndDisabled();ImGui::SameLine();ImGui::BeginDisabled(draft_.nodes.size()<=1);
    if(ImGui::Button("削除...")) ImGui::OpenPopup("ノード削除の確認");ImGui::EndDisabled();
    if(ImGui::BeginPopupModal("ノード削除の確認",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("このノードと接続線を削除します。削除後に経路をつなぎ直してください。保存前なら再読込で戻せます。");
        if(ImGui::Button("このノードを削除")) {
            const auto id=draft_.nodes[static_cast<std::size_t>(selected_)].id;
            draft_.nodes.erase(draft_.nodes.begin()+selected_);
            for(auto& node:draft_.nodes) node.next.erase(std::remove(node.next.begin(),node.next.end(),id),node.next.end());
            draft_.startNodes.erase(std::remove(draft_.startNodes.begin(),draft_.startNodes.end(),id),draft_.startNodes.end());
            selected_=(std::max)(0,selected_-1);status_="削除しました。適用前に接続を確認してください。";ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();if(ImGui::Button("キャンセル")) ImGui::CloseCurrentPopup();ImGui::EndPopup();
    }
    auto& node=draft_.nodes[static_cast<std::size_t>(selected_)];
    if(ImGui::BeginTable("NodeProperties",2,ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableNextColumn();
        const auto oldId=node.id;TextField("ID (半角英数・_・-)",node.id);
        if(node.id!=oldId) {
            for(auto& item:draft_.nodes) for(auto& edge:item.next) if(edge==oldId) edge=node.id;
            for(auto& id:draft_.startNodes) if(id==oldId) id=node.id;
        }
        TextField("表示名",node.label);
        int kind=static_cast<int>(node.kind);
        if(ImGui::Combo("種類",&kind,kNodeNames.data(),static_cast<int>(kNodeNames.size()))) {
            node.kind=static_cast<NodeKind>(kind);
            node.role=NodeRole::None;
            if(IsCombatNode(node.kind)) {
                node.combatStage=(std::max)(0,node.combatStage);node.serviceCost=0;
                if(node.roomTemplate.empty()&&!roomIds.empty()) node.roomTemplate=roomIds.front();
                if(node.clearReward==0) node.clearReward=60;
            } else {node.combatStage=-1;node.roomTemplate.clear();node.clearReward=0;if(node.kind==NodeKind::Currency) node.serviceCost=0;}
        }
        int role=static_cast<int>(node.role);
        if(ImGui::Combo("導入の役割",&role,kRoleNames.data(),static_cast<int>(kRoleNames.size()))) node.role=static_cast<NodeRole>(role);
        int displayColumn=node.column+1;if(ImGui::InputInt("左から何列目 (1〜32)",&displayColumn))node.column=displayColumn-1;
        ImGui::InputInt("段 (0〜4)",&node.row);
        bool start=std::find(draft_.startNodes.begin(),draft_.startNodes.end(),node.id)!=draft_.startNodes.end();
        if(ImGui::Checkbox("開始地点にする (1列目)",&start)) {
            if(start) draft_.startNodes.push_back(node.id);
            else draft_.startNodes.erase(std::remove(draft_.startNodes.begin(),draft_.startNodes.end(),node.id),draft_.startNodes.end());
        }
        if(IsCombatNode(node.kind)) {
            if(ImGui::BeginCombo("使用する部屋",node.roomTemplate.c_str())) {
                for(const auto& room:rooms.rooms) {
                    if((node.kind==NodeKind::Boss)!=(room.objective=="boss"))continue;
                    const auto text=room.name+" / "+room.id;
                    if(ImGui::Selectable(text.c_str(),room.id==node.roomTemplate))node.roomTemplate=room.id;
                }
                ImGui::EndCombo();
            }
            ImGui::InputInt("戦闘の段階 (0始まり)",&node.combatStage);ImGui::InputInt("クリア報酬 (CR)",&node.clearReward);
            ImGui::TextWrapped("敵・ブロック・達成目標は、部屋エディターでこの部屋を編集します。");
        } else if(node.kind==NodeKind::Currency) {
            ImGui::InputInt("支給する資材 (CR)",&node.clearReward);
            ImGui::TextWrapped("このノードを完了すると一度だけ資材を受け取ります。購入価格はありません。");
        } else {
            ImGui::InputInt(node.kind==NodeKind::Upgrade?"最低購入価格 (CR)":"施設の価格 (CR)",&node.serviceCost);
            ImGui::TextWrapped("強化は種類の価格と最低購入価格の高い方を使用。施設は利用せず通過できます。0なら無料です。");
        }
        ImGui::TableNextColumn();ImGui::TextUnformatted("次に進めるノード (最大5個)");
        ImGui::TextWrapped("接続は右の列へ進みます。最終ボス以外は、次のノードを1つ以上選んでください。");
        if(ImGui::BeginChild("Connections",{0,230},ImGuiChildFlags_Borders)) {
            for(const auto& target:draft_.nodes) {
                if(target.id==node.id) continue;
                auto it=std::find(node.next.begin(),node.next.end(),target.id);bool connected=it!=node.next.end();
                const bool allowed=target.column>node.column&&node.next.size()<5&&node.kind!=NodeKind::Boss;
                ImGui::BeginDisabled(!connected&&!allowed);ImGui::PushID(target.id.c_str());
                const auto label=target.label+" ["+std::to_string(target.column+1)+"列目]";
                if(ImGui::Checkbox(label.c_str(),&connected)) {
                    if(connected) node.next.push_back(target.id);else node.next.erase(it);
                }
                ImGui::PopID();ImGui::EndDisabled();
            }
        }
        ImGui::EndChild();ImGui::EndTable();
    }
    ImGui::End();return committed;
#else
    (void)open;(void)live;(void)rooms;(void)enemyIds;return false;
#endif
}
