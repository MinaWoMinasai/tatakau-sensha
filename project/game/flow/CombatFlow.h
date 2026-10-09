#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief CombatFlowの処理を担当し、同じプレイの共有状態を借用する。
class CombatFlow {
public:
    /// @brief 借用するワールドを設定する。
    explicit CombatFlow(GameWorld& world) : world_(world) {}
    /// @brief 基準時間で撃破/死亡演出の終了を判定し、結果表示とその選択入力を処理する。
    /// @param baseDeltaTime 演出と進行に使う経過秒。戦闘の減速倍率を掛けない。
    void UpdateGameFlow(float baseDeltaTime);

    /// @brief 適用済みHPの差分・行動イベント・死亡状態を読み、演出と戦闘終了状態を反映する。
    /// @note 進化確定/取消の通知は消費し、遠征時は主攻撃通知も消費する。2つの引数は現在の実装では使わない。
    void UpdateGameplayEventEffects(float baseDeltaTime, bool justDodgeTriggered);

    /// @brief ボス撃破演出状態へ移り、次回の戦闘更新を止める。敵・弾はこの関数では消去しない。
    void BeginBossDefeatSequence();

    /// @brief 自機死亡を進行へ記録し、ゲームオーバー状態へ移って次回の戦闘更新を止める。
    void BeginGameOver();

    /// @brief 結果状態を開始する。
    void EnterResultState(bool stageClear);

    /// @brief リザルト画面の選択を確定し、対応する次画面へ進む。
    void ConfirmResultSelection();

    /// @brief 結果文字を更新する。
    void UpdateResultText();

    /// @brief イベントCalloutを設定する。
    void SetEventCallout(const std::string& text, float duration);

private:
    GameWorld& world_;
};
} // namespace gameplay
