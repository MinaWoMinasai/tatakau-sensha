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
    /// @brief ボス戦のカメラ（通常距離の追従・登場の寄り・撃破の寄り）と、撃破の連続爆発を進める。
    /// @return 揺れを加える前のカメラ注視点。profileの距離・傾き・画角も設定する。
    cg2::Vector3 UpdateBossCinematicCamera(float baseDeltaTime, neondepth::CameraProfile& profile);

    GameWorld& world_;
};
} // namespace gameplay
