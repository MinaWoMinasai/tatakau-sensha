#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief DepthEncounterの処理を担当し、同じプレイの共有状態を借用する。
class DepthEncounter {
public:
    /// @brief 借用するワールドを設定する。
    explicit DepthEncounter(GameWorld& world) : world_(world) {}
    bool LoadNeonDepthConfig(bool hotReload = false);

    bool IsNeonDepthEncounterActive() const;

    void ConfigureNeonDepthEncounter(bool completedFixtureRoomReset = false);

    bool UpdateNeonDepthCamera(float baseDeltaTime);

    void RestoreNeonDepthCamera();

    bool UpdateNeonDepthIntro(float baseDeltaTime);

    void UpdateNeonDepthEffects();

private:
    GameWorld& world_;
};
} // namespace gameplay
