#pragma once
#include "PlayerClassConfig.h"
#include <unordered_map>

// Owns authored class values and their order. Runtime equipment stays in Player.
class PlayerClassCatalog {
public:
    // A failed load leaves all values, order and borrowed pointers unchanged.
    // A successful load replaces the values; callers must acquire them again.
    bool Load(const std::string& path);
    void Save(const std::string& path) const;
    static PlayerClassConfig CreateDefaultConfig(ClassType type);
    void ResetToDefaults();

    const PlayerClassConfig* Find(ClassType type) const;
    const PlayerClassConfig* Find(const std::string& id) const;
    PlayerClassConfig* FindMutable(const std::string& id);
    const std::vector<std::string>& OrderedIds() const { return classOrder_; }

    // Assignment preserves the first authored position of an existing ID.
    // Insertion preserves pointers to existing values (unordered_map contract).
    void InsertOrAssign(const PlayerClassConfig& config);
    // Accepts an ID borrowed from the value being erased. Its pointer expires.
    bool Erase(const std::string& id);
    // Transfers ownership of the values and order together.
    void Swap(PlayerClassCatalog& other) noexcept;

private:
    std::unordered_map<std::string, PlayerClassConfig> classConfigs_;
    std::vector<std::string> classOrder_;
};
