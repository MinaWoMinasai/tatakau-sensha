#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include "TankRunDirector.h"

namespace tankexp {
// 自動のゲームプレイ検証では、通常はプレイヤーの手元の履修記録を変えてはいけない。
// 提出版の検証だけは、隔離した提出用のゲームフォルダの中に限って保存する。
/// @brief チュートリアルの完了を保存すべきかを判定する。デモ・検証・単独提出では保存しない。
inline constexpr bool ShouldSaveTutorialCompletion(bool demo, bool validation, bool isolatedSubmission)
{
    return !demo && (!validation || isolatedSubmission);
}
/// @brief チュートリアルの手順。
enum class TutorialStep {
    Move,
    Shoot,
    Dash,
    ClearRoom,
    Route,
    Upgrade,
    Complete,
    Hidden
};

/// @brief チュートリアルの履修状況と検証結果を保持する。
struct TutorialValidationState {
    bool enabled = false;
    float elapsed = 0, stepAge = 0;
    int lastStep = -1;
    unsigned observedSteps = 0;
};

// 入力のイベントは保持しておくので、前の手順の成功メッセージを表示している間に素早く
// 敵を倒したりメニューを選んだりしても、取りこぼさない。
/// @brief 遠征の導入手順と履修状態の保存を管理する。
class ExpeditionTutorial {
public:
    /// @brief 状態と集計値を初期状態へ戻す。
    void Reset(bool previouslyCompleted, bool suppressed = false)
    {
        *this = ExpeditionTutorial{};
        maySkip_ = previouslyCompleted;
        if (suppressed) {
            step_ = TutorialStep::Hidden;
        }
    }

    /// @brief 移動を追加する。
    void AddMovement(float distance)
    {
        if (step_ == TutorialStep::Move && std::isfinite(distance) && distance > 0 && distance < 4) {
            movement_ += distance;
        }
    }

    /// @brief 撃破を記録する。
    void RecordKill()
    {
        killed_ = true;
    }

    /// @brief ダッシュを記録する。
    void RecordDash()
    {
        dashed_ = true;
    }

    /// @brief 部屋消去を記録する。
    void RecordRoomClear()
    {
        roomCleared_ = true;
    }

    /// @brief 進路を記録する。
    void RecordRoute()
    {
        routeChosen_ = true;
    }

    /// @brief 強化を記録する。
    void RecordUpgrade()
    {
        upgraded_ = true;
    }

    /// @brief チュートリアルの省略を進行状態へ反映する。
    bool Skip()
    {
        if (!maySkip_) {
            return false;
        }
        step_ = TutorialStep::Hidden;
        return true;
    }

    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    /// @param dt この処理で進める経過時間（秒）。
    void Update(float dt)
    {
        if (!std::isfinite(dt) || dt <= 0 || step_ == TutorialStep::Hidden) {
            return;
        }
        if (step_ == TutorialStep::Complete) {
            timer_ -= dt;
            if (timer_ <= 0) {
                step_ = TutorialStep::Hidden;
            }
            return;
        }
        if (success_) {
            timer_ -= dt;
            if (timer_ > 0) {
                return;
            }
            success_ = false;
            step_ = static_cast<TutorialStep>(static_cast<int>(step_) + 1);
            if (step_ == TutorialStep::Complete) {
                completed_ = true;
                timer_ = 2.8f;
                return;
            }
        }
        const std::array<bool, 6> achieved = {movement_ >= 5, killed_, dashed_, roomCleared_, routeChosen_, upgraded_};
        if (achieved[static_cast<size_t>(step_)]) {
            success_ = true;
            timer_ = 0.65f;
        }
    }

    /// @brief 段階を返す。
    TutorialStep GetStep() const
    {
        return step_;
    }

    /// @brief Successであるか判定する。
    bool IsSuccess() const
    {
        return success_;
    }

    /// @brief 表示中であるか判定する。
    bool IsVisible() const
    {
        return step_ != TutorialStep::Hidden;
    }

    /// @brief 完了であるか判定する。
    bool IsComplete() const
    {
        return completed_;
    }

    /// @brief 省略が可能か判定する。
    bool CanSkip() const
    {
        return maySkip_ && IsVisible();
    }

    // 移動とダッシュの手順が終わるまでは、敵がいなくなった最初の部屋でも遊べるようにしておく。
    /// @brief 最初の部屋を出てよいかを返す。
    bool CanLeaveFirstRoom() const
    {
        return step_ >= TutorialStep::ClearRoom;
    }

    /// @brief 移動距離を返す。
    float GetMoveDistance() const
    {
        return movement_;
    }

private:
    TutorialStep step_ = TutorialStep::Move;
    float movement_ = 0, timer_ = 0;
    bool killed_ = false, dashed_ = false, roomCleared_ = false, routeChosen_ = false, upgraded_ = false;
    bool success_ = false, completed_ = false, maySkip_ = false;
};

// 戦場に表示する通常の強化は2つまでにする。レアな強化は常に表示する。
/// @brief 表示対象の強化カードを整理して一覧にする。
inline std::array<bool, tankrun::CardCount> CompactBuildCards(const tankrun::CardCounts& cards)
{
    std::array<bool, tankrun::CardCount> visible{};
    int ordinary = 0;
    for (size_t i = 0; i < cards.size(); ++i) {
        if (cards[i] <= 0) {
            continue;
        }
        const bool rare = tankrun::IsRare(static_cast<tankrun::CardId>(i));
        visible[i] = rare || ordinary < 2;
        if (!rare && visible[i]) {
            ++ordinary;
        }
    }
    return visible;
}
} // namespace tankexp
