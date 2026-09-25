#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>

namespace tankexp {
inline constexpr const char* kBalancePath="resources/configs/tankExpeditionBalance.json";
inline nlohmann::json DefaultBalance() {
    return {
        {"schemaVersion",1}, {"bossMaxHp",900}, {"defaultRandomSpawnEnabled",false},
        {"player",{{"maxHp",120},{"bulletDamage",4.0f},{"bulletSpeed",0.27f},{"reloadSpeed",20.0f},
            {"moveSpeed",0.23f},{"maxStamina",3.0f},{"staminaRecovery",0.9f},{"bodyDamage",3},{"healToFull",false}}},
        {"playerUpgrades",{{"maxHp",0.35f},{"bulletDamage",0.80f},{"bulletSpeed",0.20f},
            {"reloadSpeed",0.40f},{"moveSpeed",0.25f},{"healthRegen",0.12f},{"bodyDamage",0.10f},{"minReloadSpeed",3.0f}}},
        {"damage",{{"damageBlock",12},{"bossContact",18},{"expEnemyContact",8},{"shooterContact",8},
            {"shooterBullet",12},{"shooterBulletSpeed",0.40f},{"shooterFireInterval",1.6f},
            {"shooterDetectionRadius",64.0f},{"shooterTurnSpeed",5.5f}}},
        {"bossAttackDefault",{{"bulletSpeed",0.24f},{"bulletCount",5},{"cooldown",1.10f},{"damage",9},
            {"spreadAngleDeg",44.0f},{"bulletHp",5.0f},{"bulletPenetration",3.0f},{"randomSpread",false},{"pattern",0}}},
        {"enemySystem",{{"expEnemyHostileToBoss",false},{"bossExpEnemyDamage",6},{"bossHealOnExpEnemyKill",0},
            {"bossKillsPerLevel",10},{"bossMaxHpGainPerLevel",0},{"bossDamageGainPerLevel",0},
            {"bossLevelingModeEnabled",false},{"bossLevelingEnterDistance",12.0f},
            {"bossLevelingExitDistance",8.0f},{"bossLevelingSearchRadius",40.0f},{"bossAimTurnHalfSeconds",1.0f}}}
    };
}
// Each accepted key has a finite bound. Missing/wrong-type/non-finite values
// recover independently, so a hand-edited bad field cannot poison the simulation.
inline nlohmann::json SanitizeBalance(const nlohmann::json& source) {
    auto result=DefaultBalance();
    if(!source.is_object()) return result;
    auto number=[&](const char* group,const char* key,double low,double high) {
        const auto& input=group[0]?source.value(group,nlohmann::json::object()):source;
        auto& output=group[0]?result[group]:result;
        if(!input.is_object()||!input.contains(key)||!input[key].is_number()) return;
        const double value=input[key].get<double>();
        if(!std::isfinite(value)) return;
        const double safe=(std::clamp)(value,low,high);
        if(output[key].is_number_integer()) output[key]=static_cast<int>(safe); else output[key]=safe;
    };
    number("","bossMaxHp",1,100000);
    number("player","maxHp",1,9999); number("player","bulletDamage",1,999);
    number("player","bulletSpeed",0.05,2); number("player","reloadSpeed",3,120);
    number("player","moveSpeed",0.05,1); number("player","maxStamina",0,20);
    number("player","staminaRecovery",0,20); number("player","bodyDamage",1,999);
    for(const char* key:{"maxHp","bulletDamage","bulletSpeed","moveSpeed","healthRegen","bodyDamage"})
        number("playerUpgrades",key,0,3);
    number("playerUpgrades","reloadSpeed",0,0.9); number("playerUpgrades","minReloadSpeed",1,60);
    for(const char* key:{"damageBlock","bossContact","expEnemyContact","shooterContact","shooterBullet"})
        number("damage",key,1,999);
    number("damage","shooterBulletSpeed",0.05,2); number("damage","shooterFireInterval",0.3,10);
    number("damage","shooterDetectionRadius",4,100); number("damage","shooterTurnSpeed",0.1,30);
    number("bossAttackDefault","bulletSpeed",0.05,2); number("bossAttackDefault","bulletCount",1,32);
    number("bossAttackDefault","cooldown",0.25,10); number("bossAttackDefault","damage",1,999);
    number("bossAttackDefault","bulletHp",0,100); number("bossAttackDefault","bulletPenetration",0,100);
    // Player healing is an explicit transient editor action, never saved as a
    // permanent on-load heal toggle. Runtime room changes preserve current HP.
    return result;
}
inline bool LoadBalance(const std::string& path,nlohmann::json& output,std::string& error) {
    std::ifstream input(path);
    if(!input) { error="Cannot open "+path; return false; }
    try {
        nlohmann::json parsed; input>>parsed;
        if(!parsed.is_object()) {error="Balance JSON must be an object."; return false;}
        output=SanitizeBalance(parsed); error.clear(); return true;
    } catch(const std::exception& e) { error=e.what(); return false; }
}
inline bool SaveBalance(const std::string& path,const nlohmann::json& input,std::string& error) {
    std::ofstream output(path,std::ios::trunc);
    if(!output) {error="Cannot write "+path;return false;}
    output<<SanitizeBalance(input).dump(2)<<'\n'; output.close();
    if(!output) {error="Writing balance JSON failed.";return false;}
    error.clear();return true;
}
struct RoomBalance {
    float hpScale;
    float contactScale;
    float bulletSpeedScale;
    float fireIntervalScale;
    int bossPressure;
};
inline constexpr RoomBalance GetRoomBalance(int index) {
    constexpr RoomBalance rooms[]={
        {0.65f,0.65f,0.72f,1.55f,0}, {0.85f,0.82f,0.87f,1.20f,0},
        {1.10f,1.00f,1.00f,1.00f,1}, {1.35f,1.15f,1.10f,0.88f,2},
        {1.60f,1.30f,1.18f,0.78f,2}
    };
    return rooms[(std::clamp)(index,0,4)];
}
} // namespace tankexp
