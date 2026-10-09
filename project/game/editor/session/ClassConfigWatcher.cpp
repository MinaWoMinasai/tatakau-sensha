#include "game/editor/session/ClassConfigWatcher.h"
#include "game/session/GameplaySystems.h"
#include "game/weapon/CombatTypes.h"
#include "RuntimeProfiler.h"
#include "StartupTrace.h"
#include "GameStartMode.h"
#include "game/ui/TankCombatNeonGeometry.h"
#include "game/effects/TankSpecialNeonGeometry.h"
#include "CollisionConfig.h"
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include "game/session/GameplayHelpers.h"

namespace gameplay {

using namespace detail;

void ClassConfigWatcher::InitializePlayerClassConfigWatch()
{
    constexpr const char* kConfigPath = "resources/configs/playerClasses.json";
    world_.combat.playerClassConfigPollTimer_ = 0.0f;
    world_.combat.playerClassConfigDebounceTimer_ = 0.0f;
    world_.combat.playerClassConfigReloadPending_ = false;
    world_.combat.playerClassConfigHasObservedWriteTime_ = false;
    world_.combat.playerClassConfigHasLoadedWriteTime_ = false;

    std::error_code error;
    const auto writeTime = std::filesystem::last_write_time(kConfigPath, error);
    if (error) {
        return;
    }
    world_.combat.playerClassConfigObservedWriteTime_ = writeTime;
    world_.combat.playerClassConfigLoadedWriteTime_ = writeTime;
    world_.combat.playerClassConfigHasObservedWriteTime_ = true;
    world_.combat.playerClassConfigHasLoadedWriteTime_ = true;
}

void ClassConfigWatcher::UpdatePlayerClassConfigWatch(float deltaTime)
{
    constexpr const char* kConfigPath = "resources/configs/playerClasses.json";
    constexpr float kPollInterval = 0.10f;
    constexpr float kDebounceDuration = 0.25f;

    if (world_.combat.playerClassConfigReloadPending_) {
        world_.combat.playerClassConfigDebounceTimer_ += (std::max)(0.0f, deltaTime);
    }
    world_.combat.playerClassConfigPollTimer_ += (std::max)(0.0f, deltaTime);
    if (world_.combat.playerClassConfigPollTimer_ < kPollInterval) {
        return;
    }
    world_.combat.playerClassConfigPollTimer_ = 0.0f;

    std::error_code error;
    const auto writeTime = std::filesystem::last_write_time(kConfigPath, error);
    if (error) {
        return;
    }
    if (!world_.combat.playerClassConfigHasObservedWriteTime_) {
        world_.combat.playerClassConfigObservedWriteTime_ = writeTime;
        world_.combat.playerClassConfigHasObservedWriteTime_ = true;
        world_.combat.playerClassConfigReloadPending_ = true;
        world_.combat.playerClassConfigDebounceTimer_ = 0.0f;
        return;
    }
    if (writeTime != world_.combat.playerClassConfigObservedWriteTime_) {
        world_.combat.playerClassConfigObservedWriteTime_ = writeTime;
        world_.combat.playerClassConfigReloadPending_ = true;
        world_.combat.playerClassConfigDebounceTimer_ = 0.0f;
        return;
    }
    if (!world_.combat.playerClassConfigReloadPending_ || world_.combat.playerClassConfigDebounceTimer_ < kDebounceDuration) {
        return;
    }

    ReloadPlayerClassConfig(true);
}

bool ClassConfigWatcher::ReloadPlayerClassConfig(bool automatic)
{
    constexpr const char* kConfigPath = "resources/configs/playerClasses.json";
    const bool succeeded = world_.resources.player_ && world_.resources.player_->ReloadPlayerClassConfigs(kConfigPath);
    world_.combat.playerClassConfigReloadPending_ = false;
    world_.combat.playerClassConfigDebounceTimer_ = 0.0f;

    std::error_code error;
    const auto writeTime = std::filesystem::last_write_time(kConfigPath, error);
    if (!error) {
        world_.combat.playerClassConfigObservedWriteTime_ = writeTime;
        world_.combat.playerClassConfigHasObservedWriteTime_ = true;
        if (succeeded) {
            world_.combat.playerClassConfigLoadedWriteTime_ = writeTime;
            world_.combat.playerClassConfigHasLoadedWriteTime_ = true;
        }
    }

    if (automatic) {
        std::cerr << "[PlayerClass] Auto reload " << (succeeded ? "succeeded." : "failed.") << std::endl;
    }
    world_.combatFlow->SetEventCallout(
        succeeded ? (automatic ? "PLAYER CONFIG AUTO RELOADED" : "PLAYER CONFIG RELOADED") : "PLAYER CONFIG RELOAD FAILED", 1.35f);
    return succeeded;
}
} // namespace gameplay
