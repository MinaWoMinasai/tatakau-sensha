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

    // 経験値の欠片とレベル。
    float magnetRadius = 5.0f;
    float collectRadius = 1.2f;
    int valuePerShard = 30;
    int maxShards = 600;
    float levelBase = 25.0f;
    float levelGrowth = 30.0f;
    float levelPower = 1.4f;
    int maxLevel = 45;

    // 部品と3択。
    std::array<PartDefinition, kPartCount> parts = DefaultParts();
    float choiceSlowTimeScale = 0.25f;
    bool choiceAutoOpen = false;
    float choiceLightBias = 2.0f;

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
    if (data.contains("choices")) {
        const auto& choices = data.at("choices");
        number(choices, "slowTimeScale", config.choiceSlowTimeScale);
        number(choices, "lightBias", config.choiceLightBias);
        if (choices.contains("autoOpen") && choices.at("autoOpen").is_boolean()) {
            config.choiceAutoOpen = choices.at("autoOpen").get<bool>();
        }
        config.choiceSlowTimeScale = (std::clamp)(config.choiceSlowTimeScale, 0.05f, 1.0f);
    }
    if (data.contains("parts") && data.at("parts").is_object()) {
        // 部品ごとの1段の上がり幅（割合）だけを上書きする。名前は PartDefinition::key。
        for (auto& part : config.parts) {
            const auto& parts = data.at("parts");
            if (parts.contains(part.key)) {
                number(parts.at(part.key), "perLevel", part.perLevel);
            }
        }
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
} // namespace survivor
