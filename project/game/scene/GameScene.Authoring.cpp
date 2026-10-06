#include "GameScene.h"
#include <fstream>
#if defined(USE_IMGUI) || defined(USE_RUNTIME_PROFILER)
#include "externals/imgui/imgui.h"
#endif

namespace {
/// @brief 制作データJsonを保存する。
bool SaveAuthoringJson(const char* path,const nlohmann::json& data,std::string& error) {
    try {
        const std::filesystem::path target(path),temporary(std::string(path)+".tmp");
        {std::ofstream out(temporary,std::ios::binary|std::ios::trunc);out<<data.dump(2)<<'\n';out.close();if(!out)throw std::runtime_error("書き込み失敗");}
        if(!MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("保存先の置換失敗");
        return true;
    }catch(const std::exception& ex){error=std::string(path)+": "+ex.what();return false;}
}
}

void GameScene::UpdateExpeditionAuthoringHub() {
#if defined(USE_IMGUI) || defined(USE_RUNTIME_PROFILER)
    if(!ImGui::GetCurrentContext()||expeditionTransition_.IsActive())return;
    if(input_->IsKeyTriggered(DIK_F3)) {
        expeditionAuthoringHubOpen_=!expeditionAuthoringHubOpen_;
        if(expeditionAuthoringHubOpen_) {expeditionPostDraft_=BuildGamePostEffectConfig();expeditionVisualDraft_=BuildGameVisualConfig();}
    }
    if(!expeditionAuthoringHubOpen_)return;
    ImGui::SetNextWindowPos({115,65},ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({1010,590},ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("制作ツール / F3で開閉",&expeditionAuthoringHubOpen_)){ImGui::End();return;}
    ImGui::TextUnformatted("戦闘は一時停止中。調整 → 適用 → エディターを閉じて試遊 → 保存。");
    if(ImGui::Button("F2 基本性能・難易度")) {
        tankExpeditionBalanceEditorOpen_=true;expeditionAuthoringHubOpen_=false;
        LoadBalanceEditorFromJson(tankExpeditionBalance_);expeditionStyleBalanceDraft_=tankExpeditionBalance_["combatStyles"];
        balanceEditor_.bossMaxHp=tankExpeditionBalance_["bossMaxHp"].get<int>();balanceEditor_.healToFull=false;
    }
    ImGui::SameLine();if(ImGui::Button("F4 部屋・敵配置")){expeditionRoomEditorOpen_=true;expeditionAuthoringHubOpen_=false;}
    ImGui::SameLine();if(ImGui::Button("F5 出現時期・ルート")){expeditionMapEditorOpen_=true;expeditionAuthoringHubOpen_=false;}
    ImGui::SameLine();if(ImGui::Button("F6 強化・進化・敵の種類")){expeditionContentEditorOpen_=true;expeditionAuthoringHubOpen_=false;}
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    if(ImGui::Button("F7 ボス戦へ移動 / 再生成")) RequestNeonBossDeveloperEncounter();
    ImGui::SameLine();
    if(ImGui::Button("Neon Boss / 本編ボス確認")) {
        showGameDebugConsole_=true;selectNeonBossTab_=true;expeditionAuthoringHubOpen_=false;
    }
    ImGui::SameLine();
    if(neonSkinnedPreview_ && ImGui::Button("Neon Skinned Previewを開く")) {
        showGameDebugConsole_=true;
        selectNeonSkinnedPreviewTab_=true;
        expeditionAuthoringHubOpen_=false;
    }
#endif
    ImGui::SeparatorText("見た目・ポストエフェクト");
    auto apply=[&] {
        ApplyGamePostEffectConfig(expeditionPostDraft_);ApplyGameVisualConfig(expeditionVisualDraft_);
        expeditionPostDraft_=BuildGamePostEffectConfig();expeditionVisualDraft_=BuildGameVisualConfig();
        RefreshExpeditionBuildCards();expeditionAuthoringStatus_="見た目を適用しました。F3で閉じると試遊できます。";
    };
    if(ImGui::Button("適用 / Apply"))apply();ImGui::SameLine();
    if(ImGui::Button("保存 / Save")) {
        expeditionPostDraft_["expeditionAuthored"]=true;expeditionVisualDraft_["expeditionAuthored"]=true;
        if(SaveAuthoringJson("resources/configs/gamePostEffects.json",expeditionPostDraft_,expeditionAuthoringStatus_)&&
           SaveAuthoringJson("resources/configs/gameVisuals.json",expeditionVisualDraft_,expeditionAuthoringStatus_)) {
            apply();expeditionAuthoringStatus_="ポスト・見た目を保存しました。次回起動にも反映されます。";
        }
    }
    ImGui::SameLine();if(ImGui::Button("再読込 / Reload")) {
        try {
            nlohmann::json post,visual;std::ifstream p("resources/configs/gamePostEffects.json"),v("resources/configs/gameVisuals.json");p>>post;v>>visual;
            if(!post.is_object()||!visual.is_object())throw std::runtime_error("設定形式が不正です");
            expeditionPostDraft_=std::move(post);expeditionVisualDraft_=std::move(visual);apply();expeditionAuthoringStatus_="保存済みのポスト・見た目に戻しました。";
        }catch(const std::exception& ex){expeditionAuthoringStatus_=std::string("再読込失敗: ")+ex.what();}
    }
    ImGui::TextWrapped("%s",expeditionAuthoringStatus_.c_str());
    auto scalar=[](nlohmann::json& object,const char* key,const char* label,float low,float high) {
        float value=object.value(key,0.0f);if(ImGui::SliderFloat(label,&value,low,high,"%.3f"))object[key]=value;
    };
    auto color=[](nlohmann::json& object,const char* key,const char* label) {
        auto& j=object[key];float c[]={j.value("x",0.0f),j.value("y",0.0f),j.value("z",0.0f),j.value("w",1.0f)};
        if(ImGui::ColorEdit4(label,c,ImGuiColorEditFlags_Float|ImGuiColorEditFlags_HDR))j={{"x",c[0]},{"y",c[1]},{"z",c[2]},{"w",c[3]}};
    };
    if(ImGui::BeginTabBar("見た目の編集")) {
        if(ImGui::BeginTabItem("ネオン・ブルーム")) {
            ImGui::TextWrapped("ネオン版の自機・剣・敵・グリッドは共通のブルームを使用。個別の明るさは下の発光倍率、光の広がりは共通ブルームで調整します。");
            auto& neon=expeditionVisualDraft_["neonGrid"];
            scalar(neon,"playerNeonEmission","プレイヤー・剣の発光倍率",0,4);
            scalar(neon,"bossNeonEmission","ボスの発光倍率",0,4);
            scalar(neon,"lineCoreIntensity","ネオン線の芯の明るさ",0,4);
            scalar(neon,"lineSoftEdgeRatio","ネオン線の柔らかさ",0.01f,0.95f);
            const char* keys[]={"grid","bulletTrail","particles","sharedObjectBloom","player","bossEnemy","expEnemy","stage"};
            const char* labels[]={"ネオン共通（戦車・剣・グリッド）","弾・剣の軌跡","パーティクル","3D本体・ブロックの共通ブルーム","プレイヤー3D（旧表示用）","ボス3D（旧表示用）","通常敵3D（旧表示用）","ブロック3D"};
            for(int i=0;i<8;++i)if(ImGui::CollapsingHeader(labels[i],i==0?ImGuiTreeNodeFlags_DefaultOpen:0)) {
                ImGui::PushID(keys[i]);auto& entry=expeditionPostDraft_[keys[i]];
                if(entry.contains("enabled")){bool enabled=entry["enabled"].get<bool>();if(ImGui::Checkbox("有効",&enabled))entry["enabled"]=enabled;}
                auto& param=entry["param"];scalar(param,"intensity","ブルーム強度",0,8);scalar(param,"threshold","発光しきい値",0,2);
                if(i==4||i==5)ImGui::TextWrapped("現在のネオン描画では、この個別3D設定は使いません。上のネオン共通と発光倍率を編集してください。");
                ImGui::PopID();
            }
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("機体・斬撃・残像")) {
            auto& neon=expeditionVisualDraft_["neonGrid"];
            scalar(neon,"actorNeonBillboardLineWidth","機体の線幅",0.01f,0.5f);
            color(neon,"actorNeonBodyFillColor","機体内部の塗り");
            scalar(neon,"playerAfterimageAlpha","ダッシュ残像の濃さ",0,1);
            scalar(neon,"playerAfterimageLifetime","ダッシュ残像の寿命（秒）",0.02f,1);
            scalar(neon,"playerMeleeBladeOuterWidthScale","刃の外側の光の幅",0.1f,5);
            scalar(neon,"playerMeleeBladeHaloWidthScale","刃の光の幅",0.1f,3);
            scalar(neon,"playerMeleeBladeCoreWidthScale","刃の白い芯の幅",0.01f,1);
            scalar(neon,"playerMeleeTrailWidthScale","剣の軌跡の幅",0.1f,3);
            scalar(neon,"playerMeleeTrailAlphaScale","剣の軌跡の濃さ",0,3);
            scalar(neon,"playerMeleeAfterimageAlphaScale","剣の残像の濃さ",0,3);
            ImGui::TextWrapped("機体や敵の種類ごとの色・形状はF6で編集できます。エッジ検出による追加アウトラインは変更しません。");
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("使い方")) {
            ImGui::TextWrapped("1. F2で基本性能を調整。F6で強化・進化・敵を新規作成または複製。\n2. F4で部屋を複製し、敵とブロック、勝利条件を配置。\n3. F5で部屋を戦闘/精鋭/ボスの抽選候補に追加し、左から何列目に出るかと重みを設定。\n4. 各画面で保存。新しい遠征を始めてルートを確認します。\n\n基本性能・既取得強化の倍率・見た目は適用後すぐ反映。敵の性能や配置は次の部屋、マップ生成ルールは次の遠征から反映。どの画面も保存するまでファイルは変わりません。\n\n新しい攻撃プログラムを記述するエディターではありません。強化は既存効果の組合せ、敵は既存9種の行動を土台として種類を増やせます。");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
#endif
}
