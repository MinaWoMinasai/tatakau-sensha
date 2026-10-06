#pragma once
#include "game/player/TankSpecialCombat.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

// The Enemy adapter resolves actual Stage geometry before committing a plan.
// The current beam is clipped after Step, then shared by hit detection and draw.
// No actor, renderer, animation, platform or random-stream dependencies.
namespace neondepth {
using Point = std::array<float, 2>;
enum class Attack { Volley, Dive, Beam };
enum class Phase { Intro, Reposition, Telegraph, Locked, Airborne, Active, Recovery, Defeated, Aborted };
enum class EventKind { IntroComplete, FloorArrival, Landing, BeamBegin, BeamEnd, Cancelled };
constexpr unsigned kMaximumCircles = 5;
constexpr unsigned kMaximumWarningSegments = 17;
constexpr float kMaximumStep = .10f;

inline bool Finite(Point point) { return std::isfinite(point[0]) && std::isfinite(point[1]); }
inline bool Bounded(Point point) { return Finite(point) && std::abs(point[0]) <= 10000 && std::abs(point[1]) <= 10000; }
inline bool Within(float value, float minimum, float maximum) { return std::isfinite(value) && value >= minimum && value <= maximum; }

struct Circle { Point center{}; float radius = 2.1f; bool operator==(const Circle&) const = default; };
struct Segment { Point start{}, end{}; float halfWidth = .8f; bool operator==(const Segment&) const = default; };
struct Durations {
    float telegraph = .85f, locked = .30f, airborne = .55f, active = .35f, recovery = 1.0f;
    bool operator==(const Durations&) const = default;
};
struct Tuning {
    float intro = 2.4f, reposition = .6f, hitInterval = .45f;
    float volleyRadius = 2.1f, diveRadius = 3.4f, beamRange = 28.0f, beamHalfWidth = .8f, beamSweepRadians = 1.2f;
    Durations volley{}, dive{.80f, .35f, .45f, .20f, 1.30f}, beam{.90f, .30f, .10f, 1.10f, 1.20f};
    uint32_t volleyDamage = 12, diveDamage = 18, beamDamage = 9;
    bool operator==(const Tuning&) const = default;
};
inline bool Valid(const Durations& value) {
    return Within(value.telegraph,.35f,5) && Within(value.locked,.15f,2) && Within(value.airborne,.05f,3) &&
        Within(value.active,.05f,5) && Within(value.recovery,.35f,5);
}
inline bool Valid(const Tuning& value) {
    return Within(value.intro,.1f,5) && Within(value.reposition,.1f,5) && Within(value.hitInterval,.1f,2) &&
        Within(value.volleyRadius,.5f,6) && Within(value.diveRadius,.5f,8) && Within(value.beamRange,4,40) &&
        Within(value.beamHalfWidth,.2f,2) && Within(value.beamSweepRadians,.2f,2.4f) &&
        Valid(value.volley) && Valid(value.dive) && Valid(value.beam) &&
        value.volleyDamage >= 1 && value.volleyDamage <= 999 && value.diveDamage >= 1 && value.diveDamage <= 999 &&
        value.beamDamage >= 1 && value.beamDamage <= 999;
}
inline bool Valid(const Circle& value) { return Bounded(value.center) && Within(value.radius,.25f,8); }
inline bool Valid(const Segment& value) { return Bounded(value.start) && Bounded(value.end) && Within(value.halfWidth,.1f,6); }
inline bool IsClippedSegment(const Segment& desired, const Segment& clipped) {
    if (!Valid(desired) || !Valid(clipped) || clipped.start != desired.start || clipped.halfWidth != desired.halfWidth) return false;
    const float dx = desired.end[0]-desired.start[0], dy = desired.end[1]-desired.start[1];
    const float cx = clipped.end[0]-clipped.start[0], cy = clipped.end[1]-clipped.start[1];
    const float lengthSquared = dx*dx+dy*dy;
    if (lengthSquared < .00001f) return clipped.end == clipped.start;
    const float fraction = (cx*dx+cy*dy)/lengthSquared;
    return Within(fraction,0,1.00001f) && std::abs(dx*cy-dy*cx) <= .0001f*lengthSquared;
}

struct BeamPlan {
    Point origin{};
    float startAngle = -.6f, endAngle = .6f, range = 28, halfWidth = .8f;
    // Authored Stage adapter supplies the same wall-clipped future band that
    // Presentation outlines. No independently hand-written danger geometry.
    std::array<Segment,kMaximumWarningSegments> warning{};
    unsigned warningCount = 0;
    bool operator==(const BeamPlan&) const = default;
};
inline float BeamWarningHalfWidth(const BeamPlan& beam) {
    if (beam.warningCount < 2) return beam.halfWidth;
    return beam.halfWidth+beam.range*std::sin(std::abs(beam.endAngle-beam.startAngle)/
        (2.0f*static_cast<float>(beam.warningCount-1)));
}
struct AttackPlan {
    // Commit assigns identity and tuning. Callers cannot forge or reuse IDs.
    uint64_t generation = 0, instance = 0;
    Attack attack = Attack::Volley;
    bool phaseTwo = false;
    Point coreStart{}, coreTarget{}, lockedTarget{};
    std::array<Circle,kMaximumCircles> circles{};
    unsigned circleCount = 3;
    Circle dive{};
    BeamPlan beam{};
    Durations duration{};
    float hitInterval = .45f;
    uint32_t damage = 12;
    bool operator==(const AttackPlan&) const = default;
};
struct PlanningRequest { Attack attack; bool phaseTwo; Tuning tuning; };
struct Event { EventKind kind; uint64_t generation, instance; unsigned index; };
struct Events {
    std::array<Event,kMaximumCircles+1> values{};
    unsigned count = 0;
};
struct Snapshot {
    uint64_t generation = 0;
    Phase phase = Phase::Aborted;
    float elapsed = 0, duration = 0, progress = 0;
    bool planning = false, vulnerable = false, inputLocked = false, phaseTwoPending = false;
    AttackPlan plan{};
    Segment activeBeam{};
    bool activeBeamClipped = false;
    unsigned activeCircleMask = 0;
    Point desiredCore{};
    uint64_t activationEvents = 0, contactClaims = 0, acceptedHits = 0;
    bool operator==(const Snapshot&) const = default;
};
struct HitRequest { uint64_t generation, instance, target, claim; unsigned index; uint32_t damage; };

class Combat {
public:
    bool Reset(uint64_t generation, Point core, const Tuning& tuning = {}) {
        // Reject reused/backward generations before touching the old state,
        // including after Abort/Defeat, so stale requests cannot match again.
        if (!generation || generation <= snapshot_.generation || !Bounded(core) || !Valid(tuning)) return false;
        *this = Combat{};
        tuning_ = pendingTuning_ = tuning;
        snapshot_.generation = generation;
        snapshot_.phase = Phase::Intro;
        snapshot_.desiredCore = core;
        Refresh();
        return true;
    }
    bool QueueTuning(const Tuning& tuning) {
        if (!Valid(tuning)) return false;
        pendingTuning_ = tuning;
        pendingTuningChanged_ = true;
        return true;
    }
    std::optional<PlanningRequest> GetPlanningRequest() const {
        if (!snapshot_.planning) return std::nullopt;
        return PlanningRequest{static_cast<Attack>(cycle_ % 3),snapshot_.phaseTwoPending,
            pendingTuningChanged_ ? pendingTuning_ : tuning_};
    }
    bool SetRepositionCore(Point actualStageCorrectedCore) {
        if (snapshot_.phase != Phase::Reposition || !Bounded(actualStageCorrectedCore)) return false;
        snapshot_.desiredCore = actualStageCorrectedCore;
        return true;
    }
    bool CommitPlan(AttackPlan candidate) {
        const auto request = GetPlanningRequest();
        if (!request || candidate.attack != request->attack || candidate.phaseTwo != request->phaseTwo ||
            !Bounded(candidate.coreStart) || !Bounded(candidate.coreTarget) || !Bounded(candidate.lockedTarget)) return false;
        if (candidate.coreStart != snapshot_.desiredCore ||
            (candidate.attack != Attack::Dive && candidate.coreTarget != candidate.coreStart)) return false;
        if (candidate.attack == Attack::Volley) {
            if (candidate.circleCount != (candidate.phaseTwo ? 5u : 3u)) return false;
            for (unsigned index = 0; index < candidate.circleCount; ++index)
                if (!Valid(candidate.circles[index]) || candidate.circles[index].radius != request->tuning.volleyRadius) return false;
        } else if (candidate.attack == Attack::Dive) {
            if (!Valid(candidate.dive) || candidate.dive.radius != request->tuning.diveRadius ||
                candidate.dive.center != candidate.coreTarget) return false;
        } else {
            const auto& beam = candidate.beam;
            if (beam.origin != candidate.coreStart) return false;
            if (!Bounded(beam.origin) || !Within(beam.startAngle,-100,100) || !Within(beam.endAngle,-100,100) ||
                !Within(beam.range,1,64) || !Within(beam.halfWidth,.1f,3) || beam.warningCount < 2 ||
                beam.warningCount > kMaximumWarningSegments) return false;
            if (beam.range != request->tuning.beamRange || beam.halfWidth != request->tuning.beamHalfWidth ||
                std::abs((beam.endAngle-beam.startAngle)-request->tuning.beamSweepRadians) > .00001f) return false;
            for (unsigned index = 0; index < beam.warningCount; ++index) {
                const float progress = static_cast<float>(index)/static_cast<float>(beam.warningCount-1);
                const float angle = beam.startAngle+(beam.endAngle-beam.startAngle)*progress;
                const Segment desired{beam.origin,{beam.origin[0]+std::cos(angle)*beam.range,
                    beam.origin[1]+std::sin(angle)*beam.range},BeamWarningHalfWidth(beam)};
                if (!IsClippedSegment(desired,beam.warning[index])) return false;
            }
        }
        if (nextInstance_ == (std::numeric_limits<uint64_t>::max)()) { Abort(); return false; }
        tuning_ = request->tuning;
        pendingTuningChanged_ = false;
        candidate.generation = snapshot_.generation;
        candidate.instance = ++nextInstance_;
        candidate.duration = candidate.attack == Attack::Volley ? tuning_.volley :
            candidate.attack == Attack::Dive ? tuning_.dive : tuning_.beam;
        candidate.hitInterval = tuning_.hitInterval;
        candidate.damage = candidate.attack == Attack::Volley ? tuning_.volleyDamage :
            candidate.attack == Attack::Dive ? tuning_.diveDamage : tuning_.beamDamage;
        snapshot_.plan = candidate;
        snapshot_.desiredCore = candidate.coreStart;
        snapshot_.planning = false;
        hits_ = {};
        Enter(Phase::Telegraph);
        return true;
    }
    Events SkipIntro() {
        Events events;
        if (snapshot_.phase == Phase::Intro) { Enter(Phase::Reposition); Push(events,EventKind::IntroComplete); }
        return events;
    }
    Events Defeat() { return Cancel(Phase::Defeated); }
    Events Abort() { return Cancel(Phase::Aborted); }
    Events Step(float dt, float hpRatio, bool bossAlive = true, bool playerAlive = true) {
        // Contact permission belongs to this positive Gameplay update. A paused
        // or invalid update keeps the displayed shape but closes its hit gate.
        contactOpen_ = false;
        // Death/abort authority is not suppressed by a paused/invalid dt.
        if (!bossAlive) return Defeat();
        if (!playerAlive) return Abort();
        Events events;
        if (Terminal() || !std::isfinite(dt) || dt <= 0) return events;
        const float elapsed = (std::min)(dt,kMaximumStep);
        if (std::isfinite(hpRatio) && hpRatio <= .5f) snapshot_.phaseTwoPending = true;
        snapshot_.elapsed = (std::min)(snapshot_.elapsed+elapsed,Duration());
        // Never carry a spike's remaining time into a newly displayed warning.
        // This is one phase transition per actual Gameplay update.
        if (snapshot_.elapsed >= Duration()) {
            switch (snapshot_.phase) {
            case Phase::Intro: Enter(Phase::Reposition); Push(events,EventKind::IntroComplete); break;
            case Phase::Reposition: snapshot_.planning = true; break;
            case Phase::Telegraph: Enter(Phase::Locked); break;
            case Phase::Locked:
                if (snapshot_.plan.attack == Attack::Beam) { Enter(Phase::Active); Push(events,EventKind::BeamBegin); }
                else Enter(Phase::Airborne);
                break;
            case Phase::Airborne:
                Enter(Phase::Active);
                if (snapshot_.plan.attack == Attack::Volley)
                    for (unsigned index = 0; index < snapshot_.plan.circleCount; ++index) Push(events,EventKind::FloorArrival,index);
                else Push(events,EventKind::Landing);
                break;
            case Phase::Active:
                if (snapshot_.plan.attack == Attack::Beam) Push(events,EventKind::BeamEnd);
                Enter(Phase::Recovery);
                break;
            case Phase::Recovery: ++cycle_; Enter(Phase::Reposition); break;
            default: break;
            }
        }
        if (snapshot_.phase == Phase::Active && snapshot_.plan.attack == Attack::Beam) snapshot_.activeBeamClipped = false;
        Refresh();
        contactOpen_ = snapshot_.phase == Phase::Active;
        return events;
    }
    Segment GetDesiredBeam() const {
        if (snapshot_.plan.attack != Attack::Beam) return {};
        const auto& beam = snapshot_.plan.beam;
        const float progress = snapshot_.phase == Phase::Active ? snapshot_.progress : 0;
        const float angle = beam.startAngle + (beam.endAngle-beam.startAngle)*progress;
        float range = beam.range;
        if (beam.warningCount >= 2) {
            const unsigned lower = (std::min)(static_cast<unsigned>(progress*static_cast<float>(beam.warningCount-1)),beam.warningCount-2);
            for (unsigned index : {lower,lower+1}) {
                const auto& sample = beam.warning[index];
                const float dx = sample.end[0]-sample.start[0], dy = sample.end[1]-sample.start[1];
                range = (std::min)(range,std::sqrt(dx*dx+dy*dy));
            }
        }
        // Active rays never exceed either adjacent conservative warning
        // capsule. Its angular padding covers the entire continuous interval.
        return {beam.origin,{beam.origin[0]+std::cos(angle)*range,beam.origin[1]+std::sin(angle)*range},beam.halfWidth};
    }
    bool ApplyBeamClip(uint64_t generation, uint64_t instance, const Segment& clipped) {
        if (!contactOpen_ || snapshot_.phase != Phase::Active || snapshot_.plan.attack != Attack::Beam ||
            generation != snapshot_.generation || instance != snapshot_.plan.instance || !Valid(clipped)) return false;
        const auto desired = GetDesiredBeam();
        if (!IsClippedSegment(desired,clipped)) return false;
        snapshot_.activeBeam = clipped;
        snapshot_.activeBeamClipped = true;
        return true;
    }
    std::optional<HitRequest> TryClaimContact(unsigned index, uint64_t target, Point position, float radius) {
        if (!contactOpen_ || snapshot_.phase != Phase::Active || !target || !Bounded(position) || !Within(radius,0,8)) return std::nullopt;
        const auto& plan = snapshot_.plan;
        bool contact = false;
        if (plan.attack == Attack::Beam) {
            if (index || !snapshot_.activeBeamClipped) return std::nullopt;
            const auto& beam = snapshot_.activeBeam;
            contact = tankspecial::SegmentTouches(beam.start[0],beam.start[1],beam.end[0],beam.end[1],position[0],position[1],beam.halfWidth+radius);
        } else {
            if (index >= (plan.attack == Attack::Volley ? plan.circleCount : 1u)) return std::nullopt;
            const Circle& circle = plan.attack == Attack::Volley ? plan.circles[index] : plan.dive;
            const float x = position[0]-circle.center[0], y = position[1]-circle.center[1], sum = circle.radius+radius;
            contact = x*x+y*y <= sum*sum;
        }
        if (!contact) return std::nullopt;
        HitRecord* available = nullptr;
        for (auto& record : hits_) {
            if (record.target == target && record.index == index) {
                if (plan.attack != Attack::Beam || snapshot_.elapsed < record.next) return std::nullopt;
                available = &record;
                break;
            }
            if (!available && !record.target) available = &record;
        }
        if (!available) return std::nullopt;
        ++snapshot_.contactClaims;
        *available = {target,index,snapshot_.elapsed+plan.hitInterval,snapshot_.contactClaims,false};
        return HitRequest{snapshot_.generation,plan.instance,target,snapshot_.contactClaims,index,plan.damage};
    }
    // Actual actor adapter reports accepted HP loss after existing immunity /
    // perfect-dodge / mitigation. Contact permission alone is not a real hit.
    bool ReportAcceptedHit(const HitRequest& request, uint32_t actualHpLoss) {
        if (!contactOpen_ || !actualHpLoss || actualHpLoss > request.damage || request.damage != snapshot_.plan.damage ||
            request.generation != snapshot_.generation || request.instance != snapshot_.plan.instance ||
            snapshot_.phase != Phase::Active) return false;
        for (auto& record : hits_)
            if (record.target == request.target && record.index == request.index && record.claim == request.claim && !record.reported) {
                record.reported = true;
                ++snapshot_.acceptedHits;
                return true;
            }
        return false;
    }
    const Snapshot& GetSnapshot() const { return snapshot_; }

private:
    struct HitRecord { uint64_t target = 0; unsigned index = 0; float next = 0; uint64_t claim = 0; bool reported = false; };
    bool Terminal() const { return snapshot_.phase == Phase::Defeated || snapshot_.phase == Phase::Aborted; }
    float Duration() const {
        switch (snapshot_.phase) {
        case Phase::Intro: return tuning_.intro;
        case Phase::Reposition: return tuning_.reposition;
        case Phase::Telegraph: return snapshot_.plan.duration.telegraph;
        case Phase::Locked: return snapshot_.plan.duration.locked;
        case Phase::Airborne: return snapshot_.plan.duration.airborne;
        case Phase::Active: return snapshot_.plan.duration.active;
        case Phase::Recovery: return snapshot_.plan.duration.recovery;
        default: return 0;
        }
    }
    void Refresh() {
        snapshot_.duration = Duration();
        snapshot_.progress = snapshot_.duration > 0 ? (std::clamp)(snapshot_.elapsed/snapshot_.duration,0.0f,1.0f) : 0;
        snapshot_.vulnerable = snapshot_.phase != Phase::Intro && !Terminal();
        snapshot_.inputLocked = snapshot_.phase == Phase::Intro;
        snapshot_.activeCircleMask = snapshot_.phase == Phase::Active && snapshot_.plan.attack != Attack::Beam
            ? ((1u << (snapshot_.plan.attack == Attack::Volley ? snapshot_.plan.circleCount : 1u))-1) : 0;
        if (snapshot_.plan.instance && snapshot_.plan.attack == Attack::Dive &&
            (snapshot_.phase == Phase::Airborne || snapshot_.phase == Phase::Active || snapshot_.phase == Phase::Recovery)) {
            const float progress = snapshot_.phase == Phase::Airborne ? snapshot_.progress :
                (snapshot_.phase == Phase::Active || snapshot_.phase == Phase::Recovery ? 1.0f : 0.0f);
            for (unsigned index = 0; index < 2; ++index)
                snapshot_.desiredCore[index] = snapshot_.plan.coreStart[index]+(snapshot_.plan.coreTarget[index]-snapshot_.plan.coreStart[index])*progress;
        }
    }
    void Enter(Phase phase) {
        contactOpen_ = false;
        snapshot_.phase = phase;
        snapshot_.elapsed = 0;
        snapshot_.activeBeam = {};
        snapshot_.activeBeamClipped = false;
        Refresh();
    }
    void Push(Events& events, EventKind kind, unsigned index = 0) {
        if (events.count < events.values.size()) events.values[events.count++] = {kind,snapshot_.generation,snapshot_.plan.instance,index};
        if (kind == EventKind::FloorArrival || kind == EventKind::Landing || kind == EventKind::BeamBegin) ++snapshot_.activationEvents;
    }
    Events Cancel(Phase terminal) {
        Events events;
        if (Terminal()) return events;
        const bool hadAttack = snapshot_.plan.instance != 0;
        snapshot_.planning = false;
        Enter(terminal);
        hits_ = {};
        if (hadAttack) Push(events,EventKind::Cancelled);
        return events;
    }
    Snapshot snapshot_{};
    Tuning tuning_{}, pendingTuning_{};
    bool pendingTuningChanged_ = false;
    bool contactOpen_ = false;
    uint64_t nextInstance_ = 0;
    unsigned cycle_ = 0;
    // One player + three companions across at most five landing circles.
    std::array<HitRecord,4*kMaximumCircles> hits_{};
};
} // namespace neondepth
