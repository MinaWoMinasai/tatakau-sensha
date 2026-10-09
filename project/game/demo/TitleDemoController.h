#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief TitleDemoControllerの処理を担当し、同じプレイの共有状態を借用する。
class TitleDemoController {
public:
    /// @brief 借用するワールドを設定する。
    explicit TitleDemoController(GameWorld& world) : world_(world) {}
    /// @brief タイトルデモを有効にする。
    void EnableTitleDemo();

    /// @brief タイトルデモであるか判定する。
    bool IsTitleDemo() const;

    /// @brief タイトルデモ状態を返す。
    const TitleDemoStatus& GetTitleDemoStatus() const;

    /// @brief タイトルデモFadeを返す。
    float GetTitleDemoFade() const;

    /// @brief タイトルデモ計測を要求を予約する。
    void RequestTitleDemoCapture(const std::string& name);

    /// @brief タイトルデモ計測をコピーする。
    void CopyTitleDemoCapture();

    /// @brief タイトルデモ計測を予約分を処理する。
    void FlushTitleDemoCapture();

    /// @brief タイトルデモ強化ビルドを返す。
    nlohmann::json GetTitleDemoBuild() const;

    /// @brief タイトルデモを初期化する。
    void InitializeTitleDemo();

    /// @brief タイトルデモを更新する。
    /// @param dt この処理で進める経過時間（秒）。
    void UpdateTitleDemo(float dt);

    /// @brief タイトルデモステージを初期状態へ戻す。
    void ResetTitleDemoStage(int stage);

    /// @brief タイトル背景デモの遷移結果を検証する。
    /// @param dt この処理で進める経過時間（秒）。
    void VerifyTitleDemoTransition(float dt);

private:
    GameWorld& world_;
};
} // namespace gameplay
