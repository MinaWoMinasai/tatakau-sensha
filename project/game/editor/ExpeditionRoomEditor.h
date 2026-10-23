#pragma once
#include "game/run/TankExpeditionRooms.h"

namespace tankexp {
/// @brief 遠征部屋の地形・出現配置を編集・保存する。
class ExpeditionRoomEditor {
public:
    // 適用・保存・再読込が受け付けられたら true を返す。不正な編集で、適用済みのデータが
    // 置き換わることはない。
    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    bool Draw(bool* open, RoomCatalog& applied, const std::vector<std::string>& enemyIds, const MapDefinition* map = nullptr,
              const MapDefinition* activeMap = nullptr);
    /// @brief 選択中部屋識別子を返す。
    const std::string& GetSelectedRoomId() const;

    /// @brief 候補選択を初期状態へ戻す。
    void ResetDraft()
    {
        initialized_ = false;
    }

private:
    RoomCatalog draft_;
    bool initialized_ = false;
    int roomIndex_ = 0, tool_ = 1, enemyIndex_ = 0, spawnIndex_ = -1, targetIndex_ = 0;
    bool dirty_ = false;
    std::string status_ = "部屋を選び、左の道具で配置します。";
};
} // namespace tankexp
