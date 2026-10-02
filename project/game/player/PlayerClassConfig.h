#pragma once
#include "game/weapon/WeaponMount.h"
#include <vector>

enum class ClassType {

    // 第一段階
    Basic,

    // 第二段階
    Twin,       // 2連装
    MachineGun, // バラつき・高速連射
    Overseer,   // ドローン使い

    // 第三段階
    Triple,   // 三連砲
    Assassin, // ステルスで移動できる
    Bounder,  // 反射弾を使える

    // 第四段階
    Ninja,   // ステルスしながら攻撃
    Smasher, // 強力な近接攻撃
    Summoner,
};

enum class PlayerBodyShape {
    Circle = 0,
    Box,
    Triangle,
    Pentagon,
};

/// @brief 機体の制作設定を表す。識別子・基礎性能・外観・砲塔設定を保持する。
struct PlayerClassConfig {
    ClassType type = ClassType::Basic;
    std::string id = "Basic";
    std::string displayName = "Basic";
    int requiredRank = 1;
    bool usesDrone = false;
    int maxDrones = 7;
    float reloadScale = 1.0f;
    float bulletSpeedScale = 1.0f;
    float bulletDamageScale = 1.0f;
    int bulletCount = 1;
    float spreadAngleDeg = 10.0f;
    bool randomSpread = true;
    bool reflect = false;
    bool penetrate = false;
    bool fireAllBarrels = false;
    bool alternateBarrels = false;
    float recoilPower = 0.01f;
    std::string specialActionId = "perfect_dodge";
    float specialActionCooldownScale = 1.0f;
    float specialActionStaminaCost = 1.0f;
    float saberCounterWindow = 0.28f;
    float saberCounterDamageScale = 2.5f;
    float saberCounterRangeScale = 1.35f;
    PlayerBodyShape bodyShape = PlayerBodyShape::Circle;
    cg2::Vector2 bodyScale = {1.0f, 1.0f};
    cg2::Vector4 bodyFillColor = {0.18f, 0.28f, 0.34f, 0.38f};
    cg2::Vector4 bodyOutlineColor = {0.50f, 1.0f, 0.35f, 1.0f};
    std::vector<WeaponMountConfig> barrels;
};

/// @brief 従来の機体種類をJSONで使う識別子へ変換して返す。
const char* ClassTypeToString(ClassType type);
