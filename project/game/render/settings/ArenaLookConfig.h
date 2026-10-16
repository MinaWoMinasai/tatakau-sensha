#pragma once
#include "game/scene/IScene.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace arenalook {
inline constexpr const char* kConfigPath = "resources/configs/arenaLook.json";

/// @brief ベクター管モニター風の既定値。参考映像の歪み・枠・走査線・残光・星雲の見え方に合わせている。
inline IScene::ArenaLookSettings Defaults()
{
    IScene::ArenaLookSettings look;
    look.enabled = true;
    look.curvature = 0.20f;
    look.bezelMargin = 0.012f;
    look.bezelRadius = 0.05f;
    look.bezelSoftness = 0.006f;
    look.vignetteIntensity = 0.35f;
    look.vignetteScale = 1.0f;
    look.chromAbAmount = 0.0025f;
    look.scanlineIntensity = 0.14f;
    look.scanlinePeriodPixels = 3.0f;
    look.noiseIntensity = 0.0f;
    look.grainIntensity = 0.06f;
    look.phosphorDecay = 0.72f;
    look.nebulaIntensity = 0.22f;
    look.nebulaScale = 0.05f;
    look.nebulaDrift = 0.004f;
    look.nebulaColorA = {0.30f, 0.05f, 0.42f};
    look.nebulaColorB = {0.06f, 0.02f, 0.30f};
    return look;
}

inline IScene::ArenaLookSettings Load()
{
    IScene::ArenaLookSettings look = Defaults();
    std::ifstream file(kConfigPath);
    if (!file) return look;
    const nlohmann::json root = nlohmann::json::parse(file, nullptr, false);
    if (!root.is_object()) return look;
    auto number = [&](const char* key, float& value) {
        if (root.contains(key) && root.at(key).is_number()) value = root.at(key).get<float>();
    };
    auto color = [&](const char* key, myEngine::Vector3& value) {
        if (!root.contains(key) || !root.at(key).is_array() || root.at(key).size() != 3) return;
        const auto& array = root.at(key);
        if (array[0].is_number() && array[1].is_number() && array[2].is_number())
            value = {array[0].get<float>(), array[1].get<float>(), array[2].get<float>()};
    };
    if (root.contains("enabled") && root.at("enabled").is_boolean()) look.enabled = root.at("enabled").get<bool>();
    number("curvature", look.curvature);
    number("bezelMargin", look.bezelMargin);
    number("bezelRadius", look.bezelRadius);
    number("bezelSoftness", look.bezelSoftness);
    number("vignetteIntensity", look.vignetteIntensity);
    number("vignetteScale", look.vignetteScale);
    number("chromAbAmount", look.chromAbAmount);
    number("scanlineIntensity", look.scanlineIntensity);
    number("scanlinePeriodPixels", look.scanlinePeriodPixels);
    number("noiseIntensity", look.noiseIntensity);
    number("grainIntensity", look.grainIntensity);
    number("phosphorDecay", look.phosphorDecay);
    number("nebulaIntensity", look.nebulaIntensity);
    number("nebulaScale", look.nebulaScale);
    number("nebulaDrift", look.nebulaDrift);
    color("nebulaColorA", look.nebulaColorA);
    color("nebulaColorB", look.nebulaColorB);
    return look;
}

/// @brief 起動後に一度だけ設定ファイルを読み、以後は同じ値を返す。
inline const IScene::ArenaLookSettings& Get()
{
    static const IScene::ArenaLookSettings look = Load();
    return look;
}
} // namespace arenalook
