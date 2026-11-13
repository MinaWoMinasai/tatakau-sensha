#pragma once
#include "game/run/SurvivorHeroes.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace survivor {
/// @brief 機体の遊び方の出撃の外の成長（計画書 p4 の K6）。ローグライトなので強化は残さず、残るのは機体の解放と記録だけ。
struct HeroSave {
    /// @brief 解放ポイント（スコアから入る）。
    int points = 0;
    /// @brief 解放した機体のキー（最初から使える機体は入れない）。
    std::vector<std::string> unlocked;
    /// @brief 一度でもボスを倒した機体のキー（初クリアのボーナスは1回だけ）。
    std::vector<std::string> cleared;
    /// @brief 最後に選んだ機体。
    std::string selected;
    int sorties = 0;
    int clears = 0;

    bool operator==(const HeroSave&) const = default;
};

/// @brief その機体を使えるか（最初から使える、または解放した）。
inline bool IsHeroUnlocked(const HeroSave& save, const HeroDefinition& hero)
{
    return hero.unlockCost <= 0 || std::find(save.unlocked.begin(), save.unlocked.end(), hero.key) != save.unlocked.end();
}

/// @brief 次に解放できる機体の番号（計画書 p4 の H 決定：最初の1体は決まり、その後は2〜3体から選ぶ）。
/// @note まだ解放していない機体のうち、unlockFirst の物があればそれだけ。無ければ並び順に3体まで。
inline std::vector<int> HeroUnlockCandidates(const HeroSave& save, const std::vector<HeroDefinition>& heroes)
{
    std::vector<int> first, others;
    for (size_t i = 0; i < heroes.size(); ++i) {
        if (IsHeroUnlocked(save, heroes[i])) {
            continue;
        }
        (heroes[i].unlockFirst ? first : others).push_back(static_cast<int>(i));
    }
    if (!first.empty()) {
        return first;
    }
    if (others.size() > 3) {
        others.resize(3);
    }
    return others;
}

/// @brief path から読む。ファイルが無いときは false（error は空）。壊れていれば false と理由。
inline bool LoadHeroSave(const std::string& path, HeroSave& out, std::string& error)
{
    error.clear();
    std::ifstream input{std::filesystem::path(path)};
    if (!input) {
        return false;
    }
    const nlohmann::json data = nlohmann::json::parse(input, nullptr, false);
    if (data.is_discarded() || !data.is_object()) {
        error = "機体のセーブが壊れている";
        return false;
    }
    HeroSave save;
    save.points = (std::max)(0, data.value("points", 0));
    save.selected = data.value("selected", std::string());
    save.sorties = data.value("sorties", 0);
    save.clears = data.value("clears", 0);
    auto readKeys = [&](const char* key, std::vector<std::string>& keys) {
        if (data.contains(key) && data.at(key).is_array()) {
            for (const auto& item : data.at(key)) {
                if (item.is_string()) {
                    keys.push_back(item.get<std::string>());
                }
            }
        }
    };
    readKeys("unlocked", save.unlocked);
    readKeys("cleared", save.cleared);
    out = std::move(save);
    return true;
}

/// @brief path へ書く。途中で落ちても前のセーブが壊れないよう、別のファイルに書いてから置き換える。
inline bool SaveHeroSave(const std::string& path, const HeroSave& save, std::string& error)
{
    nlohmann::json data = nlohmann::json::object();
    data["schemaVersion"] = 1;
    data["points"] = save.points;
    data["unlocked"] = save.unlocked;
    data["cleared"] = save.cleared;
    data["selected"] = save.selected;
    data["sorties"] = save.sorties;
    data["clears"] = save.clears;
    const std::filesystem::path target = std::filesystem::path(path);
    std::error_code ignored;
    if (target.has_parent_path()) {
        std::filesystem::create_directories(target.parent_path(), ignored);
    }
    std::filesystem::path temporary = target;
    temporary += ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            error = "機体のセーブを書けない";
            return false;
        }
        output << data.dump(2) << "\n";
        if (!output) {
            error = "機体のセーブを書けない";
            return false;
        }
    }
    std::error_code renameError;
    std::filesystem::rename(temporary, target, renameError);
    if (renameError) {
        error = "機体のセーブを置き換えられない";
        return false;
    }
    error.clear();
    return true;
}
} // namespace survivor
