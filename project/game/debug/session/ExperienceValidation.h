#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief ExperienceValidationの処理を担当し、同じプレイの共有状態を借用する。
class ExperienceValidation {
public:
    /// @brief 借用するワールドを設定する。
    explicit ExperienceValidation(GameWorld& world) : world_(world) {}
    // Opt-in end-to-end experience validation; ordinary play never enters it.
    /// @brief 経験値検証を初期化する。
    void InitializeExperienceValidation();

    /// @brief 提出版UIを記録する。
    void RecordSubmissionUi(const std::string& screen);

    /// @brief 提出版検証を更新する。
    /// @param dt この処理で進める経過時間（秒）。
    bool UpdateSubmissionValidation(float dt);

    /// @brief 経験値検証を更新する。
    /// @param dt この処理で進める経過時間（秒）。
    bool UpdateExperienceValidation(float dt);

    /// @brief 経験値検証を記録する。
    void CaptureExperienceValidation(const std::string& name);

    /// @brief 経験値検証Reportを書き込む。
    void WriteExperienceValidationReport(bool completed);

    /// @brief 経験値近接攻撃検証用を開始する。
    void BeginExperienceMeleeProbe();

private:
    GameWorld& world_;
};
} // namespace gameplay
