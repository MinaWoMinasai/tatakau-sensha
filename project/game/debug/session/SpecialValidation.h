#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief SpecialValidationの処理を担当し、同じプレイの共有状態を借用する。
class SpecialValidation {
public:
    /// @brief 借用するワールドを設定する。
    explicit SpecialValidation(GameWorld& world) : world_(world) {}
    /// @brief 特殊検証Fixtureを初期化する。
    void InitializeSpecialValidationFixture();

    /// @brief 特殊検証を更新する。
    /// @param dt この処理で進める経過時間（秒）。
    bool UpdateSpecialValidation(float dt);

private:
    GameWorld& world_;
};
} // namespace gameplay
