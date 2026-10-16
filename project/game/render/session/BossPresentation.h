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

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    void DrawNeonBossDeveloperTools();
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    void RequestNeonBossDeveloperEncounter();
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    void StartNeonBossDeveloperEncounter();
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    void UpdateNeonBossDeveloperValidation();
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    void RecordNeonBossDeveloperFrame(myEngine::DirectXCommon& dx);
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    nlohmann::json MakeNeonBossMetadata() const;
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    void WriteNeonBossValidation(bool completed);
#endif
private:
    GameWorld& world_;
};
} // namespace gameplay
