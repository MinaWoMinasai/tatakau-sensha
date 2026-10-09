#pragma once
#include "game/player/ui/PlayerUiState.h"

/// @brief PlayerClassEditorの表示と入力を担当し、必要な自機の状態を借用する。
class PlayerClassEditor {
public:
    using BodyShape = Player::BodyShape;
    /// @brief 自機と画面状態を接続する。
    PlayerClassEditor(Player& player, PlayerUiState& ui) : player_(player), ui_(ui) {}
    /// @brief 機体設定の選択・複製・編集・JSON保存を行う制作画面を表示する。
    /// @note USE_IMGUIが有効な構成で表示する。戦闘の更新と分けて呼ぶ。
    void DrawPlayerClassEditor();

private:
    Player& player_;
    PlayerUiState& ui_;
};
