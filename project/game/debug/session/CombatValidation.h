#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief CombatValidationの処理を担当し、同じプレイの共有状態を借用する。
class CombatValidation {
public:
    explicit CombatValidation(GameWorld& world) : world_(world) {}

    /// @brief 戦闘検証の初期状態（フィクスチャ）を用意する。
    void InitializeCombatValidationFixture();

    /// @brief 戦闘検証を更新する。
    /// @param dt この処理で進める経過時間（秒）。
    bool UpdateCombatValidation(float dt);

    /// @brief 戦闘検証検証用を開始する。
    void BeginCombatValidationProbe(int index);

    /// @brief 戦闘検証検証用を終了する。
    void FinishCombatValidationProbe();

    /// @brief 戦闘検証の結果をレポートに書き出す。
    void WriteCombatValidationReport(bool completed);

    /// @brief 戦闘検証を記録する。
    void CaptureCombatValidation(const std::string& name);

private:
    GameWorld& world_;
};
} // namespace gameplay
