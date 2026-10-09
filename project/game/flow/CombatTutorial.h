#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief CombatTutorialの処理を担当し、同じプレイの共有状態を借用する。
class CombatTutorial {
public:
    /// @brief 借用するワールドを設定する。
    explicit CombatTutorial(GameWorld& world) : world_(world) {}
    /// @brief 提出版UIを初期化する。
    void InitializeSubmissionUi();

    /// @brief チュートリアル設定を読み込む。
    bool LoadTutorialConfig(const std::string& filePath = "resources/configs/tutorial.json");

    /// @brief チュートリアルUIを初期化する。
    void InitializeTutorialUi();

    /// @brief チュートリアルを更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateTutorial(float deltaTime);

    /// @brief チュートリアル段階を開始する。
    void EnterTutorialStep(TutorialStep step);

    /// @brief チュートリアル段階を完了にする。
    void CompleteTutorialStep();

    /// @brief チュートリアル強化報酬を付与する。
    void GrantTutorialUpgradeReward();

    /// @brief チュートリアル進化報酬を付与する。
    void GrantTutorialEvolutionReward();

    /// @brief チュートリアル文字を更新する。
    void UpdateTutorialText();

    /// @brief チュートリアルUIを描画する。
    void DrawTutorialUi();

private:
    GameWorld& world_;
};
} // namespace gameplay
