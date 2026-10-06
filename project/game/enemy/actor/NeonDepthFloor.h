#pragma once
#include "NeonDepthCombat.h"
#include <span>

// Domain-specific XY wall clipping; the Stage adapter supplies copied
// actual merged boxes. No cg2 vector, actor, renderer or random-stream clone.
namespace neondepth {
struct FloorBox { Point minimum{}, maximum{}; };
inline bool Valid(const FloorBox& box) {
    return Bounded(box.minimum) && Bounded(box.maximum) &&
        box.minimum[0] < box.maximum[0] && box.minimum[1] < box.maximum[1];
}

// Stage already leaves a .01 collision skin outside the physical radius.
// Adding that skin again makes a legal wall-hugging start overlap every
// path's expanded box at t=0. Keep physical radius clearance and reject
// actual wall contact/crossing; no hazard or escape margin is changed.
inline bool ClearFloorPath(Point from, Point to, float radius, std::span<const FloorBox> actualWalls) {
    if (!Bounded(from) || !Bounded(to) || !Within(radius,0,8) || actualWalls.size()>4096) return false;
    for (const auto& wall : actualWalls) {
        if (!Valid(wall) || tankspecial::SegmentCrossesBox(from[0],from[1],to[0],to[1],
                wall.minimum[0]-radius,wall.minimum[1]-radius,wall.maximum[0]+radius,wall.maximum[1]+radius)) return false;
    }
    return true;
}

// Stop the beam centerline before an obstacle expanded by the capsule radius.
// Clipping only the centerline to the raw wall front leaves its rounded end
// reaching through the wall. The same stored result is used by draw and hit.
inline std::optional<Segment> ClipGroundBeamAgainstBox(const Segment& desired, const FloorBox& wall) {
    Segment result = desired;
    if (!Valid(desired) || !Valid(wall)) return std::nullopt;
    constexpr float contactGap = .001f;
    const float padding = desired.halfWidth + contactGap;
    if (desired.start[0] >= wall.minimum[0]-padding && desired.start[0] <= wall.maximum[0]+padding &&
        desired.start[1] >= wall.minimum[1]-padding && desired.start[1] <= wall.maximum[1]+padding) return std::nullopt;
    float nearFraction = 0, farFraction = 1;
    for (unsigned axis = 0; axis < 2; ++axis) {
        const float start = desired.start[axis], delta = desired.end[axis]-start;
        const float lower = wall.minimum[axis]-padding, upper = wall.maximum[axis]+padding;
        if (std::abs(delta) < .00001f) {
            if (start < lower || start > upper) return result;
        } else {
            float first = (lower-start)/delta, last = (upper-start)/delta;
            if (first > last) std::swap(first,last);
            nearFraction = (std::max)(nearFraction,first);
            farFraction = (std::min)(farFraction,last);
            if (nearFraction > farFraction) return result;
        }
    }
    const float fraction = (std::clamp)(nearFraction,0.0f,1.0f);
    for (unsigned axis = 0; axis < 2; ++axis)
        result.end[axis] = desired.start[axis]+(desired.end[axis]-desired.start[axis])*fraction;
    return result;
}

inline std::optional<Segment> ClipGroundBeam(const Segment& desired, std::span<const FloorBox> actualWalls) {
    Segment result = desired;
    // Real merged Stage geometry is bounded once per encounter. Rejecting an
    // invalid/oversized cache closes the hazard instead of ignoring a wall.
    if (!Valid(desired) || actualWalls.size() > 4096) return std::nullopt;
    for (const auto& wall : actualWalls) {
        if (!Valid(wall)) return std::nullopt;
        const auto clipped = ClipGroundBeamAgainstBox(result,wall);
        if (!clipped) return std::nullopt;
        result = *clipped;
    }
    return result;
}

// Keep a centered sweep when it is escapable. At close range the conservative
// warning union can cover every reachable exit, so also try the locked target
// on either sweep edge. All three use identical range, thickness, duration and
// clipping; the Stage adapter's original escape policy accepts or rejects them.
template<class Accept>
inline std::optional<BeamPlan> PlanGroundBeam(Point origin, Point lockedTarget, const Tuning& tuning,
        std::span<const FloorBox> actualWalls, Accept accept) {
    if (!Bounded(origin) || !Bounded(lockedTarget) || !Valid(tuning) || actualWalls.size()>4096) return std::nullopt;
    const float aim=std::atan2(lockedTarget[1]-origin[1],lockedTarget[0]-origin[0]);
    const std::array<float,3> offsets{0,tuning.beamSweepRadians*.5f,-tuning.beamSweepRadians*.5f};
    for (const float offset : offsets) {
        BeamPlan beam;
        beam.origin=origin; beam.range=tuning.beamRange; beam.halfWidth=tuning.beamHalfWidth;
        beam.startAngle=aim-tuning.beamSweepRadians*.5f+offset;
        beam.endAngle=beam.startAngle+tuning.beamSweepRadians;
        beam.warningCount=kMaximumWarningSegments;
        const float warningWidth=BeamWarningHalfWidth(beam);
        bool complete=true;
        for (unsigned index=0; index<beam.warningCount; ++index) {
            const float progress=static_cast<float>(index)/static_cast<float>(beam.warningCount-1);
            const float angle=beam.startAngle+(beam.endAngle-beam.startAngle)*progress;
            const Segment desired{origin,{origin[0]+std::cos(angle)*beam.range,origin[1]+std::sin(angle)*beam.range},warningWidth};
            const auto clipped=ClipGroundBeam(desired,actualWalls);
            if (!clipped) { complete=false; break; }
            beam.warning[index]=*clipped;
        }
        if (complete && accept(beam)) return beam;
    }
    return std::nullopt;
}
} // namespace neondepth
