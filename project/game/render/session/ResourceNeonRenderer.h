#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief ResourceNeonRendererの処理を担当し、同じプレイの共有状態を借用する。
class ResourceNeonRenderer {
public:
    explicit ResourceNeonRenderer(GameWorld& world) : world_(world) {}

    /// @brief 経験値敵ネオン塗りつぶしモデルを描画する。
    void DrawExpEnemyNeonFillModels();

    /// @brief 経験値敵ネオン深度線を描画する。
    void DrawExpEnemyNeonDepthLines();

    /// @brief 経験値敵のネオンの図形を描画待ちに積む。
    void QueueExpEnemyNeonShapes(const myEngine::Vector3& cameraRight, const myEngine::Vector3& cameraUp,
                                 const myEngine::Vector3& cameraForward);

    /// @brief 近距離カメラ2Dであるか判定する。
    myEngine::Vector3 GetFloorVisibilityCenter() const;

    bool IsNearCamera2D(const myEngine::Vector3& worldPos, float halfWidth, float halfHeight, float margin = 0.0f) const;

private:
    GameWorld& world_;
};
} // namespace gameplay
