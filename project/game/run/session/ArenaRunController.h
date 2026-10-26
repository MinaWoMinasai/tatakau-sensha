#pragma once
#include "game/session/GameWorld.h"
#include "InitializationTask.h"

namespace gameplay {
/// @brief ArenaRunControllerの処理を担当し、同じプレイの共有状態を借用する。
class ArenaRunController {
public:
    explicit ArenaRunController(GameWorld& world) : world_(world) {}

    /// @brief 遠征の強化効果の強度を表示・計算用にまとめる。
    std::array<float, tankrun::CardCount> ExpeditionEffectPowers() const;

    /// @brief 戦車遠征を初期化する。
    void InitializeTankRun();
    /// @brief 初期化を分割して行うコルーチンを作る。
    myEngine::InitializationTask BuildInitialization();

    /// @brief 戦車遠征を更新する。
    /// @param dt この処理で進める経過時間（秒）。
    void UpdateTankRun(float dt);

    /// @brief 戦車遠征UIを描画する。
    void DrawTankRunUi();

    /// @brief 攻撃の予告表示を描画待ちに積む。
    void QueueTankRunTelegraph();

    /// @brief 戦車遠征カードを現在の状態へ適用する。
    void ApplyTankRunCards();

    /// @brief 戦車遠征リソースを更新する。
    /// @param dt この処理で進める経過時間（秒）。
    void UpdateTankRunResources(float dt);

    /// @brief 戦車遠征リソース取得の通知を受けて、このオブジェクトの状態を反映する。
    void OnTankRunResourceClaim(size_t index, bool playerOwned);

    /// @brief 敵を倒したときに、通貨を落とし、回収数とコンボを加算する。
    void OnTankRunEnemyDefeated(const myEngine::Vector3& position);

    /// @brief 戦車遠征選択肢を選択する。
    void SelectTankRunOption(int index);

    /// @brief 戦車遠征UIを最新の内容へ更新する。
    void RefreshTankRunUi();

    /// @brief メニューが開いているかを返す。
    bool IsTankRunMenuOpen() const;

    /// @brief 戦車遠征計測を要求を予約する。
    void RequestTankRunCapture(const std::string& name);

    /// @brief 戦車遠征計測をコピーする。
    void CopyTankRunCapture();

    /// @brief 戦車遠征計測を終了する。
    void FinishTankRunCapture();

private:
    GameWorld& world_;
};
} // namespace gameplay
