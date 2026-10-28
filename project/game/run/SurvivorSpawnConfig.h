#pragma once
#include <algorithm>
#include <array>
#include <fstream>
#include <nlohmann/json.hpp>
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
    return true;
}
} // namespace survivor
