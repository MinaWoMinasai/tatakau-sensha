#pragma once
#include "game/session/GameplayTypes.h"

namespace gameplay {
/// @brief TitleDemoStateに属する資源と実行状態を保持する。
struct TitleDemoState {
    float titleDemoTransitionTimer_ = 0;

    bool titleDemoTransitionVerified_ = false;

    bool titleDemo_ = false;

    TitleDemoStatus titleDemoStatus_{};

    bool titleDemoPreviousDash_ = false;

    size_t titleDemoPreviousBulletCount_ = 0;

    float titleDemoRoomFade_ = 0;

    float titleDemoNavigationTimer_ = 0;

    cg2::Vector3 titleDemoMoveTarget_{};

    std::vector<cg2::Vector3> titleDemoPath_;
};
} // namespace gameplay
