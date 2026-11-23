#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief BossPresentationの処理を担当し、同じプレイの共有状態を借用する。
class BossPresentation {
public:
    explicit BossPresentation(GameWorld& world) : world_(world) {}

    /// @brief ネオン表示のボスを更新する。
    void UpdateNeonBossVisual(float deltaTime);

    /// @brief ネオン表示のボスを使うかを返す。
    bool UseNeonBossVisual() const;

    /// @brief ネオン表示のボスを描画する。
    void DrawNeonBossVisual();

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    /// @brief ボス表示の開発者ツールUIを描画する。
    void DrawNeonBossDeveloperTools();
#endif

    /// @brief ボス戦への移動（F7）を予約する。講評会の Release でも使う。
    void RequestNeonBossDeveloperEncounter();

    /// @brief ボス戦へ移る。開発用の構成では自機を無敵にし、それ以外ではふつうに被弾する。
    void StartNeonBossDeveloperEncounter();

    /// @brief 予約したボス戦への移動を、戦闘中の画面になったら行う（開発用の検証を持たない構成で使う）。
    void UpdateNeonBossQuickStart();

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    /// @brief 開発者用のボス表示の検証を進める。
    void UpdateNeonBossDeveloperValidation();
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    /// @brief 開発者用のボス表示の検証フレームを記録する。
    void RecordNeonBossDeveloperFrame(myEngine::DirectXCommon& dx);
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    /// @brief ボス表示の検証用メタデータを作る。
    nlohmann::json MakeNeonBossMetadata() const;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    /// @brief ボス表示の検証結果を書き出す。
    void WriteNeonBossValidation(bool completed);
#endif
private:
    GameWorld& world_;
};
} // namespace gameplay
