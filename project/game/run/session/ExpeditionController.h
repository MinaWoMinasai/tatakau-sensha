#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief ExpeditionControllerの処理を担当し、同じプレイの共有状態を借用する。
class ExpeditionController {
public:
    /// @brief 借用するワールドを設定する。
    explicit ExpeditionController(GameWorld& world) : world_(world) {}
    /// @brief 戦車遠征チュートリアルを更新する。
    /// @param dt この処理で進める経過時間（秒）。
    void UpdateTankExpeditionTutorial(float dt);

    /// @brief 遠征チュートリアルCompletionを保存する。
    void SaveExpeditionTutorialCompletion();

    /// @brief 戦車遠征チュートリアル検証を更新する。
    /// @param dt この処理で進める経過時間（秒）。
    void UpdateTankExpeditionTutorialValidation(float dt);

    /// @brief 戦車遠征チュートリアルを描画する。
    void DrawTankExpeditionTutorial();

    /// @brief 戦車遠征チュートリアルUIを最新の内容へ更新する。
    void RefreshTankExpeditionTutorialUi();

    /// @brief 戦車遠征を初期化する。
    void InitializeTankExpedition();

    /// @brief 戦車遠征を更新する。
    /// @param dt この処理で進める経過時間（秒）。
    void UpdateTankExpedition(float dt);

    /// @brief 戦闘フェーズなら旧部屋の敵・弾・場の攻撃を消去し、新しい部屋を配置する。
    /// @note 地形の読み込み失敗は診断出力だけで、その後の初期化・配置は続ける。
    /// 自機の部屋状態リセットは、遠征成長が有効で生存中の場合にだけ行われる。
    void StartTankExpeditionRoom();

    /// @brief 生存中の非ボス戦を報酬/進路選択へ移す。マップ式では対応する完了処理へ委譲する。
    void FinishTankExpeditionRoom();

    /// @brief 戦車遠征選択肢を選択する。
    void SelectTankExpeditionOption(int index);

    /// @brief 戦車遠征UIを最新の内容へ更新する。
    void RefreshTankExpeditionUi();

    /// @brief 戦車遠征UIを描画する。
    void DrawTankExpeditionUi();

    /// @brief 戦車遠征音声を更新する。
    /// @param dt この処理で進める経過時間（秒）。
    void UpdateTankExpeditionAudio(float dt);

    /// @brief 戦車遠征選択肢件数を返す。
    int GetTankExpeditionOptionCount() const;

private:
    GameWorld& world_;
};
} // namespace gameplay
