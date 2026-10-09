#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief GameplayRendererの処理を担当し、同じプレイの共有状態を借用する。
class GameplayRenderer {
public:
    /// @brief 借用するワールドを設定する。
    explicit GameplayRenderer(GameWorld& world) : world_(world) {}
    /// @brief 描画
    void Draw();

    /// @brief 影を描画する。
    void DrawShadow();

    /// @brief 後処理演出3Dを描画する。
    void DrawPostEffect3D();

    /// @brief 後の後処理演出3Dを描画する。
    void DrawAfterPostEffect3D();

private:
    GameWorld& world_;
};
} // namespace gameplay
