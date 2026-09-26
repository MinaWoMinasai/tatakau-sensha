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
bool tankcontent::ContentEditor::Draw(bool& open,Catalog& live,const std::vector<std::string>& usedEnemyIds){
#if defined(USE_IMGUI) || defined(USE_RUNTIME_PROFILER)
    if(!open||!ImGui::GetCurrentContext())return false;
    bool changed=false;
    auto preserveTraining=[&](Catalog& candidate) {
        for(const char* id:{"tutorial_target","tutorial_shooter","tutorial_retry"})
            if(!FindEnemy(candidate,id))if(const auto* builtin=FindEnemy(live,id))candidate.enemies.push_back(*builtin);
    };
    auto valid=[&](Catalog& candidate) {
        // Deleting or renaming a player removes its generated workshop card.
        // Hand-authored references still fail validation instead of disappearing.
        std::erase_if(candidate.upgrades,[&](const Upgrade& u){return u.id.rfind("Refit_",0)==0&&!u.refitPlayer.empty()&&!FindPlayer(candidate,u.refitPlayer);});
        EnsureRefitCards(candidate);
        if(!ValidateCatalog(candidate,status_))return false;
        for(const char* id:{"tutorial_target","tutorial_shooter","tutorial_retry"})
            if(FindEnemy(live,id)&&!FindEnemy(candidate,id)){status_=std::string("操作訓練に必要な予約IDは削除できません: ")+id;return false;}
        for(const auto& id:usedEnemyIds)if(!FindEnemy(candidate,id)){status_="配置で使用中の敵IDを削除・変更できません: "+id+"（先にF4で配置を変更）";return false;}
        return true;
    };
    ImGui::SetNextWindowPos({165,55},ImGuiCond_FirstUseEver);ImGui::SetNextWindowSize({930,610},ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("遠征の種類エディター / F6で再開",&open)){ImGui::End();return false;}
    ImGui::TextWrapped("新規作成・複製で種類を増やせます。適用後、工房や敵配置に登場します。保存すると次回も使用します。");
    if(ImGui::Button("適用 / Apply")){if(valid(draft_)){live=draft_;changed=true;status_="適用しました。敵は次回の部屋開始から反映。機体は換装カードとして工房に出ます。";}}
    ImGui::SameLine();if(ImGui::Button("保存 / Save")){if(valid(draft_)&&SaveCatalog(kCatalogPath,draft_,status_)){live=draft_;changed=true;status_="保存・適用しました。";}}
    ImGui::SameLine();if(ImGui::Button("再読込 / Reload")){Catalog loaded;if(LoadCatalog(kCatalogPath,loaded,status_)){preserveTraining(loaded);if(valid(loaded)){draft_=loaded;live=loaded;changed=true;status_="保存済みデータに戻しました。";}}}
    ImGui::SameLine();if(ImGui::Button("標準へ戻す..."))ImGui::OpenPopup("標準データの確認");
    if(ImGui::BeginPopupModal("標準データの確認",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        ImGui::TextUnformatted("編集中のデータを標準へ戻します。適用・保存は別操作です。");
        if(ImGui::Button("標準へ戻す")){draft_=DefaultCatalog();preserveTraining(draft_);ImGui::CloseCurrentPopup();}ImGui::SameLine();if(ImGui::Button("キャンセル"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
    }
    ImGui::TextWrapped("%s",status_.c_str());ImGui::Separator();
    if(ImGui::BeginTabBar("種類")){
        if(ImGui::BeginTabItem("強化")){
            SelectDefinition(draft_.upgrades,selectedUpgrade_,"Upgrade_");auto& u=draft_.upgrades[static_cast<std::size_t>(selectedUpgrade_)];TextField("説明",u.description,true);ImGui::InputInt("価格 (CR)",&u.price);ImGui::Combo("レア度",&u.rarity,tankbuild::RarityNames.data(),static_cast<int>(tankbuild::RarityNames.size()));
            const bool refit=!u.refitPlayer.empty();
            if(refit)ImGui::TextWrapped("機体換装: %s / 同系統のみ、1ラン1回。性能は自機・換装タブで編集します。",u.refitPlayer.c_str());
            ImGui::BeginDisabled(refit);
            ImGui::TextUnformatted("提示する系統（複数選択可。全て選択すると汎用枠）");
            for(int i=0;i<3;++i) {const auto style=static_cast<tankbuild::Style>(i);bool selected=(u.compatibleStyles&tankbuild::Mask(style))!=0;if(ImGui::Checkbox(tankbuild::Name(style),&selected)) {if(selected)u.compatibleStyles|=tankbuild::Mask(style);else u.compatibleStyles&=~tankbuild::Mask(style);}if(i<2)ImGui::SameLine();}
            ImGui::TextUnformatted("効果を1〜4個組み合わせます。各効果の重複取得は不可。購入上限: 1回");
            int visibleEffect=0;
            for(std::size_t i=0;i<kEffectIds.size();++i){const auto effect=static_cast<tankrun::CardId>(i);if(!tankrun::IsAvailableCard(effect))continue;auto it=std::find(u.effects.begin(),u.effects.end(),effect);bool enabled=it!=u.effects.end();ImGui::PushID(static_cast<int>(i));if(ImGui::Checkbox(kEffectNames[i],&enabled)){if(enabled&&u.effects.size()<4)u.effects.push_back(effect);else if(!enabled)u.effects.erase(it);}ImGui::PopID();if(visibleEffect%3!=2)ImGui::SameLine(210.0f+static_cast<float>(visibleEffect%3)*200.0f);++visibleEffect;}
            if(visibleEffect%3)ImGui::NewLine();
            ImGui::SeparatorText("選択した効果の強さ");
            ImGui::TextWrapped("1.00 = 標準効果。追加量・回数・持続時間に掛かります。攻撃力等の標準追加量はF2で編集。説明文は自動変更しないため、数値変更後は説明も更新してください。");
            for(const auto effect:u.effects) {
                const auto index=static_cast<std::size_t>(effect);ImGui::PushID(static_cast<int>(index));
                ImGui::SliderFloat(kEffectNames[index],&u.effectPower[index],0.1f,5.0f,"%.2f 倍");ImGui::PopID();
            }
            ImGui::TextWrapped("反射・貫通・追加機数・耐久は整数に丸めます（最低1）。短縮係数は最低5%%、ドローン総数は12機まで。効果の種類変更は次の購入から、購入済み商品の倍率変更は適用時に反映します。");
            const unsigned usable=u.compatibleStyles&CompatibleEffectStyles(u);
            if(!usable)ImGui::TextColored({1,0.45f,0.3f,1},"全効果が働く系統がありません。この組合せは工房に提示されません。");
            else {std::string shown="全効果が働く系統: ";for(int i=0;i<3;++i)if(usable&tankbuild::Mask(static_cast<tankbuild::Style>(i)))shown+=std::string(tankbuild::Name(static_cast<tankbuild::Style>(i)))+" ";ImGui::TextWrapped("%s",shown.c_str());}
            ImGui::TextWrapped("近接の基本ブレードは系統選択で取得。旧ネオンブレード効果を含む商品は工房に提示されません。蓄電池にはジャスト回避が必要です。複合商品は全効果が未所持のときだけ提示されます。");
            ImGui::EndDisabled();ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("敵")){
            SelectDefinition(draft_.enemies,selectedEnemy_,"Enemy_");auto& e=draft_.enemies[static_cast<std::size_t>(selectedEnemy_)];int behavior=static_cast<int>(e.behavior);ImGui::Combo("行動型",&behavior,kBehaviorNames.data(),static_cast<int>(kBehaviorNames.size()));e.behavior=static_cast<EnemyBehavior>(behavior);
            ImGui::InputInt("HP",&e.hp);ImGui::InputInt("接触ダメージ",&e.contactDamage);ImGui::InputInt("射撃ダメージ",&e.bulletDamage);ImGui::InputInt("撃破報酬 (CR)",&e.creditDrop);
            ImGui::SliderFloat("移動速度倍率 (移動する敵)",&e.moveSpeedScale,0.1f,3,"%.2f");ImGui::SliderFloat("攻撃間隔倍率 (小さいほど速い)",&e.fireIntervalScale,0.3f,4,"%.2f");ImGui::ColorEdit4("ネオン色",e.color.data(),ImGuiColorEditFlags_Float|ImGuiColorEditFlags_HDR);
            if(e.behavior>=EnemyBehavior::Sniper&&e.behavior<=EnemyBehavior::Suppressor){ImGui::SliderInt("弾倉の発射回数 (0=標準)",&e.magazineSize,0,8);ImGui::SliderFloat("装填秒数 (0=標準、0.8以上)",&e.reloadSeconds,0,8,"%.2f");}
            ImGui::TextWrapped("機動射撃兵: 横移動とダッシュ。接近散弾兵: 接近して散弾、離脱。制圧射撃兵: 扇状連射、長い装填。水色ゲージは装填中の隙です。");
            ImGui::TextWrapped("シールド兵: 正面を軽減、側背面が弱点。ブレード兵: 扇形予告の後に薙ぎ払い、攻撃後が隙。接触ダメージが近接威力の基準です。");
            ImGui::TextWrapped("新しいIDを部屋エディターの敵パレットから配置できます。四角・三角・五角は動かない資源です。");ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("自機・換装")){
            SelectDefinition(draft_.players,selectedPlayer_,"Player_");auto& p=draft_.players[static_cast<std::size_t>(selectedPlayer_)];TextField("説明",p.description,true);ImGui::InputInt("換装価格の初期値 (CR)",&p.price);
            int style=static_cast<int>(p.style);if(ImGui::Combo("換装の系統",&style,tankbuild::Names.data(),static_cast<int>(tankbuild::Names.size()))) {p.style=static_cast<tankbuild::Style>(style);if(p.style==tankbuild::Style::Drone&&p.drones<1)p.drones=3;if(p.style==tankbuild::Style::Melee){p.drones=0;p.reflect=false;p.penetrate=false;}}
            ImGui::Combo("換装レア度の初期値 (エピック以上)",&p.rarity,tankbuild::RarityNames.data(),static_cast<int>(tankbuild::RarityNames.size()));
            ImGui::TextUnformatted("工房に同じ系統の換装カードとして登場。1ラン1回。カードの価格・レア度は強化タブで編集。");
            const char* bases[]={"Basic","Twin","MachineGun","Overseer"};int base=0;for(int i=0;i<4;++i)if(p.baseClass==bases[i])base=i;if(ImGui::Combo("基本型",&base,bases,4))p.baseClass=bases[base];
            p.bulletCount=1;
            ImGui::SliderFloat("攻撃力倍率",&p.damageScale,0.2f,4);ImGui::SliderFloat(p.style==tankbuild::Style::Melee?"近接の動作時間倍率":"発射間隔倍率",&p.reloadScale,0.25f,4);
            if(p.style!=tankbuild::Style::Melee) {
                ImGui::SliderInt("砲門数",&p.barrels,1,6);ImGui::TextUnformatted("1砲門につき1回の発射は1発です。");ImGui::SliderFloat("砲門の開き (度)",&p.fanAngle,0,45);ImGui::SliderFloat("弾速倍率",&p.bulletSpeedScale,0.3f,3);ImGui::SliderInt("ドローン数",&p.drones,p.style==tankbuild::Style::Drone?1:0,12);
                ImGui::Checkbox("交互射撃",&p.alternate);ImGui::SameLine();ImGui::Checkbox("壁反射",&p.reflect);ImGui::SameLine();ImGui::Checkbox("貫通",&p.penetrate);
            }
            if(p.style==tankbuild::Style::Drone)ImGui::TextWrapped("ドローン数は基準3機からの差分です。実際 = F2の基本数 + この値 - 3 + 支援強化（1〜12機）。");
            ImGui::Combo("機体形状",&p.bodyShape,"円\0四角\0三角\0五角\0");ImGui::ColorEdit4("機体のネオン色",p.color.data(),ImGuiColorEditFlags_Float|ImGuiColorEditFlags_HDR);ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();return changed;
#else
    (void)open;(void)live;(void)usedEnemyIds;return false;
#endif
}
