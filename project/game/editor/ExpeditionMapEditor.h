#pragma once
#include "game/run/TankExpeditionMap.h"
#include "game/run/TankExpeditionRooms.h"

namespace tankexp {
// 編集中の定義は、進行中の ExpeditionMapRun とは別に確定する。
// グラフを適用しても、プレイヤーの現在の経路・購入・所持金は消えない。
/// @brief 遠征ルートのノード・接続・生成条件を編集・保存する。
class MapEditor {
public:
    /// @brief 編集するデータを開き、編集状態を準備する。
    void Open(const MapDefinition& live)
    {
        draft_ = live;
        selected_ = 0;
        initialized_ = true;
        status_.clear();
    }

    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    bool Draw(bool& open, MapDefinition& live, const RoomCatalog& rooms, const std::vector<std::string>* enemyIds = nullptr);

private:
    MapDefinition draft_;
    int selected_ = 0, ruleSelected_ = 0;
    bool initialized_ = false;
    std::string status_;
};
} // namespace tankexp
