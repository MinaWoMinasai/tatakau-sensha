#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief CombatEffectsの処理を担当し、同じプレイの共有状態を借用する。
class CombatEffects {
public:
    /// @brief 借用するワールドを設定する。
    explicit CombatEffects(GameWorld& world) : world_(world) {}
    /// @brief 死亡後処理パルスを発動させる。
    void TriggerDeathPostPulse(const cg2::Vector3& worldPosition, float strength);

    /// @brief 死亡後処理パルスを更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateDeathPostPulse(float deltaTime);

    /// @brief ネオン三角形粒子を更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateNeonTriangleParticles(float deltaTime);

    /// @brief ネオン三角形粒子を後で処理するために予約する。
    void QueueNeonTriangleParticles(const cg2::Vector3& cameraRight, const cg2::Vector3& cameraUp, const cg2::Vector3& cameraForward);

private:
    GameWorld& world_;
};
} // namespace gameplay
