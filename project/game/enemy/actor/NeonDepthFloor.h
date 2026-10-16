#pragma once
#include "NeonDepthCombat.h"
#include <span>

// この用途専用のXY平面上の壁による切り詰め。Stageアダプタが実際に結合した箱の
// コピーを渡す。myEngineのベクトル、アクター、描画、乱数ストリームの複製は持たない。
namespace neondepth {
struct FloorBox {
    Point minimum{}, maximum{};
};

inline bool Valid(const FloorBox& box)
{
    return Bounded(box.minimum) && Bounded(box.maximum) && box.minimum[0] < box.maximum[0] && box.minimum[1] < box.maximum[1];
}

// Stageは既に物理半径の外側に0.01の衝突用の余白を残している。
// その余白をもう一度足すと、壁沿いの正しい開始位置が t=0 の時点で
// すべての経路の拡張した箱と重なってしまう。物理半径分の隙間は保ちつつ、
// 実際の壁への接触や貫通は拒否する。危険範囲や回避の余裕は変えない。
inline bool ClearFloorPath(Point from, Point to, float radius, std::span<const FloorBox> actualWalls)
{
    if (!Bounded(from) || !Bounded(to) || !Within(radius, 0, 8) || actualWalls.size() > 4096) {
        return false;
    }
    for (const auto& wall : actualWalls) {
        if (!Valid(wall) || tankspecial::SegmentCrossesBox(from[0], from[1], to[0], to[1], wall.minimum[0] - radius,
                                                           wall.minimum[1] - radius, wall.maximum[0] + radius, wall.maximum[1] + radius)) {
            return false;
        }
    }
    return true;
}

// カプセル半径分だけ膨らませた障害物の手前でビームの中心線を止める。
// 中心線を生の壁面で切るだけだと、丸い先端が壁を突き抜けてしまう。
// 保存した同じ結果を描画と当たり判定の両方で使う。
inline std::optional<Segment> ClipGroundBeamAgainstBox(const Segment& desired, const FloorBox& wall)
{
    Segment result = desired;
    if (!Valid(desired) || !Valid(wall)) {
        return std::nullopt;
    }
    constexpr float contactGap = .001f;
    const float padding = desired.halfWidth + contactGap;
    if (desired.start[0] >= wall.minimum[0] - padding && desired.start[0] <= wall.maximum[0] + padding &&
        desired.start[1] >= wall.minimum[1] - padding && desired.start[1] <= wall.maximum[1] + padding) {
        return std::nullopt;
    }
    float nearFraction = 0, farFraction = 1;
    for (unsigned axis = 0; axis < 2; ++axis) {
        const float start = desired.start[axis], delta = desired.end[axis] - start;
        const float lower = wall.minimum[axis] - padding, upper = wall.maximum[axis] + padding;
        if (std::abs(delta) < .00001f) {
            if (start < lower || start > upper) {
                return result;
            }
        } else {
            float first = (lower - start) / delta, last = (upper - start) / delta;
            if (first > last) {
                std::swap(first, last);
            }
            nearFraction = (std::max)(nearFraction, first);
            farFraction = (std::min)(farFraction, last);
            if (nearFraction > farFraction) {
                return result;
            }
        }
    }
    const float fraction = (std::clamp)(nearFraction, 0.0f, 1.0f);
    for (unsigned axis = 0; axis < 2; ++axis) {
        result.end[axis] = desired.start[axis] + (desired.end[axis] - desired.start[axis]) * fraction;
    }
    return result;
}

inline std::optional<Segment> ClipGroundBeam(const Segment& desired, std::span<const FloorBox> actualWalls)
{
    Segment result = desired;
    // 実際に結合したStageの形状は戦闘ごとに1回だけ範囲を調べる。不正または
    // 大きすぎるキャッシュは、壁を無視するのではなく攻撃判定を閉じて拒否する。
    if (!Valid(desired) || actualWalls.size() > 4096) {
        return std::nullopt;
    }
    for (const auto& wall : actualWalls) {
        if (!Valid(wall)) {
            return std::nullopt;
        }
        const auto clipped = ClipGroundBeamAgainstBox(result, wall);
        if (!clipped) {
            return std::nullopt;
        }
        result = *clipped;
    }
    return result;
}

// 回避可能なら中央の掃引を使う。近距離では保守的な警告の和集合が
// 到達可能な出口をすべて塞ぐことがあるため、掃引の両端で固定した標的も試す。
// 3つとも射程・太さ・時間・切り詰めは同じで、
// Stageアダプタの元の回避方針で採用か却下かを決める。
template <class Accept>
inline std::optional<BeamPlan> PlanGroundBeam(Point origin, Point lockedTarget, const Tuning& tuning, std::span<const FloorBox> actualWalls,
                                              Accept accept)
{
    if (!Bounded(origin) || !Bounded(lockedTarget) || !Valid(tuning) || actualWalls.size() > 4096) {
        return std::nullopt;
    }
    const float aim = std::atan2(lockedTarget[1] - origin[1], lockedTarget[0] - origin[0]);
    const std::array<float, 3> offsets{0, tuning.beamSweepRadians * .5f, -tuning.beamSweepRadians * .5f};
    for (const float offset : offsets) {
        BeamPlan beam;
        beam.origin = origin;
        beam.range = tuning.beamRange;
        beam.halfWidth = tuning.beamHalfWidth;
        beam.startAngle = aim - tuning.beamSweepRadians * .5f + offset;
        beam.endAngle = beam.startAngle + tuning.beamSweepRadians;
        beam.warningCount = kMaximumWarningSegments;
        const float warningWidth = BeamWarningHalfWidth(beam);
        bool complete = true;
        for (unsigned index = 0; index < beam.warningCount; ++index) {
            const float progress = static_cast<float>(index) / static_cast<float>(beam.warningCount - 1);
            const float angle = beam.startAngle + (beam.endAngle - beam.startAngle) * progress;
            const Segment desired{
                origin, {origin[0] + std::cos(angle) * beam.range, origin[1] + std::sin(angle) * beam.range}, warningWidth};
            const auto clipped = ClipGroundBeam(desired, actualWalls);
            if (!clipped) {
                complete = false;
                break;
            }
            beam.warning[index] = *clipped;
        }
        if (complete && accept(beam)) {
            return beam;
        }
    }
    return std::nullopt;
}
} // namespace neondepth
