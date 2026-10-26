#include "NeonGridGeometry.h"
#include "Calculation.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace myEngine {

namespace {
constexpr uint32_t kContourRadii = 4;
constexpr uint32_t kContourOffsets = kContourRadii * 2;
constexpr uint32_t kContourSpanVertices = (kContourOffsets - 1) * 6;
constexpr uint32_t kContourMaxPoints = 96;
constexpr uint32_t kContourCapSegments = 8;
constexpr float kContourJoinStep = 0.5235987756f; // 角丸1区間あたり最大30度。

/// @brief ベクトルの全成分が有限かを返す。
bool ContourFinite(const Vector3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

/// @brief 中心座標に広がりを足しても有限のままかを返す。
bool ContourPositionRange(const Vector3& center, float extent)
{
    return std::isfinite(std::abs(center.x) + extent) && std::isfinite(std::abs(center.y) + extent) &&
           std::isfinite(std::abs(center.z) + extent);
}

/// @brief 2つのベクトルの内積を返す。
float ContourDot(const Vector3& a, const Vector3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

/// @brief 値を範囲内に収める。非有限ならfallbackを返す。
float ContourBounded(float value, float fallback, float minimum, float maximum)
{
    return std::isfinite(value) ? (std::clamp)(value, minimum, maximum) : fallback;
}

/// @brief 前方向を正規化する。不正な値ならZ+を返す。
Vector3 ContourForward(const Vector3& forward)
{
    const float length = ContourFinite(forward) ? Length(forward) : 0.0f;
    return std::isfinite(length) && length > 0.0001f ? forward / length : Vector3{0.0f, 0.0f, 1.0f};
}

struct ContourProfile {
    std::array<float, kContourRadii> radii{};
    std::array<Vector4, kContourRadii> colors{};
};

/// @brief 線の太さと色から輪郭の断面プロファイル（半径ごとの色）を作る。不正な入力ならfalseを返す。
bool MakeContourProfile(float width, const Vector4& source, const NeonContourStyle& requested, ContourProfile& profile)
{
    if (!std::isfinite(width) || width <= 0.0f || !std::isfinite(source.w) || source.w <= 0.001f || !std::isfinite(source.x) ||
        !std::isfinite(source.y) || !std::isfinite(source.z)) {
        return false;
    }
    const Vector3 rgb{(std::clamp)(source.x, 0.0f, 8.0f), (std::clamp)(source.y, 0.0f, 8.0f), (std::clamp)(source.z, 0.0f, 8.0f)};
    const float peak = (std::max)({rgb.x, rgb.y, rgb.z});
    if (peak <= 0.0001f) {
        return false;
    }
    const float alpha = (std::clamp)(source.w, 0.0f, 1.0f);
    const NeonContourStyle defaults{};
    const float coreRatio = ContourBounded(requested.coreWidthRatio, defaults.coreWidthRatio, 0.08f, 0.8f);
    const float core = ContourBounded(requested.coreIntensity, defaults.coreIntensity, 0.0f, 12.0f);
    const float white = ContourBounded(requested.coreWhiteMix, defaults.coreWhiteMix, 0.0f, 1.0f);
    const float shoulder = ContourBounded(requested.shoulderIntensity, defaults.shoulderIntensity, 0.0f, 8.0f);
    const float haloScale = ContourBounded(requested.haloWidthScale, defaults.haloWidthScale, 1.4f, 5.0f);
    const float halo = ContourBounded(requested.haloIntensity, defaults.haloIntensity, 0.0f, 4.0f);
    const float haloAlpha = ContourBounded(requested.haloAlpha, defaults.haloAlpha, 0.0f, 0.4f);
    const float half = width * 0.5f;
    if (!std::isfinite(half * haloScale)) {
        return false;
    }
    profile.radii = {half * coreRatio, half * (std::max)(0.65f, coreRatio + 0.10f), half * (1.0f + (haloScale - 1.0f) * 0.36f),
                     half * haloScale};
    const Vector3 pale = rgb * (1.0f - white) + Vector3{peak, peak, peak} * white;
    profile.colors = {{{pale.x * core, pale.y * core, pale.z * core, alpha},
                       {rgb.x * shoulder, rgb.y * shoulder, rgb.z * shoulder, alpha},
                       {rgb.x * halo, rgb.y * halo, rgb.z * halo, alpha * haloAlpha},
                       {rgb.x * halo * 0.35f, rgb.y * halo * 0.35f, rgb.z * halo * 0.35f, 0.0f}}};
    return true;
}

struct ContourSection {
    Vector3 center{};
    Vector3 positive{};
    Vector3 negative{};
};

/// @brief 断面のoffset番目の頂点位置を返す。
Vector3 ContourSectionPoint(const ContourSection& section, uint32_t offset, const ContourProfile& profile)
{
    if (offset < kContourRadii) {
        return section.center - section.negative * profile.radii[kContourRadii - 1 - offset];
    }
    return section.center + section.positive * profile.radii[offset - kContourRadii];
}

/// @brief 断面のoffset番目の頂点色を返す。
Vector4 ContourSectionColor(uint32_t offset, const ContourProfile& profile)
{
    return profile.colors[offset < kContourRadii ? kContourRadii - 1 - offset : offset - kContourRadii];
}
} // namespace

void NeonGridGeometry::SetVertexBuffer(std::span<TrailVertex> vertices)
{
    vertexData_ = vertices.data();
    vertexCapacity_ = static_cast<uint32_t>((std::min)(vertices.size(), static_cast<size_t>(kMaxVertices)));
    vertexCount_ = 0;
}

void NeonGridGeometry::BeginFrame()
{
    vertexCount_ = 0;
}

void NeonGridGeometry::SetLineStyle(float softEdgeRatio, float coreIntensity)
{
    lineSoftEdgeRatio_ = std::clamp(softEdgeRatio, 0.0f, 0.95f);
    lineCoreIntensity_ = (std::max)(0.0f, coreIntensity);
}

void NeonGridGeometry::QueueLine(const Vector3& a, const Vector3& b, float lineWidth, const Vector4& color)
{
    AddLineQuad(a, b, lineWidth, color);
}

void NeonGridGeometry::QueueCameraFacingLine(const Vector3& a, const Vector3& b, float lineWidth, const Vector4& color,
                                             const Vector3& cameraForward)
{
    AddCameraFacingLineQuad(a, b, lineWidth, color, cameraForward);
}

void NeonGridGeometry::QueueContourLine(const Vector3& a, const Vector3& b, float lineWidth, const Vector4& color,
                                        const Vector3& cameraForward, const NeonContourStyle& style)
{
    ContourProfile profile;
    if (!vertexData_ || !ContourFinite(a) || !ContourFinite(b) || !MakeContourProfile(lineWidth, color, style, profile)) {
        return;
    }
    const Vector3 forward = ContourForward(cameraForward);
    Vector3 direction = b - a;
    direction = direction - forward * ContourDot(direction, forward);
    const float length = Length(direction);
    if (!std::isfinite(length) || length <= 0.0001f) {
        return;
    }
    direction = direction / length;
    const Vector3 normal = Cross(forward, direction);
    if (!ContourPositionRange(a, profile.radii.back()) || !ContourPositionRange(b, profile.radii.back())) {
        return;
    }
    const uint32_t required = kContourSpanVertices + (style.roundCaps ? 2 * kContourCapSegments * (3 + (kContourRadii - 1) * 6) : 0);
    if (required > vertexCapacity_ || vertexCount_ > vertexCapacity_ - required) {
        return;
    }

    const ContourSection begin{a, normal, normal}, end{b, normal, normal};
    auto triangle = [&](const Vector3& p0, const Vector4& c0, const Vector3& p1, const Vector4& c1, const Vector3& p2, const Vector4& c2) {
        PushVertex(p0, c0, {0.5f, 0.5f});
        PushVertex(p1, c1, {0.5f, 0.5f});
        PushVertex(p2, c2, {0.5f, 0.5f});
    };
    for (uint32_t offset = 0; offset + 1 < kContourOffsets; ++offset) {
        const Vector3 p0 = ContourSectionPoint(begin, offset, profile), p1 = ContourSectionPoint(begin, offset + 1, profile);
        const Vector3 p2 = ContourSectionPoint(end, offset, profile), p3 = ContourSectionPoint(end, offset + 1, profile);
        const Vector4 c0 = ContourSectionColor(offset, profile), c1 = ContourSectionColor(offset + 1, profile);
        triangle(p0, c0, p1, c1, p2, c0);
        triangle(p1, c1, p3, c1, p2, c0);
    }
    if (!style.roundCaps) {
        return;
    }
    // この半円キャップは本体の端の断面とまったく同じ形を共有する。
    // 円を重ねる方式と違い、両端で発光が二重にならない。
    constexpr float kContourPi = 3.14159265359f;
    for (int cap = 0; cap < 2; ++cap) {
        const Vector3 center = cap == 0 ? a : b;
        const Vector3 outward = direction * (cap == 0 ? -1.0f : 1.0f);
        for (uint32_t segment = 0; segment < kContourCapSegments; ++segment) {
            const float angle0 = -kContourPi * 0.5f + kContourPi * static_cast<float>(segment) / static_cast<float>(kContourCapSegments);
            const float angle1 =
                -kContourPi * 0.5f + kContourPi * static_cast<float>(segment + 1) / static_cast<float>(kContourCapSegments);
            const Vector3 radial0 = outward * std::cos(angle0) + normal * std::sin(angle0);
            const Vector3 radial1 = outward * std::cos(angle1) + normal * std::sin(angle1);
            triangle(center, profile.colors[0], center + radial0 * profile.radii[0], profile.colors[0], center + radial1 * profile.radii[0],
                     profile.colors[0]);
            for (uint32_t radius = 0; radius + 1 < kContourRadii; ++radius) {
                const Vector3 p0 = center + radial0 * profile.radii[radius], p1 = center + radial0 * profile.radii[radius + 1];
                const Vector3 p2 = center + radial1 * profile.radii[radius], p3 = center + radial1 * profile.radii[radius + 1];
                triangle(p0, profile.colors[radius], p1, profile.colors[radius + 1], p2, profile.colors[radius]);
                triangle(p1, profile.colors[radius + 1], p3, profile.colors[radius + 1], p2, profile.colors[radius]);
            }
        }
    }
}

void NeonGridGeometry::QueueContourPolygon(const Vector3* points, uint32_t pointCount, float lineWidth, const Vector4& color,
                                           const Vector3& cameraForward, const NeonContourStyle& style)
{
    ContourProfile profile;
    if (!vertexData_ || !points || pointCount < 3 || pointCount > kContourMaxPoints ||
        !MakeContourProfile(lineWidth, color, style, profile)) {
        return;
    }
    const Vector3 forward = ContourForward(cameraForward);
    const float maximumExtent = profile.radii.back() * 4.0f;
    if (!std::isfinite(maximumExtent)) {
        return;
    }
    std::array<Vector3, kContourMaxPoints> normals{};
    for (uint32_t point = 0; point < pointCount; ++point) {
        if (!ContourFinite(points[point]) || !ContourPositionRange(points[point], maximumExtent)) {
            return;
        }
        Vector3 direction = points[(point + 1) % pointCount] - points[point];
        direction = direction - forward * ContourDot(direction, forward);
        const float length = Length(direction);
        if (!std::isfinite(length) || length <= 0.0001f) {
            return;
        }
        normals[point] = Cross(forward, direction / length);
    }
    // サイズ上限付きのスタック作業領域。オブジェクトごとの確保や可変な描画スタイル状態は持たない。
    std::array<ContourSection, kContourMaxPoints * 8> sections{};
    uint32_t sectionCount = 0;
    for (uint32_t point = 0; point < pointCount; ++point) {
        const Vector3 previous = normals[(point + pointCount - 1) % pointCount], next = normals[point];
        const float cosine = (std::clamp)(ContourDot(previous, next), -1.0f, 1.0f);
        const float sine = ContourDot(forward, Cross(previous, next));
        const float angle = std::atan2(sine, cosine);
        const float angleSize = std::abs(angle);
        Vector3 miter = previous + next;
        const float miterLength = Length(miter);
        if (!std::isfinite(miterLength) || miterLength <= 0.0001f) {
            return; // 折り返した辺や逆向きの辺は輪郭として扱わない。
        }
        miter = miter / miterLength;
        const float denominator = (std::max)(0.001f, std::abs(ContourDot(miter, next)));
        miter = miter * (std::min)(4.0f, 1.0f / denominator);
        // もともと滑らかな円のシルエットは、共有のマイター断面1つで十分。
        // 30度未満なら円弧との差は4%未満で、ここで扇状の断面を増やすと
        // 頂点数が無駄に倍になる。
        const uint32_t steps =
            angleSize <= kContourJoinStep ? 0 : (std::min)(6u, static_cast<uint32_t>(std::ceil(angleSize / kContourJoinStep)));
        for (uint32_t step = 0; step <= steps; ++step) {
            const float t = steps == 0 ? 0.0f : static_cast<float>(step) / static_cast<float>(steps);
            const float rotation = angle * t;
            const Vector3 rounded = steps == 0 ? miter : previous * std::cos(rotation) + Cross(forward, previous) * std::sin(rotation);
            // 内側の境界は共有の交点1つのままにし、外側の境界だけを円弧に沿わせる。
            // そのため隣り合う帯の間に隙間ができたり、角で明るい帯が二重に重なったりしない。
            sections[sectionCount++] = {points[point], angle >= 0.0f ? miter : rounded, angle >= 0.0f ? rounded : miter};
        }
    }
    const uint32_t required = sectionCount * kContourSpanVertices;
    if (required > vertexCapacity_ || vertexCount_ > vertexCapacity_ - required) {
        return;
    }
    for (uint32_t section = 0; section < sectionCount; ++section) {
        const ContourSection& a = sections[section];
        const ContourSection& b = sections[(section + 1) % sectionCount];
        for (uint32_t offset = 0; offset + 1 < kContourOffsets; ++offset) {
            const Vector3 p0 = ContourSectionPoint(a, offset, profile), p1 = ContourSectionPoint(a, offset + 1, profile);
            const Vector3 p2 = ContourSectionPoint(b, offset, profile), p3 = ContourSectionPoint(b, offset + 1, profile);
            const Vector4 c0 = ContourSectionColor(offset, profile), c1 = ContourSectionColor(offset + 1, profile);
            PushVertex(p0, c0, {0.5f, 0.5f});
            PushVertex(p1, c1, {0.5f, 0.5f});
            PushVertex(p2, c0, {0.5f, 0.5f});
            PushVertex(p1, c1, {0.5f, 0.5f});
            PushVertex(p3, c1, {0.5f, 0.5f});
            PushVertex(p2, c0, {0.5f, 0.5f});
        }
    }
}

void NeonGridGeometry::QueueContourTriangle(const Vector3& a, const Vector3& b, const Vector3& c, float lineWidth, const Vector4& color,
                                            const NeonContourStyle& style)
{
    const Vector3 points[]{a, b, c};
    QueueContourPolygon(points, 3, lineWidth, color, {0.0f, 0.0f, 1.0f}, style);
}

void NeonGridGeometry::QueueContourRectangle(const Vector3& center, const Vector3& size, float lineWidth, const Vector4& color,
                                             const NeonContourStyle& style)
{
    if (!ContourFinite(center) || !ContourFinite(size) || size.x <= 0.0f || size.y <= 0.0f) {
        return;
    }
    const float halfX = size.x * 0.5f, halfY = size.y * 0.5f, z = center.z - 0.02f;
    const Vector3 points[]{{center.x - halfX, center.y + halfY, z},
                           {center.x + halfX, center.y + halfY, z},
                           {center.x + halfX, center.y - halfY, z},
                           {center.x - halfX, center.y - halfY, z}};
    QueueContourPolygon(points, 4, lineWidth, color, {0.0f, 0.0f, 1.0f}, style);
}

void NeonGridGeometry::QueueBillboardContourTriangle(const Vector3& center, float radius, float rotationRad, float lineWidth,
                                                     const Vector4& color, const Vector3& cameraRight, const Vector3& cameraUp,
                                                     const Vector3& cameraForward, const NeonContourStyle& style)
{
    if (!ContourFinite(center) || !ContourFinite(cameraRight) || !ContourFinite(cameraUp) || !std::isfinite(radius) || radius <= 0.0f ||
        !std::isfinite(rotationRad)) {
        return;
    }
    constexpr float twoPi = 6.28318530718f;
    Vector3 points[3]{};
    for (int point = 0; point < 3; ++point) {
        const float angle = -twoPi * 0.25f + rotationRad + static_cast<float>(point) * twoPi / 3.0f;
        points[point] = center + cameraRight * (std::cos(angle) * radius) + cameraUp * (std::sin(angle) * radius);
    }
    QueueContourPolygon(points, 3, lineWidth, color, cameraForward, style);
}

void NeonGridGeometry::QueueBillboardContourRectangle(const Vector3& center, const Vector2& size, float rotationRad, float lineWidth,
                                                      const Vector4& color, const Vector3& cameraRight, const Vector3& cameraUp,
                                                      const Vector3& cameraForward, const NeonContourStyle& style)
{
    if (!ContourFinite(center) || !ContourFinite(cameraRight) || !ContourFinite(cameraUp) || !std::isfinite(size.x) ||
        !std::isfinite(size.y) || size.x <= 0.0f || size.y <= 0.0f || !std::isfinite(rotationRad)) {
        return;
    }
    const float c = std::cos(rotationRad), s = std::sin(rotationRad);
    auto point = [&](float x, float y) {
        return center + cameraRight * (x * c - y * s) + cameraUp * (x * s + y * c);
    };
    const float halfX = size.x * 0.5f, halfY = size.y * 0.5f;
    const Vector3 points[]{point(-halfX, halfY), point(halfX, halfY), point(halfX, -halfY), point(-halfX, -halfY)};
    QueueContourPolygon(points, 4, lineWidth, color, cameraForward, style);
}

void NeonGridGeometry::QueueTriangle(const Vector3& a, const Vector3& b, const Vector3& c, float lineWidth, const Vector4& color)
{
    AddLineQuad(a, b, lineWidth, color);
    AddLineQuad(b, c, lineWidth, color);
    AddLineQuad(c, a, lineWidth, color);
}

void NeonGridGeometry::QueueBillboardTriangle(const Vector3& center, float radius, float rotationRad, float lineWidth, const Vector4& color,
                                              const Vector3& cameraRight, const Vector3& cameraUp, const Vector3& cameraForward)
{
    if (radius <= 0.0f || lineWidth <= 0.0f || color.w <= 0.001f) {
        return;
    }

    constexpr float kTwoPi = 6.28318530718f;
    constexpr float kStartAngle = -kTwoPi * 0.25f;
    Vector3 points[3]{};
    for (int i = 0; i < 3; ++i) {
        const float angle = kStartAngle + rotationRad + static_cast<float>(i) * (kTwoPi / 3.0f);
        const float x = std::cos(angle) * radius;
        const float y = std::sin(angle) * radius;
        points[i] = center + cameraRight * x + cameraUp * y;
    }

    AddCameraFacingLineQuad(points[0], points[1], lineWidth, color, cameraForward);
    AddCameraFacingLineQuad(points[1], points[2], lineWidth, color, cameraForward);
    AddCameraFacingLineQuad(points[2], points[0], lineWidth, color, cameraForward);
}

void NeonGridGeometry::QueueBillboardRectangle(const Vector3& center, const Vector2& size, float rotationRad, float lineWidth,
                                               const Vector4& color, const Vector3& cameraRight, const Vector3& cameraUp,
                                               const Vector3& cameraForward)
{
    if (size.x <= 0.0f || size.y <= 0.0f || lineWidth <= 0.0f || color.w <= 0.001f) {
        return;
    }

    const float c = std::cos(rotationRad);
    const float s = std::sin(rotationRad);
    auto makePoint = [&](float x, float y) {
        const float rx = x * c - y * s;
        const float ry = x * s + y * c;
        return center + cameraRight * rx + cameraUp * ry;
    };

    const float halfX = size.x * 0.5f;
    const float halfY = size.y * 0.5f;
    const Vector3 points[4] = {makePoint(-halfX, halfY), makePoint(halfX, halfY), makePoint(halfX, -halfY), makePoint(-halfX, -halfY)};

    AddCameraFacingLineQuad(points[0], points[1], lineWidth, color, cameraForward);
    AddCameraFacingLineQuad(points[1], points[2], lineWidth, color, cameraForward);
    AddCameraFacingLineQuad(points[2], points[3], lineWidth, color, cameraForward);
    AddCameraFacingLineQuad(points[3], points[0], lineWidth, color, cameraForward);
}

void NeonGridGeometry::QueueBillboardDisc(const Vector3& center, float radius, const Vector4& color, const Vector3& cameraRight,
                                          const Vector3& cameraUp, int segments)
{
    if (radius <= 0.0f || color.w <= 0.001f) {
        return;
    }
    segments = (std::clamp)(segments, 8, 96);
    if (vertexCount_ + static_cast<uint32_t>(segments * 3) > vertexCapacity_) {
        return;
    }

    constexpr float kTwoPi = 6.28318530718f;
    for (int i = 0; i < segments; ++i) {
        const float angle0 = static_cast<float>(i) * kTwoPi / static_cast<float>(segments);
        const float angle1 = static_cast<float>(i + 1) * kTwoPi / static_cast<float>(segments);
        const Vector3 p0 = center + cameraRight * (std::cos(angle0) * radius) + cameraUp * (std::sin(angle0) * radius);
        const Vector3 p1 = center + cameraRight * (std::cos(angle1) * radius) + cameraUp * (std::sin(angle1) * radius);
        PushVertex(center, color, {0.5f, 0.5f});
        PushVertex(p0, color, {0.0f, 0.0f});
        PushVertex(p1, color, {1.0f, 1.0f});
    }
}

void NeonGridGeometry::QueueBillboardRegularPolygonFill(const Vector3& center, int segments, float radius, float rotationRad,
                                                        const Vector2& scale, const Vector4& color, const Vector3& cameraRight,
                                                        const Vector3& cameraUp)
{
    if (radius <= 0.0f || scale.x <= 0.0f || scale.y <= 0.0f || color.w <= 0.001f) {
        return;
    }
    segments = (std::clamp)(segments, 3, 96);
    if (vertexCount_ + static_cast<uint32_t>(segments * 3) > vertexCapacity_) {
        return;
    }

    constexpr float kTwoPi = 6.28318530718f;
    for (int i = 0; i < segments; ++i) {
        const float angle0 = rotationRad + static_cast<float>(i) * kTwoPi / static_cast<float>(segments);
        const float angle1 = rotationRad + static_cast<float>(i + 1) * kTwoPi / static_cast<float>(segments);
        const Vector3 p0 = center + cameraRight * (std::cos(angle0) * radius * scale.x) + cameraUp * (std::sin(angle0) * radius * scale.y);
        const Vector3 p1 = center + cameraRight * (std::cos(angle1) * radius * scale.x) + cameraUp * (std::sin(angle1) * radius * scale.y);
        PushVertex(center, color, {0.5f, 0.5f});
        PushVertex(p0, color, {0.0f, 0.0f});
        PushVertex(p1, color, {1.0f, 1.0f});
    }
}

void NeonGridGeometry::SetBodyShadingEye(const Vector3* eye)
{
    bodyShading_ = eye != nullptr;
    if (eye) {
        bodyEye_ = *eye;
    }
}

Vector4 NeonGridGeometry::ShadeBody(const Vector3& position, const Vector3& normal, const Vector4& baseColor, const Vector4& tint) const
{
    Vector3 toEye = bodyEye_ - position;
    toEye = NormalizeOr(toEye, Vector3{0.0f, 0.0f, -1.0f});
    // キーライトは左上から、XYのゲームプレイ平面のカメラ側に置く。
    const Vector3 light = Normalize(Vector3{-0.45f, 0.55f, -0.70f});
    const float facing = std::clamp(Dot(normal, toEye), 0.0f, 1.0f);
    const float fresnel = std::pow(1.0f - facing, 3.0f);
    const float diffuse = std::clamp(Dot(normal, light), 0.0f, 1.0f);
    const Vector3 halfVector = Normalize(light + toEye);
    const float specular = std::pow(std::clamp(Dot(normal, halfVector), 0.0f, 1.0f), 110.0f);
    // 右下からの柔らかい2つ目のライトで、奥側がのっぺりしないようにする。
    const Vector3 fill = Normalize(Vector3{0.55f, -0.45f, -0.70f});
    const float fillSpecular = std::pow(std::clamp(Dot(normal, Normalize(fill + toEye)), 0.0f, 1.0f), 18.0f);
    const float peak = (std::max)({tint.x, tint.y, tint.z, 0.001f});
    const Vector3 hue{tint.x / peak, tint.y / peak, tint.z / peak};
    // スモークブラックの漆塗り風: ほのかな本体の色味、自身のネオンを映した色付きのリム反射、鋭い白いハイライト。
    const float rim = 0.02f + 0.05f * diffuse + 0.85f * fresnel + 0.10f * fillSpecular;
    const float glint = specular * 2.2f;
    return {baseColor.x + hue.x * rim + glint, baseColor.y + hue.y * rim + glint, baseColor.z + hue.z * rim + glint, baseColor.w};
}

void NeonGridGeometry::PushShadedTriangle(const Vector3& a, const Vector3& b, const Vector3& c, const Vector4& ca, const Vector4& cb,
                                          const Vector4& cc)
{
    // 塗りつぶしは深度を書かずにアルファブレンドする。視点側の半分だけを出力すれば、
    // 凸形状の本体は描画順に依存しない。
    const Vector3 normal = Cross(b - a, c - a);
    const Vector3 centroid = (a + b + c) / 3.0f;
    if (Dot(normal, bodyEye_ - centroid) <= 0.0f) {
        return;
    }
    PushVertex(a, ca, {0.5f, 0.5f});
    PushVertex(b, cb, {0.5f, 0.5f});
    PushVertex(c, cc, {0.5f, 0.5f});
}

void NeonGridGeometry::QueueShadedBody(const Vector3* points, uint32_t count, const Vector4& baseColor, const Vector4& tint)
{
    Vector3 center{};
    for (uint32_t i = 0; i < count; ++i) {
        center += points[i];
    }
    center = center / static_cast<float>(count);
    float radius = 0.0f;
    for (uint32_t i = 0; i < count; ++i) {
        radius = (std::max)(radius, Length(points[i] - center));
    }
    if (radius <= 0.0001f) {
        return;
    }
    // 視点側へ持ち上げる。輪郭線は法線がカメラを向く平面上にある。
    Vector3 up = Cross(points[1] - points[0], points[2] - points[0]);
    if (Length(up) <= 0.000001f) {
        return;
    }
    up = Normalize(up);
    if (Dot(up, bodyEye_ - center) < 0.0f) {
        up = up * -1.0f;
    }

    const bool round = count >= 12;
    // プロファイルの各リングは（輪郭の拡大率, 高さ/半径）で表す。丸い本体は滑らかなドーム、
    // 多角形はアリーナのブロックのような面取りした多面体の錐台にする。
    constexpr int kRoundRings = 7;
    constexpr std::array<float, 4> kPolygonScale{1.0f, 0.88f, 0.64f, 0.0f};
    constexpr std::array<float, 4> kPolygonHeight{0.0f, 0.26f, 0.42f, 0.42f};
    const int rings = round ? kRoundRings : static_cast<int>(kPolygonScale.size());
    const uint32_t required = count * static_cast<uint32_t>(rings) * 6u;
    if (vertexCount_ + required > vertexCapacity_) {
        return;
    }
    const float domeHeight = radius * 0.92f;
    auto ringScale = [&](int ring) {
        if (!round) {
            return kPolygonScale[static_cast<size_t>(ring)];
        }
        return std::cos(1.5707963f * static_cast<float>(ring) / static_cast<float>(kRoundRings - 1));
    };
    auto ringHeight = [&](int ring) {
        if (!round) {
            return kPolygonHeight[static_cast<size_t>(ring)] * radius;
        }
        return std::sin(1.5707963f * static_cast<float>(ring) / static_cast<float>(kRoundRings - 1)) * domeHeight;
    };
    auto position = [&](uint32_t i, int ring) {
        return center + (points[i % count] - center) * ringScale(ring) + up * ringHeight(ring);
    };
    auto roundNormal = [&](uint32_t i, int ring) {
        // ドーム形状の解析的な法線: radial * dh/dt - up * |p - c| * ds/dt。
        const float t = 1.5707963f * static_cast<float>(ring) / static_cast<float>(kRoundRings - 1);
        const Vector3 radial = points[i % count] - center;
        const float length = Length(radial);
        const Vector3 direction = length > 0.0001f ? radial / length : Vector3{};
        const Vector3 n = direction * (std::cos(t) * domeHeight) + up * (length * std::sin(t));
        return NormalizeOr(n, up);
    };
    for (int ring = 0; ring + 1 < rings; ++ring) {
        for (uint32_t i = 0; i < count; ++i) {
            const Vector3 a = position(i, ring), b = position(i + 1, ring);
            const Vector3 c = position(i + 1, ring + 1), d = position(i, ring + 1);
            // 各四角形を両方の巻き順で渡し、PushShadedTriangleが視点側を向くほうだけを残す。
            if (round) {
                const Vector4 ca = ShadeBody(a, roundNormal(i, ring), baseColor, tint);
                const Vector4 cb = ShadeBody(b, roundNormal(i + 1, ring), baseColor, tint);
                const Vector4 cc = ShadeBody(c, roundNormal(i + 1, ring + 1), baseColor, tint);
                const Vector4 cd = ShadeBody(d, roundNormal(i, ring + 1), baseColor, tint);
                PushShadedTriangle(a, b, c, ca, cb, cc);
                PushShadedTriangle(a, c, d, ca, cc, cd);
                PushShadedTriangle(a, c, b, ca, cc, cb);
                PushShadedTriangle(a, d, c, ca, cd, cc);
            } else {
                // 平らな面: 四角形ごとに法線を1つにして、ブロックのような鋭い角を保つ。
                Vector3 faceNormal = Cross(b - a, d - a);
                if (Length(faceNormal) <= 0.000001f) {
                    faceNormal = Cross(c - b, a - b);
                }
                if (Length(faceNormal) <= 0.000001f) {
                    continue;
                }
                faceNormal = Normalize(faceNormal);
                if (Dot(faceNormal, up) < 0.0f) {
                    faceNormal = faceNormal * -1.0f;
                }
                const Vector4 face = ShadeBody((a + b + c + d) * 0.25f, faceNormal, baseColor, tint);
                PushShadedTriangle(a, b, c, face, face, face);
                PushShadedTriangle(a, c, d, face, face, face);
                PushShadedTriangle(a, c, b, face, face, face);
                PushShadedTriangle(a, d, c, face, face, face);
            }
        }
    }
}

void NeonGridGeometry::QueueShadedBox(const Vector3& base, const Vector3& tip, const Vector3& right, float halfWidth, float height,
                                      const Vector4& baseColor, const Vector4& tint)
{
    if (!bodyShading_ || halfWidth <= 0.0f || height <= 0.0f || vertexCount_ + 36 > vertexCapacity_) {
        return;
    }
    const Vector3 forward = tip - base;
    if (Length(forward) <= 0.0001f || Length(right) <= 0.0001f) {
        return;
    }
    const Vector3 side = Normalize(right) * halfWidth;
    Vector3 up = Cross(forward, side);
    if (Length(up) <= 0.000001f) {
        return;
    }
    up = Normalize(up);
    if (Dot(up, bodyEye_ - (base + tip) * 0.5f) < 0.0f) {
        up = up * -1.0f;
    }
    const Vector3 lift = up * height;
    const Vector3 corners[8] = {base - side,        base + side,        tip + side,        tip - side,
                                base - side + lift, base + side + lift, tip + side + lift, tip - side + lift};
    const int faces[6][4] = {{4, 5, 6, 7}, {0, 1, 5, 4}, {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7}, {3, 2, 1, 0}};
    const Vector3 boxCenter = (base + tip) * 0.5f + lift * 0.5f;
    for (const auto& f : faces) {
        const Vector3 a = corners[f[0]], b = corners[f[1]], c = corners[f[2]], d = corners[f[3]];
        Vector3 normal = Normalize(Cross(b - a, d - a));
        if (Dot(normal, (a + c) * 0.5f - boxCenter) < 0.0f) {
            normal = normal * -1.0f;
        }
        const Vector4 color = ShadeBody((a + c) * 0.5f, normal, baseColor, tint);
        PushShadedTriangle(a, b, c, color, color, color);
        PushShadedTriangle(a, c, d, color, color, color);
        PushShadedTriangle(a, c, b, color, color, color);
        PushShadedTriangle(a, d, c, color, color, color);
    }
}

void NeonGridGeometry::QueueBeveledPolygonFill(const Vector3* points, uint32_t count, const Vector4& baseColor, const Vector4& tint,
                                               const Vector3& lightDirection)
{
    if (!points || count < 3 || count > 96 || baseColor.w <= 0.001f || vertexCount_ + count * 9 > vertexCapacity_) {
        return;
    }
    if (bodyShading_) {
        for (uint32_t i = 0; i < count; ++i) {
            if (!std::isfinite(points[i].x) || !std::isfinite(points[i].y) || !std::isfinite(points[i].z)) {
                return;
            }
        }
        QueueShadedBody(points, count, baseColor, tint);
        return;
    }
    Vector3 center{};
    for (uint32_t i = 0; i < count; ++i) {
        if (!std::isfinite(points[i].x) || !std::isfinite(points[i].y) || !std::isfinite(points[i].z)) {
            return;
        }
        center += points[i];
    }
    center = center / static_cast<float>(count);
    const float peak = (std::max)({tint.x, tint.y, tint.z, 0.001f});
    for (uint32_t i = 0; i < count; ++i) {
        const Vector3 p0 = points[i], p1 = points[(i + 1) % count];
        const Vector3 inner0 = center + (p0 - center) * 0.87f, inner1 = center + (p1 - center) * 0.87f;
        const Vector3 radial = (p0 + p1) * 0.5f - center;
        const float lit = Length(radial) > 0.0001f ? (std::max)(0.0f, Dot(Normalize(radial), lightDirection)) : 0.0f;
        Vector4 face = baseColor, bevel = baseColor;
        face.x += tint.x / peak * 0.010f;
        face.y += tint.y / peak * 0.010f;
        face.z += tint.z / peak * 0.010f;
        const float shade = 0.025f + lit * 0.075f;
        bevel.x += tint.x / peak * shade;
        bevel.y += tint.y / peak * shade;
        bevel.z += tint.z / peak * shade;
        PushVertex(center, baseColor, {0.5f, 0.5f});
        PushVertex(inner0, face, {0.5f, 0.5f});
        PushVertex(inner1, face, {0.5f, 0.5f});
        PushVertex(inner0, bevel, {0.5f, 0.5f});
        PushVertex(p0, bevel, {0.5f, 0.5f});
        PushVertex(inner1, bevel, {0.5f, 0.5f});
        PushVertex(p0, bevel, {0.5f, 0.5f});
        PushVertex(p1, bevel, {0.5f, 0.5f});
        PushVertex(inner1, bevel, {0.5f, 0.5f});
    }
}

void NeonGridGeometry::QueueWorldGrid(float minX, float maxX, float minY, float maxY, float spacing, float lineWidth, const Vector4& color,
                                      float planeZ)
{
    if (spacing <= 0.0f || lineWidth <= 0.0f) {
        return;
    }

    const float startX = std::floor(minX / spacing) * spacing;
    const float startY = std::floor(minY / spacing) * spacing;
    const float z = planeZ;
    for (float x = startX; x <= maxX + 0.001f; x += spacing) {
        AddLineQuad({x, minY, z}, {x, maxY, z}, lineWidth, color);
    }
    for (float y = startY; y <= maxY + 0.001f; y += spacing) {
        AddLineQuad({minX, y, z}, {maxX, y, z}, lineWidth, color);
    }
}

void NeonGridGeometry::QueueRectangle(const Vector3& center, const Vector3& size, float lineWidth, const Vector4& color)
{
    if (size.x <= 0.0f || size.y <= 0.0f || lineWidth <= 0.0f) {
        return;
    }

    const float halfX = size.x * 0.5f;
    const float halfY = size.y * 0.5f;
    const float z = center.z - 0.02f;
    const Vector3 leftTop = {center.x - halfX, center.y + halfY, z};
    const Vector3 rightTop = {center.x + halfX, center.y + halfY, z};
    const Vector3 rightBottom = {center.x + halfX, center.y - halfY, z};
    const Vector3 leftBottom = {center.x - halfX, center.y - halfY, z};

    AddLineQuad(leftTop, rightTop, lineWidth, color);
    AddLineQuad(rightTop, rightBottom, lineWidth, color);
    AddLineQuad(rightBottom, leftBottom, lineWidth, color);
    AddLineQuad(leftBottom, leftTop, lineWidth, color);
}

void NeonGridGeometry::QueueLocalGrid(const Vector3& center, float radius, float spacing, float lineWidth, const Vector4& color)
{
    QueueLocalGridClipped(center, radius, spacing, lineWidth, color, center.x - radius, center.x + radius, center.y - radius,
                          center.y + radius);
}

void NeonGridGeometry::QueueLocalGridClipped(const Vector3& center, float radius, float spacing, float lineWidth, const Vector4& color,
                                             float minX, float maxX, float minY, float maxY, float planeZ)
{
    if (radius <= 0.0f || spacing <= 0.0f || lineWidth <= 0.0f) {
        return;
    }

    const float circleMinX = center.x - radius;
    const float circleMaxX = center.x + radius;
    const float circleMinY = center.y - radius;
    const float circleMaxY = center.y + radius;
    const float clippedMinX = (std::max)(minX, circleMinX);
    const float clippedMaxX = (std::min)(maxX, circleMaxX);
    const float clippedMinY = (std::max)(minY, circleMinY);
    const float clippedMaxY = (std::min)(maxY, circleMaxY);
    if (clippedMinX >= clippedMaxX || clippedMinY >= clippedMaxY) {
        return;
    }

    const float startX = std::floor(clippedMinX / spacing) * spacing;
    const float startY = std::floor(clippedMinY / spacing) * spacing;
    const float z = planeZ;

    auto fadeAt = [&](float x, float y) {
        const float dx = x - center.x;
        const float dy = y - center.y;
        const float normalizedDistance = std::sqrt(dx * dx + dy * dy) / radius;
        // 陣地は均一に色付けし、縁だけをぼかす。
        const float t = std::clamp((normalizedDistance - 0.45f) / 0.55f, 0.0f, 1.0f);
        return 1.0f - t * t * (3.0f - 2.0f * t);
    };

    auto addFadedSegment = [&](const Vector3& a, const Vector3& b) {
        const float length = Length(b - a);
        const int segmentCount = (std::max)(1, static_cast<int>(std::ceil(length / (spacing * 0.5f))));
        for (int i = 0; i < segmentCount; ++i) {
            const float t0 = static_cast<float>(i) / static_cast<float>(segmentCount);
            const float t1 = static_cast<float>(i + 1) / static_cast<float>(segmentCount);
            Vector3 p0 = a + (b - a) * t0;
            Vector3 p1 = a + (b - a) * t1;
            const float midX = (p0.x + p1.x) * 0.5f;
            const float midY = (p0.y + p1.y) * 0.5f;
            Vector4 segmentColor = color;
            segmentColor.w *= fadeAt(midX, midY);
            AddLineQuad(p0, p1, lineWidth, segmentColor);
        }
    };

    for (float x = startX; x <= clippedMaxX + 0.001f; x += spacing) {
        if (x < clippedMinX - 0.001f) {
            continue;
        }
        const float dx = x - center.x;
        const float inside = radius * radius - dx * dx;
        if (inside <= 0.0f) {
            continue;
        }
        const float yExtent = std::sqrt(inside);
        const float y0 = (std::max)(clippedMinY, center.y - yExtent);
        const float y1 = (std::min)(clippedMaxY, center.y + yExtent);
        if (y0 < y1) {
            addFadedSegment({x, y0, z}, {x, y1, z});
        }
    }
    for (float y = startY; y <= clippedMaxY + 0.001f; y += spacing) {
        if (y < clippedMinY - 0.001f) {
            continue;
        }
        const float dy = y - center.y;
        const float inside = radius * radius - dy * dy;
        if (inside <= 0.0f) {
            continue;
        }
        const float xExtent = std::sqrt(inside);
        const float x0 = (std::max)(clippedMinX, center.x - xExtent);
        const float x1 = (std::min)(clippedMaxX, center.x + xExtent);
        if (x0 < x1) {
            addFadedSegment({x0, y, z}, {x1, y, z});
        }
    }
}

void NeonGridGeometry::AddLineQuad(const Vector3& a, const Vector3& b, float width, const Vector4& color)
{
    if (color.w <= 0.001f || vertexCount_ + 6 > vertexCapacity_) {
        return;
    }

    Vector3 dir = b - a;
    const float len = Length(dir);
    if (len <= 0.0001f) {
        return;
    }
    dir = dir / len;
    Vector3 normal = {-dir.y, dir.x, 0.0f};
    AddSoftLineQuad(a, b, normal, width, color);
}

void NeonGridGeometry::AddCameraFacingLineQuad(const Vector3& a, const Vector3& b, float width, const Vector4& color,
                                               const Vector3& cameraForward)
{
    if (color.w <= 0.001f || vertexCount_ + 6 > vertexCapacity_) {
        return;
    }

    Vector3 dir = b - a;
    const float len = Length(dir);
    if (len <= 0.0001f) {
        return;
    }
    dir = dir / len;

    Vector3 forward = cameraForward;
    if (Length(forward) <= 0.0001f) {
        forward = {0.0f, 0.0f, 1.0f};
    } else {
        forward = Normalize(forward);
    }

    Vector3 normal = Cross(forward, dir);
    if (Length(normal) <= 0.0001f) {
        normal = Cross({0.0f, 1.0f, 0.0f}, dir);
        if (Length(normal) <= 0.0001f) {
            normal = Cross({1.0f, 0.0f, 0.0f}, dir);
        }
    } else {
        normal = Normalize(normal);
    }
    AddSoftLineQuad(a, b, normal, width, color);
}

void NeonGridGeometry::AddSoftLineQuad(const Vector3& a, const Vector3& b, const Vector3& normalDir, float width, const Vector4& color)
{
    if (color.w <= 0.001f || width <= 0.0f || vertexCount_ + 18 > vertexCapacity_) {
        return;
    }

    Vector3 normal = normalDir;
    const float normalLength = Length(normal);
    if (normalLength <= 0.0001f) {
        return;
    }
    normal = normal / normalLength;

    const float halfWidth = width * 0.5f;
    const float softRatio = std::clamp(lineSoftEdgeRatio_, 0.0f, 0.95f);
    if (softRatio <= 0.001f) {
        Vector4 hardColor = color;
        hardColor.x *= lineCoreIntensity_;
        hardColor.y *= lineCoreIntensity_;
        hardColor.z *= lineCoreIntensity_;
        PushLineStrip(a, b, normal, -halfWidth, halfWidth, hardColor, hardColor);
        return;
    }

    const float coreHalfWidth = (std::max)(halfWidth * (1.0f - softRatio), halfWidth * 0.05f);
    Vector4 outerColor = color;
    outerColor.x *= 0.35f;
    outerColor.y *= 0.35f;
    outerColor.z *= 0.35f;
    outerColor.w = 0.0f;

    Vector4 coreColor = color;
    coreColor.x *= lineCoreIntensity_;
    coreColor.y *= lineCoreIntensity_;
    coreColor.z *= lineCoreIntensity_;

    PushLineStrip(a, b, normal, -halfWidth, -coreHalfWidth, outerColor, coreColor);
    PushLineStrip(a, b, normal, -coreHalfWidth, coreHalfWidth, coreColor, coreColor);
    PushLineStrip(a, b, normal, coreHalfWidth, halfWidth, coreColor, outerColor);
}

void NeonGridGeometry::PushLineStrip(const Vector3& a, const Vector3& b, const Vector3& normalDir, float offset0, float offset1,
                                     const Vector4& color0, const Vector4& color1)
{
    if (vertexCount_ + 6 > vertexCapacity_) {
        return;
    }

    const Vector3 p0 = a + normalDir * offset0;
    const Vector3 p1 = a + normalDir * offset1;
    const Vector3 p2 = b + normalDir * offset0;
    const Vector3 p3 = b + normalDir * offset1;

    PushVertex(p0, color0, {0.0f, 0.0f});
    PushVertex(p1, color1, {0.0f, 1.0f});
    PushVertex(p2, color0, {1.0f, 0.0f});
    PushVertex(p1, color1, {0.0f, 1.0f});
    PushVertex(p3, color1, {1.0f, 1.0f});
    PushVertex(p2, color0, {1.0f, 0.0f});
}

void NeonGridGeometry::PushVertex(const Vector3& pos, const Vector4& color, const Vector2& uv)
{
    if (vertexCount_ >= vertexCapacity_) {
        return;
    }
    vertexData_[vertexCount_].pos = pos;
    vertexData_[vertexCount_].color = color;
    vertexData_[vertexCount_].uv = uv;
    ++vertexCount_;
}

} // namespace myEngine
