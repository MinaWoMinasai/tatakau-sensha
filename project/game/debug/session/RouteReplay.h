#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief RouteReplayの処理を担当し、同じプレイの共有状態を借用する。
class RouteReplay {
public:
    /// @brief 借用するワールドを設定する。
    explicit RouteReplay(GameWorld& world) : world_(world) {}
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    void InitializeNormalRouteReplay();
#endif

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    void PrepareNormalRouteReplay();
#endif

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    void RecordNormalRouteReplay();
#endif

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    void StopNormalRouteReplay(const char* reason);
#endif

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    nlohmann::json MakeNormalRouteReplayState() const;
#endif
private:
    GameWorld& world_;
};
} // namespace gameplay
