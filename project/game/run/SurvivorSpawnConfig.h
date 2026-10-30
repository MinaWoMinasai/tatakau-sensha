#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>
#include "game/player/combat/PlayerHeat.h"
#include "game/run/SurvivorForms.h"
#include "game/run/SurvivorParts.h"
#include <string>
#include <utility>
#include <vector>

namespace survivor {
/// @brief 1区画の湧き方の設定。resources/configs/survivor_proto.json から読む。無い項目は既定値のまま。
struct SpawnConfig {
    /// @brief ある時刻からの図形の種類の重み（四角・三角・五角）。
    struct ShapeWeights {
        float from = 0.0f;
        std::array<float, 3> weights{6.0f, 3.0f, 1.0f};
    };

    /// @brief 一定の間隔で湧く枠（三角の群れ・敵戦車・大五角）。
    struct Timed {
        float from = 0.0f;
        float interval = 10.0f;
        int maxAlive = 0;
    };

    float exitOpenSeconds = 180.0f;
    float exitRadius = 3.0f;
    float ringMinDistance = 26.0f;
    float ringMaxDistance = 36.0f;
    float maxSpawnsPerSecond = 10.0f;

    std::vector<std::pair<float, float>> shapeTargetAlive{
        {0.0f, 24.0f}, {60.0f, 45.0f}, {120.0f, 88.0f}, {180.0f, 100.0f}, {240.0f, 112.0f}};
    float shapeOvertimePerMinute = 24.0f;
    std::vector<ShapeWeights> shapeWeights{ShapeWeights{}};

    Timed swarm{20.0f, 20.0f, 0};
    int swarmCount = 7;
    float swarmSpread = 3.0f;
    float swarmAcceleration = 9.0f;
    float swarmMaxSpeed = 6.5f;

    Timed tanks{60.0f, 7.0f, 4};
    std::vector<std::string> tankTypes{"Shooter", "Charger", "Sniper"};

    Timed bigPentagon{120.0f, 18.0f, 3};
    float bigPentagonScale = 2.2f;
    int bigPentagonHp = 140;
    int bigPentagonExp = 160;

    /// @brief 脅威の段階。stageSeconds ごとに1段上がり、それから湧く敵を硬く、群れを大きく・速く・頻繁にする。
    struct Threat {
        float stageSeconds = 60.0f;
        int maxStage = 8;
        // 1段ごとに足す量。HP は割合（0.25 なら1段で +25%）、群れの間隔は1段ごとに掛ける倍率。
        float enemyHpPerStage = 0.25f;
        int swarmCountPerStage = 3;
        float swarmIntervalScalePerStage = 0.9f;
        float swarmSpeedPerStage = 0.6f;
        int tankMaxAlivePerStage = 1;
        // 包囲の輪：ringFromStage 段から、ringInterval 秒ごとに、自機を中心とした輪の上に三角を並べて寄らせる。
        int ringFromStage = 3;
        float ringInterval = 30.0f;
        int ringCount = 18;
        float ringRadius = 17.0f;
    } threat;

    /// @brief 予告と、寄ってくる敵の上限。群れと包囲の輪は lead 秒前に予告してから湧く。
    /// 寄ってくる敵（三角の群れ・輪・重い四角・突進する五角）が maxChasers 体いる間は、群れと輪を湧かせない分だけ減らす。
    struct Telegraph {
        float lead = 1.5f;
        int maxChasers = 70;
    } telegraph;

    /// @brief 重い四角：大きく硬く、ゆっくり迫る四角。fromStage 段から interval 秒ごとに count 体。
    struct HeavySquare {
        int fromStage = 2;
        float interval = 14.0f;
        int count = 2;
        float scale = 1.7f;
        float hpScale = 5.0f;
        float acceleration = 3.0f;
        float maxSpeed = 2.6f;
    } heavySquare;

    /// @brief 突進する五角：fromStage 段から、湧く五角の chance の割合が、ゆっくり寄りながら interval 秒ごとに
    /// windup 秒溜めて duration 秒 lungeSpeed で迫る。
    struct LungePentagon {
        int fromStage = 2;
        float chance = 0.5f;
        float acceleration = 4.0f;
        float maxSpeed = 2.0f;
        float lungeSpeed = 13.0f;
        float interval = 3.5f;
        float windup = 0.5f;
        float duration = 0.5f;
    } lungePentagon;

    /// @brief 宿敵。exitOpenSeconds に出口の代わりに現れ、倒すと出口が開く。enabled が偽なら、時間で出口が開く。
    struct Rival {
        bool enabled = true;
        int hp = 1500;
        int exp = 600;
    } rival;

    /// @brief 経過秒での脅威の段階（1から）を返す。
    int ThreatStage(float seconds) const
    {
        const float step = (std::max)(1.0f, threat.stageSeconds);
        return (std::clamp)(1 + static_cast<int>(seconds / step), 1, (std::max)(1, threat.maxStage));
    }

    // 経験値の欠片とレベル。
    float magnetRadius = 5.0f;
    float collectRadius = 1.2f;
    int valuePerShard = 30;
    int maxShards = 600;
    float levelBase = 25.0f;
    float levelGrowth = 30.0f;
    float levelPower = 1.7f;
    int maxLevel = 45;

    /// @brief 積載量。base + Lv / perLevels（3レベルごとに+1）。外した部品は Lv × refundPerLevel の経験値の欠片を落とす。
    struct Load {
        int base = 6;
        int perLevels = 3;
        int refundPerLevel = 60;
    } load;

    /// @brief 特性（区画の始めに選ぶ）。冷却炉は熱の上限を furnaceHeatMax 倍、冷える速さを furnaceCool 倍にする。
    struct Traits {
        float furnaceHeatMax = 2.0f;
        float furnaceCool = 0.7f;
    } traits;

    /// @brief レリック（大五角の宝箱から）。一発屋は威力 oneShotDamage 倍・1秒の発射数 oneShotFireRate 倍。
    /// 不動砲台は、移動の入力が無い間の威力 +turretDamage（0.8 で +80%）で、ダッシュできない。
    struct Relics {
        float oneShotDamage = 3.5f;
        float oneShotFireRate = 0.3f;
        float turretDamage = 0.8f;
    } relics;

    /// @brief 合体の部品の強さ。雷鳴は放電の届く数 +thunderExtraTargets・届く距離 ×thunderRadiusScale。
    /// 爆走はダッシュの衝撃波の半径 ×rampageRadiusScale・威力 ×rampageDamageScale、ダッシュのたびにシールドを上限の rampageShieldRefill 回復。
    /// 臨界点はオーバーヒートした瞬間に、半径 criticalRadius・威力 主砲1発 × criticalDamage の爆発。
    struct Fusions {
        int thunderExtraTargets = 3;
        float thunderRadiusScale = 1.4f;
        float rampageRadiusScale = 1.8f;
        float rampageDamageScale = 2.0f;
        float rampageShieldRefill = 0.5f;
        float criticalRadius = 9.0f;
        float criticalDamage = 20.0f;
    } fusions;

    /// @brief 排熱砲（Q）。熱が minHeat 未満なら使えない。威力の倍率は 熱 × damagePerHeat × 効果の強さ（Lv）。使うと cooldown 秒待つ。
    struct HeatVent {
        float cooldown = 3.0f;
        float minHeat = 15.0f;
        float damagePerHeat = 0.12f;
    } heatVent;

    /// @brief T3 の部品の強さ（Lv1 の値。Lv2・3 は効果の強さ effectPower を掛ける）。
    struct PartTuning {
        float residualHeatBonus = 0.5f;   // 余熱：熱0のときの威力の上がり幅
        float ramShieldBonus = 1.5f;      // 衝角シールド：シールドが満タンのときの体当たりの上がり幅
        float moralePerKill = 0.001f;     // 戦意：1体倒すごとの主砲の威力の上がり幅（区画で千体以上倒すので、設計案の1%より小さい）
        float purgeRadiusBase = 6.0f;     // 緊急放出：爆発の半径 = base + 重さ × perWeight
        float purgeRadiusPerWeight = 2.0f;
        float purgeDamagePerWeight = 15.0f; // 緊急放出：威力 = 主砲1発 × 重さ × これ
    } partTuning;

    /// @brief レベル level での積載量を返す。
    int CapacityAt(int level) const
    {
        return load.base + (std::max)(0, level) / (std::max)(1, load.perLevels);
    }

    // 部品と3択。
    std::array<PartDefinition, kPartCount> parts = DefaultParts();
    float choiceSlowTimeScale = 0.25f;
    bool choiceAutoOpen = false;
    /// @brief 3択を開いている間はゲームを止めるか。false ならスロー（choiceSlowTimeScale）にする。
    bool choicePause = true;
    float choiceLightBias = 2.0f;
    /// @brief 合体の札（元の2つが最大 Lv のとき）がほかの札より出やすい倍率。
    float choiceFusionBias = 3.0f;

    // 熱とシールド。試作では基本で持つ。
    PlayerHeat::Config heat{true, 100.0f, 3.0f, 0.5f, 40.0f, 0.0f, 2.0f};
    float shieldMaxHpRatio = 0.2f;
    float shieldRegenDelay = 3.0f;
    float shieldRegenPerSecond = 0.25f;

    // 形態。formLevel に上がったら、止めて形態を選ぶ。
    int formLevel = 5;
    std::array<FormDefinition, kFormCount> forms = DefaultForms();

    /// @brief レベル level から次へ上がるのに要る経験値を返す（base + growth × (level-1)^power、最低1）。
    int ExpToNextLevel(int level) const
    {
        const float steps = static_cast<float>((std::max)(0, level - 1));
        const float value = levelBase + levelGrowth * std::pow(steps, levelPower);
        return (std::max)(1, static_cast<int>(std::lround(value)));
    }

    /// @brief 経過秒での図形の生存数の目標を返す。点の間は直線でつなぎ、最後の点より後は毎分 overtime ずつ増やす。
    float ShapeTarget(float seconds) const
    {
        if (shapeTargetAlive.empty()) {
            return 0.0f;
        }
        if (seconds <= shapeTargetAlive.front().first) {
            return shapeTargetAlive.front().second;
        }
        for (size_t i = 1; i < shapeTargetAlive.size(); ++i) {
            const auto& a = shapeTargetAlive[i - 1];
            const auto& b = shapeTargetAlive[i];
            if (seconds <= b.first) {
                const float t = b.first > a.first ? (seconds - a.first) / (b.first - a.first) : 1.0f;
                return a.second + (b.second - a.second) * t;
            }
        }
        const auto& last = shapeTargetAlive.back();
        return last.second + shapeOvertimePerMinute * (seconds - last.first) / 60.0f;
    }

    /// @brief 経過秒で使う図形の重み（四角・三角・五角）を返す。
    const std::array<float, 3>& ShapeWeightsAt(float seconds) const
    {
        const ShapeWeights* chosen = &shapeWeights.front();
        for (const auto& entry : shapeWeights) {
            if (seconds >= entry.from) {
                chosen = &entry;
            }
        }
        return chosen->weights;
    }
};

/// @brief 設定ファイルを読む。読めない・形が違う項目は既定値のままにし、読めたら true を返す。
inline bool LoadSpawnConfig(const std::string& path, SpawnConfig& config, std::string& error)
{
    std::ifstream input(path);
    if (!input) {
        error = "survivor config not found: " + path;
        return false;
    }
    const nlohmann::json data = nlohmann::json::parse(input, nullptr, false);
    if (data.is_discarded() || !data.is_object()) {
        error = "survivor config is not a JSON object: " + path;
        return false;
    }
    auto number = [](const nlohmann::json& object, const char* key, auto& value) {
        if (object.is_object() && object.contains(key) && object.at(key).is_number()) {
            value = object.at(key).get<std::remove_reference_t<decltype(value)>>();
        }
    };
    auto timed = [&](const nlohmann::json& object, SpawnConfig::Timed& value) {
        number(object, "from", value.from);
        number(object, "interval", value.interval);
        number(object, "maxAlive", value.maxAlive);
        value.interval = (std::max)(0.5f, value.interval);
    };
    number(data, "exitOpenSeconds", config.exitOpenSeconds);
    number(data, "exitRadius", config.exitRadius);
    if (data.contains("spawnRing")) {
        const auto& ring = data.at("spawnRing");
        number(ring, "minDistance", config.ringMinDistance);
        number(ring, "maxDistance", config.ringMaxDistance);
        number(ring, "maxPerSecond", config.maxSpawnsPerSecond);
    }
    if (data.contains("shapes")) {
        const auto& shapes = data.at("shapes");
        if (shapes.contains("targetAlive") && shapes.at("targetAlive").is_array()) {
            std::vector<std::pair<float, float>> points;
            for (const auto& point : shapes.at("targetAlive")) {
                if (point.is_array() && point.size() == 2 && point[0].is_number() && point[1].is_number()) {
                    points.emplace_back(point[0].get<float>(), point[1].get<float>());
                }
            }
            if (!points.empty()) {
                config.shapeTargetAlive = std::move(points);
            }
        }
        number(shapes, "overtimePerMinute", config.shapeOvertimePerMinute);
        if (shapes.contains("weights") && shapes.at("weights").is_array()) {
            std::vector<SpawnConfig::ShapeWeights> weights;
            for (const auto& entry : shapes.at("weights")) {
                SpawnConfig::ShapeWeights w;
                number(entry, "from", w.from);
                number(entry, "square", w.weights[0]);
                number(entry, "triangle", w.weights[1]);
                number(entry, "pentagon", w.weights[2]);
                weights.push_back(w);
            }
            if (!weights.empty()) {
                config.shapeWeights = std::move(weights);
            }
        }
    }
    if (data.contains("triangleSwarm")) {
        const auto& swarm = data.at("triangleSwarm");
        timed(swarm, config.swarm);
        number(swarm, "count", config.swarmCount);
        number(swarm, "spread", config.swarmSpread);
        number(swarm, "acceleration", config.swarmAcceleration);
        number(swarm, "maxSpeed", config.swarmMaxSpeed);
    }
    if (data.contains("tanks")) {
        const auto& tanks = data.at("tanks");
        timed(tanks, config.tanks);
        if (tanks.contains("types") && tanks.at("types").is_array()) {
            std::vector<std::string> types;
            for (const auto& type : tanks.at("types")) {
                if (type.is_string()) {
                    types.push_back(type.get<std::string>());
                }
            }
            if (!types.empty()) {
                config.tankTypes = std::move(types);
            }
        }
    }
    if (data.contains("bigPentagon")) {
        const auto& big = data.at("bigPentagon");
        timed(big, config.bigPentagon);
        number(big, "scale", config.bigPentagonScale);
        number(big, "hp", config.bigPentagonHp);
        number(big, "exp", config.bigPentagonExp);
    }
    if (data.contains("threat")) {
        const auto& threat = data.at("threat");
        auto& t = config.threat;
        number(threat, "stageSeconds", t.stageSeconds);
        number(threat, "maxStage", t.maxStage);
        number(threat, "enemyHpPerStage", t.enemyHpPerStage);
        number(threat, "swarmCountPerStage", t.swarmCountPerStage);
        number(threat, "swarmIntervalScalePerStage", t.swarmIntervalScalePerStage);
        number(threat, "swarmSpeedPerStage", t.swarmSpeedPerStage);
        number(threat, "tankMaxAlivePerStage", t.tankMaxAlivePerStage);
        number(threat, "ringFromStage", t.ringFromStage);
        number(threat, "ringInterval", t.ringInterval);
        number(threat, "ringCount", t.ringCount);
        number(threat, "ringRadius", t.ringRadius);
        t.stageSeconds = (std::max)(1.0f, t.stageSeconds);
        t.ringInterval = (std::max)(1.0f, t.ringInterval);
        t.swarmIntervalScalePerStage = (std::clamp)(t.swarmIntervalScalePerStage, 0.2f, 2.0f);
    }
    if (data.contains("telegraph")) {
        const auto& telegraph = data.at("telegraph");
        number(telegraph, "lead", config.telegraph.lead);
        number(telegraph, "maxChasers", config.telegraph.maxChasers);
        config.telegraph.lead = (std::max)(0.0f, config.telegraph.lead);
    }
    if (data.contains("heavySquare")) {
        const auto& heavy = data.at("heavySquare");
        auto& h = config.heavySquare;
        number(heavy, "fromStage", h.fromStage);
        number(heavy, "interval", h.interval);
        number(heavy, "count", h.count);
        number(heavy, "scale", h.scale);
        number(heavy, "hpScale", h.hpScale);
        number(heavy, "acceleration", h.acceleration);
        number(heavy, "maxSpeed", h.maxSpeed);
        h.interval = (std::max)(0.5f, h.interval);
    }
    if (data.contains("lungePentagon")) {
        const auto& lunge = data.at("lungePentagon");
        auto& l = config.lungePentagon;
        number(lunge, "fromStage", l.fromStage);
        number(lunge, "chance", l.chance);
        number(lunge, "acceleration", l.acceleration);
        number(lunge, "maxSpeed", l.maxSpeed);
        number(lunge, "lungeSpeed", l.lungeSpeed);
        number(lunge, "interval", l.interval);
        number(lunge, "windup", l.windup);
        number(lunge, "duration", l.duration);
        l.interval = (std::max)(l.windup + l.duration + 0.1f, l.interval);
    }
    if (data.contains("rival")) {
        const auto& rival = data.at("rival");
        number(rival, "hp", config.rival.hp);
        number(rival, "exp", config.rival.exp);
        if (rival.is_object() && rival.contains("enabled") && rival.at("enabled").is_boolean()) {
            config.rival.enabled = rival.at("enabled").get<bool>();
        }
    }
    if (data.contains("experience")) {
        const auto& experience = data.at("experience");
        number(experience, "magnetRadius", config.magnetRadius);
        number(experience, "collectRadius", config.collectRadius);
        number(experience, "valuePerShard", config.valuePerShard);
        number(experience, "maxShards", config.maxShards);
        number(experience, "maxLevel", config.maxLevel);
        if (experience.contains("levelCurve")) {
            const auto& curve = experience.at("levelCurve");
            number(curve, "base", config.levelBase);
            number(curve, "growth", config.levelGrowth);
            number(curve, "power", config.levelPower);
        }
    }
    if (data.contains("partTuning")) {
        const auto& tuning = data.at("partTuning");
        auto& t = config.partTuning;
        number(tuning, "residualHeatBonus", t.residualHeatBonus);
        number(tuning, "ramShieldBonus", t.ramShieldBonus);
        number(tuning, "moralePerKill", t.moralePerKill);
        number(tuning, "purgeRadiusBase", t.purgeRadiusBase);
        number(tuning, "purgeRadiusPerWeight", t.purgeRadiusPerWeight);
        number(tuning, "purgeDamagePerWeight", t.purgeDamagePerWeight);
    }
    if (data.contains("traits")) {
        const auto& traits = data.at("traits");
        number(traits, "furnaceHeatMax", config.traits.furnaceHeatMax);
        number(traits, "furnaceCool", config.traits.furnaceCool);
        config.traits.furnaceHeatMax = (std::max)(0.1f, config.traits.furnaceHeatMax);
        config.traits.furnaceCool = (std::max)(0.0f, config.traits.furnaceCool);
    }
    if (data.contains("fusions")) {
        const auto& fusions = data.at("fusions");
        number(fusions, "thunderExtraTargets", config.fusions.thunderExtraTargets);
        number(fusions, "thunderRadiusScale", config.fusions.thunderRadiusScale);
        number(fusions, "rampageRadiusScale", config.fusions.rampageRadiusScale);
        number(fusions, "rampageDamageScale", config.fusions.rampageDamageScale);
        number(fusions, "rampageShieldRefill", config.fusions.rampageShieldRefill);
        number(fusions, "criticalRadius", config.fusions.criticalRadius);
        number(fusions, "criticalDamage", config.fusions.criticalDamage);
        config.fusions.thunderExtraTargets = (std::clamp)(config.fusions.thunderExtraTargets, 0, 5);
    }
    if (data.contains("relics")) {
        const auto& relics = data.at("relics");
        number(relics, "oneShotDamage", config.relics.oneShotDamage);
        number(relics, "oneShotFireRate", config.relics.oneShotFireRate);
        number(relics, "turretDamage", config.relics.turretDamage);
        config.relics.oneShotDamage = (std::max)(0.1f, config.relics.oneShotDamage);
        config.relics.oneShotFireRate = (std::max)(0.05f, config.relics.oneShotFireRate);
        config.relics.turretDamage = (std::max)(0.0f, config.relics.turretDamage);
    }
    if (data.contains("heatVent")) {
        const auto& vent = data.at("heatVent");
        number(vent, "cooldown", config.heatVent.cooldown);
        number(vent, "minHeat", config.heatVent.minHeat);
        number(vent, "damagePerHeat", config.heatVent.damagePerHeat);
        config.heatVent.cooldown = (std::max)(0.0f, config.heatVent.cooldown);
    }
    if (data.contains("load")) {
        const auto& load = data.at("load");
        number(load, "base", config.load.base);
        number(load, "perLevels", config.load.perLevels);
        number(load, "refundPerLevel", config.load.refundPerLevel);
        config.load.base = (std::max)(1, config.load.base);
        config.load.perLevels = (std::max)(1, config.load.perLevels);
    }
    if (data.contains("choices")) {
        const auto& choices = data.at("choices");
        number(choices, "slowTimeScale", config.choiceSlowTimeScale);
        number(choices, "lightBias", config.choiceLightBias);
        number(choices, "fusionBias", config.choiceFusionBias);
        if (choices.contains("autoOpen") && choices.at("autoOpen").is_boolean()) {
            config.choiceAutoOpen = choices.at("autoOpen").get<bool>();
        }
        if (choices.contains("pause") && choices.at("pause").is_boolean()) {
            config.choicePause = choices.at("pause").get<bool>();
        }
        config.choiceSlowTimeScale = (std::clamp)(config.choiceSlowTimeScale, 0.05f, 1.0f);
    }
    if (data.contains("parts") && data.at("parts").is_object()) {
        // 部品ごとの1段の上がり幅（割合）だけを上書きする。名前は PartDefinition::key。
        for (auto& part : config.parts) {
            const auto& parts = data.at("parts");
            if (parts.contains(part.key)) {
                number(parts.at(part.key), "perLevel", part.perLevel);
                number(parts.at(part.key), "weight", part.weight);
                part.weight = (std::clamp)(part.weight, 0, 9);
            }
        }
        survivor::SyncFusionWeights(config.parts);
    }
    if (data.contains("heat")) {
        const auto& heat = data.at("heat");
        number(heat, "max", config.heat.maxHeat);
        number(heat, "perShot", config.heat.heatPerShot);
        number(heat, "coolDelay", config.heat.coolDelay);
        number(heat, "coolPerSecond", config.heat.coolPerSecond);
        number(heat, "coolPerSecondWhileFiring", config.heat.coolPerSecondWhileFiring);
        number(heat, "overheatSeconds", config.heat.overheatSeconds);
        if (heat.is_object() && heat.contains("enabled") && heat.at("enabled").is_boolean()) {
            config.heat.enabled = heat.at("enabled").get<bool>();
        }
        config.heat.maxHeat = (std::max)(1.0f, config.heat.maxHeat);
    }
    if (data.contains("shield")) {
        const auto& shield = data.at("shield");
        number(shield, "maxHpRatio", config.shieldMaxHpRatio);
        number(shield, "regenDelay", config.shieldRegenDelay);
        number(shield, "regenPerSecond", config.shieldRegenPerSecond);
        config.shieldMaxHpRatio = (std::max)(0.0f, config.shieldMaxHpRatio);
    }
    if (data.contains("forms") && data.at("forms").is_object()) {
        const auto& forms = data.at("forms");
        number(forms, "level", config.formLevel);
        for (auto& form : config.forms) {
            if (!forms.contains(form.key)) {
                continue;
            }
            const auto& item = forms.at(form.key);
            number(item, "reloadScale", form.reloadScale);
            number(item, "damageScale", form.damageScale);
            number(item, "speedScale", form.speedScale);
            number(item, "heatScale", form.heatScale);
            number(item, "barrelLength", form.barrelLength);
            number(item, "railPower", form.railPower);
            number(item, "railHeatBase", form.railHeatBase);
            number(item, "railHeatPerCharge", form.railHeatPerCharge);
            form.reloadScale = (std::max)(0.05f, form.reloadScale);
        }
    }
    return true;
}

/// @brief 設定を path に書き出す。元のファイルの説明（comment）や知らない項目は残し、項目の順番は決まった並びにする。
/// @note 調整パネルの保存に使う。数値は小数4桁に丸める。
inline bool SaveSpawnConfig(const std::string& path, const SpawnConfig& config, std::string& error)
{
    nlohmann::json data = nlohmann::json::object();
    if (std::ifstream input(path); input) {
        nlohmann::json existing = nlohmann::json::parse(input, nullptr, false);
        if (!existing.is_discarded() && existing.is_object()) {
            data = std::move(existing);
        }
    }
    auto round = [](float value) {
        return std::round(static_cast<double>(value) * 10000.0) / 10000.0;
    };
    auto section = [&](const char* key) -> nlohmann::json& {
        if (!data[key].is_object()) {
            data[key] = nlohmann::json::object();
        }
        return data[key];
    };
    auto timed = [&](nlohmann::json& object, const SpawnConfig::Timed& value, bool withMaxAlive) {
        object["from"] = round(value.from);
        object["interval"] = round(value.interval);
        if (withMaxAlive) {
            object["maxAlive"] = value.maxAlive;
        }
    };

    data["exitOpenSeconds"] = round(config.exitOpenSeconds);
    data["exitRadius"] = round(config.exitRadius);
    auto& ring = section("spawnRing");
    ring["minDistance"] = round(config.ringMinDistance);
    ring["maxDistance"] = round(config.ringMaxDistance);
    ring["maxPerSecond"] = round(config.maxSpawnsPerSecond);
    auto& shapes = section("shapes");
    shapes["targetAlive"] = nlohmann::json::array();
    for (const auto& [seconds, alive] : config.shapeTargetAlive) {
        shapes["targetAlive"].push_back({round(seconds), round(alive)});
    }
    shapes["overtimePerMinute"] = round(config.shapeOvertimePerMinute);
    shapes["weights"] = nlohmann::json::array();
    for (const auto& entry : config.shapeWeights) {
        shapes["weights"].push_back({{"from", round(entry.from)},
                                     {"square", round(entry.weights[0])},
                                     {"triangle", round(entry.weights[1])},
                                     {"pentagon", round(entry.weights[2])}});
    }
    auto& swarm = section("triangleSwarm");
    timed(swarm, config.swarm, false);
    swarm["count"] = config.swarmCount;
    swarm["spread"] = round(config.swarmSpread);
    swarm["acceleration"] = round(config.swarmAcceleration);
    swarm["maxSpeed"] = round(config.swarmMaxSpeed);
    auto& tanks = section("tanks");
    timed(tanks, config.tanks, true);
    tanks["types"] = config.tankTypes;
    auto& experience = section("experience");
    experience["magnetRadius"] = round(config.magnetRadius);
    experience["collectRadius"] = round(config.collectRadius);
    experience["valuePerShard"] = config.valuePerShard;
    experience["maxShards"] = config.maxShards;
    experience["maxLevel"] = config.maxLevel;
    experience["levelCurve"] = {{"base", round(config.levelBase)}, {"growth", round(config.levelGrowth)}, {"power", round(config.levelPower)}};
    auto& tuning = section("partTuning");
    tuning["residualHeatBonus"] = round(config.partTuning.residualHeatBonus);
    tuning["ramShieldBonus"] = round(config.partTuning.ramShieldBonus);
    tuning["moralePerKill"] = round(config.partTuning.moralePerKill);
    tuning["purgeRadiusBase"] = round(config.partTuning.purgeRadiusBase);
    tuning["purgeRadiusPerWeight"] = round(config.partTuning.purgeRadiusPerWeight);
    tuning["purgeDamagePerWeight"] = round(config.partTuning.purgeDamagePerWeight);
    auto& traits = section("traits");
    traits["furnaceHeatMax"] = round(config.traits.furnaceHeatMax);
    traits["furnaceCool"] = round(config.traits.furnaceCool);
    auto& fusions = section("fusions");
    fusions["thunderExtraTargets"] = config.fusions.thunderExtraTargets;
    fusions["thunderRadiusScale"] = round(config.fusions.thunderRadiusScale);
    fusions["rampageRadiusScale"] = round(config.fusions.rampageRadiusScale);
    fusions["rampageDamageScale"] = round(config.fusions.rampageDamageScale);
    fusions["rampageShieldRefill"] = round(config.fusions.rampageShieldRefill);
    fusions["criticalRadius"] = round(config.fusions.criticalRadius);
    fusions["criticalDamage"] = round(config.fusions.criticalDamage);
    auto& relics = section("relics");
    relics["oneShotDamage"] = round(config.relics.oneShotDamage);
    relics["oneShotFireRate"] = round(config.relics.oneShotFireRate);
    relics["turretDamage"] = round(config.relics.turretDamage);
    auto& vent = section("heatVent");
    vent["cooldown"] = round(config.heatVent.cooldown);
    vent["minHeat"] = round(config.heatVent.minHeat);
    vent["damagePerHeat"] = round(config.heatVent.damagePerHeat);
    auto& load = section("load");
    load["base"] = config.load.base;
    load["perLevels"] = config.load.perLevels;
    load["refundPerLevel"] = config.load.refundPerLevel;
    auto& choices = section("choices");
    choices["slowTimeScale"] = round(config.choiceSlowTimeScale);
    choices["autoOpen"] = config.choiceAutoOpen;
    choices["pause"] = config.choicePause;
    choices["lightBias"] = round(config.choiceLightBias);
    choices["fusionBias"] = round(config.choiceFusionBias);
    auto& heat = section("heat");
    heat["max"] = round(config.heat.maxHeat);
    heat["perShot"] = round(config.heat.heatPerShot);
    heat["coolDelay"] = round(config.heat.coolDelay);
    heat["coolPerSecond"] = round(config.heat.coolPerSecond);
    heat["coolPerSecondWhileFiring"] = round(config.heat.coolPerSecondWhileFiring);
    heat["overheatSeconds"] = round(config.heat.overheatSeconds);
    auto& shield = section("shield");
    shield["maxHpRatio"] = round(config.shieldMaxHpRatio);
    shield["regenDelay"] = round(config.shieldRegenDelay);
    shield["regenPerSecond"] = round(config.shieldRegenPerSecond);
    auto& forms = section("forms");
    forms["level"] = config.formLevel;
    for (const auto& form : config.forms) {
        auto& item = forms[form.key];
        item["reloadScale"] = round(form.reloadScale);
        item["damageScale"] = round(form.damageScale);
        item["speedScale"] = round(form.speedScale);
        item["heatScale"] = round(form.heatScale);
        item["barrelLength"] = round(form.barrelLength);
        if (form.railCharge) {
            item["railPower"] = round(form.railPower);
            item["railHeatBase"] = round(form.railHeatBase);
            item["railHeatPerCharge"] = round(form.railHeatPerCharge);
        }
    }
    auto& parts = section("parts");
    for (const auto& part : config.parts) {
        parts[part.key]["perLevel"] = round(part.perLevel);
        parts[part.key]["weight"] = part.weight;
    }
    auto& big = section("bigPentagon");
    timed(big, config.bigPentagon, true);
    big["scale"] = round(config.bigPentagonScale);
    big["hp"] = config.bigPentagonHp;
    big["exp"] = config.bigPentagonExp;
    auto& threat = section("threat");
    threat["stageSeconds"] = round(config.threat.stageSeconds);
    threat["maxStage"] = config.threat.maxStage;
    threat["enemyHpPerStage"] = round(config.threat.enemyHpPerStage);
    threat["swarmCountPerStage"] = config.threat.swarmCountPerStage;
    threat["swarmIntervalScalePerStage"] = round(config.threat.swarmIntervalScalePerStage);
    threat["swarmSpeedPerStage"] = round(config.threat.swarmSpeedPerStage);
    threat["tankMaxAlivePerStage"] = config.threat.tankMaxAlivePerStage;
    threat["ringFromStage"] = config.threat.ringFromStage;
    threat["ringInterval"] = round(config.threat.ringInterval);
    threat["ringCount"] = config.threat.ringCount;
    threat["ringRadius"] = round(config.threat.ringRadius);
    auto& telegraph = section("telegraph");
    telegraph["lead"] = round(config.telegraph.lead);
    telegraph["maxChasers"] = config.telegraph.maxChasers;
    auto& heavy = section("heavySquare");
    heavy["fromStage"] = config.heavySquare.fromStage;
    heavy["interval"] = round(config.heavySquare.interval);
    heavy["count"] = config.heavySquare.count;
    heavy["scale"] = round(config.heavySquare.scale);
    heavy["hpScale"] = round(config.heavySquare.hpScale);
    heavy["acceleration"] = round(config.heavySquare.acceleration);
    heavy["maxSpeed"] = round(config.heavySquare.maxSpeed);
    auto& lunge = section("lungePentagon");
    lunge["fromStage"] = config.lungePentagon.fromStage;
    lunge["chance"] = round(config.lungePentagon.chance);
    lunge["acceleration"] = round(config.lungePentagon.acceleration);
    lunge["maxSpeed"] = round(config.lungePentagon.maxSpeed);
    lunge["lungeSpeed"] = round(config.lungePentagon.lungeSpeed);
    lunge["interval"] = round(config.lungePentagon.interval);
    lunge["windup"] = round(config.lungePentagon.windup);
    lunge["duration"] = round(config.lungePentagon.duration);
    auto& rival = section("rival");
    rival["enabled"] = config.rival.enabled;
    rival["hp"] = config.rival.hp;
    rival["exp"] = config.rival.exp;

    // 上の段は決まった順に、2段目は1行ずつ（comment を先に）書く。それより深いものは1行にまとめる。
    static constexpr std::array<const char*, 26> kOrder{
        "schemaVersion", "comment",   "exitOpenSeconds", "exitRadius",    "spawnRing", "shapes",   "triangleSwarm", "tanks",
        "threat",        "telegraph", "heavySquare",     "lungePentagon", "rival",     "experience", "load",        "choices",
        "heat",          "shield",    "forms",           "traits",        "relics",    "fusions",    "parts",       "heatVent",
        "partTuning",    "bigPentagon"};
    std::vector<std::string> keys(kOrder.begin(), kOrder.end());
    for (const auto& item : data.items()) {
        if (std::find(keys.begin(), keys.end(), item.key()) == keys.end()) {
            keys.push_back(item.key());
        }
    }
    std::string text = "{\n";
    bool first = true;
    for (const auto& key : keys) {
        if (!data.contains(key)) {
            continue;
        }
        const nlohmann::json& value = data.at(key);
        text += std::string(first ? "" : ",\n") + "  " + nlohmann::json(key).dump() + ": ";
        first = false;
        if (!value.is_object() || value.empty()) {
            text += value.dump();
            continue;
        }
        text += "{\n";
        std::vector<std::string> inner;
        if (value.contains("comment")) {
            inner.push_back("comment");
        }
        for (const auto& item : value.items()) {
            if (item.key() != "comment") {
                inner.push_back(item.key());
            }
        }
        for (size_t i = 0; i < inner.size(); ++i) {
            text += "    " + nlohmann::json(inner[i]).dump() + ": " + value.at(inner[i]).dump() + (i + 1 < inner.size() ? ",\n" : "\n");
        }
        text += "  }";
    }
    text += "\n}\n";
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        error = "survivor config could not be written: " + path;
        return false;
    }
    output << text;
    return static_cast<bool>(output);
}
} // namespace survivor
