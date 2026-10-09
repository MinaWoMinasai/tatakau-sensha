#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief RunVisualSettingsの処理を担当し、同じプレイの共有状態を借用する。
class RunVisualSettings {
public:
    /// @brief 借用するワールドを設定する。
    explicit RunVisualSettings(GameWorld& world) : world_(world) {}
    /// @brief 戦車遠征表示情報を初期化する。
    void InitializeTankRunVisuals();

private:
    GameWorld& world_;
};
} // namespace gameplay
