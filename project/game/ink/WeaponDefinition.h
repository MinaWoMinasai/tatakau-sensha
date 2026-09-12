#pragma once
#include "ShooterWeaponParams.h"
#include "StringerWeaponParams.h"
#include <string>

namespace ink {
enum class WeaponClass { Shooter, Stringer };

// Owned strings remain valid after JSON parsing, catalog replacement and cloning.
// Only the payload selected by type is serialized and used for gameplay.
struct WeaponDefinition {
    std::string id;
    std::string displayNameJa;
    WeaponClass type = WeaponClass::Shooter;
    std::string reference;
    ShooterWeaponParams shooter;
    StringerWeaponParams stringer;
};
} // namespace ink
