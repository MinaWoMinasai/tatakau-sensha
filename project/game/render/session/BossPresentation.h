#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief BossPresentationの処理を担当し、同じプレイの共有状態を借用する。
class BossPresentation {
public:
    /// @brief 借用するワールドを設定する。
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

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    /// @brief 開発者用のボス戦の開始を予約する。
    void RequestNeonBossDeveloperEncounter();
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    /// @brief 開発者用のボス戦を開始する。
    void StartNeonBossDeveloperEncounter();
#endif

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
