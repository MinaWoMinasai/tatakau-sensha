#pragma once
#include <algorithm>
#include <fstream>
#include <string>
#include <utility>
#include <vector>
#include <nlohmann/json.hpp>
#include "Struct.h"

/// @brief 自機の履帯戦車の形（計画書 p10 の10章・11章、16章の M1）。部品ごとの輪郭と高さを持つ。
/// 座標は機体の前が +x、左右が y。1 が1ワールド単位。真上の見え方（M1）では高さを使わず、立体の線（M6）で使う。
namespace playertank {
struct Part {
    std::string name;
    /// @brief 砲塔と一緒に狙う向きへ回るか（偽なら車体と一緒に進む向きへ回る）。
    bool aims = false;
    /// @brief 下と上の輪郭。上を省くと下と同じ。真上では、下で黒く塗り、上を線で描く。
    std::vector<myEngine::Vector2> bottom;
    std::vector<myEngine::Vector2> top;
    /// @brief 床からの高さ（下・上）。
    float low = 0.0f;
    float high = 0.0f;
    /// @brief 線の太さ（倍）。0 なら黒い面だけ。
    float line = 1.0f;
};

/// @brief 履帯の光：左右の帯の位置、流れる光の数・長さ、帯そのものの淡さ。
struct Treads {
    float side = 0.78f;
    float half = 0.95f;
    int links = 5;
    float length = 0.16f;
    float line = 1.15f;
    float rail = 0.35f;
    float low = 0.0f;
    float high = 0.4f;
};

/// @brief 砲身：付け根と先（砲塔の中心から）、付け根と先の半分の幅。砲の数・向き・反動は武器の配置に従う。
struct Barrel {
    float from = 0.35f;
    float to = 1.45f;
    float halfBase = 0.13f;
    float halfTip = 0.104f;
    float line = 0.85f;
    float low = 0.55f;
    float high = 0.75f;
};

struct Shape {
    std::vector<Part> parts;
    Treads treads;
    Barrel barrel;
};

/// @brief 設定ファイルが読めないときの形（player_tank_shape.json と同じ）。
inline Shape Default()
{
    Shape shape;
    Part base;
    base.name = "base";
    base.bottom = {{0.95f, -0.8f}, {-0.95f, -0.8f}, {-0.95f, 0.8f}, {0.95f, 0.8f}};
    base.high = 0.4f;
    base.line = 0.0f;
    Part hull;
    hull.name = "hull";
    hull.bottom = {{0.95f, -0.3f}, {0.7f, -0.5f}, {-0.8f, -0.5f}, {-0.8f, 0.5f}, {0.7f, 0.5f}, {0.95f, 0.3f}};
    hull.low = 0.15f;
    hull.high = 0.5f;
    hull.line = 0.9f;
    Part turret;
    turret.name = "turret";
    turret.aims = true;
    turret.bottom = {{0.42f, 0.0f}, {0.21f, 0.3637f}, {-0.21f, 0.3637f}, {-0.42f, 0.0f}, {-0.21f, -0.3637f}, {0.21f, -0.3637f}};
    turret.low = 0.5f;
    turret.high = 0.8f;
    turret.line = 0.85f;
    shape.parts = {base, hull, turret};
    for (Part& part : shape.parts) {
        part.top = part.bottom;
    }
    return shape;
}

/// @brief JSON から読む。読めない所は既定の形のまま。
inline Shape Load(const std::string& path)
{
    Shape shape = Default();
    std::ifstream file(path);
    if (!file.is_open()) {
        return shape;
    }
    const nlohmann::json root = nlohmann::json::parse(file, nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        return shape;
    }
    auto number = [](const nlohmann::json& from, const char* key, float& out) {
        if (from.contains(key) && from.at(key).is_number()) {
            out = from.at(key).get<float>();
        }
    };
    auto text = [](const nlohmann::json& from, const char* key) {
        return from.contains(key) && from.at(key).is_string() ? from.at(key).get<std::string>() : std::string{};
    };
    auto outline = [](const nlohmann::json& from, const char* key, std::vector<myEngine::Vector2>& out) {
        if (!from.contains(key) || !from.at(key).is_array()) {
            return;
        }
        std::vector<myEngine::Vector2> points;
        for (const auto& point : from.at(key)) {
            if (!point.is_array() || point.size() != 2 || !point[0].is_number() || !point[1].is_number()) {
                return;
            }
            points.push_back({point[0].get<float>(), point[1].get<float>()});
        }
        if (points.size() >= 3 && points.size() <= 32) {
            out = std::move(points);
        }
    };
    if (root.contains("parts") && root.at("parts").is_array()) {
        std::vector<Part> parts;
        for (const auto& item : root.at("parts")) {
            if (!item.is_object()) {
                continue;
            }
            Part part;
            part.name = text(item, "name");
            part.aims = text(item, "turn") == "aim";
            outline(item, "bottom", part.bottom);
            part.top = part.bottom;
            outline(item, "top", part.top);
            number(item, "low", part.low);
            number(item, "high", part.high);
            number(item, "line", part.line);
            if (!part.bottom.empty()) {
                parts.push_back(std::move(part));
            }
        }
        if (!parts.empty()) {
            shape.parts = std::move(parts);
        }
    }
    if (root.contains("treads") && root.at("treads").is_object()) {
        const auto& treads = root.at("treads");
        number(treads, "side", shape.treads.side);
        number(treads, "half", shape.treads.half);
        number(treads, "length", shape.treads.length);
        number(treads, "line", shape.treads.line);
        number(treads, "rail", shape.treads.rail);
        number(treads, "low", shape.treads.low);
        number(treads, "high", shape.treads.high);
        float links = static_cast<float>(shape.treads.links);
        number(treads, "links", links);
        shape.treads.links = (std::clamp)(static_cast<int>(links), 0, 24);
    }
    if (root.contains("barrel") && root.at("barrel").is_object()) {
        const auto& barrel = root.at("barrel");
        number(barrel, "from", shape.barrel.from);
        number(barrel, "to", shape.barrel.to);
        number(barrel, "halfBase", shape.barrel.halfBase);
        number(barrel, "halfTip", shape.barrel.halfTip);
        number(barrel, "line", shape.barrel.line);
        number(barrel, "low", shape.barrel.low);
        number(barrel, "high", shape.barrel.high);
    }
    return shape;
}

/// @brief 起動後に一度だけ読み、以後は同じ形を返す。
inline const Shape& Get()
{
    static const Shape shape = Load("resources/configs/player_tank_shape.json");
    return shape;
}
} // namespace playertank
