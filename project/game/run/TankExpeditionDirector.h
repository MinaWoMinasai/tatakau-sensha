#pragma once
#include "TankRunDirector.h"

#include <array>
#include <cmath>
#include <limits>

namespace tankexp {

enum class Phase {
    Dormant,
    Combat,
    Reward,
    Route,
    Event,
    Evolution,
    Clear,
    Dead,
    Map
};
enum class RoomKind {
    Skirmish,
    Resource,
    Elite,
    Reflection,
    Drone,
    Guard,
    Boss
};

// A short expedition has five fights, two route choices, and one event.
// Rendering, card acquisition, enemy objectives, and HP costs belong to the scene.
// No menu advances the combat clocks, and no deadline completes a room by itself.
/// @brief 遠征の部屋開始・戦闘終了・報酬・進路選択の進行を管理する。
class ExpeditionDirector {
public:
    /// @brief 状態と集計値を初期状態へ戻す。
    void Reset()
    {
        *this = ExpeditionDirector{};
    }
    // Authored expeditions use the same combat clocks/objectives, with their
    // routing and economy owned by ExpeditionMapRun.
    /// @brief 遠征ルートの選択画面へ進む。
    bool OpenMap()
    {
        if (phase_ == Phase::Clear || phase_ == Phase::Dead)
            return false;
        phase_ = Phase::Map;
        return true;
    }
    /// @brief マップ部屋を開始する。
    bool BeginMapRoom(int index, RoomKind kind)
    {
        if (phase_ != Phase::Map || index < 0 || index > 31)
            return false;
        EnterRoom(index, kind);
        return true;
    }
    /// @brief 動作を開始し、開始時の条件を保持する。
    bool Start()
    {
        if (phase_ != Phase::Dormant)
            return false;
        EnterRoom(0, RoomKind::Skirmish);
        return true;
    }
    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    bool Update(double dt)
    {
        if (!IsCombat() || !std::isfinite(dt) || dt <= 0)
            return false;
        if (dt > (std::numeric_limits<double>::max)() - runElapsed_)
            return false;
        runElapsed_ += dt;
        roomElapsed_ += dt;
        return true;
    }
    /// @brief 部屋を完了にする。
    bool CompleteRoom()
    {
        if (!IsCombat())
            return false;
        if (roomKind_ == RoomKind::Boss) {
            phase_ = Phase::Clear;
            return true;
        }
        if (roomIndex_ == 0) {
            routeRound_ = 0;
            phase_ = Phase::Route;
            return true;
        }
        const bool rare = roomKind_ == RoomKind::Elite || roomKind_ == RoomKind::Guard;
        tankrun::CardId affinity = tankrun::CardId::Count;
        if (roomKind_ == RoomKind::Reflection)
            affinity = tankrun::CardId::Ricochet;
        if (roomKind_ == RoomKind::Drone)
            affinity = tankrun::CardId::Drones;
        BeginReward(rare, affinity, RewardOrigin::Room);
        return true;
    }
    /// @brief 報酬Doneを選ぶ。
    bool ChooseRewardDone()
    {
        if (phase_ != Phase::Reward)
            return false;
        ++rewardCount_;
        rewardRare_ = false;
        rewardAffinity_ = tankrun::CardId::Count;
        if (rewardOrigin_ == RewardOrigin::Event)
            phase_ = Phase::Evolution;
        else if (roomIndex_ == 0)
            EnterRoom(1, visitedRoutes_[0] == 0 ? RoomKind::Resource : RoomKind::Elite);
        else if (roomIndex_ == 1)
            phase_ = Phase::Event;
        else if (roomIndex_ == 2)
            EnterRoom(3, RoomKind::Guard);
        else if (roomIndex_ == 3)
            EnterRoom(4, RoomKind::Boss);
        return true;
    }
    /// @brief 進路を選ぶ。
    bool ChooseRoute(int index)
    {
        if (phase_ != Phase::Route || index < 0 || index > 1)
            return false;
        visitedRoutes_[static_cast<std::size_t>(routeRound_)] = index;
        if (routeRound_ == 0)
            BeginReward(false, tankrun::CardId::Rapid, RewardOrigin::Room);
        else
            EnterRoom(2, index == 0 ? RoomKind::Reflection : RoomKind::Drone);
        return true;
    }
    /// @brief イベントを選ぶ。
    bool ChooseEvent(int index)
    {
        if (phase_ != Phase::Event || index < 0 || index > 1)
            return false;
        eventChoice_ = index;
        if (index == 0)
            phase_ = Phase::Evolution;
        else
            BeginReward(true, tankrun::CardId::Count, RewardOrigin::Event);
        return true;
    }
    /// @brief 進化を完了にする。
    bool CompleteEvolution()
    {
        if (phase_ != Phase::Evolution)
            return false;
        routeRound_ = 1;
        phase_ = Phase::Route;
        return true;
    }
    /// @brief 対象が死亡したことを進行状態へ記録する。
    bool MarkDead()
    {
        if (!IsCombat())
            return false;
        phase_ = Phase::Dead;
        return true;
    }

    /// @brief 戦闘であるか判定する。
    bool IsCombat() const
    {
        return phase_ == Phase::Combat;
    }
    /// @brief 段階を返す。
    Phase GetPhase() const
    {
        return phase_;
    }
    /// @brief 部屋種類を返す。
    RoomKind GetRoomKind() const
    {
        return roomKind_;
    }
    /// @brief 部屋添字を返す。
    int GetRoomIndex() const
    {
        return roomIndex_;
    }
    /// @brief 部屋件数を返す。
    static constexpr int GetRoomCount()
    {
        return 5;
    }
    /// @brief 進路Roundを返す。
    int GetRouteRound() const
    {
        return routeRound_;
    }
    /// @brief 到達済み進路Indexesを返す。
    const std::array<int, 2>& GetVisitedRouteIndexes() const
    {
        return visitedRoutes_;
    }
    /// @brief 進路Choiceを返す。
    int GetRouteChoice(int round) const
    {
        return round >= 0 && round < static_cast<int>(visitedRoutes_.size()) ? visitedRoutes_[static_cast<std::size_t>(round)] : -1;
    }
    /// @brief イベントChoiceを返す。
    int GetEventChoice() const
    {
        return eventChoice_;
    }
    /// @brief 報酬Rareを返す。
    bool GetRewardRare() const
    {
        return phase_ == Phase::Reward && rewardRare_;
    }
    /// @brief 報酬Affinityを返す。
    tankrun::CardId GetRewardAffinity() const
    {
        return phase_ == Phase::Reward ? rewardAffinity_ : tankrun::CardId::Count;
    }
    /// @brief 報酬件数を返す。
    int GetRewardCount() const
    {
        return rewardCount_;
    }
    /// @brief 部屋経過時間秒を返す。
    double GetRoomElapsedSeconds() const
    {
        return roomElapsed_;
    }
    /// @brief 遠征経過時間秒を返す。
    double GetRunElapsedSeconds() const
    {
        return runElapsed_;
    }

private:
    enum class RewardOrigin {
        Room,
        Event
    };
    /// @brief 指定した部屋へ進み、開始時の状態を用意する。
    void EnterRoom(int index, RoomKind kind)
    {
        roomIndex_ = index;
        roomKind_ = kind;
        roomElapsed_ = 0;
        phase_ = Phase::Combat;
    }
    /// @brief 報酬を開始する。
    void BeginReward(bool rare, tankrun::CardId affinity, RewardOrigin origin)
    {
        rewardRare_ = rare;
        rewardAffinity_ = affinity;
        rewardOrigin_ = origin;
        phase_ = Phase::Reward;
    }

    Phase phase_ = Phase::Dormant;
    RoomKind roomKind_ = RoomKind::Skirmish;
    RewardOrigin rewardOrigin_ = RewardOrigin::Room;
    int roomIndex_ = -1, routeRound_ = 0, eventChoice_ = -1, rewardCount_ = 0;
    std::array<int, 2> visitedRoutes_{{-1, -1}};
    bool rewardRare_ = false;
    tankrun::CardId rewardAffinity_ = tankrun::CardId::Count;
    double roomElapsed_ = 0, runElapsed_ = 0;
};

} // namespace tankexp
