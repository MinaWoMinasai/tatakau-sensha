#include "GameScene.h"
#include "game/run/TankExpeditionBalance.h"
#if defined(USE_IMGUI) || defined(USE_RUNTIME_PROFILER)
#include "externals/imgui/imgui.h"
#endif

void GameScene::InitializeTankExpeditionBalance() {
    tankExpeditionBalance_=tankexp::DefaultBalance();
    std::string error;
    if(!tankexp::LoadBalance(tankexp::kBalancePath,tankExpeditionBalance_,error))
        balanceEditor_.statusMessage="既定値を使用 / "+error;
    LoadBalanceEditorFromJson(tankExpeditionBalance_);
    expeditionStyleBalanceDraft_=tankExpeditionBalance_["combatStyles"];
    balanceEditor_.bossMaxHp=tankExpeditionBalance_["bossMaxHp"].get<int>();
    ApplyLevelBalance(tankExpeditionBalance_);
    player_->ApplyCombatStyleBalance(tankexp::ReadCombatStyleBalances(tankExpeditionBalance_));
    // Initialization grants the authored full HP once; subsequent Apply does
    // not erase damage or revive a defeated player.
    player_->HealRunPlayer(player_->GetMaxHp());
    ApplyTankExpeditionRoomBalance();
}

void GameScene::ApplyTankExpeditionRoomBalance() {
    if(!expeditionRun_||!tankExpeditionBalance_.is_object()) return;
    auto effective=tankExpeditionBalance_;
    const bool resourceRoom=tankExpedition_.GetRoomKind()==tankexp::RoomKind::Resource;
    effective["enemySystem"]["expEnemyHostileToBoss"]=resourceRoom;
    effective["enemySystem"]["bossLevelingModeEnabled"]=resourceRoom;
    // Room transitions change hostile pressure only. Player stats, current HP,
    // cards, maintenance points, and in-progress XP must survive the checkpoint.
    effective.erase("player"); effective.erase("playerUpgrades");effective.erase("combatStyles");
    const auto curve=tankexp::GetRoomBalance(tankExpedition_.GetRoomIndex());
    auto& damage=effective["damage"];
    for(const char* key:{"expEnemyContact","shooterContact","shooterBullet"})
        damage[key]=(std::max)(1,static_cast<int>(std::round(damage[key].get<float>()*curve.contactScale)));
    damage["shooterBulletSpeed"]=damage["shooterBulletSpeed"].get<float>()*curve.bulletSpeedScale;
    damage["shooterFireInterval"]=damage["shooterFireInterval"].get<float>()*curve.fireIntervalScale;
    ApplyLevelBalance(effective);
    for(auto* actor:enemyManager_->GetEnemyPtrs()) {
        if(!actor || actor->IsRunResource() || (expeditionMapEnabled_ && actor->HasAuthoredDefinition())) continue;
        actor->SetDamage(damage[actor->GetType()==ExpEnemyType::Shooter || actor->GetType()==ExpEnemyType::Sniper ?
            "shooterContact":"expEnemyContact"].get<uint32_t>());
    }
    enemy_->SetPrototypeAttackTuning(enemy_->GetBossAttackConfig());
    enemy_->SetPrototypePressure(tankExpedition_.GetRoomKind()==tankexp::RoomKind::Boss?curve.bossPressure:0);
    const int requested=tankExpeditionBalance_["bossMaxHp"].get<int>();
    const int maxHp=tankExpedition_.GetRoomKind()==tankexp::RoomKind::Boss?requested:
        (std::max)(1,static_cast<int>(std::round(requested*0.52f)));
    if(enemy_->GetMaxHp()!=maxHp) enemy_->SetPrototypeMaxHp(maxHp,false);
}

void GameScene::UpdateTankExpeditionBalanceEditor() {
    if(expeditionTransition_.IsActive()) return;
#if defined(USE_IMGUI) || defined(USE_RUNTIME_PROFILER)
    if(!expeditionRun_||!ImGui::GetCurrentContext()) return;
    if(input_->IsKeyTriggered(DIK_F2)) {
        tankExpeditionBalanceEditorOpen_=!tankExpeditionBalanceEditorOpen_;
        if(tankExpeditionBalanceEditorOpen_) {
            LoadBalanceEditorFromJson(tankExpeditionBalance_);
            expeditionStyleBalanceDraft_=tankExpeditionBalance_["combatStyles"];
            balanceEditor_.bossMaxHp=tankExpeditionBalance_["bossMaxHp"].get<int>();
            balanceEditor_.healToFull=false;
        }
    }
#endif
}

void GameScene::DrawTankExpeditionBalanceEditor() {
#if defined(USE_IMGUI) || defined(USE_RUNTIME_PROFILER)
    if(!expeditionRun_||!tankExpeditionBalanceEditorOpen_||!ImGui::GetCurrentContext()) return;
    ImGui::SetNextWindowPos({210,82},ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({860,560},ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("遠征バランス / F2で再開",&tankExpeditionBalanceEditorOpen_)) {ImGui::End();return;}
    ImGui::TextWrapped("戦闘は一時停止中。数値を編集 → Apply → F2でその場から試遊。Saveで次回起動にも反映します。");
    auto apply=[&]() {
        auto edited=BuildBalanceJsonFromEditor(); edited["bossMaxHp"]=balanceEditor_.bossMaxHp;
        edited["schemaVersion"]=2;edited["combatStyles"]=expeditionStyleBalanceDraft_;
        tankExpeditionBalance_=tankexp::SanitizeBalance(edited);
        ApplyLevelBalance(tankExpeditionBalance_);
        player_->ApplyCombatStyleBalance(tankexp::ReadCombatStyleBalances(tankExpeditionBalance_));
        ApplyTankExpeditionRoomBalance();
        LoadBalanceEditorFromJson(tankExpeditionBalance_);
        expeditionStyleBalanceDraft_=tankExpeditionBalance_["combatStyles"];
        balanceEditor_.bossMaxHp=tankExpeditionBalance_["bossMaxHp"].get<int>();
    };
    if(ImGui::Button("Apply / 適用")) {apply();balanceEditor_.statusMessage="適用しました。現在HP・改造・進化を保持しています。";}
    ImGui::SameLine();
    if(ImGui::Button("Save / JSON保存")) {
        apply(); std::string error;
        balanceEditor_.statusMessage=tankexp::SaveBalance(tankexp::kBalancePath,tankExpeditionBalance_,error)?
            std::string("保存済み: ")+tankexp::kBalancePath:error;
    }
    ImGui::SameLine();
    if(ImGui::Button("Reload / 再読込")) {
        nlohmann::json loaded;std::string error;
        if(tankexp::LoadBalance(tankexp::kBalancePath,loaded,error)) {
            tankExpeditionBalance_=loaded;LoadBalanceEditorFromJson(loaded);
            expeditionStyleBalanceDraft_=loaded["combatStyles"];
            balanceEditor_.bossMaxHp=loaded["bossMaxHp"].get<int>();apply();
            balanceEditor_.statusMessage="保存済みJSONを読み込み、実行中の遠征に適用しました。";
        } else balanceEditor_.statusMessage="再読込失敗 / 現在値を保持: "+error;
    }
    ImGui::SameLine();
    if(ImGui::Button("Reset / 既定値")) {
        tankExpeditionBalance_=tankexp::DefaultBalance();LoadBalanceEditorFromJson(tankExpeditionBalance_);
        expeditionStyleBalanceDraft_=tankExpeditionBalance_["combatStyles"];
        balanceEditor_.bossMaxHp=tankExpeditionBalance_["bossMaxHp"].get<int>();apply();
        balanceEditor_.statusMessage="既定値へ戻して適用しました。JSONはSaveするまで変更しません。";
    }
    const auto& stats=player_->GetStats();
    const auto combat=player_->GetRunCombatSnapshot();
    ImGui::Text("現在HP %d/%d   実攻撃力 %.1f   実攻撃間隔 %.3f秒   移動 %.3f",
        player_->GetHp(),player_->GetMaxHp(),combat.shotDamage,combat.reloadSeconds,stats.moveSpeed);
    ImGui::TextWrapped("HPとスタミナは自動回復しません。上限を下げた場合のみ新しい上限まで調整します。取得済み改造・進化・通貨は保持します。");
    ImGui::TextWrapped("%s",balanceEditor_.statusMessage.c_str());
    ImGui::Separator();
    if(ImGui::BeginTabBar("ExpeditionBalanceTabs")) {
        if(ImGui::BeginTabItem("3系統の基礎性能")) {
            auto profiles=tankexp::ReadCombatStyleBalances(nlohmann::json{{"combatStyles",expeditionStyleBalanceDraft_}});
            ImGui::TextWrapped("導入後に選ぶ系統の基礎値です。現在の進化機体にも、この値 × 進化倍率 × 取得改造が反映されます。");
            if(ImGui::BeginTabBar("CombatStyleProfiles")) {for(size_t i=0;i<profiles.size();++i) {
                if(!ImGui::BeginTabItem(tankbuild::Names[i]))continue;
                auto& p=profiles[i];ImGui::PushID(static_cast<int>(i));
                ImGui::DragFloat("最大HP",&p.maxHp,1,1,9999,"%.0f");
                ImGui::DragFloat("移動速度",&p.moveSpeed,.005f,.01f,2,"%.3f");
                ImGui::DragFloat("最大スタミナ",&p.maxStamina,.1f,0,50);
                ImGui::DragFloat("スタミナ回復 / 秒",&p.staminaRecovery,.05f,0,50);
                ImGui::DragFloat("接触攻撃力",&p.bodyDamage,.1f,1,999);
                ImGui::Separator();
                ImGui::DragFloat(i==2?"斬撃1段目の威力":i==1?"ドローン1発の威力":"主砲1発の威力",&p.attackDamage,.1f,.1f,999);
                ImGui::DragFloat(i==2?"1段目の攻撃間隔 / 秒":i==1?"各ドローンの発射間隔 / 秒":"主砲の発射間隔 / 秒",&p.attackIntervalSeconds,.01f,.05f,10,"%.3f");
                if(i!=2)ImGui::DragFloat("弾速",&p.bulletSpeed,.005f,.01f,4,"%.3f");
                if(i==2) {
                    ImGui::DragFloat("斬撃の射程",&p.meleeRange,.05f,.5f,20);
                    ImGui::DragFloat("1段目の押し出し",&p.meleeKnockback,.01f,0,.95f);
                    ImGui::TextWrapped("2・3段目はこの基礎値に連撃倍率を掛けます。射程延長・終撃強化も重なります。");
                    ImGui::TextWrapped("敵の押し出し速度上限は0.95。ボスは35%%に軽減します。威力は整数化され、最少1ダメージです。");
                }
                if(i==1) {
                    ImGui::DragInt("基礎ドローン機数",&p.droneCount,1,1,12);
                    ImGui::DragFloat("追従速度",&p.droneFollowSpeed,.01f,.01f,2);
                    ImGui::DragFloat("離れた時の最高速度",&p.droneCatchupSpeed,.01f,p.droneFollowSpeed,4);
                    ImGui::DragFloat("追従の反応速度",&p.droneResponse,.1f,.1f,30);
                    ImGui::DragFloat("プレイヤーとの距離",&p.droneFormationRadius,.05f,0,8);
                    ImGui::TextWrapped("進化機数はF6設定の3機との差を加算します。例：基礎5機＋進化2機なら4機。援護強化を加えた実上限は12機です。");
                }
                ImGui::PopID();ImGui::EndTabItem();
            }
            ImGui::EndTabBar();}
            expeditionStyleBalanceDraft_=tankexp::CombatStylesToJson(profiles);
            ImGui::TextWrapped("安全上の下限：実攻撃間隔0.05秒。改造の間隔短縮は最大95%%、整数回数は最少1、ジャスト回避の窓はダッシュ全長0.3秒まで。");
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("導入・旧遠征の基礎値")) {
            ImGui::TextWrapped("系統選択前の導入戦闘と旧遠征に使用します。系統選択後は左の3系統設定を使用します。");
            ImGui::DragInt("最大HP",&balanceEditor_.playerMaxHp,1,1,9999);
            ImGui::DragFloat("攻撃力",&balanceEditor_.playerBulletDamage,0.1f,1,999);
            ImGui::DragFloat("弾速",&balanceEditor_.playerBulletSpeed,0.005f,0.05f,2);
            ImGui::DragFloat("発射間隔 / frames (小さいほど速い)",&balanceEditor_.playerReloadSpeed,0.25f,3,120);
            ImGui::DragFloat("移動速度",&balanceEditor_.playerMoveSpeed,0.005f,0.05f,1);
            ImGui::DragFloat("最大スタミナ",&balanceEditor_.playerMaxStamina,0.1f,0,20);
            ImGui::DragFloat("スタミナ回復 / 秒",&balanceEditor_.playerStaminaRecovery,0.05f,0,20);
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("強化")) {
            ImGui::TextWrapped("0.25 = +25%%。以前の隠れた上限はありません。基礎強化率 × F6の効果倍率を取得済みカードへ適用します。商品ごとの倍率はF6で調整できます。");
            ImGui::DragFloat("HP強化率 / 予備装甲",&balanceEditor_.maxHpUpgradeAmount,0.01f,0,3);
            ImGui::DragFloat("攻撃力強化率 / 重い弾頭",&balanceEditor_.playerBulletDamageUpgrade,0.01f,0,3);
            ImGui::DragFloat("弾速強化率 / 重い弾頭",&balanceEditor_.playerBulletSpeedUpgrade,0.01f,0,3);
            ImGui::DragFloat("発射間隔短縮率 / 高速装填",&balanceEditor_.playerReloadUpgrade,0.01f,0,0.90f);
            ImGui::DragFloat("移動速度強化率 / スラスター",&balanceEditor_.playerMoveSpeedUpgrade,0.01f,0,3);
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("敵")) {
            if(expeditionMapEnabled_) ImGui::TextWrapped("マップ遠征の敵はF6の種類エディターで調整します。弾倉数・装填時間・移動速度もF6で変更できます。以下は旧遠征の共通設定です。");
            ImGui::TextWrapped("第3区画を基準値とし、序盤はゆっくり・終盤は高圧力に自動補正します。予告時間は短縮しません。");
            ImGui::DragInt("接触ダメージ",&balanceEditor_.expEnemyContact,1,1,999);
            ImGui::DragInt("射撃敵の接触",&balanceEditor_.shooterContact,1,1,999);
            ImGui::DragInt("敵弾ダメージ",&balanceEditor_.shooterBullet,1,1,999);
            ImGui::DragFloat("敵弾速度",&balanceEditor_.shooterBulletSpeed,0.005f,0.05f,2);
            ImGui::DragFloat("射撃間隔 / 秒",&balanceEditor_.shooterFireInterval,0.05f,0.3f,10);
            ImGui::TextWrapped("突進・狙撃は射撃間隔に応じて攻撃後の隙が変化します。狙撃弾は基準弾速の1.3倍です。");
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("ボス")) {
            ImGui::DragInt("ボス最大HP",&balanceEditor_.bossMaxHp,10,1,100000);
            ImGui::DragFloat("ボス弾速",&balanceEditor_.bossBulletSpeed,0.005f,0.05f,2);
            ImGui::BeginDisabled(expeditionMapEnabled_);
            ImGui::DragInt("扇状弾数 (リングも同比率)",&balanceEditor_.bossBulletCount,1,1,32);
            ImGui::DragFloat("攻撃後の間隔 / 秒",&balanceEditor_.bossCooldown,0.05f,0.25f,10);
            ImGui::EndDisabled();
            ImGui::DragInt("ボス弾ダメージ",&balanceEditor_.bossBulletDamage,1,1,999);
            ImGui::TextWrapped(expeditionMapEnabled_?"ライバルは弾倉4発 / HP半分から5発。狙い撃ち・扇状・掃射を切り替え、ダッシュ後に装填します。弾倉・予告の時間は行動ごとに固定です。弾速の実効範囲は0.24〜0.46です。":"HP減少でリング・連続掃射が解禁。予告を残したまま弾数と回復時間で強さを調整します。");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
#endif
}
