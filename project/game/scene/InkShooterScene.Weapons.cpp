#include "InkShooterScene.h"
#include <algorithm>
#include <cstring>
#include <set>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void InkShooterScene::InitializeWeapons() {
    weaponCatalog_=ink::WeaponCatalog::Defaults();
    std::string error;
    if(!weaponCatalog_.Load(weaponCatalogPath_,error))
        weaponStatus_="ブキファイルを読めませんでした。内蔵の基準値で起動します。\n"+error;
    else weaponStatus_="ブキを読み込みました。";
    selectedWeaponId_=weaponCatalog_.DefaultId();
    weaponDraft_=*weaponCatalog_.Find(selectedWeaponId_);
    simulation_.Equip(weaponDraft_);
}

bool InkShooterScene::CommitWeaponDraft() {
    if(!weaponDraftDirty_) return true;
    std::string error;
    if(!weaponCatalog_.Upsert(weaponDraft_,error)) { weaponStatus_=error; return false; }
    weaponDraftDirty_=false; weaponCatalogDirty_=true;
    return true;
}

bool InkShooterScene::SelectWeaponDraft(const std::string& id) {
    if(!CommitWeaponDraft()) return false;
    const auto* entry=weaponCatalog_.Find(id);
    if(!entry) { weaponStatus_="選択したブキが見つかりません。"; return false; }
    selectedWeaponId_=id; weaponDraft_=*entry; weaponDraftDirty_=false;
    return true;
}

void InkShooterScene::EquipCatalogWeapon(const std::string& id) {
    if(!CommitWeaponDraft()) return;
    if(const auto* entry=weaponCatalog_.Find(id)) {
        simulation_.Equip(*entry);
        selectedWeaponId_=id; weaponDraft_=*entry;
        weaponStatus_="試し撃ちに反映："+entry->displayNameJa;
    }
}

void InkShooterScene::CycleWeapon() {
    if(!CommitWeaponDraft()) return;
    const auto& entries=weaponCatalog_.Entries();
    if(entries.empty()) return;
    size_t next=0;
    for(size_t n=0;n<entries.size();++n) if(entries[n].id==simulation_.ActiveWeaponId()) { next=(n+1)%entries.size(); break; }
    const auto id=entries[next].id; EquipCatalogWeapon(id);
}

void InkShooterScene::DrawWeaponEditor() {
#ifdef USE_IMGUI
    if(!ImGui::CollapsingHeader("ブキを選ぶ・作る###WeaponEditor",ImGuiTreeNodeFlags_DefaultOpen)) return;
    ImGui::Text("使用中：%s",simulation_.ActiveWeaponName().c_str());
    ImGui::TextWrapped("1：シューター　2：トライストリンガー\nQ：次のブキ。左クリック長押しでチャージ、離すと発射。Shiftでチャージを中断。");
    if(ImGui::BeginCombo("編集中のブキ###WeaponDraft",weaponDraft_.displayNameJa.c_str())) {
        std::string requested;
        for(const auto& entry:weaponCatalog_.Entries()) {
            if(ImGui::Selectable((entry.displayNameJa+"##"+entry.id).c_str(),entry.id==selectedWeaponId_)) requested=entry.id;
        }
        ImGui::EndCombo();
        // Mutations happen after the combo's iteration, so vector references
        // cannot be invalidated by storing the previous draft.
        if(!requested.empty()) SelectWeaponDraft(requested);
    }
    if(ImGui::Button("このブキで試し撃ち###ApplyWeapon",{-1,0})) {
        if(CommitWeaponDraft()) EquipCatalogWeapon(selectedWeaponId_);
    }
    if(ImGui::Button("複製して新しいブキを作る###CloneWeapon",{-1,0})) {
        if(CommitWeaponDraft()) {
            std::string error;
            const auto id=weaponCatalog_.Clone(selectedWeaponId_,weaponDraft_.displayNameJa+" のコピー",error);
            if(id.empty()) weaponStatus_=error;
            else { weaponCatalogDirty_=true; SelectWeaponDraft(id); weaponStatus_="複製しました。名前と数値を編集できます。"; }
        }
    }
    // Catalog names allow 240 UTF-8 bytes; retain the complete valid name.
    char name[256]{};
    std::memcpy(name,weaponDraft_.displayNameJa.data(),(std::min)(weaponDraft_.displayNameJa.size(),sizeof(name)-1));
    if(ImGui::InputText("名前###WeaponName",name,sizeof(name))) { weaponDraft_.displayNameJa=name; weaponDraftDirty_=true; }
    ImGui::Text("種類：%s",weaponDraft_.type==ink::WeaponClass::Shooter?"シューター":"ストリンガー");
    ImGui::TextDisabled("ID：%s",selectedWeaponId_.c_str());
    ImGui::TextWrapped("数値変更は下書きです。「試し撃ち」で反映し、「保存」で次回起動にも残します。ブキを選び直しても、有効な下書きはこの一覧に保持します。");

    auto fields=[&](auto& params,const auto& floats,const auto& ints,const auto& bools) {
        std::set<std::string> groups;
        for(const auto& field:floats) groups.insert(field.group);
        for(const auto& field:ints) groups.insert(field.group);
        for(const auto& field:bools) groups.insert(field.group);
        for(const auto& group:groups) {
            if(!ImGui::TreeNode(group.c_str())) continue;
            const bool reference=group=="参考";
            if(reference) ImGui::TextWrapped("公開データの参照値です。実際の調整に使う項目は他の分類にあります。");
            ImGui::BeginDisabled(reference);
            for(const auto& field:floats) if(group==field.group) {
                ImGui::PushID(field.key);
                const std::string label=std::string(field.label)+"###Value";
                ImGui::SetNextItemWidth((std::max)(90.0f,ImGui::GetContentRegionAvail().x*0.43f));
                if(ImGui::InputFloat(label.c_str(),&(params.*field.member),0,0,"%.4f")) weaponDraftDirty_=true;
                if(ImGui::BeginItemTooltip()) {
                    ImGui::Text("単位：%s / 範囲 %.4f ～ %.4f",field.unit,field.min,field.max);
                    ImGui::EndTooltip();
                }
                ImGui::PopID();
            }
            for(const auto& field:ints) if(group==field.group) {
                ImGui::PushID(field.key);
                ImGui::SetNextItemWidth((std::max)(90.0f,ImGui::GetContentRegionAvail().x*0.43f));
                if(ImGui::InputInt(field.label,&(params.*field.member))) weaponDraftDirty_=true;
                if(ImGui::BeginItemTooltip()) { ImGui::Text("範囲 %d ～ %d %s",field.min,field.max,field.unit); ImGui::EndTooltip(); }
                ImGui::PopID();
            }
            for(const auto& field:bools) if(group==field.group) {
                ImGui::PushID(field.key); ImGui::BeginDisabled(!field.editable);
                if(ImGui::Checkbox(field.label,&(params.*field.member))) weaponDraftDirty_=true;
                ImGui::EndDisabled(); ImGui::PopID();
            }
            ImGui::EndDisabled(); ImGui::TreePop();
        }
    };
    if(ImGui::TreeNode("ブキの数値を編集###WeaponParameters")) {
        ImGui::TextWrapped("1F＝1/60秒、距離はCG2の単位です。各項目にマウスを重ねると、秒・Fなどの単位と範囲を表示します。");
        if(weaponDraft_.type==ink::WeaponClass::Shooter)
            fields(weaponDraft_.shooter,ink::ShooterFloatFields(),ink::ShooterIntFields(),ink::ShooterBoolFields());
        else fields(weaponDraft_.stringer,ink::StringerFloatFields(),ink::StringerIntFields(),ink::StringerBoolFields());
        ImGui::TreePop();
    }
    if(ImGui::Button("ブキ一覧をファイルに保存###SaveWeapons",{-1,0})) {
        if(CommitWeaponDraft()) {
            std::string error;
            if(weaponCatalog_.Save(weaponCatalogPath_,error)) { weaponCatalogDirty_=false; weaponStatus_="保存しました。次回起動にも反映されます。"; }
            else weaponStatus_=error;
        }
    }
    if(ImGui::Button("未保存の変更を戻して再読込###ReloadWeapons",{-1,0})) {
        std::string error;
        if(weaponCatalog_.Load(weaponCatalogPath_,error)) {
            weaponDraftDirty_=false; weaponCatalogDirty_=false;
            if(!weaponCatalog_.Find(selectedWeaponId_)) selectedWeaponId_=weaponCatalog_.DefaultId();
            weaponDraft_=*weaponCatalog_.Find(selectedWeaponId_);
            weaponStatus_="読み込みました。使用中のブキは「試し撃ち」で更新できます。";
        } else weaponStatus_=error;
    }
    ImGui::Text("保存状態：%s",weaponDraftDirty_||weaponCatalogDirty_?"未保存の変更あり":"保存済み");
    if(!weaponStatus_.empty()) ImGui::TextWrapped("%s",weaponStatus_.c_str());
    ImGui::TextDisabled("保存先：resources/configs/ink_weapons.json");
    ImGui::Separator();
#endif
}
