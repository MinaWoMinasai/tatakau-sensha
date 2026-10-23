#pragma once
#include "game/run/TankExpeditionContent.h"

namespace tankcontent {
// ImGui::NewFrame の後でだけ描画する。true が返ったら、シーンは更新後のカタログを
// 自機・敵の処理へ反映する。適用していない編集はエディターの中にとどまる。
/// @brief 遠征の敵・強化・機体の制作データを編集・保存する。
class ContentEditor {
public:
    /// @brief 編集するデータを開き、編集状態を準備する。
    void Open(const Catalog& live)
    {
        draft_ = live;
        selectedUpgrade_ = selectedEnemy_ = selectedPlayer_ = 0;
        status_.clear();
    }

    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    bool Draw(bool& open, Catalog& live, const std::vector<std::string>& usedEnemyIds = {});

private:
    Catalog draft_ = DefaultCatalog();
    int selectedUpgrade_ = 0, selectedEnemy_ = 0, selectedPlayer_ = 0;
    std::string status_;
};
} // namespace tankcontent
