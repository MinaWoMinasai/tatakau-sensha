#pragma once
#include "game/session/GameWorld.h"
#include "InitializationTask.h"

namespace gameplay {
/// @brief ExpeditionBuildSelectionの処理を担当し、同じプレイの共有状態を借用する。
class ExpeditionBuildSelection {
public:
    explicit ExpeditionBuildSelection(GameWorld& world) : world_(world) {}

    /// @brief 遠征強化ビルドカードを初期化する。
    void InitializeExpeditionBuildCards();
    /// @brief 初期化を分割して行うコルーチンを作る。
    myEngine::InitializationTask BuildInitialization();

    /// @brief 遠征強化ビルドカードを最新の内容へ更新する。
    void RefreshExpeditionBuildCards();

    /// @brief 遠征強化ビルドカードを更新する。
    /// @param dt この処理で進める経過時間（秒）。
    void UpdateExpeditionBuildCards(float dt);

    /// @brief 遠征強化ビルド外観を選択する。機体で遊ぶとき（expeditionHeroMode_）は、index は機体の番号で、戦い方は射撃に固定する。
    void SelectExpeditionBuildStyle(int index);

    /// @brief 遠征強化ビルドカード画面であるか判定する。
    bool IsExpeditionBuildCardScreen() const;

    /// @brief 選んだ機体（機体で遊んでいない・まだ選んでいないなら nullptr）。計画書 p5 の H2。
    const survivor::HeroDefinition* SelectedExpeditionHero() const;

private:
    /// @brief 工房の id を札として出せるか（強化の札か、機体の技の強化）。
    bool IsServiceOfferShown(const std::string& id) const;
    /// @brief 選べる機体を、育つ戦車の試作と同じ設定（survivor_proto.json）から読む。
    void LoadExpeditionHeroes();

    GameWorld& world_;
};
} // namespace gameplay
