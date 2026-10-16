#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief DepthEncounterの処理を担当し、同じプレイの共有状態を借用する。
class DepthEncounter {
public:
    /// @brief 借用するワールドを設定する。
    explicit DepthEncounter(GameWorld& world) : world_(world) {}

    /// @brief Depthボスの設定ファイルを読み込む。hotReloadなら戦闘中の値も次の攻撃から差し替える。
    bool LoadNeonDepthConfig(bool hotReload = false);

    bool IsNeonDepthEncounterActive() const;

    /// @brief Depthボス戦を設定する。
    void ConfigureNeonDepthEncounter(bool completedFixtureRoomReset = false);

    /// @brief Depthボス戦用のカメラを更新する。ボス戦カメラを使った場合はtrueを返す。
    bool UpdateNeonDepthCamera(float baseDeltaTime);

    /// @brief Depthボス戦の前のカメラ設定に戻す。
    void RestoreNeonDepthCamera();

    /// @brief Depthボスの登場演出を進める。演出中はtrueを返す。
    bool UpdateNeonDepthIntro(float baseDeltaTime);

    /// @brief Depthボスのエフェクトを更新する。
    void UpdateNeonDepthEffects();

private:
    /// @brief ボス戦のカメラ（通常距離の追従・登場の寄り・撃破の寄り）と、撃破の連続爆発を進める。
    /// @return 揺れを加える前のカメラ注視点。profileの距離・傾き・画角も設定する。
    myEngine::Vector3 UpdateBossCinematicCamera(float baseDeltaTime, neondepth::CameraProfile& profile);

    GameWorld& world_;
};
} // namespace gameplay
