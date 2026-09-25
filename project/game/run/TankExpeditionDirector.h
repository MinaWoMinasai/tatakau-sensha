#pragma once
#include "TankRunDirector.h"

#include <array>
#include <cmath>
#include <limits>

namespace tankexp {

enum class Phase { Dormant, Combat, Reward, Route, Event, Evolution, Clear, Dead, Map };
enum class RoomKind { Skirmish, Resource, Elite, Reflection, Drone, Guard, Boss };

// A short expedition has five fights, two route choices, and one event.
// Rendering, card acquisition, enemy objectives, and HP costs belong to the scene.
// No menu advances the combat clocks, and no deadline completes a room by itself.
class ExpeditionDirector {
public:
    void Reset() { *this=ExpeditionDirector{}; }
    // Authored expeditions use the same combat clocks/objectives, with their
    // routing and economy owned by ExpeditionMapRun.
    bool OpenMap() {
        if(phase_==Phase::Clear||phase_==Phase::Dead) return false;
        phase_=Phase::Map; return true;
    }
    bool BeginMapRoom(int index,RoomKind kind) {
        if(phase_!=Phase::Map||index<0||index>31) return false;
        EnterRoom(index,kind); return true;
    }
    bool Start() {
        if(phase_!=Phase::Dormant) return false;
        EnterRoom(0,RoomKind::Skirmish);
        return true;
    }
    bool Update(double dt) {
        if(!IsCombat()||!std::isfinite(dt)||dt<=0) return false;
        if(dt>(std::numeric_limits<double>::max)()-runElapsed_) return false;
        runElapsed_+=dt;
        roomElapsed_+=dt;
        return true;
    }
    bool CompleteRoom() {
        if(!IsCombat()) return false;
        if(roomKind_==RoomKind::Boss) {
            phase_=Phase::Clear;
            return true;
        }
        if(roomIndex_==0) { routeRound_=0; phase_=Phase::Route; return true; }
        const bool rare=roomKind_==RoomKind::Elite||roomKind_==RoomKind::Guard;
        tankrun::CardId affinity=tankrun::CardId::Count;
        if(roomKind_==RoomKind::Reflection) affinity=tankrun::CardId::Ricochet;
        if(roomKind_==RoomKind::Drone) affinity=tankrun::CardId::Drones;
        BeginReward(rare,affinity,RewardOrigin::Room);
        return true;
    }
    bool ChooseRewardDone() {
        if(phase_!=Phase::Reward) return false;
        ++rewardCount_;
        rewardRare_=false;
        rewardAffinity_=tankrun::CardId::Count;
        if(rewardOrigin_==RewardOrigin::Event) phase_=Phase::Evolution;
        else if(roomIndex_==0) EnterRoom(1,visitedRoutes_[0]==0?RoomKind::Resource:RoomKind::Elite);
        else if(roomIndex_==1) phase_=Phase::Event;
        else if(roomIndex_==2) EnterRoom(3,RoomKind::Guard);
        else if(roomIndex_==3) EnterRoom(4,RoomKind::Boss);
        return true;
    }
    bool ChooseRoute(int index) {
        if(phase_!=Phase::Route||index<0||index>1) return false;
        visitedRoutes_[static_cast<std::size_t>(routeRound_)]=index;
        if(routeRound_==0) BeginReward(false,tankrun::CardId::Rapid,RewardOrigin::Room);
        else EnterRoom(2,index==0?RoomKind::Reflection:RoomKind::Drone);
        return true;
    }
    bool ChooseEvent(int index) {
        if(phase_!=Phase::Event||index<0||index>1) return false;
        eventChoice_=index;
        if(index==0) phase_=Phase::Evolution;
        else BeginReward(true,tankrun::CardId::Count,RewardOrigin::Event);
        return true;
    }
    bool CompleteEvolution() {
        if(phase_!=Phase::Evolution) return false;
        routeRound_=1;
        phase_=Phase::Route;
        return true;
    }
    bool MarkDead() {
        if(!IsCombat()) return false;
        phase_=Phase::Dead;
        return true;
    }

    bool IsCombat() const { return phase_==Phase::Combat; }
    Phase GetPhase() const { return phase_; }
    RoomKind GetRoomKind() const { return roomKind_; }
    int GetRoomIndex() const { return roomIndex_; }
    static constexpr int GetRoomCount() { return 5; }
    int GetRouteRound() const { return routeRound_; }
    const std::array<int,2>& GetVisitedRouteIndexes() const { return visitedRoutes_; }
    int GetRouteChoice(int round) const {
        return round>=0&&round<static_cast<int>(visitedRoutes_.size())?
            visitedRoutes_[static_cast<std::size_t>(round)]:-1;
    }
    int GetEventChoice() const { return eventChoice_; }
    bool GetRewardRare() const { return phase_==Phase::Reward&&rewardRare_; }
    tankrun::CardId GetRewardAffinity() const {
        return phase_==Phase::Reward?rewardAffinity_:tankrun::CardId::Count;
    }
    int GetRewardCount() const { return rewardCount_; }
    double GetRoomElapsedSeconds() const { return roomElapsed_; }
    double GetRunElapsedSeconds() const { return runElapsed_; }

private:
    enum class RewardOrigin { Room, Event };
    void EnterRoom(int index,RoomKind kind) {
        roomIndex_=index;
        roomKind_=kind;
        roomElapsed_=0;
        phase_=Phase::Combat;
    }
    void BeginReward(bool rare,tankrun::CardId affinity,RewardOrigin origin) {
        rewardRare_=rare;
        rewardAffinity_=affinity;
        rewardOrigin_=origin;
        phase_=Phase::Reward;
    }

    Phase phase_=Phase::Dormant;
    RoomKind roomKind_=RoomKind::Skirmish;
    RewardOrigin rewardOrigin_=RewardOrigin::Room;
    int roomIndex_=-1,routeRound_=0,eventChoice_=-1,rewardCount_=0;
    std::array<int,2> visitedRoutes_{{-1,-1}};
    bool rewardRare_=false;
    tankrun::CardId rewardAffinity_=tankrun::CardId::Count;
    double roomElapsed_=0,runElapsed_=0;
};

} // namespace tankexp
