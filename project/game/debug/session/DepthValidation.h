#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief DepthValidationの処理を担当し、同じプレイの共有状態を借用する。
class DepthValidation {
public:
    /// @brief 借用するワールドを設定する。
    explicit DepthValidation(GameWorld& world) : world_(world) {}
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    void InitializeNeonDepthValidation();
#endif

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    void PrepareNeonDepthValidation();
#endif

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    void RecordNeonDepthValidation();
#endif

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    void FinishNeonDepthValidation();
#endif

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    void DrawNeonDepthValidation();
#endif

#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    nlohmann::json MakeNeonDepthValidationConditions() const;
#endif
private:
    GameWorld& world_;
};
} // namespace gameplay
