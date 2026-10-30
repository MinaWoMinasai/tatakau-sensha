#include "game/editor/SurvivorTuningPanel.h"
#if defined(USE_IMGUI)
#include "externals/imgui/imgui.h"
#endif
#include <iomanip>
#include <sstream>

namespace survivor {

#if defined(USE_IMGUI)
namespace {
std::string Clock(float seconds)
{
    const int total = (std::max)(0, static_cast<int>(seconds));
    std::ostringstream text;
    text << total / 60 << ":" << std::setw(2) << std::setfill('0') << total % 60;
    return text.str();
}

/// @brief 熱とシールド。
bool DrawHeatAndShield(SpawnConfig& config)
{
    bool changed = false;
    if (!ImGui::CollapsingHeader("熱とシールド", ImGuiTreeNodeFlags_DefaultOpen)) {
        return false;
    }
    auto& heat = config.heat;
    changed |= ImGui::Checkbox("熱を使う", &heat.enabled);
    changed |= ImGui::DragFloat("熱の上限", &heat.maxHeat, 1.0f, 10.0f, 400.0f, "%.0f");
    changed |= ImGui::DragFloat("1発の熱", &heat.heatPerShot, 0.1f, 0.0f, 30.0f, "%.1f");
    changed |= ImGui::DragFloat("冷え始めるまで（秒）", &heat.coolDelay, 0.05f, 0.0f, 5.0f, "%.2f");
    changed |= ImGui::DragFloat("止めたときの冷え（毎秒）", &heat.coolPerSecond, 0.5f, 0.0f, 200.0f, "%.1f");
    changed |= ImGui::DragFloat("撃ちながらの冷え（毎秒）", &heat.coolPerSecondWhileFiring, 0.25f, 0.0f, 100.0f, "%.2f");
    changed |= ImGui::DragFloat("オーバーヒート（秒）", &heat.overheatSeconds, 0.05f, 0.0f, 10.0f, "%.2f");
    heat.maxHeat = (std::max)(1.0f, heat.maxHeat);
    ImGui::Separator();
    changed |= ImGui::DragFloat("シールド：最大HPに対する割合", &config.shieldMaxHpRatio, 0.01f, 0.0f, 2.0f, "%.2f");
    changed |= ImGui::DragFloat("シールド：回復までの秒", &config.shieldRegenDelay, 0.05f, 0.0f, 20.0f, "%.2f");
    changed |= ImGui::DragFloat("シールド：毎秒の回復（上限比）", &config.shieldRegenPerSecond, 0.01f, 0.0f, 5.0f, "%.2f");
    return changed;
}

/// @brief 湧き方。図形の生存数の目標は、点を直接いじり、曲線を下に描く。
bool DrawSpawns(SpawnConfig& config)
{
    bool changed = false;
    if (!ImGui::CollapsingHeader("湧き方")) {
        return false;
    }
    changed |= ImGui::DragFloat("出口が開く（秒）", &config.exitOpenSeconds, 1.0f, 10.0f, 900.0f, "%.0f");
    changed |= ImGui::DragFloat("1秒に湧く上限", &config.maxSpawnsPerSecond, 0.5f, 0.5f, 200.0f, "%.1f");
    ImGui::SeparatorText("図形の生存数の目標（秒, 数）");
    for (size_t i = 0; i < config.shapeTargetAlive.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        float point[2]{config.shapeTargetAlive[i].first, config.shapeTargetAlive[i].second};
        if (ImGui::DragFloat2("##point", point, 1.0f, 0.0f, 1200.0f, "%.0f")) {
            config.shapeTargetAlive[i] = {point[0], point[1]};
            changed = true;
        }
        ImGui::SameLine();
        if (config.shapeTargetAlive.size() > 1 && ImGui::SmallButton("消す")) {
            config.shapeTargetAlive.erase(config.shapeTargetAlive.begin() + static_cast<std::ptrdiff_t>(i));
            changed = true;
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
    if (ImGui::SmallButton("点を足す")) {
        const auto last = config.shapeTargetAlive.empty() ? std::pair<float, float>{0.0f, 24.0f} : config.shapeTargetAlive.back();
        config.shapeTargetAlive.emplace_back(last.first + 60.0f, last.second);
        changed = true;
    }
    changed |= ImGui::DragFloat("最後の点より後（毎分）", &config.shapeOvertimePerMinute, 0.5f, 0.0f, 300.0f, "%.1f");
    float curve[61]{};
    for (int i = 0; i <= 60; ++i) {
        curve[i] = config.ShapeTarget(static_cast<float>(i) * 5.0f);
    }
    ImGui::PlotLines("##target", curve, 61, 0, "生存数の目標 0:00〜5:00", 0.0f, 260.0f, {0.0f, 70.0f});
    ImGui::SeparatorText("群れ・敵戦車・大五角");
    changed |= ImGui::DragFloat("三角の群れ：間隔（秒）", &config.swarm.interval, 0.5f, 0.5f, 120.0f, "%.1f");
    changed |= ImGui::DragInt("三角の群れ：数", &config.swarmCount, 0.2f, 0, 40);
    changed |= ImGui::DragFloat("敵戦車：開始（秒）", &config.tanks.from, 1.0f, 0.0f, 600.0f, "%.0f");
    changed |= ImGui::DragFloat("敵戦車：間隔（秒）", &config.tanks.interval, 0.25f, 0.5f, 120.0f, "%.2f");
    changed |= ImGui::DragInt("敵戦車：同時の上限", &config.tanks.maxAlive, 0.1f, 0, 30);
    changed |= ImGui::DragFloat("大五角：開始（秒）", &config.bigPentagon.from, 1.0f, 0.0f, 600.0f, "%.0f");
    changed |= ImGui::DragFloat("大五角：間隔（秒）", &config.bigPentagon.interval, 0.5f, 0.5f, 300.0f, "%.1f");
    changed |= ImGui::DragInt("大五角：同時の上限", &config.bigPentagon.maxAlive, 0.1f, 0, 10);
    return changed;
}

/// @brief 脅威の段階と宿敵。
bool DrawThreatAndRival(SpawnConfig& config, float elapsed)
{
    bool changed = false;
    if (!ImGui::CollapsingHeader("脅威の段階と宿敵", ImGuiTreeNodeFlags_DefaultOpen)) {
        return false;
    }
    auto& threat = config.threat;
    ImGui::TextDisabled("今の段階：Lv%d（変えた値は、これから湧く敵から効く）", config.ThreatStage(elapsed));
    changed |= ImGui::DragFloat("1段の長さ（秒）", &threat.stageSeconds, 1.0f, 5.0f, 600.0f, "%.0f");
    changed |= ImGui::DragInt("段の上限", &threat.maxStage, 0.1f, 1, 30);
    changed |= ImGui::DragFloat("敵のHP（1段ごと、割合）", &threat.enemyHpPerStage, 0.01f, 0.0f, 3.0f, "+%.2f");
    changed |= ImGui::DragInt("群れの数（1段ごと）", &threat.swarmCountPerStage, 0.1f, 0, 20);
    changed |= ImGui::DragFloat("群れの間隔（1段ごとの倍率）", &threat.swarmIntervalScalePerStage, 0.005f, 0.2f, 2.0f, "×%.3f");
    changed |= ImGui::DragFloat("群れの最高速度（1段ごと）", &threat.swarmSpeedPerStage, 0.05f, 0.0f, 5.0f, "+%.2f");
    changed |= ImGui::DragInt("敵戦車の上限（1段ごと）", &threat.tankMaxAlivePerStage, 0.1f, 0, 10);
    ImGui::SeparatorText("包囲の輪");
    changed |= ImGui::DragInt("始まる段", &threat.ringFromStage, 0.1f, 1, 30);
    changed |= ImGui::DragFloat("間隔（秒）", &threat.ringInterval, 0.5f, 1.0f, 300.0f, "%.1f");
    changed |= ImGui::DragInt("三角の数", &threat.ringCount, 0.2f, 0, 60);
    changed |= ImGui::DragFloat("半径", &threat.ringRadius, 0.1f, 5.0f, 40.0f, "%.1f");
    ImGui::SeparatorText("予告と、寄ってくる敵");
    changed |= ImGui::DragFloat("予告してから湧くまで（秒）", &config.telegraph.lead, 0.05f, 0.0f, 5.0f, "%.2f");
    changed |= ImGui::DragInt("寄ってくる敵の上限", &config.telegraph.maxChasers, 1.0f, 0, 400);
    auto& heavy = config.heavySquare;
    changed |= ImGui::DragInt("重い四角：始まる段", &heavy.fromStage, 0.1f, 1, 30);
    changed |= ImGui::DragFloat("重い四角：間隔（秒）", &heavy.interval, 0.25f, 0.5f, 120.0f, "%.2f");
    changed |= ImGui::DragInt("重い四角：数", &heavy.count, 0.1f, 0, 20);
    changed |= ImGui::DragFloat("重い四角：HP の倍率", &heavy.hpScale, 0.1f, 0.1f, 50.0f, "x%.1f");
    changed |= ImGui::DragFloat("重い四角：最高速度", &heavy.maxSpeed, 0.05f, 0.0f, 20.0f, "%.2f");
    auto& lunge = config.lungePentagon;
    changed |= ImGui::DragInt("突進する五角：始まる段", &lunge.fromStage, 0.1f, 1, 30);
    changed |= ImGui::SliderFloat("突進する五角：割合", &lunge.chance, 0.0f, 1.0f, "%.2f");
    changed |= ImGui::DragFloat("突進する五角：突進の速さ", &lunge.lungeSpeed, 0.1f, 0.0f, 40.0f, "%.1f");
    changed |= ImGui::DragFloat("突進する五角：周期（秒）", &lunge.interval, 0.05f, 0.5f, 20.0f, "%.2f");
    changed |= ImGui::DragFloat("突進する五角：溜め（秒）", &lunge.windup, 0.02f, 0.0f, 3.0f, "%.2f");
    ImGui::SeparatorText("宿敵（出口の時刻に現れる）");
    changed |= ImGui::Checkbox("宿敵を出す", &config.rival.enabled);
    changed |= ImGui::DragInt("宿敵のHP", &config.rival.hp, 10.0f, 1, 100000);
    changed |= ImGui::DragInt("宿敵の経験値", &config.rival.exp, 5.0f, 0, 10000);
    return changed;
}

/// @brief 経験値の曲線。主なレベルまでに要る合計と、欠片の数の目安を表にする。
bool DrawExperience(SpawnConfig& config)
{
    bool changed = false;
    if (!ImGui::CollapsingHeader("経験値の曲線", ImGuiTreeNodeFlags_DefaultOpen)) {
        return false;
    }
    ImGui::TextDisabled("次のレベルまで = base + growth × (Lv-1)^power");
    changed |= ImGui::DragFloat("base", &config.levelBase, 0.5f, 1.0f, 1000.0f, "%.1f");
    changed |= ImGui::DragFloat("growth", &config.levelGrowth, 0.5f, 0.0f, 1000.0f, "%.1f");
    changed |= ImGui::DragFloat("power", &config.levelPower, 0.01f, 0.5f, 3.0f, "%.2f");
    changed |= ImGui::DragInt("欠片1つの経験値", &config.valuePerShard, 0.2f, 1, 500);
    changed |= ImGui::DragFloat("引き寄せの半径", &config.magnetRadius, 0.05f, 0.5f, 30.0f, "%.2f");
    changed |= ImGui::DragInt("形態を選ぶ Lv", &config.formLevel, 0.1f, 2, 30);
    changed |= ImGui::DragInt("Lv の上限", &config.maxLevel, 0.2f, 2, 99);
    if (ImGui::BeginTable("levelCurve", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Lv");
        ImGui::TableSetupColumn("次まで");
        ImGui::TableSetupColumn("合計");
        ImGui::TableSetupColumn("欠片（合計）");
        ImGui::TableHeadersRow();
        int total = 0;
        for (int level = 1; level <= 30; ++level) {
            const int next = config.ExpToNextLevel(level);
            if (level == 1 || level == 4 || level % 5 == 0) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%d→%d", level, level + 1);
                ImGui::TableNextColumn();
                ImGui::Text("%d", next);
                ImGui::TableNextColumn();
                ImGui::Text("%d", total + next);
                ImGui::TableNextColumn();
                ImGui::Text("%d", (total + next + config.valuePerShard - 1) / (std::max)(1, config.valuePerShard));
            }
            total += next;
        }
        ImGui::EndTable();
    }
    return changed;
}

/// @brief 部品の1段の上がり幅と、3択。
bool DrawPartsAndChoices(SpawnConfig& config)
{
    bool changed = false;
    if (!ImGui::CollapsingHeader("部品と3択")) {
        return false;
    }
    changed |= ImGui::SliderFloat("3択を開いている間の時間", &config.choiceSlowTimeScale, 0.05f, 1.0f, "×%.2f");
    changed |= ImGui::DragFloat("1枚目に軽い部品が出やすい倍率", &config.choiceLightBias, 0.05f, 0.0f, 10.0f, "%.2f");
    changed |= ImGui::DragFloat("合体の札が出やすい倍率", &config.choiceFusionBias, 0.05f, 0.0f, 20.0f, "%.2f");
    changed |= ImGui::Checkbox("レベルが上がったらすぐ開く", &config.choiceAutoOpen);
    ImGui::SeparatorText("排熱砲（Q）");
    changed |= ImGui::DragFloat("待ち時間（秒）", &config.heatVent.cooldown, 0.05f, 0.0f, 30.0f, "%.2f");
    changed |= ImGui::DragFloat("使える熱の下限", &config.heatVent.minHeat, 0.5f, 0.0f, 200.0f, "%.0f");
    changed |= ImGui::DragFloat("熱1あたりの威力", &config.heatVent.damagePerHeat, 0.002f, 0.0f, 2.0f, "x%.3f");
    ImGui::SeparatorText("余熱・衝角シールド・戦意・緊急放出（Lv1 の値）");
    auto& tuning = config.partTuning;
    changed |= ImGui::DragFloat("余熱：熱0での威力の上がり幅", &tuning.residualHeatBonus, 0.01f, 0.0f, 5.0f, "+%.2f");
    changed |= ImGui::DragFloat("衝角シールド：満タンでの体当たりの上がり幅", &tuning.ramShieldBonus, 0.02f, 0.0f, 10.0f, "+%.2f");
    changed |= ImGui::DragFloat("戦意：1体ごとの威力の上がり幅", &tuning.moralePerKill, 0.0005f, 0.0f, 0.2f, "+%.4f");
    changed |= ImGui::DragFloat("緊急放出：半径（基本）", &tuning.purgeRadiusBase, 0.1f, 0.0f, 40.0f, "%.1f");
    changed |= ImGui::DragFloat("緊急放出：半径（重さ1ごと）", &tuning.purgeRadiusPerWeight, 0.1f, 0.0f, 20.0f, "%.1f");
    changed |= ImGui::DragFloat("緊急放出：威力（重さ1ごと）", &tuning.purgeDamagePerWeight, 0.5f, 0.0f, 200.0f, "x%.1f");
    ImGui::SeparatorText("積載量（base + Lv / perLevels）");
    changed |= ImGui::DragInt("最初の積載量", &config.load.base, 0.1f, 1, 40);
    changed |= ImGui::DragInt("何レベルごとに+1", &config.load.perLevels, 0.1f, 1, 20);
    changed |= ImGui::DragInt("外したときの欠片（Lv あたり）", &config.load.refundPerLevel, 1.0f, 0, 1000);
    ImGui::SeparatorText("1段の上がり幅（軽い部品は割合、ほかは効果の強さ）と重さ");
    for (auto& part : config.parts) {
        ImGui::PushID(part.key);
        ImGui::SetNextItemWidth(180.0f);
        changed |= ImGui::DragFloat("##perLevel", &part.perLevel, 0.005f, 0.0f, 3.0f, "%.3f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        // 合体の部品の重さは、元の2つから決まる（下の SyncFusionWeights）ので、ここでは変えない。
        if (IsFusion(part)) {
            ImGui::Text("重さ %d", part.weight);
            ImGui::SameLine();
            ImGui::Text("%s（合体：%s＋%s）", part.name, config.parts[static_cast<size_t>(part.fuseA)].name,
                        config.parts[static_cast<size_t>(part.fuseB)].name);
        } else {
            changed |= ImGui::DragInt("##weight", &part.weight, 0.05f, 0, 9, "重さ %d");
            ImGui::SameLine();
            ImGui::Text("%s%s", part.name, part.light ? "（軽）" : "");
        }
        ImGui::PopID();
    }
    SyncFusionWeights(config.parts);
    ImGui::SeparatorText("特性（冷却炉）");
    changed |= ImGui::DragFloat("熱の上限の倍率", &config.traits.furnaceHeatMax, 0.05f, 0.1f, 5.0f, "x%.2f");
    changed |= ImGui::DragFloat("冷える速さの倍率", &config.traits.furnaceCool, 0.01f, 0.0f, 3.0f, "x%.2f");
    ImGui::SeparatorText("レリック（宝箱）");
    changed |= ImGui::DragFloat("一発屋：主砲の威力", &config.relics.oneShotDamage, 0.05f, 0.1f, 20.0f, "x%.2f");
    changed |= ImGui::DragFloat("一発屋：1秒の発射数", &config.relics.oneShotFireRate, 0.01f, 0.05f, 2.0f, "x%.2f");
    changed |= ImGui::DragFloat("不動砲台：止まっている間の威力", &config.relics.turretDamage, 0.02f, 0.0f, 10.0f, "+%.2f");
    ImGui::SeparatorText("形態（次に選んだときから効く）");
    for (auto& form : config.forms) {
        ImGui::PushID(form.key);
        ImGui::TextUnformatted(form.name);
        changed |= ImGui::DragFloat("連射の倍率", &form.reloadScale, 0.01f, 0.05f, 5.0f, "%.2f");
        changed |= ImGui::DragFloat("威力の倍率", &form.damageScale, 0.01f, 0.05f, 10.0f, "%.2f");
        changed |= ImGui::DragFloat("熱の倍率", &form.heatScale, 0.01f, 0.0f, 5.0f, "%.2f");
        ImGui::PopID();
    }
    return changed;
}
} // namespace
#endif

TuningPanelResult DrawTuningPanel(SpawnConfig& config, bool& open, bool& pauseWhileOpen, const std::string& status,
                                  const TuningSnapshot& snapshot)
{
    TuningPanelResult result;
#if defined(USE_IMGUI)
    if (!open || !ImGui::GetCurrentContext()) {
        return result;
    }
    ImGui::SetNextWindowPos({20, 90}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({460, 640}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("育つ戦車の調整 / F2", &open)) {
        ImGui::End();
        return result;
    }
    ImGui::Text("区画 %s   Lv %d   敵 %d   %.0f fps", Clock(snapshot.elapsed).c_str(), snapshot.level, snapshot.alive, snapshot.fps);
    ImGui::Text("熱 %.0f / %.0f   オーバーヒート %d 回   生存数の目標 %.0f", snapshot.heat, snapshot.heatMax, snapshot.overheats,
                config.ShapeTarget(snapshot.elapsed));
    ImGui::Checkbox("開いている間は止める", &pauseWhileOpen);
    ImGui::TextDisabled("変えた値はすぐ効く。保存で survivor_proto.json に書く。");
    if (ImGui::Button("保存")) {
        result.saveRequested = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("再読込")) {
        result.reloadRequested = true;
    }
    if (!status.empty()) {
        ImGui::SameLine();
        ImGui::TextUnformatted(status.c_str());
    }
    result.changed |= DrawHeatAndShield(config);
    result.changed |= DrawExperience(config);
    result.changed |= DrawThreatAndRival(config, snapshot.elapsed);
    result.changed |= DrawSpawns(config);
    result.changed |= DrawPartsAndChoices(config);
    ImGui::End();
#else
    (void)config;
    (void)open;
    (void)pauseWhileOpen;
    (void)status;
    (void)snapshot;
#endif
    return result;
}
} // namespace survivor
