// Dedicated Gameplay adapter: bounded floor plans, movement and hostile hazards.
// Existing Enemy owns the single floor position/HP; presentation reads its values.
#include "Enemy.h"
#include "Stage.h"
#include "Player.h"
#include "NeonDepthFloor.h"
#include <array>
#include <limits>
#include <span>

namespace {
using neondepth::Point;
using neondepth::FloorBox;
constexpr unsigned kColumns = 45, kRows = 30, kCells = kColumns*kRows;
constexpr float kCellStep = 2.0f;
constexpr float kCoreClearance = 2.30f; // Covers existing sphere2 and AABB half-size1.6 corners.
constexpr float kMaximumDiveTravel = 12.0f;
constexpr float kRepositionSpeed = 3.4f; // Target-profile movement only, world units/sec.
constexpr float kConservativePlayerSpeed = 6.0f; // Below unmodified normal movement; runtime verify3styles.
constexpr uint16_t kUnreachable = (std::numeric_limits<uint16_t>::max)();
Point XY(const cg2::Vector3& value) { return {value.x,value.y}; }
cg2::Vector3 Floor(Point point) { return {point[0],point[1],0}; }
float SquaredDistance(Point a, Point b) {
    const float x=a[0]-b[0], y=a[1]-b[1]; return x*x+y*y;
}
Point Cell(unsigned id) { return {static_cast<float>(id%kColumns)*2,static_cast<float>(kRows-1-id/kColumns)*2}; }
bool InRoom(Point point, float radius) {
    // Actual final_duel interior wall faces: grid10..34/7..22, cell width2.
    // Candidate cell centers are22..66/16..42. Radius must fit wall-face bounds
    //21..67/15..43, avoiding double-insetting and rejecting real wall-hugging players.
    return neondepth::Bounded(point) && point[0]-radius>=21 && point[0]+radius<=67 &&
        point[1]-radius>=15 && point[1]+radius<=43;
}
bool Clear(Stage& stage, Point point, float radius) {
    return InRoom(point,radius) && !stage.IsCollisionWithAnyBlock(Floor(point),radius);
}
bool ClearPath(Point from, Point to, float radius, std::span<const FloorBox> walls) {
    if (!InRoom(from,radius) || !InRoom(to,radius)) return false;
    return neondepth::ClearFloorPath(from,to,radius,walls);
}
struct ReachableFloor {
    std::array<uint16_t,kCells> distance{};
    float initialDistance = 0;
};
ReachableFloor Reachable(Stage& stage, Point start, float radius, std::span<const FloorBox> walls) {
    ReachableFloor result; result.distance.fill(kUnreachable);
    std::array<bool,kCells> legal{};
    unsigned seed=kCells; float nearest=9.0f;
    for (unsigned id=0; id<kCells; ++id) {
        legal[id]=Clear(stage,Cell(id),radius);
        const float squared=SquaredDistance(start,Cell(id));
        if (legal[id] && squared<nearest && ClearPath(start,Cell(id),radius,walls)) { seed=id; nearest=squared; }
    }
    if (seed==kCells) return result;
    result.initialDistance=std::sqrt(nearest);
    std::array<unsigned,kCells> queue{}; unsigned read=0,write=0;
    result.distance[seed]=0; queue[write++]=seed;
    while (read<write) {
        const unsigned id=queue[read++]; const int x=static_cast<int>(id%kColumns), y=static_cast<int>(id/kColumns);
        constexpr std::array<std::array<int,2>,4> directions{{{1,0},{-1,0},{0,1},{0,-1}}};
        for (const auto& direction : directions) {
            const int nx=x+direction[0],ny=y+direction[1];
            if (nx<0 || ny<0 || nx>=static_cast<int>(kColumns) || ny>=static_cast<int>(kRows)) continue;
            const unsigned next=static_cast<unsigned>(ny)*kColumns+static_cast<unsigned>(nx);
            if (!legal[next] || result.distance[next]!=kUnreachable || !ClearPath(Cell(id),Cell(next),radius,walls)) continue;
            result.distance[next]=static_cast<uint16_t>(result.distance[id]+1);
            queue[write++]=next; // Each cell enqueued once; fixed1350 maximum.
        }
    }
    return result;
}
template<class Accept>
std::optional<Point> NearestLegal(Stage& stage, const ReachableFloor& reachable, Point desired, float radius, Accept accept) {
    std::optional<Point> result; float best=(std::numeric_limits<float>::max)();
    for (unsigned id=0; id<kCells; ++id) {
        const Point point=Cell(id);
        if (reachable.distance[id]==kUnreachable || !Clear(stage,point,radius) || !accept(id,point)) continue;
        const float score=SquaredDistance(point,desired);
        if (score<best) { best=score; result=point; }
    }
    return result;
}
bool PointDangerous(const neondepth::AttackPlan& plan, Point point, float radius) {
    if (plan.attack==neondepth::Attack::Beam) {
        // Committed conservative capsule union, matching the displayed warning.
        for (unsigned index=0; index<plan.beam.warningCount; ++index) {
            const auto& sample=plan.beam.warning[index];
            if (tankspecial::SegmentTouches(sample.start[0],sample.start[1],sample.end[0],sample.end[1],
                    point[0],point[1],sample.halfWidth+radius)) return true;
        }
        return false;
    }
    const unsigned count=plan.attack==neondepth::Attack::Volley ? plan.circleCount : 1;
    for (unsigned index=0; index<count; ++index) {
        const auto& circle=plan.attack==neondepth::Attack::Volley ? plan.circles[index] : plan.dive;
        const float sum=circle.radius+radius;
        if (SquaredDistance(circle.center,point)<=sum*sum) return true;
    }
    return false;
}
bool HasEscape(const neondepth::AttackPlan& plan, const ReachableFloor& reachable, float radius,
        const neondepth::PlanningRequest& request) {
    const auto& duration=plan.attack==neondepth::Attack::Volley ? request.tuning.volley :
        plan.attack==neondepth::Attack::Dive ? request.tuning.dive : request.tuning.beam;
    const float travelBudget=(duration.telegraph+duration.locked)*kConservativePlayerSpeed-.5f;
    for (unsigned id=0; id<kCells; ++id)
        if (reachable.distance[id]!=kUnreachable &&
            reachable.initialDistance+static_cast<float>(reachable.distance[id])*kCellStep<=travelBudget &&
            !PointDangerous(plan,Cell(id),radius+.25f)) return true;
    return false;
}
} // namespace

bool Enemy::CacheNeonDepthStageGeometry()
{
    neonDepthGeometryReady_=false; neonDepthWallCount_=0;
    if (!stage_ || stage_->GetMergedBlocks().size()>neonDepthWalls_.size()) return false;
    for (const auto& block : stage_->GetMergedBlocks()) {
        const neondepth::FloorBox box{{block.aabb.min.x,block.aabb.min.y},{block.aabb.max.x,block.aabb.max.y}};
        if (!neondepth::Valid(box)) return false;
        // Actual floor walls are flat XY blockers. No borrowed Stage pointer is cached.
        neonDepthWalls_[neonDepthWallCount_++]=box;
    }
    neonDepthGeometryReady_=true; return true;
}

void Enemy::EnableNeonDepthEncounter(bool enabled, const neondepth::Tuning& tuning)
{
    if (!enabled) {
        neonDepthEvents_=neonDepthCombat_.Abort();
        neonDepthEnabled_=false; neonDepthGeometryReady_=false; neonDepthWallCount_=0;
        velocity_={}; impactVelocity_={}; return;
    }
    if (!object_ || !stage_ || !player_ || !runEncounterEnabled_ || isDead_ || hp_<=0 ||
        !neondepth::Valid(tuning) || encounterGeneration_==(std::numeric_limits<uint64_t>::max)()) return;
    if (!CacheNeonDepthStageGeometry() || !Clear(*stage_,XY(GetWorldPosition()),kCoreClearance)) {
        neonDepthPlanRejected_=true;
        if (neonDepthEnabled_) AbortNeonDepthEncounter();
        return;
    }
    // Enabling an encounter, even after toggling it off, always has a fresh ID.
    ++encounterGeneration_;
    if (!neonDepthCombat_.Reset(encounterGeneration_,XY(GetWorldPosition()),tuning)) return;
    neonDepthEnabled_=true; neonDepthPlanRejected_=false; neonDepthEvents_={};
    expeditionRivalEnabled_=false; prototypeCombatEnabled_=false;
    rivalCombat_.Reset(); prototypeCombat_.Reset();
    rivalDashDistance_=0; velocity_={}; impactVelocity_={};
    prototypeResourceFocus_=false; prototypeResourceTargetActive_=false; levelingModeActive_=false;
    neonDepthRepositionKey_=(std::numeric_limits<uint64_t>::max)();
}

bool Enemy::QueueNeonDepthTuning(const neondepth::Tuning& tuning) { return neonDepthCombat_.QueueTuning(tuning); }
void Enemy::SkipNeonDepthIntro() { if (neonDepthEnabled_) neonDepthEvents_=neonDepthCombat_.SkipIntro(); }
void Enemy::AbortNeonDepthEncounter()
{
    neonDepthEvents_=neonDepthCombat_.Abort(); velocity_={}; impactVelocity_={};
    // Preserve raw policy identity for retained presentation; this is not legacy enable.
}

bool Enemy::MoveNeonDepthCore(const cg2::Vector3& desiredFloor)
{
    const Point desired=XY(desiredFloor);
    if (!stage_ || !Clear(*stage_,desired,kCoreClearance)) return false;
    cg2::Vector3 delta=Floor(desired)-GetWorldPosition(); delta.z=0;
    const float largest=(std::max)(std::abs(delta.x),std::abs(delta.y));
    if (!std::isfinite(largest) || largest>16.0f) return false;
    const unsigned steps=(std::max)(1u,static_cast<unsigned>(std::ceil(largest/.35f)));
    if (steps>48) return false;
    const cg2::Vector3 step=delta*(1.0f/static_cast<float>(steps));
    for (unsigned index=0; index<steps; ++index) {
        auto position=GetWorldPosition(); position.x+=step.x; position.z=0;
        SetWorldPosition(position); stage_->ResolveEnemyCollision(*this,cg2::X);
        position=GetWorldPosition(); position.y+=step.y; position.z=0;
        SetWorldPosition(position); stage_->ResolveEnemyCollision(*this,cg2::Y);
    }
    return SquaredDistance(XY(GetWorldPosition()),desired)<=.0001f;
}

std::optional<neondepth::AttackPlan> Enemy::PlanNeonDepthAttack(const neondepth::PlanningRequest& request)
{
    if (!stage_ || !player_ || !neonDepthGeometryReady_) return std::nullopt;
    const auto walls=std::span<const neondepth::FloorBox>{neonDepthWalls_.data(),neonDepthWallCount_};
    const Point core=XY(GetWorldPosition()), target=XY(player_->GetWorldPosition());
    const float playerRadius=player_->GetRadius();
    if (!Clear(*stage_,core,kCoreClearance) || !Clear(*stage_,target,playerRadius)) return std::nullopt;
    const auto playerReach=Reachable(*stage_,target,playerRadius,walls);
    const auto coreReach=Reachable(*stage_,core,kCoreClearance,walls);
    neondepth::AttackPlan plan;
    plan.attack=request.attack; plan.phaseTwo=request.phaseTwo;
    plan.coreStart=plan.coreTarget=core; plan.lockedTarget=target;
    const float dx=target[0]-core[0],dy=target[1]-core[1];
    const float length=std::sqrt(dx*dx+dy*dy);
    const Point axis=length>.001f ? Point{dx/length,dy/length} : Point{1,0};
    if (plan.attack==neondepth::Attack::Volley) {
        plan.circleCount=request.phaseTwo ? 5 : 3;
        const float radius=request.tuning.volleyRadius, spacing=2*radius+1.6f;
        for (unsigned index=0; index<plan.circleCount; ++index) {
            const float offset=(static_cast<float>(index)-static_cast<float>(plan.circleCount-1)*.5f)*spacing;
            const Point desired{target[0]-axis[1]*offset,target[1]+axis[0]*offset};
            const auto center=NearestLegal(*stage_,playerReach,desired,radius,[&](unsigned,Point point) {
                for (unsigned previous=0; previous<index; ++previous)
                    if (SquaredDistance(point,plan.circles[previous].center)<spacing*spacing) return false;
                return true;
            });
            if (!center) return std::nullopt;
            plan.circles[index]={*center,radius};
        }
    } else if (plan.attack==neondepth::Attack::Dive) {
        const float radius=request.tuning.diveRadius;
        const auto center=NearestLegal(*stage_,coreReach,target,(std::max)(radius,kCoreClearance),[&](unsigned id,Point point) {
            return playerReach.distance[id]!=kUnreachable && SquaredDistance(core,point)<=kMaximumDiveTravel*kMaximumDiveTravel &&
                ClearPath(core,point,kCoreClearance,walls);
        });
        if (!center) return std::nullopt;
        plan.coreTarget=*center; plan.dive={*center,radius};
    } else {
        const auto beam=neondepth::PlanGroundBeam(core,target,request.tuning,walls,[&](const neondepth::BeamPlan& candidate) {
            plan.beam=candidate;
            return HasEscape(plan,playerReach,playerRadius,request);
        });
        if (!beam) return std::nullopt;
        plan.beam=*beam;
    }
    if (!HasEscape(plan,playerReach,playerRadius,request)) return std::nullopt;
    return plan;
}

void Enemy::UpdateNeonDepthCombat(float dt)
{
    const bool bossAlive=runEncounterEnabled_ && !isDead_ && hp_>0;
    const bool playerAlive=player_ && !player_->IsDead();
    const float hpRatio=maxHP_>0 ? static_cast<float>(hp_)/static_cast<float>(maxHP_) : 0;
    const bool permitted=std::isfinite(dt) && dt>0 && bossAlive && playerAlive && stage_ && neonDepthGeometryReady_;
    if (!permitted) {
        neonDepthEvents_=neonDepthCombat_.Step(0,hpRatio,bossAlive,playerAlive);
        if (bossAlive && playerAlive && (!stage_ || !neonDepthGeometryReady_)) {
            neonDepthPlanRejected_=true; AbortNeonDepthEncounter();
        }
        return;
    }
    const float stepTime=(std::min)(dt,neondepth::kMaximumStep);
    velocity_={}; impactVelocity_={}; // Never run old per-frame-unit movement in this policy.
    const auto before=neonDepthCombat_.GetSnapshot();
    const auto walls=std::span<const neondepth::FloorBox>{neonDepthWalls_.data(),neonDepthWallCount_};
    if (before.phase==neondepth::Phase::Reposition) {
        if (neonDepthRepositionKey_!=before.plan.instance) {
            neonDepthRepositionKey_=before.plan.instance;
            const Point core=XY(GetWorldPosition()), player=XY(player_->GetWorldPosition());
            const float x=core[0]-player[0],y=core[1]-player[1],length=std::sqrt(x*x+y*y);
            const Point away=length>.001f ? Point{x/length,y/length} : Point{1,0};
            const Point desired{player[0]+away[0]*8,player[1]+away[1]*8};
            const auto reachable=Reachable(*stage_,core,kCoreClearance,walls);
            const auto selected=NearestLegal(*stage_,reachable,desired,kCoreClearance,[](unsigned,Point) { return true; });
            neonDepthRepositionTarget_=selected ? Floor(*selected) : GetWorldPosition();
        }
        auto direction=neonDepthRepositionTarget_-GetWorldPosition(); direction.z=0;
        const float remaining=cg2::Length(direction);
        if (remaining>.02f) {
            direction=cg2::Normalize(direction);
            if (!ClearPath(XY(GetWorldPosition()),XY(neonDepthRepositionTarget_),kCoreClearance,walls)) {
                const auto path=FindPathDirectionToTarget(neonDepthRepositionTarget_);
                if (path) direction=*path; else direction={};
            }
            const auto desired=GetWorldPosition()+direction*(std::min)(remaining,kRepositionSpeed*stepTime);
            // A reposition collision may shorten travel; the corrected actual floor is still the next plan origin.
            MoveNeonDepthCore(desired);
        }
        neonDepthCombat_.SetRepositionCore(XY(GetWorldPosition()));
    }
    neonDepthEvents_=neonDepthCombat_.Step(stepTime,hpRatio,bossAlive,playerAlive);
    auto snapshot=neonDepthCombat_.GetSnapshot();
    if (snapshot.plan.attack==neondepth::Attack::Dive && snapshot.plan.instance &&
        (snapshot.phase==neondepth::Phase::Airborne || snapshot.phase==neondepth::Phase::Active || snapshot.phase==neondepth::Phase::Recovery)) {
        if (!MoveNeonDepthCore(Floor(snapshot.desiredCore))) {
            neonDepthPlanRejected_=true; AbortNeonDepthEncounter(); return;
        }
    }
    if (const auto request=neonDepthCombat_.GetPlanningRequest()) {
        const auto plan=PlanNeonDepthAttack(*request);
        if (plan && neonDepthCombat_.CommitPlan(*plan)) neonDepthPlanRejected_=false;
        else neonDepthPlanRejected_=true; // Stable diagnostic state; no unsafe plan/shrunk shape.
        snapshot=neonDepthCombat_.GetSnapshot();
    }
    if (snapshot.plan.instance) {
        const Point toward=snapshot.plan.lockedTarget;
        worldTransform_.rotate.z=std::atan2(toward[1]-GetWorldPosition().y,toward[0]-GetWorldPosition().x);
    }
    if (snapshot.phase!=neondepth::Phase::Active) return;
    if (snapshot.plan.attack==neondepth::Attack::Beam) {
        // GetDesiredBeam already caps range to the shortest adjacent warning.
        const auto clipped=neondepth::ClipGroundBeam(neonDepthCombat_.GetDesiredBeam(),walls);
        if (!clipped || !neonDepthCombat_.ApplyBeamClip(snapshot.generation,snapshot.plan.instance,*clipped)) {
            neonDepthPlanRejected_=true; AbortNeonDepthEncounter(); return;
        }
    }
    const unsigned count=snapshot.plan.attack==neondepth::Attack::Volley ? snapshot.plan.circleCount : 1;
    for (unsigned index=0; index<count; ++index) {
        const Point position=XY(player_->GetWorldPosition());
        if (const auto hit=neonDepthCombat_.TryClaimContact(index,player_->GetCollisionId(),position,player_->GetRadius())) {
            const uint32_t loss=player_->ReceiveHostileHazard(hit->damage,true);
            neonDepthCombat_.ReportAcceptedHit(*hit,loss);
            if (player_->IsDead()) { AbortNeonDepthEncounter(); return; }
        }
    }
}
