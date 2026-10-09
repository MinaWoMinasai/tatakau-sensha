#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief ExpeditionBalanceEditorの処理を担当し、同じプレイの共有状態を借用する。
class ExpeditionBalanceEditor {
public:
    /// @brief 借用するワールドを設定する。
    explicit ExpeditionBalanceEditor(GameWorld& world) : world_(world) {}
    /// @brief 戦車遠征性能調整を初期化する。
    void InitializeTankExpeditionBalance();

    /// @brief 戦車遠征部屋性能調整を現在の状態へ適用する。
    void ApplyTankExpeditionRoomBalance();

    /// @brief 戦車遠征性能調整編集画面を更新する。
    void UpdateTankExpeditionBalanceEditor();

    /// @brief 戦車遠征性能調整編集画面を描画する。
    void DrawTankExpeditionBalanceEditor();

private:
    GameWorld& world_;
};
} // namespace gameplay
