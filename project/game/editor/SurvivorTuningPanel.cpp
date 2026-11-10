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
    changed |= ImGui::DragInt("進化（第2段）の Lv", &config.stage2Level, 0.1f, 3, 40);
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
    changed |= ImGui::Checkbox("3択を開いている間は止める", &config.choicePause);
    changed |= ImGui::SliderFloat("止めないときの時間", &config.choiceSlowTimeScale, 0.05f, 1.0f, "×%.2f");
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
    ImGui::SeparatorText("ダメージの数字");
    changed |= ImGui::Checkbox("数字を出す", &config.damageNumbers.enabled);
    changed |= ImGui::DragInt("同時に出す数", &config.damageNumbers.maxActive, 0.2f, 1, 80);
    changed |= ImGui::DragFloat("出ている秒数", &config.damageNumbers.lifeSeconds, 0.01f, 0.1f, 3.0f, "%.2f");
    changed |= ImGui::DragFloat("文字の大きさ", &config.damageNumbers.scale, 0.01f, 0.3f, 3.0f, "x%.2f");
    changed |= ImGui::DragFloat("大きな数字（主砲の何倍から）", &config.damageNumbers.bigRatio, 0.05f, 1.0f, 20.0f, "x%.2f");
    changed |= ImGui::DragFloat("まとめる秒数", &config.damageNumbers.mergeSeconds, 0.01f, 0.0f, 1.0f, "%.2f");
    ImGui::SeparatorText("合体");
    changed |= ImGui::DragInt("雷鳴：放電の届く数（足す）", &config.fusions.thunderExtraTargets, 0.1f, 0, 5);
    changed |= ImGui::DragFloat("雷鳴：届く距離の倍率", &config.fusions.thunderRadiusScale, 0.02f, 0.5f, 3.0f, "x%.2f");
    changed |= ImGui::DragFloat("爆走：衝撃波の半径の倍率", &config.fusions.rampageRadiusScale, 0.02f, 0.5f, 5.0f, "x%.2f");
    changed |= ImGui::DragFloat("爆走：衝撃波の威力の倍率", &config.fusions.rampageDamageScale, 0.05f, 0.1f, 10.0f, "x%.2f");
    changed |= ImGui::DragFloat("爆走：ダッシュで回復するシールド", &config.fusions.rampageShieldRefill, 0.01f, 0.0f, 1.0f, "%.2f");
    changed |= ImGui::DragFloat("臨界点：爆発の半径", &config.fusions.criticalRadius, 0.1f, 1.0f, 30.0f, "%.1f");
    changed |= ImGui::DragFloat("臨界点：爆発の威力（主砲の倍）", &config.fusions.criticalDamage, 0.5f, 0.0f, 200.0f, "x%.1f");
    ImGui::SeparatorText("特性（冷却炉）");
    changed |= ImGui::DragFloat("熱の上限の倍率", &config.traits.furnaceHeatMax, 0.05f, 0.1f, 5.0f, "x%.2f");
    changed |= ImGui::DragFloat("冷える速さの倍率", &config.traits.furnaceCool, 0.01f, 0.0f, 3.0f, "x%.2f");
    ImGui::SeparatorText("レリック（宝箱）");
    changed |= ImGui::DragFloat("一発屋：主砲の威力", &config.relics.oneShotDamage, 0.05f, 0.1f, 20.0f, "x%.2f");
    changed |= ImGui::DragFloat("一発屋：1秒の発射数", &config.relics.oneShotFireRate, 0.01f, 0.05f, 2.0f, "x%.2f");
    changed |= ImGui::DragFloat("不動砲台：止まっている間の威力", &config.relics.turretDamage, 0.02f, 0.0f, 10.0f, "+%.2f");
    ImGui::SeparatorText("形態（倍率・扇・ばらつき・太さは次に選んだときから、印・跳ね返り・放電はすぐ効く）");
    for (auto& form : config.forms) {
        ImGui::PushID(form.key);
        if (form.stage >= 2) {
            ImGui::Text("%s（%s から%s）", form.name, config.forms[static_cast<size_t>(form.parent)].name,
                        form.keyPart != PartId::Count ? "・鍵付き" : "");
        } else {
            ImGui::TextUnformatted(form.name);
        }
        changed |= ImGui::DragFloat("連射の倍率", &form.reloadScale, 0.01f, 0.05f, 5.0f, "%.2f");
        changed |= ImGui::DragFloat("威力の倍率", &form.damageScale, 0.01f, 0.05f, 10.0f, "%.2f");
        changed |= ImGui::DragFloat("熱の倍率", &form.heatScale, 0.01f, 0.0f, 5.0f, "%.2f");
        // 第2段の固有の能力。持っている形態だけ出す。
        if (form.fanAngle > 0.0f) {
            changed |= ImGui::DragFloat("扇の角度", &form.fanAngle, 0.2f, 1.0f, 60.0f, "%.1f°");
        }
        if (form.spreadDeg >= 0.0f) {
            changed |= ImGui::DragFloat("弾のばらつき", &form.spreadDeg, 0.2f, 0.0f, 45.0f, "%.1f°");
        }
        if (form.markDetonation) {
            changed |= ImGui::DragInt("爆破までの命中", &form.markHits, 0.05f, 1, 8);
        }
        if (form.wallBounces > 0) {
            changed |= ImGui::DragInt("壁で跳ね返る回数", &form.wallBounces, 0.05f, 1, 8);
        }
        if (form.chainExtraTargets > 0) {
            changed |= ImGui::DragInt("放電の届く数（足す）", &form.chainExtraTargets, 0.05f, 1, 5);
            changed |= ImGui::DragFloat("放電の距離の倍率", &form.chainRadiusScale, 0.02f, 0.5f, 3.0f, "x%.2f");
        }
        if (form.railPierce > 0) {
            changed |= ImGui::DragFloat("ため撃ちの太さ", &form.railWidth, 0.02f, 0.5f, 5.0f, "x%.2f");
            changed |= ImGui::DragInt("ため撃ちの貫通（足す）", &form.railPierce, 0.05f, 0, 8);
        }
        ImGui::PopID();
    }
    return changed;
}

/// @brief レア度ごとの値を1行ずつ。
template <class T, class Draw>
bool DrawByRarity(std::array<T, kRarityCount>& values, Draw draw)
{
    bool changed = false;
    for (size_t i = 0; i < kRarityCount; ++i) {
        changed |= draw(RarityName(static_cast<Rarity>(i)), values[i]);
    }
    return changed;
}

/// @brief 素材4種とお金の組。
bool DrawCost(CraftCost& cost)
{
    bool changed = false;
    for (size_t i = 0; i < kMaterialCount; ++i) {
        changed |= ImGui::DragInt(MaterialName(static_cast<MaterialId>(i)), &cost.materials[i], 0.2f, 0, 999);
    }
    changed |= ImGui::DragInt("お金", &cost.money, 2.0f, 0, 99999);
    return changed;
}

/// @brief 武器の値（カスタマイズシューティング、計画書 p3 の W11）。
bool DrawWeapons(SpawnConfig& config)
{
    bool changed = false;
    if (!ImGui::CollapsingHeader("武器", ImGuiTreeNodeFlags_DefaultOpen)) {
        return false;
    }
    ImGui::TextDisabled("構えている武器にすぐ効く。");
    for (auto& weapon : config.weapons) {
        ImGui::PushID(weapon.key.c_str());
        if (ImGui::TreeNode(weapon.name.c_str())) {
            changed |= ImGui::DragFloat("威力の倍率", &weapon.damageScale, 0.01f, 0.05f, 20.0f, "%.2f");
            changed |= ImGui::DragFloat("発射間隔の倍率（小さいほど速い）", &weapon.reloadScale, 0.01f, 0.05f, 8.0f, "%.2f");
            changed |= ImGui::DragFloat("弾速の倍率", &weapon.speedScale, 0.01f, 0.1f, 6.0f, "%.2f");
            changed |= ImGui::DragFloat("熱の倍率", &weapon.heatScale, 0.01f, 0.0f, 8.0f, "%.2f");
            changed |= ImGui::DragFloat("ばらつき（度。負は形態のまま）", &weapon.spreadDeg, 0.1f, -1.0f, 45.0f, "%.1f");
            changed |= ImGui::DragInt("弾倉（0は弾倉なし）", &weapon.magazine, 0.2f, 0, 300);
            changed |= ImGui::DragFloat("切り替え（秒）", &weapon.swapSeconds, 0.02f, 0.0f, 6.0f, "%.2f");
            changed |= ImGui::DragFloat("リロード（秒）", &weapon.reloadSeconds, 0.02f, 0.0f, 8.0f, "%.2f");
            changed |= ImGui::DragInt("チップ枠（レア度の分とは別）", &weapon.chipSlots, 0.05f, 0, 6);
            changed |= ImGui::DragFloat("爆発の半径（0はなし）", &weapon.blastRadius, 0.05f, 0.0f, 20.0f, "%.2f");
            if (weapon.blastRadius > 0.0f) {
                changed |= ImGui::DragFloat("爆発：当たった敵の倍率", &weapon.blastCenter, 0.01f, 0.0f, 10.0f, "%.2f");
                changed |= ImGui::DragFloat("爆発：まわりの倍率", &weapon.blastSplash, 0.01f, 0.0f, 10.0f, "%.2f");
            }
            changed |= ImGui::DragFloat("誘導の強さ（0はなし）", &weapon.homingPower, 0.02f, 0.0f, 5.0f, "%.2f");
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    ImGui::SeparatorText("レア度の効き");
    changed |= DrawByRarity(config.crafting.rarityDamage, [](const char* name, float& value) {
        return ImGui::DragFloat((std::string("威力：") + name).c_str(), &value, 0.01f, 0.1f, 10.0f, "x%.2f");
    });
    changed |= DrawByRarity(config.crafting.rarityMagazine, [](const char* name, float& value) {
        return ImGui::DragFloat((std::string("弾倉：") + name).c_str(), &value, 0.01f, 0.1f, 10.0f, "x%.2f");
    });
    return changed;
}

/// @brief 落とす物と値段。
bool DrawLoot(SpawnConfig& config)
{
    bool changed = false;
    if (!ImGui::CollapsingHeader("落とす物と値段")) {
        return false;
    }
    auto& loot = config.loot;
    changed |= ImGui::DragFloat("お金（経験値に掛ける）", &loot.moneyPerExp, 0.005f, 0.0f, 5.0f, "%.3f");
    changed |= ImGui::DragFloat("素材の確率", &loot.materialChance, 0.005f, 0.0f, 1.0f, "%.3f");
    changed |= ImGui::DragFloat("ボスの素材の確率（敵戦車）", &loot.bossMaterialChance, 0.005f, 0.0f, 1.0f, "%.3f");
    changed |= ImGui::DragFloat("チップの確率", &loot.chipChance, 0.001f, 0.0f, 1.0f, "%.3f");
    changed |= ImGui::DragFloat("武器の確率", &loot.weaponChance, 0.001f, 0.0f, 1.0f, "%.3f");
    changed |= ImGui::DragFloat("大きな敵の確率の倍率", &loot.bigScale, 0.05f, 0.0f, 50.0f, "x%.2f");
    changed |= ImGui::DragFloat("寄ってくる距離", &loot.magnetRadius, 0.05f, 0.0f, 30.0f, "%.2f");
    changed |= ImGui::DragFloat("弾と素材が残る秒", &loot.lifetime, 0.5f, 1.0f, 300.0f, "%.0f");
    for (size_t i = 0; i < kAmmoTypeCount; ++i) {
        const std::string name = AmmoName(static_cast<AmmoType>(i));
        ImGui::SeparatorText(name.c_str());
        ImGui::PushID(static_cast<int>(i));
        changed |= ImGui::DragFloat("落ちる確率", &loot.ammoChance[i], 0.002f, 0.0f, 1.0f, "%.3f");
        changed |= ImGui::DragInt("1つで増える数", &loot.ammoAmount[i], 0.2f, 1, 300);
        changed |= ImGui::DragInt("予備の上限", &loot.reserveMax[i], 1.0f, 1, 9999);
        changed |= ImGui::DragInt("区画の始めの予備", &config.startReserve[i], 1.0f, 0, 9999);
        ImGui::PopID();
    }
    ImGui::SeparatorText("レア度の出やすさ（重み）");
    changed |= DrawByRarity(loot.rarityWeights, [](const char* name, float& value) {
        return ImGui::DragFloat(name, &value, 0.1f, 0.0f, 1000.0f, "%.1f");
    });
    ImGui::SeparatorText("売る値段");
    changed |= DrawByRarity(loot.weaponPrice, [](const char* name, int& value) {
        return ImGui::DragInt((std::string("武器：") + name).c_str(), &value, 1.0f, 0, 99999);
    });
    changed |= DrawByRarity(loot.chipPrice, [](const char* name, int& value) {
        return ImGui::DragInt((std::string("チップ：") + name).c_str(), &value, 1.0f, 0, 99999);
    });
    for (size_t i = 0; i < kMaterialCount; ++i) {
        changed |= ImGui::DragInt((std::string("1つ：") + MaterialName(static_cast<MaterialId>(i))).c_str(), &loot.materialPrice[i], 0.2f, 0, 9999);
    }
    return changed;
}

/// @brief 作る・進化・枝に要る物。
bool DrawCrafting(SpawnConfig& config)
{
    bool changed = false;
    if (!ImGui::CollapsingHeader("作る・派生の木")) {
        return false;
    }
    auto& crafting = config.crafting;
    for (auto& recipe : crafting.recipes) {
        if (!recipe.craftable) {
            continue;
        }
        ImGui::PushID(recipe.key.c_str());
        if (ImGui::TreeNode((std::string("作る：") + recipe.key).c_str())) {
            changed |= DrawCost(recipe.cost);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    for (size_t i = 1; i < kRarityCount; ++i) {
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::TreeNode((std::string("進化：") + RarityName(static_cast<Rarity>(i))).c_str())) {
            changed |= ImGui::DragInt("主な素材の数", &crafting.evolveMain[i], 0.2f, 0, 999);
            changed |= DrawCost(crafting.evolve[i]);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    for (size_t i = 0; i < crafting.branches.size(); ++i) {
        WeaponBranch& branch = crafting.branches[i];
        ImGui::PushID(static_cast<int>(100 + i));
        if (ImGui::TreeNode((std::string("枝：") + branch.from + " → " + branch.to).c_str())) {
            int rarity = static_cast<int>(branch.minRarity);
            if (ImGui::SliderInt("必要なレア度（0 コモン〜4 レジェンド）", &rarity, 0, static_cast<int>(kRarityCount) - 1)) {
                branch.minRarity = static_cast<Rarity>(rarity);
                changed = true;
            }
            changed |= DrawCost(branch.cost);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    return changed;
}

/// @brief ステージ・バリア・新しい敵。
bool DrawStagesAndEnemies(SpawnConfig& config)
{
    bool changed = false;
    if (!ImGui::CollapsingHeader("ステージとバリア")) {
        return changed;
    }
    auto& stages = config.stages;
    for (size_t i = 0; i < stages.exitSeconds.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        ImGui::SeparatorText(i + 1 == stages.exitSeconds.size() ? "ボスステージ" : (std::string("ステージ ") + std::to_string(i + 1)).c_str());
        changed |= ImGui::DragFloat(i + 1 == stages.exitSeconds.size() ? "ボスが出るまで（秒）" : "出口が開くまで（秒）", &stages.exitSeconds[i], 1.0f, 1.0f, 900.0f, "%.0f");
        changed |= ImGui::DragInt("脅威の上乗せ", &stages.threatOffset[i], 0.05f, 0, 8);
        changed |= ImGui::DragFloat("敵のHPの倍率", &stages.enemyHp[i], 0.01f, 0.1f, 10.0f, "x%.2f");
        ImGui::PopID();
    }
    changed |= ImGui::DragFloat("群れと包囲の数の倍率", &stages.swarmScale, 0.01f, 0.05f, 3.0f, "x%.2f");
    changed |= ImGui::DragInt("ボスの素材の数", &stages.bossMaterials, 0.1f, 0, 99);
    auto& barrier = config.barrier;
    ImGui::SeparatorText("バリアとスタミナ");
    changed |= ImGui::DragFloat("半径", &barrier.radius, 0.05f, 1.0f, 20.0f, "%.2f");
    changed |= ImGui::DragFloat("開き（度）", &barrier.arcDegrees, 0.5f, 10.0f, 360.0f, "%.0f");
    changed |= ImGui::DragFloat("スタミナの上限", &barrier.staminaMax, 0.5f, 1.0f, 1000.0f, "%.0f");
    changed |= ImGui::DragFloat("張っている間の減り（毎秒）", &barrier.drainPerSecond, 0.1f, 0.0f, 200.0f, "%.1f");
    changed |= ImGui::DragFloat("弾を止めたときの減り", &barrier.hitCost, 0.1f, 0.0f, 100.0f, "%.1f");
    changed |= ImGui::DragFloat("敵1体が触れている間の減り（毎秒）", &barrier.contactPerSecond, 0.05f, 0.0f, 50.0f, "%.2f");
    changed |= ImGui::DragFloat("回復（毎秒）", &barrier.regenPerSecond, 0.1f, 0.0f, 200.0f, "%.1f");
    changed |= ImGui::DragFloat("回復が始まるまで（秒）", &barrier.regenDelay, 0.02f, 0.0f, 10.0f, "%.2f");
    changed |= ImGui::DragFloat("尽きた後に張れない秒", &barrier.exhaustedSeconds, 0.02f, 0.0f, 10.0f, "%.2f");
    return changed;
}

/// @brief 弾の軌跡のにじみと、拾い物の光り方。
bool DrawGlow(SpawnConfig& config)
{
    bool changed = false;
    if (!ImGui::CollapsingHeader("光り方")) {
        return false;
    }
    auto& glow = config.glow;
    ImGui::SeparatorText("弾の軌跡のにじみ");
    changed |= ImGui::DragFloat("強さ", &glow.trailBloomIntensity, 0.02f, 0.0f, 8.0f, "%.2f");
    changed |= ImGui::DragFloat("明るさ", &glow.trailBloomGain, 0.02f, 0.0f, 4.0f, "%.2f");
    changed |= ImGui::DragFloat("広がり", &glow.trailBloomScatter, 0.01f, 0.0f, 0.9f, "%.2f");
    changed |= ImGui::DragFloat("半径", &glow.trailBloomRadius, 0.01f, 0.25f, 2.0f, "%.2f");
    ImGui::SeparatorText("弾の軌跡の尾");
    changed |= ImGui::DragFloat("尾の明るさ（先端に対して）", &glow.trailTailBrightness, 0.01f, 0.0f, 2.0f, "x%.2f");
    changed |= ImGui::DragFloat("薄れ方（大きいほど尾まで色を保つ）", &glow.trailFadeCurve, 0.02f, 0.2f, 6.0f, "%.2f");
    changed |= ImGui::DragFloat("尾の太さ（先端に対して）", &glow.trailTailWidth, 0.01f, 0.0f, 2.0f, "x%.2f");
    changed |= ImGui::DragFloat("軌跡が残る秒", &glow.trailSeconds, 0.005f, 0.02f, 1.5f, "%.3f");
    ImGui::SeparatorText("拾い物");
    changed |= ImGui::DragFloat("輪の半径（0 で無し）", &glow.lootRingRadius, 0.01f, 0.0f, 2.0f, "%.2f");
    changed |= ImGui::DragFloat("輪の太さ", &glow.lootRingWidth, 0.005f, 0.0f, 0.5f, "%.3f");
    changed |= ImGui::DragFloat("光の尾の秒（0 で無し）", &glow.lootTrailSeconds, 0.005f, 0.0f, 1.0f, "%.3f");
    changed |= ImGui::DragFloat("光の尾の太さ", &glow.lootTrailWidth, 0.005f, 0.0f, 1.0f, "%.3f");
    changed |= ImGui::DragFloat("拾った光の秒（0 で無し）", &glow.collectFlashSeconds, 0.005f, 0.0f, 2.0f, "%.3f");
    changed |= ImGui::DragFloat("拾った光の半径", &glow.collectFlashRadius, 0.02f, 0.0f, 6.0f, "%.2f");
    return changed;
}

/// @brief ステージ2からの新しい敵。
bool DrawElites(SpawnConfig& config)
{
    bool changed = false;
    if (!ImGui::CollapsingHeader("新しい敵")) {
        return false;
    }
    auto& elites = config.elites;
    changed |= ImGui::DragInt("出始めるステージ", &elites.fromStage, 0.05f, 1, 4);
    ImGui::SeparatorText("ばらばらな三角");
    changed |= ImGui::DragFloat("群れの中の割合", &elites.erraticRatio, 0.01f, 0.0f, 1.0f, "%.2f");
    changed |= ImGui::DragFloat("揺れの強さ", &elites.erraticStrength, 0.02f, 0.0f, 6.0f, "%.2f");
    changed |= ImGui::DragFloat("揺れの速さ", &elites.erraticFrequency, 0.05f, 0.0f, 20.0f, "%.2f");
    ImGui::SeparatorText("回復する敵");
    changed |= ImGui::DragFloat("最初に出る秒##healer", &elites.healerFirst, 0.5f, 0.0f, 300.0f, "%.0f");
    changed |= ImGui::DragFloat("湧く間隔##healer", &elites.healerInterval, 0.5f, 1.0f, 300.0f, "%.0f");
    changed |= ImGui::DragInt("同時の上限##healer", &elites.healerMaxAlive, 0.05f, 0, 20);
    changed |= ImGui::DragFloat("HPの倍率##healer", &elites.healerHpScale, 0.02f, 0.1f, 30.0f, "x%.2f");
    changed |= ImGui::DragFloat("回復の間隔（秒）", &elites.healInterval, 0.02f, 0.1f, 30.0f, "%.2f");
    changed |= ImGui::DragFloat("回復の半径", &elites.healRadius, 0.05f, 0.5f, 40.0f, "%.2f");
    changed |= ImGui::DragFloat("回復の割合", &elites.healRatio, 0.005f, 0.0f, 1.0f, "%.3f");
    ImGui::SeparatorText("ワープする敵");
    changed |= ImGui::DragFloat("最初に出る秒##warper", &elites.warperFirst, 0.5f, 0.0f, 300.0f, "%.0f");
    changed |= ImGui::DragFloat("湧く間隔##warper", &elites.warperInterval, 0.5f, 1.0f, 300.0f, "%.0f");
    changed |= ImGui::DragInt("同時の上限##warper", &elites.warperMaxAlive, 0.05f, 0, 20);
    changed |= ImGui::DragFloat("HPの倍率##warper", &elites.warperHpScale, 0.02f, 0.1f, 30.0f, "x%.2f");
    changed |= ImGui::DragFloat("ワープの間隔（秒）", &elites.warpInterval, 0.02f, 1.0f, 30.0f, "%.2f");
    changed |= ImGui::DragFloat("自機からの距離", &elites.warpDistance, 0.05f, 2.0f, 40.0f, "%.2f");
    ImGui::SeparatorText("地雷をまく敵");
    changed |= ImGui::DragFloat("最初に出る秒##layer", &elites.mineLayerFirst, 0.5f, 0.0f, 300.0f, "%.0f");
    changed |= ImGui::DragFloat("湧く間隔##layer", &elites.mineLayerInterval, 0.5f, 1.0f, 300.0f, "%.0f");
    changed |= ImGui::DragInt("同時の上限##layer", &elites.mineLayerMaxAlive, 0.05f, 0, 20);
    changed |= ImGui::DragFloat("HPの倍率##layer", &elites.mineLayerHpScale, 0.02f, 0.1f, 30.0f, "x%.2f");
    changed |= ImGui::DragFloat("地雷を置く間隔（秒）", &elites.layInterval, 0.02f, 0.2f, 30.0f, "%.2f");
    changed |= ImGui::DragInt("地雷の上限", &elites.mineMaxAlive, 0.1f, 0, 100);
    changed |= ImGui::DragFloat("反応するまで（秒）", &elites.mineArmSeconds, 0.02f, 0.0f, 10.0f, "%.2f");
    changed |= ImGui::DragFloat("残る秒", &elites.mineLifeSeconds, 0.1f, 1.0f, 120.0f, "%.1f");
    changed |= ImGui::DragFloat("反応する半径", &elites.mineTriggerRadius, 0.02f, 0.2f, 20.0f, "%.2f");
    changed |= ImGui::DragFloat("爆発の半径##mine", &elites.mineBlastRadius, 0.02f, 0.2f, 30.0f, "%.2f");
    changed |= ImGui::DragInt("爆発のダメージ", &elites.mineDamage, 0.1f, 0, 500);
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
    if (config.weaponsEnabled) {
        result.changed |= DrawWeapons(config);
        result.changed |= DrawLoot(config);
        result.changed |= DrawCrafting(config);
        result.changed |= DrawStagesAndEnemies(config);
        result.changed |= DrawElites(config);
        result.changed |= DrawGlow(config);
        ImGui::SeparatorText("ここから下は、これまでの遊び方と共通の値");
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
