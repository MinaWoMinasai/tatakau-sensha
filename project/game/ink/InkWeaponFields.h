#pragma once
#include "WeaponDefinition.h"
#include <vector>

namespace ink {
template<class T> struct TypedFloatField {
    const char* key;
    const char* label;
    const char* unit;
    const char* group;
    float T::*member;
    float min;
    float max;
};
template<class T> struct TypedIntField {
    const char* key;
    const char* label;
    const char* unit;
    const char* group;
    int T::*member;
    int min;
    int max;
};
template<class T> struct TypedBoolField {
    const char* key;
    const char* label;
    const char* unit;
    const char* group;
    bool T::*member;
    bool editable = true;
};
const std::vector<TypedFloatField<ShooterWeaponParams>>& ShooterFloatFields();
const std::vector<TypedIntField<ShooterWeaponParams>>& ShooterIntFields();
const std::vector<TypedBoolField<ShooterWeaponParams>>& ShooterBoolFields();
const std::vector<TypedFloatField<StringerWeaponParams>>& StringerFloatFields();
const std::vector<TypedIntField<StringerWeaponParams>>& StringerIntFields();
const std::vector<TypedBoolField<StringerWeaponParams>>& StringerBoolFields();
} // namespace ink
