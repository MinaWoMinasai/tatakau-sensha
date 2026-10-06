#pragma once
// Shared value-only floor, placement, camera and input projection math.
#include "Calculation.h"
#include <cmath>
#include <algorithm>

namespace neondepth {

// The room is XY, viewed from negative Z. U x V is +Z; the semantic height
// normal is -Z. Model orientation below remains a proper rotation, not a mirror.
struct FloorBasis {
    cg2::Vector3 origin{0, 0, 0};
    static constexpr cg2::Vector3 u{1, 0, 0};
    static constexpr cg2::Vector3 v{0, 1, 0};
    static constexpr cg2::Vector3 normal{0, 0, -1};
};

inline bool Finite(float value) { return std::isfinite(value); }
inline bool Finite(const cg2::Vector2& v) { return Finite(v.x) && Finite(v.y); }
inline bool Finite(const cg2::Vector3& v) { return Finite(v.x) && Finite(v.y) && Finite(v.z); }
inline bool Finite(const cg2::Matrix4x4& m) {
    for (const auto& row : m.m) for (float value : row) if (!Finite(value)) return false;
    return true;
}
inline bool InRange(float value, float low, float high) {
    return Finite(value) && value >= low && value <= high;
}

// Validate determinant before using the engine inverse (which divides without
// a singular guard). Elimination only checks invertibility; engine math still
// produces the inverse. Failed queries preserve the caller's previous matrix.
inline bool TryInverseViewProjection(const cg2::Matrix4x4& matrix, cg2::Matrix4x4& output) {
    if (!Finite(matrix)) return false;
    double values[4][4]{};
    for (int row=0;row<4;++row) for(int column=0;column<4;++column) values[row][column]=matrix.m[row][column];
    double determinant=1;
    for(int column=0;column<4;++column) {
        int pivot=column;
        for(int row=column+1;row<4;++row)
            if(std::abs(values[row][column])>std::abs(values[pivot][column])) pivot=row;
        if(std::abs(values[pivot][column])<=1.0e-12) return false;
        if(pivot!=column) { for(int index=0;index<4;++index) std::swap(values[column][index],values[pivot][index]); determinant=-determinant; }
        const double diagonal=values[column][column]; determinant*=diagonal;
        for(int row=column+1;row<4;++row) {
            const double factor=values[row][column]/diagonal;
            for(int index=column+1;index<4;++index) values[row][index]-=factor*values[column][index];
        }
    }
    if(!std::isfinite(determinant) || std::abs(determinant)<=1.0e-12) return false;
    const auto candidate=cg2::Inverse(matrix);
    if(!Finite(candidate)) return false;
    const auto identity=cg2::Multiply(matrix,candidate);
    for(int row=0;row<4;++row) for(int column=0;column<4;++column)
        if(!Finite(identity.m[row][column]) || std::abs(identity.m[row][column]-(row==column?1.0f:0.0f))>.02f) return false;
    output=candidate; return true;
}

struct PlacementConfig {
    // Determined once from bind skeleton / current-model bind bounds at load.
    cg2::Vector3 stableFootLocal{};
    float boundsHeight = 1.5674443f;
    float worldHeight = 18.0f;
    // This asset's candidate front is local -Z, initially world floor -Y.
    float forwardYawOffset = cg2::pi * 0.5f;
};
struct PlacementInput {
    cg2::Vector3 floorAnchor{}; // Authoritative Enemy XY / room floor Z.
    cg2::Vector2 floorOffset{}; // Purely visual retreat; never move the collider.
    float hoverHeight = 3.0f;
    float bodyHeading = -cg2::pi * 0.5f;
    cg2::Vector3 localLean{}; // Fresh bounded angles, never previous-pose deltas.
};
struct PlacementResult {
    cg2::Matrix4x4 world{};
    cg2::Vector3 footWorld{};
    cg2::Vector3 localUpWorld{};
    float uniformScale = 1.0f;
};

// Invalid input leaves the previous valid output unchanged. Stable foot is
// centered before scale/lean/upright/yaw; animated feet never redefine it.
inline bool TryPlacement(const FloorBasis& floor, const PlacementConfig& config,
    const PlacementInput& input, PlacementResult& output) {
    if (!Finite(floor.origin) || !Finite(config.stableFootLocal) ||
        !InRange(config.boundsHeight, 0.001f, 1000.0f) ||
        !InRange(config.worldHeight, 0.1f, 80.0f) || !Finite(config.forwardYawOffset) ||
        !Finite(input.floorAnchor) || !Finite(input.floorOffset) ||
        std::abs(input.floorAnchor.z - floor.origin.z) > 1.0e-4f ||
        !InRange(input.hoverHeight, 0.0f, 80.0f) || !Finite(input.bodyHeading) ||
        !InRange(input.localLean.x, -0.8f, 0.8f) ||
        !InRange(input.localLean.y, -0.8f, 0.8f) ||
        !InRange(input.localLean.z, -0.8f, 0.8f)) return false;
    const float scale = config.worldHeight / config.boundsHeight;
    const cg2::Vector3 footWorld{input.floorAnchor.x + input.floorOffset.x,
        input.floorAnchor.y + input.floorOffset.y, floor.origin.z - input.hoverHeight};
    if (!Finite(footWorld)) return false;
    const auto center = cg2::MakeTranslateMatrix({-config.stableFootLocal.x,
        -config.stableFootLocal.y, -config.stableFootLocal.z});
    const auto scaled = cg2::MakeScaleMatrix({scale, scale, scale});
    const auto lean = cg2::MakeAffineMatrix({1, 1, 1}, input.localLean, {0, 0, 0});
    const auto upright = cg2::MakeRotateXMatrix(-cg2::pi * 0.5f);
    const auto yaw = cg2::MakeRotateZMatrix(input.bodyHeading + config.forwardYawOffset);
    const auto placed = cg2::MakeTranslateMatrix(footWorld);
    PlacementResult candidate;
    candidate.world = cg2::Multiply(cg2::Multiply(cg2::Multiply(cg2::Multiply(
        cg2::Multiply(center, scaled), lean), upright), yaw), placed);
    candidate.footWorld = footWorld;
    candidate.localUpWorld = cg2::TransformNormal({0, 1, 0}, candidate.world);
    const float upLength = cg2::Length(candidate.localUpWorld);
    if (!Finite(candidate.world) || !Finite(upLength) || upLength <= 1.0e-6f) return false;
    candidate.localUpWorld.x /= upLength;
    candidate.localUpWorld.y /= upLength;
    candidate.localUpWorld.z /= upLength;
    candidate.uniformScale = scale;
    output = candidate;
    return true;
}

struct CameraProfile {
    float tiltDegrees = 20.0f; // Compare actual normal gameplay at 15 / 20 / 25.
    float distance = 64.0f;
    float fovY = 0.55f;
    float aspect = 1280.0f / 720.0f;
    float nearClip = 0.1f;
    float farClip = 5000.0f;
};
struct CameraFrame {
    cg2::Vector3 position{}, rotation{};
    cg2::Matrix4x4 world{}, view{}, projection{}, viewProjection{}, inverseViewProjection{};
};
inline bool TryCamera(const CameraProfile& profile, const cg2::Vector3& focus,
    CameraFrame& output) {
    if (!Finite(focus) || !InRange(profile.tiltDegrees, 0.0f, 35.0f) ||
        !InRange(profile.distance, 5.0f, 500.0f) ||
        !InRange(profile.fovY, 0.1f, 1.5f) || !InRange(profile.aspect, 0.2f, 5.0f) ||
        !InRange(profile.nearClip, 0.01f, 10.0f) ||
        !InRange(profile.farClip, profile.nearClip + 1.0f, 10000.0f)) return false;
    const float tilt = profile.tiltDegrees * cg2::pi / 180.0f;
    CameraFrame candidate;
    candidate.position = {focus.x, focus.y - profile.distance * std::sin(tilt),
        focus.z - profile.distance * std::cos(tilt)};
    candidate.rotation = {-tilt, 0, 0};
    candidate.world = cg2::MakeAffineMatrix({1, 1, 1}, candidate.rotation, candidate.position);
    candidate.view = cg2::Inverse(candidate.world);
    candidate.projection = cg2::MakePerspectiveForMatrix(profile.fovY, profile.aspect,
        profile.nearClip, profile.farClip);
    candidate.viewProjection = cg2::Multiply(candidate.view, candidate.projection);
    if (!TryInverseViewProjection(candidate.viewProjection,candidate.inverseViewProjection)) return false;
    if (!Finite(candidate.world) || !Finite(candidate.view) || !Finite(candidate.projection) ||
        !Finite(candidate.viewProjection) || !Finite(candidate.inverseViewProjection)) return false;
    output = candidate;
    return true;
}

// Homogeneous conversion deliberately avoids the existing Vector4 transform's
// implicit w handling and Vector3 transform's assert for w == 0.
inline bool TryHomogeneousPoint(const cg2::Vector3& point, const cg2::Matrix4x4& matrix,
    cg2::Vector3& output, bool requirePositiveW = false) {
    if (!Finite(point) || !Finite(matrix)) return false;
    const double values[4] = {
        double(point.x)*matrix.m[0][0] + double(point.y)*matrix.m[1][0] + double(point.z)*matrix.m[2][0] + matrix.m[3][0],
        double(point.x)*matrix.m[0][1] + double(point.y)*matrix.m[1][1] + double(point.z)*matrix.m[2][1] + matrix.m[3][1],
        double(point.x)*matrix.m[0][2] + double(point.y)*matrix.m[1][2] + double(point.z)*matrix.m[2][2] + matrix.m[3][2],
        double(point.x)*matrix.m[0][3] + double(point.y)*matrix.m[1][3] + double(point.z)*matrix.m[2][3] + matrix.m[3][3]};
    if (!std::isfinite(values[3]) || std::abs(values[3]) <= 1.0e-8 ||
        (requirePositiveW && values[3] <= 0.0)) return false;
    const cg2::Vector3 candidate{static_cast<float>(values[0]/values[3]),
        static_cast<float>(values[1]/values[3]), static_cast<float>(values[2]/values[3])};
    if (!Finite(candidate)) return false;
    output = candidate;
    return true;
}

// Offscreen coordinates remain useful for framing; positive w and clip depth
// are required. Drawing/picking policy handles the rectangle bounds separately.
inline bool TryProject(const cg2::Vector3& world, const cg2::Matrix4x4& viewProjection,
    const cg2::Vector2& viewport, cg2::Vector2& output, float* clipDepth = nullptr) {
    if (!Finite(viewport) || viewport.x <= 0 || viewport.y <= 0) return false;
    cg2::Vector3 ndc{};
    if (!TryHomogeneousPoint(world, viewProjection, ndc, true)) return false;
    // Compare unrounded clip depth, matching the direct floor query. Rounding
    // to float first can turn a point just beyond far into exactly1 and accept
    // a point the displayed-VP floor query correctly rejects.
    const double clipZ=double(world.x)*viewProjection.m[0][2]+double(world.y)*viewProjection.m[1][2]+
        double(world.z)*viewProjection.m[2][2]+viewProjection.m[3][2];
    const double clipW=double(world.x)*viewProjection.m[0][3]+double(world.y)*viewProjection.m[1][3]+
        double(world.z)*viewProjection.m[2][3]+viewProjection.m[3][3];
    const double depth=clipZ/clipW;
    if(!std::isfinite(depth)||depth<0||depth>1) return false;
    const cg2::Vector2 candidate{(ndc.x * 0.5f + 0.5f)*viewport.x,
        (0.5f - ndc.y*0.5f)*viewport.y};
    if (!Finite(candidate)) return false;
    output = candidate;
    if (clipDepth) *clipDepth = float(depth);
    return true;
}

// An inverse of the exact displayed matrix, including the same-frame shake,
// must be provided. The encounter jitter policy is fixed before actor update.
// Floor picking is restricted to the displayed near/far segment, so an invisible
// point outside the depth clip interval cannot become an attack aim target.
inline bool TryScreenToFloor(const cg2::Vector2& pixel, const cg2::Vector2& viewport,
    const cg2::Matrix4x4& inverseViewProjection, float floorZ, cg2::Vector3& output) {
    if (!Finite(pixel) || !Finite(viewport) || viewport.x <= 0 || viewport.y <= 0 || !Finite(floorZ)) return false;
    const float x = 2.0f*pixel.x/viewport.x - 1.0f;
    const float y = 1.0f - 2.0f*pixel.y/viewport.y;
    cg2::Vector3 nearPoint{}, farPoint{};
    if (!TryHomogeneousPoint({x, y, 0}, inverseViewProjection, nearPoint) ||
        !TryHomogeneousPoint({x, y, 1}, inverseViewProjection, farPoint)) return false;
    const double dz = double(farPoint.z) - nearPoint.z;
    const double dx = double(farPoint.x) - nearPoint.x;
    const double dy = double(farPoint.y) - nearPoint.y;
    const double length = std::sqrt(dx*dx + dy*dy + dz*dz);
    if (!std::isfinite(length) || length <= 1.0e-8 || std::abs(dz) <= length*1.0e-6) return false;
    const double t = (double(floorZ) - nearPoint.z)/dz;
    if (!std::isfinite(t) || t < 0.0 || t > 1.0) return false;
    const cg2::Vector3 candidate{static_cast<float>(nearPoint.x + dx*t),
        static_cast<float>(nearPoint.y + dy*t), floorZ};
    if (!Finite(candidate)) return false;
    output = candidate;
    return true;
}

// Prefer this for actual aim: solve the displayed floor directly from the same
// float VP used to draw. A float inverse stores camera translation as the
// difference of large terms at near.1/far5000 and loses subpixel precision even
// when homogeneous evaluation uses doubles. This guarded2x2 floor restriction
// avoids that inverse rounding; cg2::Inverse still validates full invertibility.
inline bool TryScreenToFloorFromViewProjection(const cg2::Vector2& pixel,
    const cg2::Vector2& viewport,const cg2::Matrix4x4& viewProjection,
    float floorZ,cg2::Vector3& output) {
    if(!Finite(pixel)||!Finite(viewport)||viewport.x<=0||viewport.y<=0||!Finite(floorZ)) return false;
    cg2::Matrix4x4 checkedInverse;
    if(!TryInverseViewProjection(viewProjection,checkedInverse)) return false;
    const double x=2.0*pixel.x/viewport.x-1.0,y=1.0-2.0*pixel.y/viewport.y;
    const double a=viewProjection.m[0][0]-x*viewProjection.m[0][3];
    const double b=viewProjection.m[1][0]-x*viewProjection.m[1][3];
    const double c=viewProjection.m[0][1]-y*viewProjection.m[0][3];
    const double d=viewProjection.m[1][1]-y*viewProjection.m[1][3];
    const double constantW=double(floorZ)*viewProjection.m[2][3]+viewProjection.m[3][3];
    const double first=x*constantW-double(floorZ)*viewProjection.m[2][0]-viewProjection.m[3][0];
    const double second=y*constantW-double(floorZ)*viewProjection.m[2][1]-viewProjection.m[3][1];
    const double determinant=a*d-b*c;
    const double norm=(std::max)({std::abs(a),std::abs(b),std::abs(c),std::abs(d)});
    if(!std::isfinite(determinant)||norm<=1.0e-12||std::abs(determinant)<=norm*norm*1.0e-12) return false;
    const cg2::Vector3 candidate{float((first*d-b*second)/determinant),
        float((a*second-first*c)/determinant),floorZ};
    if(!Finite(candidate)) return false;
    const double clipW=double(candidate.x)*viewProjection.m[0][3]+double(candidate.y)*viewProjection.m[1][3]+constantW;
    const double clipZ=double(candidate.x)*viewProjection.m[0][2]+double(candidate.y)*viewProjection.m[1][2]+
        double(floorZ)*viewProjection.m[2][2]+viewProjection.m[3][2];
    if(!std::isfinite(clipW)||!std::isfinite(clipZ)||clipW<=1.0e-8) return false;
    const double depth=clipZ/clipW;
    if(!std::isfinite(depth)||depth<0||depth>1) return false;
    output=candidate;return true;
}

inline bool TryClientToViewport(const cg2::Vector2& clientPixel,
    const cg2::Vector2& clientSize, const cg2::Vector2& viewportSize, cg2::Vector2& output) {
    if (!Finite(clientPixel) || !Finite(clientSize) || !Finite(viewportSize) ||
        clientSize.x <= 0 || clientSize.y <= 0 || viewportSize.x <= 0 || viewportSize.y <= 0) return false;
    const cg2::Vector2 candidate{clientPixel.x*viewportSize.x/clientSize.x,
        clientPixel.y*viewportSize.y/clientSize.y};
    if (!Finite(candidate)) return false;
    output = candidate;
    return true;
}

} // namespace neondepth
