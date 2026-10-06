// Graphics-free regression tests execute the production Depth contract and clipping.
// Compile later with /std:c++20 /UNDEBUG and the project root on include path.
#include "game/enemy/actor/NeonDepthCombat.h"
#include "game/enemy/actor/NeonDepthFloor.h"
#include <cassert>
#include <iostream>
#include <limits>
#include <vector>
using namespace neondepth;
static std::array<FloorBox,38> ActualFinalDuelWalls() {
    // Exact Stage horizontal merges from expedition_final_duel.csv:
    // centers (column*2,(29-row)*2), block half-size1, 94 blocks -> 38 boxes.
    std::array<FloorBox,38> walls{}; unsigned box=0;
    for (unsigned row=7;row<=22;++row) {
        const float y=static_cast<float>(29-row)*2;
        if (row==7 || row==22) walls[box++]={{19,y-1},{69,y+1}};
        else {
            walls[box++]={{19,y-1},{21,y+1}};
            if (row==10 || row==11 || row==18 || row==19) {
                walls[box++]={{31,y-1},{35,y+1}}; walls[box++]={{53,y-1},{57,y+1}};
            }
            walls[box++]={{67,y-1},{69,y+1}};
        }
    }
    assert(box==walls.size());
    return walls;
}


static Events Until(Combat& combat, Phase phase, float dt = 1.0f/60) {
    Events accumulated;
    for (unsigned frame = 0; frame < 4000 && combat.GetSnapshot().phase != phase; ++frame) {
        const auto events = combat.Step(dt,.9f);
        for (unsigned index = 0; index < events.count; ++index)
            if (accumulated.count < accumulated.values.size()) accumulated.values[accumulated.count++] = events.values[index];
    }
    assert(combat.GetSnapshot().phase == phase);
    return accumulated;
}
static AttackPlan Plan(const PlanningRequest& request, Point actualCore = {44,30}) {
    AttackPlan plan;
    plan.attack = request.attack;
    plan.phaseTwo = request.phaseTwo;
    plan.coreStart = actualCore;
    plan.coreTarget = request.attack == Attack::Dive ? Point{actualCore[0]+4,actualCore[1]+2} : plan.coreStart;
    plan.lockedTarget = {40,27};
    plan.circleCount = request.phaseTwo ? 5 : 3;
    for (unsigned index = 0; index < plan.circleCount; ++index)
        plan.circles[index] = {{30+static_cast<float>(index)*5,27},request.tuning.volleyRadius};
    plan.dive = {plan.coreTarget,request.tuning.diveRadius};
    plan.beam.origin = plan.coreStart;
    plan.beam.startAngle = -request.tuning.beamSweepRadians*.5f;
    plan.beam.endAngle = request.tuning.beamSweepRadians*.5f;
    plan.beam.range = request.tuning.beamRange;
    plan.beam.halfWidth = request.tuning.beamHalfWidth;
    plan.beam.warningCount = kMaximumWarningSegments;
    for (unsigned index = 0; index < plan.beam.warningCount; ++index) {
        const float progress = static_cast<float>(index)/static_cast<float>(plan.beam.warningCount-1);
        const float angle = plan.beam.startAngle+(plan.beam.endAngle-plan.beam.startAngle)*progress;
        // Deterministic clipped-half-range fixture, not a simulation of Stage.
        plan.beam.warning[index] = {plan.beam.origin,
            {plan.beam.origin[0]+std::cos(angle)*14,plan.beam.origin[1]+std::sin(angle)*14},BeamWarningHalfWidth(plan.beam)};
    }
    return plan;
}
static AttackPlan CommitNext(Combat& combat, float hp = .9f) {
    for (unsigned frame = 0; frame < 4000 && !combat.GetPlanningRequest(); ++frame) combat.Step(1.0f/60,hp);
    const auto request = combat.GetPlanningRequest();
    assert(request);
    auto plan = Plan(*request,combat.GetSnapshot().desiredCore);
    assert(combat.CommitPlan(plan));
    return combat.GetSnapshot().plan;
}
static Combat Prepared(Attack attack, const Tuning& tuning = {}) {
    Combat combat;
    assert(combat.Reset(7,{44,30},tuning));
    assert(combat.SkipIntro().count == 1);
    while (true) {
        const auto plan = CommitNext(combat);
        if (plan.attack == attack) return combat;
        Until(combat,Phase::Recovery);
        Until(combat,Phase::Reposition);
    }
}
static Segment HalfClip(const Combat& combat) {
    auto clipped = combat.GetDesiredBeam();
    for (unsigned index = 0; index < 2; ++index) clipped.end[index] = clipped.start[index]+(clipped.end[index]-clipped.start[index])*.5f;
    return clipped;
}
static void AssertCurrentBeamWarningCoverage(const Combat& combat) {
    const auto desired=combat.GetDesiredBeam();
    const auto& warning=combat.GetSnapshot().plan.beam;
    for (unsigned sample=0; sample<=20; ++sample) {
        const float fraction=static_cast<float>(sample)/20;
        const Point point{desired.start[0]+(desired.end[0]-desired.start[0])*fraction,
            desired.start[1]+(desired.end[1]-desired.start[1])*fraction};
        bool covered=false;
        for (unsigned index=0; index<warning.warningCount; ++index) {
            const auto& capsule=warning.warning[index];
            covered=covered || tankspecial::SegmentTouches(capsule.start[0],capsule.start[1],capsule.end[0],capsule.end[1],
                point[0],point[1],capsule.halfWidth-desired.halfWidth+.0001f);
        }
        assert(covered); // Radius erosion proves the whole active disk is in the warning union.
    }
}
static void ValidatedConfigAndIntro() {
    Combat bad;
    assert(!bad.Reset(0,{44,30}));
    assert(!bad.Reset(1,{std::numeric_limits<float>::quiet_NaN(),30}));
    Tuning config;
    config.volley.telegraph = .1f;
    assert(!bad.Reset(1,{44,30},config));
    config = {}; config.volleyRadius = .49f;
    assert(!bad.Reset(1,{44,30},config));
    config = {}; config.diveRadius = 8.01f;
    assert(!bad.Reset(1,{44,30},config));
    config = {}; config.beamRange = 41;
    assert(!bad.Reset(1,{44,30},config));
    config = {}; config.beamHalfWidth = .19f;
    assert(!bad.Reset(1,{44,30},config));
    config = {}; config.beamSweepRadians = std::numeric_limits<float>::quiet_NaN();
    assert(!bad.Reset(1,{44,30},config));
    Combat natural, skipped;
    assert(natural.Reset(1,{44,30}) && skipped.Reset(1,{44,30}));
    assert(natural.GetSnapshot().inputLocked && !natural.GetSnapshot().vulnerable);
    unsigned introEvents = 0;
    for (unsigned frame = 0; frame < 500 && natural.GetSnapshot().phase == Phase::Intro; ++frame) {
        auto events = natural.Step(1.0f/60,.9f);
        for (unsigned index = 0; index < events.count; ++index) introEvents += events.values[index].kind == EventKind::IntroComplete;
        assert(!natural.TryClaimContact(0,1,{44,30},1));
    }
    assert(introEvents == 1 && skipped.SkipIntro().count == 1 && skipped.SkipIntro().count == 0);
    assert(natural.GetSnapshot() == skipped.GetSnapshot());
    assert(skipped.GetSnapshot().vulnerable && !skipped.GetSnapshot().inputLocked);
    const auto before = skipped.GetSnapshot();
    for (float dt : {0.0f,-1.0f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) {
        assert(skipped.Step(dt,.9f).count == 0);
        assert(skipped.GetSnapshot() == before);
    }
}
static void ImmutablePlanAndNextBoundaryConfig() {
    auto combat = Prepared(Attack::Volley);
    const auto original = combat.GetSnapshot().plan;
    assert(original.instance == 1 && original.generation == 7 && original.circleCount == 3);
    auto forged = original;
    forged.circles[0].center = {0,0};
    assert(!combat.CommitPlan(forged)); // Only a planning boundary can commit.
    Tuning next;
    next.volley.telegraph = 1.2f;
    next.dive.recovery = 2.0f;
    next.volleyRadius = 2.4f;
    next.diveRadius = 3.7f;
    next.beamRange = 24;
    next.beamHalfWidth = .7f;
    next.beamSweepRadians = 1.0f;
    assert(combat.QueueTuning(next));
    Until(combat,Phase::Locked);
    combat.Step(.01f,.25f);
    assert(combat.GetSnapshot().phaseTwoPending);
    assert(combat.GetSnapshot().plan == original); // HP/config do not reshape locked attack.
    Until(combat,Phase::Recovery);
    Until(combat,Phase::Reposition);
    const auto dive = CommitNext(combat);
    assert(dive.attack == Attack::Dive && dive.phaseTwo && dive.instance == 2);
    assert(dive.duration.recovery == 2.0f);
    assert(dive.dive.radius == next.diveRadius);
    Until(combat,Phase::Recovery);
    assert(combat.GetSnapshot().desiredCore == dive.coreTarget);
    Until(combat,Phase::Reposition);
    assert(combat.GetSnapshot().desiredCore == dive.coreTarget); // No reset to pre-dive start.
    const auto beam = CommitNext(combat);
    assert(beam.attack == Attack::Beam && beam.instance == 3);
    assert(beam.beam.range == next.beamRange && beam.beam.halfWidth == next.beamHalfWidth);
    assert(std::abs(beam.beam.endAngle-beam.beam.startAngle-next.beamSweepRadians) < .00001f);
    assert(beam.coreStart == dive.coreTarget && beam.beam.origin == dive.coreTarget);
    assert(combat.GetSnapshot().desiredCore == dive.coreTarget); // Commit cannot teleport a landed core.
    Until(combat,Phase::Recovery);
    Until(combat,Phase::Reposition);
    const auto secondVolley = CommitNext(combat);
    assert(secondVolley.attack == Attack::Volley && secondVolley.circleCount == 5 && secondVolley.phaseTwo);
    assert(secondVolley.duration.telegraph == 1.2f && secondVolley.instance == 4);
    assert(secondVolley.circles[0].radius == next.volleyRadius);
}
static void PlanningValidationAndValueCopy() {
    Combat combat;
    assert(combat.Reset(12,{44,30}));
    combat.SkipIntro();
    for (unsigned frame = 0; frame < 100 && !combat.GetPlanningRequest(); ++frame) combat.Step(.1f,.9f);
    const auto request = combat.GetPlanningRequest();
    assert(request);
    auto valid = Plan(*request);
    auto invalid = valid;
    invalid.attack = Attack::Dive;
    assert(!combat.CommitPlan(invalid));
    invalid = valid; invalid.phaseTwo = true;
    assert(!combat.CommitPlan(invalid));
    invalid = valid; invalid.circleCount = 100;
    assert(!combat.CommitPlan(invalid));
    invalid = valid; invalid.coreStart = {45,30};
    assert(!combat.CommitPlan(invalid)); // Cannot jump away from the actual corrected floor.
    invalid = valid; invalid.circles[1].radius = std::numeric_limits<float>::infinity();
    assert(!combat.CommitPlan(invalid));
    assert(combat.GetPlanningRequest() && combat.GetSnapshot().plan.instance == 0);
    assert(combat.CommitPlan(valid));
    const auto copied = combat.GetSnapshot().plan;
    valid.circles[0].center = {60,40};
    assert(combat.GetSnapshot().plan == copied && copied.instance == 1);
    Tuning bad;
    bad.beamDamage = 0;
    assert(!combat.QueueTuning(bad));
    Until(combat,Phase::Recovery); Until(combat,Phase::Reposition);
    CommitNext(combat); // Dive.
    Until(combat,Phase::Recovery); Until(combat,Phase::Reposition);
    for (unsigned frame = 0; frame < 100 && !combat.GetPlanningRequest(); ++frame) combat.Step(.1f,.9f);
    auto beam = Plan(*combat.GetPlanningRequest(),combat.GetSnapshot().desiredCore);
    assert(beam.attack == Attack::Beam);
    invalid = beam; invalid.beam.warning[2].end[1] += 2;
    assert(!combat.CommitPlan(invalid));
    invalid = beam; invalid.beam.warningCount = 1;
    assert(!combat.CommitPlan(invalid));
    assert(combat.CommitPlan(beam));
}
static void SpikeWarningsAndFloorArrival() {
    auto combat = Prepared(Attack::Volley);
    combat.Step(1000000,.9f);
    assert(combat.GetSnapshot().phase == Phase::Telegraph && combat.GetSnapshot().elapsed == kMaximumStep);
    assert(combat.GetSnapshot().activationEvents == 0);
    Until(combat,Phase::Locked,.1f);
    assert(combat.GetSnapshot().elapsed == 0 && !combat.TryClaimContact(0,1,{30,27},.8f));
    combat.Step(1000000,.9f);
    assert(combat.GetSnapshot().phase == Phase::Locked);
    Until(combat,Phase::Airborne,.1f);
    assert(combat.GetSnapshot().elapsed == 0 && combat.GetSnapshot().activationEvents == 0);
    assert(!combat.TryClaimContact(0,1,{30,27},.8f)); // Air projection never hurts.
    const auto events = Until(combat,Phase::Active,.1f);
    assert(events.count == 3 && combat.GetSnapshot().activationEvents == 3);
    for (unsigned index = 0; index < events.count; ++index) {
        assert(events.values[index].kind == EventKind::FloorArrival && events.values[index].index == index);
        assert(events.values[index].generation == 7 && events.values[index].instance == 1);
    }
    assert(combat.Step(0,.9f).count == 0);
    for (unsigned read = 0; read < 50; ++read) assert(combat.GetSnapshot().activationEvents == 3);
    assert(!combat.TryClaimContact(0,1,{30,27},.8f)); // No hit permission while paused.
    assert(combat.Step(.01f,.9f).count == 0);
    auto hit = combat.TryClaimContact(0,1,{30,27},.8f);
    assert(hit && !combat.TryClaimContact(0,1,{30,27},.8f));
    assert(!combat.ReportAcceptedHit(*hit,0)); // Invulnerability is not actual HP loss.
    assert(combat.ReportAcceptedHit(*hit,hit->damage));
    assert(!combat.ReportAcceptedHit(*hit,hit->damage)); // Actor report itself is one-shot.
    assert(combat.GetSnapshot().acceptedHits == 1);
    assert(!combat.TryClaimContact(0,2,{70,27},.8f));
    for (unsigned index = 0; index < 3; ++index)
        for (uint64_t target = 2; target < 6; ++target) assert(combat.TryClaimContact(index,target,{30+static_cast<float>(index)*5.0f,27},.8f));
    Until(combat,Phase::Recovery,.1f);
    assert(combat.GetSnapshot().activeCircleMask == 0 && combat.GetSnapshot().vulnerable);
    assert(!combat.TryClaimContact(0,10,{30,27},.8f));
    assert(combat.GetSnapshot().activationEvents == 3);
}
static void DiveAndBeamHits() {
    auto dive = Prepared(Attack::Dive);
    const auto plan = dive.GetSnapshot().plan;
    Until(dive,Phase::Airborne);
    dive.Step(.1f,.9f);
    assert(dive.GetSnapshot().desiredCore[0] > plan.coreStart[0] && dive.GetSnapshot().desiredCore[0] < plan.coreTarget[0]);
    assert(!dive.TryClaimContact(0,1,plan.dive.center,.8f));
    const auto landed = Until(dive,Phase::Active);
    assert(landed.count == 1 && landed.values[0].kind == EventKind::Landing);
    assert(dive.GetSnapshot().desiredCore == plan.coreTarget);
    auto hit = dive.TryClaimContact(0,1,plan.dive.center,.8f);
    assert(hit && !dive.TryClaimContact(0,1,plan.dive.center,.8f));
    Until(dive,Phase::Recovery);
    assert(!dive.TryClaimContact(0,1,plan.dive.center,.8f));

    auto beam = Prepared(Attack::Beam);
    const auto beamPlan = beam.GetSnapshot().plan;
    assert(!beam.ApplyBeamClip(7,beamPlan.instance,HalfClip(beam)));
    const auto started = Until(beam,Phase::Active);
    assert(started.count == 1 && started.values[0].kind == EventKind::BeamBegin);
    assert(!beam.TryClaimContact(0,1,beamPlan.beam.origin,.8f)); // Must be clipped first.
    auto clipped = HalfClip(beam);
    assert(!beam.ApplyBeamClip(6,beamPlan.instance,clipped));
    assert(!beam.ApplyBeamClip(7,beamPlan.instance-1,clipped));
    auto tooLong = beam.GetDesiredBeam();
    for (unsigned index = 0; index < 2; ++index) tooLong.end[index] += tooLong.end[index]-tooLong.start[index];
    assert(!beam.ApplyBeamClip(7,beamPlan.instance,tooLong));
    auto wrongDirection = clipped;
    wrongDirection.end[1] += 5;
    assert(!beam.ApplyBeamClip(7,beamPlan.instance,wrongDirection));
    assert(beam.ApplyBeamClip(7,beamPlan.instance,clipped));
    assert(beam.GetSnapshot().activeBeam == clipped);
    const auto desired = beam.GetDesiredBeam();
    Point behindWall;
    for (unsigned index = 0; index < 2; ++index) behindWall[index] = desired.start[index]+(desired.end[index]-desired.start[index])*.9f;
    assert(!beam.TryClaimContact(0,2,behindWall,.8f));
    unsigned ticks = 0;
    for (unsigned frame = 0; frame < 120 && beam.GetSnapshot().phase == Phase::Active; ++frame) {
        clipped = HalfClip(beam);
        assert(beam.ApplyBeamClip(7,beamPlan.instance,clipped));
        if (const auto tick = beam.TryClaimContact(0,1,beamPlan.beam.origin,.8f)) {
            ++ticks;
            assert(beam.ReportAcceptedHit(*tick,tick->damage));
        }
        assert(!beam.TryClaimContact(0,1,beamPlan.beam.origin,.8f));
        beam.Step(1.0f/60,.9f);
    }
    assert(ticks >= 2 && ticks <= 3 && beam.GetSnapshot().phase == Phase::Recovery);
    assert(!beam.TryClaimContact(0,1,beamPlan.beam.origin,.8f));
    assert(!beam.ApplyBeamClip(7,beamPlan.instance,clipped));
}
static void DeathAbortAndIdentity() {
    Combat intro;
    assert(intro.Reset(4,{44,30}));
    assert(intro.Defeat().count == 0 && intro.GetSnapshot().phase == Phase::Defeated && !intro.GetSnapshot().inputLocked);
    Combat reposition;
    assert(reposition.Reset(4,{44,30}));
    reposition.SkipIntro();
    assert(reposition.Abort().count == 0 && !reposition.GetPlanningRequest());
    for (auto attack : {Attack::Volley,Attack::Dive,Attack::Beam}) {
        for (auto phase : {Phase::Telegraph,Phase::Locked,Phase::Airborne,Phase::Active,Phase::Recovery}) {
            if (attack == Attack::Beam && phase == Phase::Airborne) continue;
            auto combat = Prepared(attack);
            Until(combat,phase);
            const auto old = combat.GetSnapshot();
            const auto cancelled = combat.Defeat();
            assert(cancelled.count == 1 && cancelled.values[0].kind == EventKind::Cancelled);
            assert(combat.GetSnapshot().phase == Phase::Defeated && !combat.GetSnapshot().vulnerable);
            assert(!combat.GetSnapshot().inputLocked && combat.GetSnapshot().activeCircleMask == 0);
            assert(!combat.GetSnapshot().activeBeamClipped && combat.GetSnapshot().desiredCore == old.desiredCore);
            const auto terminal = combat.GetSnapshot();
            assert(combat.Defeat().count == 0 && combat.Abort().count == 0);
            for (unsigned frame = 0; frame < 200; ++frame) assert(combat.Step(1,.1f).count == 0);
            assert(combat.GetSnapshot() == terminal && !combat.TryClaimContact(0,1,{44,30},.8f));
            assert(combat.Reset(8,{44,30}));
            combat.SkipIntro();
            const auto first = CommitNext(combat);
            assert(first.generation == 8 && first.instance == 1);
            assert(!combat.ApplyBeamClip(old.generation,old.plan.instance,old.activeBeam));
        }
    }
    auto deathAtZeroDt = Prepared(Attack::Dive);
    assert(deathAtZeroDt.Step(std::numeric_limits<float>::quiet_NaN(),.1f,false).count == 1);
    assert(deathAtZeroDt.GetSnapshot().phase == Phase::Defeated);
    auto playerDeath = Prepared(Attack::Beam);
    Until(playerDeath,Phase::Active);
    assert(playerDeath.Step(0,.9f,true,false).count == 1 && playerDeath.GetSnapshot().phase == Phase::Aborted);
}
static void PausedContactsAndWallThickness() {
    for (auto attack : {Attack::Volley,Attack::Dive,Attack::Beam}) {
        auto combat = Prepared(attack);
        Until(combat,Phase::Active);
        const auto plan = combat.GetSnapshot().plan;
        if (attack == Attack::Beam) assert(combat.ApplyBeamClip(7,plan.instance,HalfClip(combat)));
        const Point hitPoint = attack == Attack::Volley ? plan.circles[0].center :
            attack == Attack::Dive ? plan.dive.center : plan.beam.origin;
        const auto frozen = combat.GetSnapshot();
        for (float dt : {0.0f,-1.0f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) {
            assert(combat.Step(dt,.9f).count == 0 && combat.GetSnapshot() == frozen);
            assert(!combat.TryClaimContact(0,1,hitPoint,.8f));
            if (attack == Attack::Beam) assert(!combat.ApplyBeamClip(7,plan.instance,HalfClip(combat)));
        }
        combat.Step(.01f,.9f);
        if (attack == Attack::Beam) assert(combat.ApplyBeamClip(7,plan.instance,HalfClip(combat)));
        const auto hit = combat.TryClaimContact(0,1,hitPoint,.8f);
        assert(hit);
        combat.Step(0,.9f);
        assert(!combat.ReportAcceptedHit(*hit,hit->damage));
        assert(combat.GetSnapshot().acceptedHits == 0);
    }

    const Segment desired{{0,0},{28,0},.8f};
    const std::array<FloorBox,1> wall{{FloorBox{{10,-3},{10.5f,3}}}};
    const Point behindWall{11.4f,0};
    assert(tankspecial::SegmentTouches(0,0,10,0,behindWall[0],behindWall[1],.8f+.8f));
    const auto canonical = ClipGroundBeam(desired,wall);
    assert(canonical && IsClippedSegment(desired,*canonical) && canonical->end[0] < 9.2f && canonical->end[0] > 9.19f);
    assert(!tankspecial::SegmentTouches(canonical->start[0],canonical->start[1],canonical->end[0],canonical->end[1],
        behindWall[0],behindWall[1],canonical->halfWidth+.8f));
    // This is the intended production clip primitive; actual Stage box-copy,
    // moving actors and occlusion still require real adapter/runtime tests.
    const std::array<FloorBox,1> invalidWall{{FloorBox{{10,-3},{9,3}}}};
    assert(!ClipGroundBeam(desired,invalidWall));
    assert(ClipGroundBeam(desired,std::span<const FloorBox>{})->end == desired.end);
    const std::array<FloorBox,1> originBlocked{{FloorBox{{-.1f,-.1f},{.1f,.1f}}}};
    assert(!ClipGroundBeam(desired,originBlocked)); // A zero-length capsule would still be damaging.

    Combat beam;
    assert(beam.Reset(17,{0,0})); beam.SkipIntro();
    while (true) {
        for (unsigned frame = 0; frame < 4000 && !beam.GetPlanningRequest(); ++frame) beam.Step(1.0f/60,.9f);
        const auto request = beam.GetPlanningRequest(); assert(request);
        auto plan = Plan(*request,beam.GetSnapshot().desiredCore);
        if (plan.attack != Attack::Beam) {
            assert(beam.CommitPlan(plan)); Until(beam,Phase::Recovery); Until(beam,Phase::Reposition); continue;
        }
        plan.beam.startAngle = 0; plan.beam.endAngle = request->tuning.beamSweepRadians;
        const Point origin = plan.beam.origin;
        const std::array<FloorBox,1> actualWall{{FloorBox{{origin[0]+10,origin[1]-3},{origin[0]+10.5f,origin[1]+3}}}};
        for (unsigned index = 0; index < plan.beam.warningCount; ++index) {
            const float angle = plan.beam.endAngle*static_cast<float>(index)/static_cast<float>(plan.beam.warningCount-1);
            const Segment ray{origin,{origin[0]+std::cos(angle)*plan.beam.range,origin[1]+std::sin(angle)*plan.beam.range},BeamWarningHalfWidth(plan.beam)};
            const auto clip = ClipGroundBeam(ray,actualWall); assert(clip); plan.beam.warning[index] = *clip;
        }
        assert(beam.CommitPlan(plan)); Until(beam,Phase::Active);
        const auto clip = ClipGroundBeam(beam.GetDesiredBeam(),actualWall); assert(clip);
        assert(beam.ApplyBeamClip(17,beam.GetSnapshot().plan.instance,*clip));
        assert(!beam.TryClaimContact(0,1,{origin[0]+11.4f,origin[1]},.8f));
        assert(beam.TryClaimContact(0,2,{origin[0]+3,origin[1]},.8f));
        // Around the wall's upper corner neighboring clipped rays have very
        // different lengths. The conservative active cap must cover that gap.
        while (beam.GetSnapshot().phase==Phase::Active) {
            AssertCurrentBeamWarningCoverage(beam);
            beam.Step(.001f,.9f);
        }
        break;
    }
}
static void ReadOnlyAndDeterminism() {
    Combat a, b;
    assert(a.Reset(99,{44,30}) && b.Reset(99,{44,30}));
    a.SkipIntro(); b.SkipIntro();
    for (unsigned frame = 0; frame < 1600; ++frame) {
        for (Combat* combat : {&a,&b}) {
            if (const auto request = combat->GetPlanningRequest())
                assert(combat->CommitPlan(Plan(*request,combat->GetSnapshot().desiredCore)));
            combat->Step(1.0f/60,frame < 300 ? .9f : .4f);
            if (combat->GetSnapshot().phase == Phase::Active && combat->GetSnapshot().plan.attack == Attack::Beam)
                assert(combat->ApplyBeamClip(99,combat->GetSnapshot().plan.instance,HalfClip(*combat)));
        }
        const auto stable = b.GetSnapshot();
        for (unsigned draw = 0; draw < 5; ++draw) assert(b.GetSnapshot() == stable);
        assert(a.GetSnapshot() == b.GetSnapshot());
    }
    // Different dt tests legal transitions and warning clocks, not bit equality.
    for (float dt : {1.0f/30,1.0f/60,1.0f/120}) {
        auto combat = Prepared(Attack::Dive);
        const auto priorActivations = combat.GetSnapshot().activationEvents;
        float warningTime = 0;
        while (combat.GetSnapshot().phase == Phase::Telegraph) { warningTime += dt; combat.Step(dt,.9f); }
        assert(warningTime >= combat.GetSnapshot().plan.duration.telegraph);
        Until(combat,Phase::Locked,dt);
        Until(combat,Phase::Active,dt);
        assert(combat.GetSnapshot().activationEvents == priorActivations+1);
        Until(combat,Phase::Recovery,dt);
        assert(combat.GetSnapshot().activeCircleMask == 0);
    }
}
static void ResetIdentityAndStaleHitReports() {
    auto combat = Prepared(Attack::Volley);
    Until(combat,Phase::Active);
    const auto stale = combat.TryClaimContact(0,1,{30,27},.8f); assert(stale);
    const auto retained = combat.GetSnapshot();
    assert(!combat.Reset(7,{44,30}) && combat.GetSnapshot() == retained);
    assert(!combat.Reset(6,{44,30}) && combat.GetSnapshot() == retained);
    assert(!combat.Reset(0,{44,30}) && combat.GetSnapshot() == retained);
    assert(!combat.Reset(8,{std::numeric_limits<float>::quiet_NaN(),30}) && combat.GetSnapshot() == retained);
    assert(combat.ReportAcceptedHit(*stale,stale->damage));
    assert(combat.Reset(8,{44,30})); combat.SkipIntro();
    const auto plan = CommitNext(combat); assert(plan.generation == 8 && plan.instance == 1);
    Until(combat,Phase::Active);
    const auto current = combat.TryClaimContact(0,1,{30,27},.8f); assert(current);
    assert(current->claim == stale->claim && current->instance == stale->instance && current->generation != stale->generation);
    assert(!combat.ReportAcceptedHit(*stale,stale->damage));
    assert(combat.ReportAcceptedHit(*current,current->damage) && combat.GetSnapshot().acceptedHits == 1);
    combat.Abort();
    const auto terminal = combat.GetSnapshot();
    assert(!combat.Reset(8,{44,30}) && combat.GetSnapshot() == terminal);
}
static void ContinuousBeamWarningCoverage() {
    Tuning large; large.beamRange=40; large.beamHalfWidth=2; large.beamSweepRadians=2.4f;
    for (const Tuning& tuning : {Tuning{},large}) {
        auto combat=Prepared(Attack::Beam,tuning); Until(combat,Phase::Active);
        unsigned sampled=0;
        while (combat.GetSnapshot().phase==Phase::Active) {
            AssertCurrentBeamWarningCoverage(combat);
            ++sampled; combat.Step(.001f,.9f);
        }
        assert(sampled>1000);
    }
    Combat reposition; assert(reposition.Reset(1,{44,30}));
    assert(!reposition.SetRepositionCore({46,30})); reposition.SkipIntro();
    const auto old=reposition.GetSnapshot(); assert(reposition.SetRepositionCore({46,30}));
    const Point moved{46,30};
    assert(reposition.GetSnapshot().desiredCore==moved && reposition.GetSnapshot().elapsed==old.elapsed);
    const auto next=CommitNext(reposition); assert(next.coreStart==moved);
    assert(!reposition.SetRepositionCore({48,30})); // A committed warning cannot be moved.
}
static void CloseBeamKeepsEscapeAndFullWarning() {
    const auto walls=ActualFinalDuelWalls();
    // These 24 cells are the actual Stage BFS outputs within the unchanged
    // 6.7-unit escape budget from (26,28), radius.8. Every path is clear here.
    std::array<Point,24> exits{}; unsigned exit=0;
    for (int x=-3;x<=3;++x) for (int y=-3;y<=3;++y)
        if (std::abs(x)+std::abs(y)<=3 && x>=-2) exits[exit++]={26+2.0f*static_cast<float>(x),28+2.0f*static_cast<float>(y)};
    assert(exit==exits.size());
    auto dangerous=[](const BeamPlan& beam, Point point, float radius) {
        for (unsigned i=0;i<beam.warningCount;++i) {
            const auto& s=beam.warning[i];
            if (tankspecial::SegmentTouches(s.start[0],s.start[1],s.end[0],s.end[1],point[0],point[1],s.halfWidth+radius)) return true;
        }
        return false;
    };
    Tuning tuning; const Point core{34,28},target{26,28};
    unsigned attempts=0;
    const auto selected=PlanGroundBeam(core,target,tuning,walls,[&](const BeamPlan& candidate) {
        ++attempts;
        bool escape=false; for (const auto point:exits) escape|=!dangerous(candidate,point,1.05f);
        if (attempts==1) assert(!escape); // Original centered plan really has no exit.
        assert(dangerous(candidate,target,.8f)); // Locked target remains in the visible future band.
        return escape;
    });
    assert(selected && attempts==2 && !dangerous(*selected,{26,32},1.05f));
    assert(selected->origin==core && selected->range==tuning.beamRange && selected->halfWidth==tuning.beamHalfWidth);
    assert(std::abs(selected->endAngle-selected->startAngle-tuning.beamSweepRadians)<.00001f);
    assert(selected->warningCount==kMaximumWarningSegments);
    for (const auto& warning:selected->warning) assert(warning.halfWidth==BeamWarningHalfWidth(*selected));
    const auto centered=PlanGroundBeam(core,target,tuning,walls,[](const BeamPlan&) {return true;}); assert(centered);
    assert(std::abs(centered->startAngle-(std::atan2(0.0f,-8.0f)-.6f))<.00001f);
    const auto lower=PlanGroundBeam(core,target,tuning,walls,[&](const BeamPlan& candidate) {return !dangerous(candidate,{26,24},1.05f);}); assert(lower);
    assert(lower->endAngle<centered->endAngle && dangerous(*lower,target,.8f));
    attempts=0; assert(!PlanGroundBeam(core,target,tuning,walls,[&](const BeamPlan&) {++attempts;return false;})); assert(attempts==3);
    attempts=0; assert(!PlanGroundBeam({32,38},target,tuning,walls,[&](const BeamPlan&) {++attempts;return true;})); assert(attempts==0);
    auto invalid=tuning; invalid.beamHalfWidth=std::numeric_limits<float>::quiet_NaN();
    assert(!PlanGroundBeam(core,target,invalid,walls,[](const BeamPlan&) {return true;}));

    auto combat=Prepared(Attack::Dive); Until(combat,Phase::Recovery); Until(combat,Phase::Reposition);
    assert(combat.SetRepositionCore(core));
    for (unsigned frame=0;frame<120 && !combat.GetPlanningRequest();++frame) combat.Step(1.0f/60,.2f);
    const auto request=combat.GetPlanningRequest(); assert(request && request->attack==Attack::Beam);
    auto plan=Plan(*request,core); plan.lockedTarget=target; plan.beam=*selected;
    assert(combat.CommitPlan(plan)); Until(combat,Phase::Active);
    while (combat.GetSnapshot().phase==Phase::Active) {
        const auto& snapshot=combat.GetSnapshot(); const auto clipped=ClipGroundBeam(combat.GetDesiredBeam(),walls); assert(clipped);
        assert(combat.ApplyBeamClip(snapshot.generation,snapshot.plan.instance,*clipped));
        AssertCurrentBeamWarningCoverage(combat);
        assert(!combat.TryClaimContact(0,1,{26,32},.8f)); // The real active capsule never damages the promised exit.
        combat.Step(1.0f/600,.2f);
    }
}
static void WallSkinPreservesPhysicalClearance() {
    const auto walls=ActualFinalDuelWalls();
    constexpr float radius=.8f, skin=radius+.01f;
    struct Departure { Point from,to; };
    const std::array<Departure,8> departures{{
        {{26,15+skin},{26,16}}, {{26,43-skin},{26,42}},
        {{21+skin,28},{22,28}}, {{67-skin,28},{66,28}},
        {{21+skin,15+skin},{22,16}}, {{67-skin,15+skin},{66,16}},
        {{21+skin,43-skin},{22,42}}, {{67-skin,43-skin},{66,42}}
    }};
    for (const auto& departure:departures) {
        assert(ClearFloorPath(departure.from,departure.to,radius,walls));
        assert(ClearFloorPath(departure.to,departure.from,radius,walls));
        assert(ClearFloorPath(departure.from,departure.from,radius,walls));
    }
    // Exact styles_preflight_g failure: old radius+.01 padding rejects t=0.
    const Point actualPlayer{26,15.8100004196167f},nearestSeed{26,16};
    const auto& lower=walls.back();
    assert(lower.maximum[1]==15 && actualPlayer[1]==15+skin);
    assert(tankspecial::SegmentCrossesBox(actualPlayer[0],actualPlayer[1],nearestSeed[0],nearestSeed[1],
        lower.minimum[0]-skin,lower.minimum[1]-skin,lower.maximum[0]+skin,lower.maximum[1]+skin));
    assert(ClearFloorPath(actualPlayer,nearestSeed,radius,walls));
    assert(ClearFloorPath(actualPlayer,{28,actualPlayer[1]},radius,walls)); // Skin-parallel motion is physically clear.
    // Physical-radius contact is still rejected; removing an extra skin does
    // not allow travel through a wall or reduce the core's 2.3 clearance.
    assert(!ClearFloorPath({26,15+radius},nearestSeed,radius,walls));
    assert(!ClearFloorPath({26,43-radius},{26,42},radius,walls));
    assert(!ClearFloorPath({21+radius,28},{22,28},radius,walls));
    assert(!ClearFloorPath({67-radius,28},{66,28},radius,walls));
    assert(!ClearFloorPath(actualPlayer,{26,14},radius,walls));
    assert(!ClearFloorPath({26,43-skin},{26,44},radius,walls));
    assert(!ClearFloorPath({21+skin,28},{20,28},radius,walls));
    assert(!ClearFloorPath({67-skin,28},{68,28},radius,walls));
    assert(!ClearFloorPath({30,38},{36,38},radius,walls)); // Actual interior pillar.
    const Point actualCore{59.96519470214844f,29.854637145996094f},landing{50,26};
    assert(ClearFloorPath(actualCore,landing,2.3f,walls));
    assert(!ClearFloorPath(landing,{54,24},2.3f,walls));
    // The unchanged full-size Dive has a reachable safe seed within the
    // unchanged Telegraph+Locked travel budget. This is a production-helper
    // boundary/contract regression, not a replacement Stage planner runtime.
    const Tuning tuning;
    const float travel=std::sqrt((nearestSeed[0]-actualPlayer[0])*(nearestSeed[0]-actualPlayer[0])+
        (nearestSeed[1]-actualPlayer[1])*(nearestSeed[1]-actualPlayer[1]));
    const float budget=(tuning.dive.telegraph+tuning.dive.locked)*6-.5f;
    assert(travel<=budget);
    assert(!tankspecial::SegmentTouches(landing[0],landing[1],landing[0],landing[1],
        nearestSeed[0],nearestSeed[1],tuning.diveRadius+radius+.25f));
    auto dive=Prepared(Attack::Volley); Until(dive,Phase::Recovery); Until(dive,Phase::Reposition);
    assert(dive.SetRepositionCore(actualCore));
    for (unsigned frame=0;frame<120 && !dive.GetPlanningRequest();++frame) dive.Step(1.0f/60,.9f);
    const auto request=dive.GetPlanningRequest(); assert(request && request->attack==Attack::Dive);
    auto candidate=Plan(*request,actualCore);
    candidate.lockedTarget=actualPlayer; candidate.coreTarget=landing; candidate.dive={landing,request->tuning.diveRadius};
    assert(dive.CommitPlan(candidate));
    assert(dive.GetSnapshot().plan.dive.radius==3.4f && dive.GetSnapshot().plan.duration==tuning.dive);
    Until(dive,Phase::Active);
    assert(dive.GetSnapshot().desiredCore==landing && !dive.TryClaimContact(0,1,nearestSeed,radius));

    const float nan=std::numeric_limits<float>::quiet_NaN(),infinity=std::numeric_limits<float>::infinity();
    assert(!ClearFloorPath({nan,28},nearestSeed,radius,walls));
    assert(!ClearFloorPath(actualPlayer,{26,infinity},radius,walls));
    assert(!ClearFloorPath(actualPlayer,nearestSeed,nan,walls));
    assert(!ClearFloorPath(actualPlayer,nearestSeed,infinity,walls));
    assert(!ClearFloorPath(actualPlayer,nearestSeed,-.001f,walls));
    assert(!ClearFloorPath(actualPlayer,nearestSeed,8.001f,walls));
    assert(!ClearFloorPath({10001,28},nearestSeed,radius,walls));
    const std::array<FloorBox,1> invalid{{FloorBox{{10,10},{9,11}}}};
    const std::array<FloorBox,1> nonfinite{{FloorBox{{nan,10},{11,11}}}};
    assert(!ClearFloorPath(actualPlayer,nearestSeed,radius,invalid));
    assert(!ClearFloorPath(actualPlayer,nearestSeed,radius,nonfinite));
    const std::vector<FloorBox> oversized(4097,walls.front());
    assert(!ClearFloorPath(actualPlayer,nearestSeed,radius,oversized));
    assert(ClearFloorPath({0,0},{1,1},0,std::span<const FloorBox>{}));
}
int main() {
    WallSkinPreservesPhysicalClearance();
    ValidatedConfigAndIntro(); ImmutablePlanAndNextBoundaryConfig(); PlanningValidationAndValueCopy(); SpikeWarningsAndFloorArrival();
    DiveAndBeamHits(); DeathAbortAndIdentity(); PausedContactsAndWallThickness(); ReadOnlyAndDeterminism(); ResetIdentityAndStaleHitReports(); ContinuousBeamWarningCoverage(); CloseBeamKeepsEscapeAndFullWarning();
    std::cout << "NeonDepth pure contract: bounded timing, plans, shapes, event identity, hit cooldown, cancellation and immutable reads passed.\n";
}
