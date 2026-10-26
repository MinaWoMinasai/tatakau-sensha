#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief DepthValidationの処理を担当し、同じプレイの共有状態を借用する。
class DepthValidation {
public:
    explicit DepthValidation(GameWorld& world) : world_(world) {}
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    /// @brief Depthボス検証を初期化する。
    void InitializeNeonDepthValidation();
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    /// @brief Depthボス検証のフレーム前の準備を行う。
    void PrepareNeonDepthValidation();
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    /// @brief Depthボス検証のフレーム結果を記録する。
    void RecordNeonDepthValidation();
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    /// @brief Depthボス検証を終了して結果を書き出す。
    void FinishNeonDepthValidation();
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    /// @brief Depthボス検証用の描画を行う。
    void DrawNeonDepthValidation();
#endif

#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
    /// @brief Depthボス検証の実行条件をJSONで返す。
    nlohmann::json MakeNeonDepthValidationConditions() const;
#endif
private:
    GameWorld& world_;
};
} // namespace gameplay
