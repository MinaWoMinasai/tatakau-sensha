#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief RouteReplayの処理を担当し、同じプレイの共有状態を借用する。
class RouteReplay {
public:
    explicit RouteReplay(GameWorld& world) : world_(world) {}
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    /// @brief 通常ルートの再生検証を初期化する。
    void InitializeNormalRouteReplay();
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    /// @brief 通常ルートの再生検証のフレーム前の準備を行う。
    void PrepareNormalRouteReplay();
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    /// @brief 通常ルートの再生検証のフレーム結果を記録する。
    void RecordNormalRouteReplay();
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    /// @brief 通常ルートの再生検証を理由付きで停止する。
    void StopNormalRouteReplay(const char* reason);
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    /// @brief 通常ルートの再生検証の状態をJSONで返す。
    nlohmann::json MakeNormalRouteReplayState() const;
#endif
private:
    GameWorld& world_;
};
} // namespace gameplay
