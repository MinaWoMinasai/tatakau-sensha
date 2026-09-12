#pragma once
#include "InkWeaponFields.h"
#include <filesystem>
#include <string>
#include <vector>

namespace ink {
bool ValidateWeapon(const WeaponDefinition& definition, std::string& error);

// CPU-only definitions and I/O. The scene separately owns drafts and the
// currently equipped snapshot; no references survive successful mutations.
class WeaponCatalog {
public:
    static WeaponCatalog Defaults();
    const std::vector<WeaponDefinition>& Entries() const { return entries_; }
    const WeaponDefinition* Find(const std::string& id) const;
    const std::string& DefaultId() const { return defaultId_; }
    bool Upsert(const WeaponDefinition& definition, std::string& error);
    std::string Clone(const std::string& sourceId, const std::string& newName, std::string& error);
    bool Load(const std::filesystem::path& path, std::string& error);
    bool Save(const std::filesystem::path& path, std::string& error) const;
private:
    std::vector<WeaponDefinition> entries_;
    std::string defaultId_;
};
} // namespace ink
