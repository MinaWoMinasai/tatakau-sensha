#include "ExpeditionContentEditor.h"
#if defined(USE_IMGUI) || defined(USE_RUNTIME_PROFILER)
#include "externals/imgui/imgui.h"
#include <cstdio>
namespace {
void TextField(const char* label,std::string& value,bool multi=false){
    std::array<char,1024> buffer{};std::snprintf(buffer.data(),buffer.size(),"%s",value.c_str());
    const bool changed=multi?ImGui::InputTextMultiline(label,buffer.data(),buffer.size(),{430,65}):ImGui::InputText(label,buffer.data(),buffer.size());
    if(changed)value=buffer.data();
}
template<class T> void SelectDefinition(std::vector<T>& items,int& selected,const char* prefix){
    selected=(std::clamp)(selected,0,static_cast<int>(items.size())-1);
    if(ImGui::BeginCombo("編集する種類",items[static_cast<std::size_t>(selected)].name.c_str())){
        for(std::size_t i=0;i<items.size();++i){ImGui::PushID(static_cast<int>(i));if(ImGui::Selectable(items[i].name.c_str(),selected==static_cast<int>(i)))selected=static_cast<int>(i);ImGui::PopID();}ImGui::EndCombo();
    }
    auto unique=[&](){for(int index=1;;++index){const auto id=std::string(prefix)+std::to_string(index);if(!tankcontent::FindDefinition(items,id))return id;}};
    ImGui::BeginDisabled(items.size()>=tankcontent::kMaxDefinitions);
    if(ImGui::Button("新規作成")){T value;value.id=unique();value.name="新しい種類";items.push_back(value);selected=static_cast<int>(items.size())-1;}
    ImGui::SameLine();
    if(ImGui::Button("選択を複製")){T value=items[static_cast<std::size_t>(selected)];value.id=unique();value.name+=" コピー";items.push_back(value);selected=static_cast<int>(items.size())-1;}
    ImGui::EndDisabled();ImGui::SameLine();ImGui::BeginDisabled(items.size()<=1);
    if(ImGui::Button("削除..."))ImGui::OpenPopup("削除の確認");ImGui::EndDisabled();
    if(ImGui::BeginPopupModal("削除の確認",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::TextWrapped("この種類を削除します。配置済みの敵IDは部屋エディターで変更してください。保存前なら再読込で戻せます。");
        if(ImGui::Button("この種類を削除")){items.erase(items.begin()+selected);selected=(std::max)(0,selected-1);ImGui::CloseCurrentPopup();}ImGui::SameLine();if(ImGui::Button("キャンセル"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
    }
    auto& item=items[static_cast<std::size_t>(selected)];TextField("ID (半角英数・_・-)",item.id);TextField("表示名",item.name);
}
}
#endif
bool tankcontent::ContentEditor::Draw(bool& open,Catalog& live){
#if defined(USE_IMGUI) || defined(USE_RUNTIME_PROFILER)
    if(!open||!ImGui::GetCurrentContext())return false;
    bool changed=false;
    ImGui::SetNextWindowPos({165,55},ImGuiCond_FirstUseEver);ImGui::SetNextWindowSize({930,610},ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("遠征の種類エディター / F6で再開",&open)){ImGui::End();return false;}
    ImGui::TextWrapped("新規作成・複製で種類を増やせます。適用後、強化・進化の施設や敵配置に登場します。保存すると次回も使用します。");
    if(ImGui::Button("適用 / Apply")){if(ValidateCatalog(draft_,status_)){live=draft_;changed=true;status_="適用しました。敵は次回の部屋開始から反映。機体は進化施設で選べます。";}}
    ImGui::SameLine();if(ImGui::Button("保存 / Save")){if(SaveCatalog(kCatalogPath,draft_,status_)){live=draft_;changed=true;status_="保存・適用しました。";}}
    ImGui::SameLine();if(ImGui::Button("再読込 / Reload")){Catalog loaded;if(LoadCatalog(kCatalogPath,loaded,status_)){draft_=loaded;live=loaded;changed=true;status_="保存済みデータに戻しました。";}}
    ImGui::SameLine();if(ImGui::Button("標準へ戻す..."))ImGui::OpenPopup("標準データの確認");
    if(ImGui::BeginPopupModal("標準データの確認",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::TextUnformatted("編集中のデータを標準へ戻します。適用・保存は別操作です。");
        if(ImGui::Button("標準へ戻す")){draft_=DefaultCatalog();ImGui::CloseCurrentPopup();}ImGui::SameLine();if(ImGui::Button("キャンセル"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
    }
    ImGui::TextWrapped("%s",status_.c_str());ImGui::Separator();
    if(ImGui::BeginTabBar("種類")){
        if(ImGui::BeginTabItem("強化")){
            SelectDefinition(draft_.upgrades,selectedUpgrade_,"Upgrade_");auto& u=draft_.upgrades[static_cast<std::size_t>(selectedUpgrade_)];TextField("説明",u.description,true);ImGui::InputInt("価格 (CR)",&u.price);ImGui::Combo("レア度",&u.rarity,"通常\0レア\0特別\0");
            ImGui::TextUnformatted("効果を1〜4個組み合わせます。各効果の重複取得は不可。購入上限: 1回");
            for(std::size_t i=0;i<kEffectIds.size();++i){const auto effect=static_cast<tankrun::CardId>(i);auto it=std::find(u.effects.begin(),u.effects.end(),effect);bool enabled=it!=u.effects.end();ImGui::PushID(static_cast<int>(i));if(ImGui::Checkbox(kEffectNames[i],&enabled)){if(enabled&&u.effects.size()<4)u.effects.push_back(effect);else if(!enabled)u.effects.erase(it);}ImGui::PopID();if(i%3!=2)ImGui::SameLine(210.0f+static_cast<float>(i%3)*200.0f);}
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("敵")){
            SelectDefinition(draft_.enemies,selectedEnemy_,"Enemy_");auto& e=draft_.enemies[static_cast<std::size_t>(selectedEnemy_)];int behavior=static_cast<int>(e.behavior);ImGui::Combo("行動型",&behavior,"四角資源\0三角資源\0五角資源\0射撃砲台\0突進兵\0狙撃兵\0");e.behavior=static_cast<EnemyBehavior>(behavior);
            ImGui::InputInt("HP",&e.hp);ImGui::InputInt("接触ダメージ",&e.contactDamage);ImGui::InputInt("射撃ダメージ",&e.bulletDamage);ImGui::InputInt("撃破報酬 (CR)",&e.creditDrop);
            ImGui::SliderFloat("移動速度倍率 (突進兵)",&e.moveSpeedScale,0.1f,3,"%.2f");ImGui::SliderFloat("攻撃間隔倍率 (小さいほど速い)",&e.fireIntervalScale,0.3f,4,"%.2f");ImGui::ColorEdit4("ネオン色",e.color.data(),ImGuiColorEditFlags_Float|ImGuiColorEditFlags_HDR);
            ImGui::TextWrapped("新しいIDを部屋エディターの敵パレットから配置できます。四角・三角・五角は動かない資源です。");ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("自機・進化")){
            SelectDefinition(draft_.players,selectedPlayer_,"Player_");auto& p=draft_.players[static_cast<std::size_t>(selectedPlayer_)];TextField("説明",p.description,true);ImGui::InputInt("進化価格 (CR)",&p.price);
            const char* bases[]={"Basic","Twin","MachineGun","Overseer"};int base=0;for(int i=0;i<4;++i)if(p.baseClass==bases[i])base=i;if(ImGui::Combo("基本型",&base,bases,4))p.baseClass=bases[base];
            ImGui::SliderInt("砲門数",&p.barrels,1,6);ImGui::SliderInt("砲門ごとの弾数",&p.bulletCount,1,5);ImGui::SliderFloat("砲門の開き (度)",&p.fanAngle,0,45);ImGui::SliderFloat("威力倍率",&p.damageScale,0.2f,4);ImGui::SliderFloat("発射間隔倍率",&p.reloadScale,0.25f,4);ImGui::SliderFloat("弾速倍率",&p.bulletSpeedScale,0.3f,3);ImGui::SliderInt("支援ドローン数",&p.drones,0,7);
            ImGui::Checkbox("交互射撃",&p.alternate);ImGui::SameLine();ImGui::Checkbox("壁反射",&p.reflect);ImGui::SameLine();ImGui::Checkbox("貫通",&p.penetrate);ImGui::Combo("機体形状",&p.bodyShape,"円\0四角\0三角\0五角\0");ImGui::ColorEdit4("機体のネオン色",p.color.data(),ImGuiColorEditFlags_Float|ImGuiColorEditFlags_HDR);ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();return changed;
#else
    (void)open;(void)live;return false;
#endif
}
