#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief ExpeditionBuildSelectionの処理を担当し、同じプレイの共有状態を借用する。
class ExpeditionBuildSelection {
public:
    /// @brief 借用するワールドを設定する。
    explicit ExpeditionBuildSelection(GameWorld& world) : world_(world) {}
    /// @brief 遠征強化ビルドカードを初期化する。
    void InitializeExpeditionBuildCards();

    /// @brief 遠征強化ビルドカードを最新の内容へ更新する。
    void RefreshExpeditionBuildCards();

    /// @brief 遠征強化ビルドカードを更新する。
    /// @param dt この処理で進める経過時間（秒）。
    void UpdateExpeditionBuildCards(float dt);

    /// @brief 遠征強化ビルド外観を選択する。
    void SelectExpeditionBuildStyle(int index);

    /// @brief 遠征強化ビルドカード画面であるか判定する。
    bool IsExpeditionBuildCardScreen() const;

private:
    GameWorld& world_;
};
} // namespace gameplay
