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

    /// @brief タイトルデモのフェードの量を返す。
    float GetTitleDemoFade() const;

    /// @brief タイトルデモの画面を、指定した名前でPNGに保存するよう予約する。
    void RequestTitleDemoCapture(const std::string& name);

    /// @brief タイトルデモの画面を、保存用にコピーする。
    void CopyTitleDemoCapture();

    /// @brief 予約していたタイトルデモの画面を、PNGに書き出す。
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
