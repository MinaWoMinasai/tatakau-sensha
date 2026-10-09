#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief BossPresentationの処理を担当し、同じプレイの共有状態を借用する。
class BossPresentation {
public:
    /// @brief 借用するワールドを設定する。
    explicit BossPresentation(GameWorld& world) : world_(world) {}
    void UpdateNeonBossVisual(float deltaTime);

    bool UseNeonBossVisual() const;

    void DrawNeonBossVisual();

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    void DrawNeonBossDeveloperTools();
#endif

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    void RequestNeonBossDeveloperEncounter();
#endif

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    void StartNeonBossDeveloperEncounter();
#endif

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    void UpdateNeonBossDeveloperValidation();
#endif

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    void RecordNeonBossDeveloperFrame(cg2::DirectXCommon& dx);
#endif

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    nlohmann::json MakeNeonBossMetadata() const;
#endif

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    void WriteNeonBossValidation(bool completed);
#endif
private:
    GameWorld& world_;
};
} // namespace gameplay
