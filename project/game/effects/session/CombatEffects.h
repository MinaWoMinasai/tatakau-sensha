#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief CombatEffectsの処理を担当し、同じプレイの共有状態を借用する。
class CombatEffects {
public:
    explicit CombatEffects(GameWorld& world) : world_(world) {}

    /// @brief 死亡後処理パルスを発動させる。
    void TriggerDeathPostPulse(const myEngine::Vector3& worldPosition, float strength);

    /// @brief 死亡後処理パルスを更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateDeathPostPulse(float deltaTime);

    /// @brief ネオン三角形粒子を更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateNeonTriangleParticles(float deltaTime);

    /// @brief ネオンの三角形のパーティクルを描画待ちに積む。
    void QueueNeonTriangleParticles(const myEngine::Vector3& cameraRight, const myEngine::Vector3& cameraUp,
                                    const myEngine::Vector3& cameraForward);

private:
    GameWorld& world_;
    /// @brief 破片と火花を割合で間引くときの端数（計画書 p10 の16章の M3）。
    float tubeSparkCarry_ = 0.0f;
    float tubeDebrisCarry_ = 0.0f;
};
} // namespace gameplay
