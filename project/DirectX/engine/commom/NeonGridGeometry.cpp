#include "NeonGridGeometry.h"
#include "Calculation.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace cg2 {

namespace {
constexpr uint32_t kContourRadii = 4;
constexpr uint32_t kContourOffsets = kContourRadii * 2;
constexpr uint32_t kContourSpanVertices = (kContourOffsets - 1) * 6;
constexpr uint32_t kContourMaxPoints = 96;
constexpr uint32_t kContourCapSegments = 8;
constexpr float kContourJoinStep = 0.5235987756f; // At most 30 degrees per round-corner segment.

bool ContourFinite(const Vector3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool ContourPositionRange(const Vector3& center, float extent) {
    return std::isfinite(std::abs(center.x) + extent) && std::isfinite(std::abs(center.y) + extent) &&
           std::isfinite(std::abs(center.z) + extent);
}

float ContourDot(const Vector3& a, const Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float ContourBounded(float value, float fallback, float minimum, float maximum) {
    return std::isfinite(value) ? (std::clamp)(value, minimum, maximum) : fallback;
}

Vector3 ContourForward(const Vector3& forward) {
    const float length = ContourFinite(forward) ? Length(forward) : 0.0f;
    return std::isfinite(length) && length > 0.0001f ? forward / length : Vector3{0.0f, 0.0f, 1.0f};
}

struct ContourProfile {
    std::array<float, kContourRadii> radii{};
    std::array<Vector4, kContourRadii> colors{};
};

bool MakeContourProfile(float width, const Vector4& source, const NeonContourStyle& requested, ContourProfile& profile) {
    if (!std::isfinite(width) || width <= 0.0f || !std::isfinite(source.w) || source.w <= 0.001f ||
        !std::isfinite(source.x) || !std::isfinite(source.y) || !std::isfinite(source.z)) return false;
    const Vector3 rgb{(std::clamp)(source.x, 0.0f, 8.0f), (std::clamp)(source.y, 0.0f, 8.0f), (std::clamp)(source.z, 0.0f, 8.0f)};
    const float peak = (std::max)({rgb.x, rgb.y, rgb.z});
    if (peak <= 0.0001f) return false;
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
    if (!std::isfinite(half * haloScale)) return false;
    profile.radii = {half * coreRatio, half * (std::max)(0.65f, coreRatio + 0.10f),
                     half * (1.0f + (haloScale - 1.0f) * 0.36f), half * haloScale};
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

Vector3 ContourSectionPoint(const ContourSection& section, uint32_t offset, const ContourProfile& profile) {
    if (offset < kContourRadii) return section.center - section.negative * profile.radii[kContourRadii - 1 - offset];
    return section.center + section.positive * profile.radii[offset - kContourRadii];
}

Vector4 ContourSectionColor(uint32_t offset, const ContourProfile& profile) {
    return profile.colors[offset < kContourRadii ? kContourRadii - 1 - offset : offset - kContourRadii];
}
} // namespace

void NeonGridGeometry::SetVertexBuffer(std::span<TrailVertex> vertices) {
    vertexData_ = vertices.data();
    vertexCapacity_ = static_cast<uint32_t>((std::min)(vertices.size(), static_cast<size_t>(kMaxVertices)));
    vertexCount_ = 0;
}

void NeonGridGeometry::BeginFrame() {
    vertexCount_ = 0;
}

void NeonGridGeometry::SetLineStyle(float softEdgeRatio, float coreIntensity) {
    lineSoftEdgeRatio_ = std::clamp(softEdgeRatio, 0.0f, 0.95f);
    lineCoreIntensity_ = (std::max)(0.0f, coreIntensity);
}

void NeonGridGeometry::QueueLine(const Vector3& a, const Vector3& b, float lineWidth, const Vector4& color) {
    AddLineQuad(a, b, lineWidth, color);
}

void NeonGridGeometry::QueueCameraFacingLine(const Vector3& a, const Vector3& b, float lineWidth, const Vector4& color, const Vector3& cameraForward) {
    AddCameraFacingLineQuad(a, b, lineWidth, color, cameraForward);
}

void NeonGridGeometry::QueueContourLine(const Vector3& a, const Vector3& b, float lineWidth, const Vector4& color,
                                      const Vector3& cameraForward, const NeonContourStyle& style) {
    ContourProfile profile;
    if (!vertexData_ || !ContourFinite(a) || !ContourFinite(b) || !MakeContourProfile(lineWidth, color, style, profile)) return;
    const Vector3 forward = ContourForward(cameraForward);
    Vector3 direction = b - a;
    direction = direction - forward * ContourDot(direction, forward);
    const float length = Length(direction);
    if (!std::isfinite(length) || length <= 0.0001f) return;
    direction = direction / length;
    const Vector3 normal = Cross(forward, direction);
    if (!ContourPositionRange(a, profile.radii.back()) || !ContourPositionRange(b, profile.radii.back())) return;
    const uint32_t required = kContourSpanVertices + (style.roundCaps ? 2 * kContourCapSegments * (3 + (kContourRadii - 1) * 6) : 0);
    if (required > vertexCapacity_ || vertexCount_ > vertexCapacity_ - required) return;

    const ContourSection begin{a, normal, normal}, end{b, normal, normal};
    auto triangle = [&](const Vector3& p0, const Vector4& c0, const Vector3& p1, const Vector4& c1,
                        const Vector3& p2, const Vector4& c2) {
        PushVertex(p0, c0, {0.5f, 0.5f}); PushVertex(p1, c1, {0.5f, 0.5f}); PushVertex(p2, c2, {0.5f, 0.5f});
    };
    for (uint32_t offset = 0; offset + 1 < kContourOffsets; ++offset) {
        const Vector3 p0 = ContourSectionPoint(begin, offset, profile), p1 = ContourSectionPoint(begin, offset + 1, profile);
        const Vector3 p2 = ContourSectionPoint(end, offset, profile), p3 = ContourSectionPoint(end, offset + 1, profile);
        const Vector4 c0 = ContourSectionColor(offset, profile), c1 = ContourSectionColor(offset + 1, profile);
        triangle(p0, c0, p1, c1, p2, c0); triangle(p1, c1, p3, c1, p2, c0);
    }
    if (!style.roundCaps) return;
    // These semicircular caps share the exact end cross-sections of the body.
    // Unlike overlapping discs, they do not double the emission at either tip.
    constexpr float kContourPi = 3.14159265359f;
    for (int cap = 0; cap < 2; ++cap) {
        const Vector3 center = cap == 0 ? a : b;
        const Vector3 outward = direction * (cap == 0 ? -1.0f : 1.0f);
        for (uint32_t segment = 0; segment < kContourCapSegments; ++segment) {
            const float angle0 = -kContourPi * 0.5f + kContourPi * static_cast<float>(segment) / static_cast<float>(kContourCapSegments);
            const float angle1 = -kContourPi * 0.5f + kContourPi * static_cast<float>(segment + 1) / static_cast<float>(kContourCapSegments);
            const Vector3 radial0 = outward * std::cos(angle0) + normal * std::sin(angle0);
            const Vector3 radial1 = outward * std::cos(angle1) + normal * std::sin(angle1);
            triangle(center, profile.colors[0], center + radial0 * profile.radii[0], profile.colors[0],
                     center + radial1 * profile.radii[0], profile.colors[0]);
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
                                         const Vector3& cameraForward, const NeonContourStyle& style) {
    ContourProfile profile;
    if (!vertexData_ || !points || pointCount < 3 || pointCount > kContourMaxPoints || !MakeContourProfile(lineWidth, color, style, profile)) return;
    const Vector3 forward = ContourForward(cameraForward);
    const float maximumExtent = profile.radii.back() * 4.0f;
    if (!std::isfinite(maximumExtent)) return;
    std::array<Vector3, kContourMaxPoints> normals{};
    for (uint32_t point = 0; point < pointCount; ++point) {
        if (!ContourFinite(points[point]) || !ContourPositionRange(points[point], maximumExtent)) return;
        Vector3 direction = points[(point + 1) % pointCount] - points[point];
        direction = direction - forward * ContourDot(direction, forward);
        const float length = Length(direction);
        if (!std::isfinite(length) || length <= 0.0001f) return;
        normals[point] = Cross(forward, direction / length);
    }
    // Bounded stack scratch; no per-object allocation or mutable style state.
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
        if (!std::isfinite(miterLength) || miterLength <= 0.0001f) return; // A folded/reversed edge is not a contour.
        miter = miter / miterLength;
        const float denominator = (std::max)(0.001f, std::abs(ContourDot(miter, next)));
        miter = miter * (std::min)(4.0f, 1.0f / denominator);
        // Already-smooth circle silhouettes need one shared miter section. Its
        // extent differs from a circular arc by less than 4% below 30 degrees;
        // extra fan sections here would double their vertex budget needlessly.
        const uint32_t steps = angleSize <= kContourJoinStep ? 0 : (std::min)(6u, static_cast<uint32_t>(std::ceil(angleSize / kContourJoinStep)));
        for (uint32_t step = 0; step <= steps; ++step) {
            const float t = steps == 0 ? 0.0f : static_cast<float>(step) / static_cast<float>(steps);
            const float rotation = angle * t;
            const Vector3 rounded = steps == 0 ? miter : previous * std::cos(rotation) + Cross(forward, previous) * std::sin(rotation);
            // The inner boundary remains one shared intersection; only the outer
            // boundary follows an arc. Adjacent bands therefore never leave gaps
            // or overlap with a second bright strip at the corner.
            sections[sectionCount++] = {points[point], angle >= 0.0f ? miter : rounded, angle >= 0.0f ? rounded : miter};
        }
    }
    const uint32_t required = sectionCount * kContourSpanVertices;
    if (required > vertexCapacity_ || vertexCount_ > vertexCapacity_ - required) return;
    for (uint32_t section = 0; section < sectionCount; ++section) {
        const ContourSection& a = sections[section];
        const ContourSection& b = sections[(section + 1) % sectionCount];
        for (uint32_t offset = 0; offset + 1 < kContourOffsets; ++offset) {
            const Vector3 p0 = ContourSectionPoint(a, offset, profile), p1 = ContourSectionPoint(a, offset + 1, profile);
            const Vector3 p2 = ContourSectionPoint(b, offset, profile), p3 = ContourSectionPoint(b, offset + 1, profile);
            const Vector4 c0 = ContourSectionColor(offset, profile), c1 = ContourSectionColor(offset + 1, profile);
            PushVertex(p0, c0, {0.5f, 0.5f}); PushVertex(p1, c1, {0.5f, 0.5f}); PushVertex(p2, c0, {0.5f, 0.5f});
            PushVertex(p1, c1, {0.5f, 0.5f}); PushVertex(p3, c1, {0.5f, 0.5f}); PushVertex(p2, c0, {0.5f, 0.5f});
        }
    }
}

void NeonGridGeometry::QueueContourTriangle(const Vector3& a, const Vector3& b, const Vector3& c, float lineWidth,
                                          const Vector4& color, const NeonContourStyle& style) {
    const Vector3 points[]{a, b, c};
    QueueContourPolygon(points, 3, lineWidth, color, {0.0f, 0.0f, 1.0f}, style);
}

void NeonGridGeometry::QueueContourRectangle(const Vector3& center, const Vector3& size, float lineWidth, const Vector4& color,
                                           const NeonContourStyle& style) {
    if (!ContourFinite(center) || !ContourFinite(size) || size.x <= 0.0f || size.y <= 0.0f) return;
    const float halfX = size.x * 0.5f, halfY = size.y * 0.5f, z = center.z - 0.02f;
    const Vector3 points[]{{center.x - halfX, center.y + halfY, z}, {center.x + halfX, center.y + halfY, z},
                           {center.x + halfX, center.y - halfY, z}, {center.x - halfX, center.y - halfY, z}};
    QueueContourPolygon(points, 4, lineWidth, color, {0.0f, 0.0f, 1.0f}, style);
}

void NeonGridGeometry::QueueBillboardContourTriangle(const Vector3& center, float radius, float rotationRad, float lineWidth, const Vector4& color,
                                                   const Vector3& cameraRight, const Vector3& cameraUp, const Vector3& cameraForward,
                                                   const NeonContourStyle& style) {
    if (!ContourFinite(center) || !ContourFinite(cameraRight) || !ContourFinite(cameraUp) ||
        !std::isfinite(radius) || radius <= 0.0f || !std::isfinite(rotationRad)) return;
    constexpr float twoPi = 6.28318530718f;
    Vector3 points[3]{};
    for (int point = 0; point < 3; ++point) {
        const float angle = -twoPi * 0.25f + rotationRad + static_cast<float>(point) * twoPi / 3.0f;
        points[point] = center + cameraRight * (std::cos(angle) * radius) + cameraUp * (std::sin(angle) * radius);
    }
    QueueContourPolygon(points, 3, lineWidth, color, cameraForward, style);
}

void NeonGridGeometry::QueueBillboardContourRectangle(const Vector3& center, const Vector2& size, float rotationRad, float lineWidth, const Vector4& color,
                                                    const Vector3& cameraRight, const Vector3& cameraUp, const Vector3& cameraForward,
                                                    const NeonContourStyle& style) {
    if (!ContourFinite(center) || !ContourFinite(cameraRight) || !ContourFinite(cameraUp) || !std::isfinite(size.x) || !std::isfinite(size.y) ||
        size.x <= 0.0f || size.y <= 0.0f || !std::isfinite(rotationRad)) return;
    const float c = std::cos(rotationRad), s = std::sin(rotationRad);
    auto point = [&](float x, float y) {return center + cameraRight * (x * c - y * s) + cameraUp * (x * s + y * c);};
    const float halfX = size.x * 0.5f, halfY = size.y * 0.5f;
    const Vector3 points[]{point(-halfX, halfY), point(halfX, halfY), point(halfX, -halfY), point(-halfX, -halfY)};
    QueueContourPolygon(points, 4, lineWidth, color, cameraForward, style);
}

void NeonGridGeometry::QueueTriangle(const Vector3& a, const Vector3& b, const Vector3& c, float lineWidth, const Vector4& color) {
    AddLineQuad(a, b, lineWidth, color);
    AddLineQuad(b, c, lineWidth, color);
    AddLineQuad(c, a, lineWidth, color);
}

void NeonGridGeometry::QueueBillboardTriangle(
    const Vector3& center,
    float radius,
    float rotationRad,
    float lineWidth,
    const Vector4& color,
    const Vector3& cameraRight,
    const Vector3& cameraUp,
    const Vector3& cameraForward) {
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

void NeonGridGeometry::QueueBillboardRectangle(
    const Vector3& center,
    const Vector2& size,
    float rotationRad,
    float lineWidth,
    const Vector4& color,
    const Vector3& cameraRight,
    const Vector3& cameraUp,
    const Vector3& cameraForward) {
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
    const Vector3 points[4] = {
        makePoint(-halfX, halfY),
        makePoint(halfX, halfY),
        makePoint(halfX, -halfY),
        makePoint(-halfX, -halfY)
    };

    AddCameraFacingLineQuad(points[0], points[1], lineWidth, color, cameraForward);
    AddCameraFacingLineQuad(points[1], points[2], lineWidth, color, cameraForward);
    AddCameraFacingLineQuad(points[2], points[3], lineWidth, color, cameraForward);
    AddCameraFacingLineQuad(points[3], points[0], lineWidth, color, cameraForward);
}

void NeonGridGeometry::QueueBillboardDisc(
    const Vector3& center,
    float radius,
    const Vector4& color,
    const Vector3& cameraRight,
    const Vector3& cameraUp,
    int segments) {
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
        PushVertex(center, color, { 0.5f, 0.5f });
        PushVertex(p0, color, { 0.0f, 0.0f });
        PushVertex(p1, color, { 1.0f, 1.0f });
    }
}

void NeonGridGeometry::QueueBillboardRegularPolygonFill(
    const Vector3& center,
    int segments,
    float radius,
    float rotationRad,
    const Vector2& scale,
    const Vector4& color,
    const Vector3& cameraRight,
    const Vector3& cameraUp) {
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
        const Vector3 p0 = center +
            cameraRight * (std::cos(angle0) * radius * scale.x) +
            cameraUp * (std::sin(angle0) * radius * scale.y);
        const Vector3 p1 = center +
            cameraRight * (std::cos(angle1) * radius * scale.x) +
            cameraUp * (std::sin(angle1) * radius * scale.y);
        PushVertex(center, color, { 0.5f, 0.5f });
        PushVertex(p0, color, { 0.0f, 0.0f });
        PushVertex(p1, color, { 1.0f, 1.0f });
    }
}

void NeonGridGeometry::QueueBeveledPolygonFill(const Vector3* points, uint32_t count, const Vector4& baseColor,
                                               const Vector4& tint, const Vector3& lightDirection) {
    if (!points || count < 3 || count > 96 || baseColor.w <= 0.001f || vertexCount_ + count * 9 > vertexCapacity_) return;
    Vector3 center{};
    for (uint32_t i = 0; i < count; ++i) {
        if (!std::isfinite(points[i].x) || !std::isfinite(points[i].y) || !std::isfinite(points[i].z)) return;
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
        face.x += tint.x / peak * 0.010f; face.y += tint.y / peak * 0.010f; face.z += tint.z / peak * 0.010f;
        const float shade = 0.025f + lit * 0.075f;
        bevel.x += tint.x / peak * shade; bevel.y += tint.y / peak * shade; bevel.z += tint.z / peak * shade;
        PushVertex(center, baseColor, {0.5f,0.5f}); PushVertex(inner0, face, {0.5f,0.5f}); PushVertex(inner1, face, {0.5f,0.5f});
        PushVertex(inner0, bevel, {0.5f,0.5f}); PushVertex(p0, bevel, {0.5f,0.5f}); PushVertex(inner1, bevel, {0.5f,0.5f});
        PushVertex(p0, bevel, {0.5f,0.5f}); PushVertex(p1, bevel, {0.5f,0.5f}); PushVertex(inner1, bevel, {0.5f,0.5f});
    }
}

void NeonGridGeometry::QueueWorldGrid(float minX, float maxX, float minY, float maxY, float spacing, float lineWidth, const Vector4& color) {
    if (spacing <= 0.0f || lineWidth <= 0.0f) {
        return;
    }

    const float startX = std::floor(minX / spacing) * spacing;
    const float startY = std::floor(minY / spacing) * spacing;
    const float z = -0.04f;
    for (float x = startX; x <= maxX + 0.001f; x += spacing) {
        AddLineQuad({ x, minY, z }, { x, maxY, z }, lineWidth, color);
    }
    for (float y = startY; y <= maxY + 0.001f; y += spacing) {
        AddLineQuad({ minX, y, z }, { maxX, y, z }, lineWidth, color);
    }
}

void NeonGridGeometry::QueueRectangle(const Vector3& center, const Vector3& size, float lineWidth, const Vector4& color) {
    if (size.x <= 0.0f || size.y <= 0.0f || lineWidth <= 0.0f) {
        return;
    }

    const float halfX = size.x * 0.5f;
    const float halfY = size.y * 0.5f;
    const float z = center.z - 0.02f;
    const Vector3 leftTop = { center.x - halfX, center.y + halfY, z };
    const Vector3 rightTop = { center.x + halfX, center.y + halfY, z };
    const Vector3 rightBottom = { center.x + halfX, center.y - halfY, z };
    const Vector3 leftBottom = { center.x - halfX, center.y - halfY, z };

    AddLineQuad(leftTop, rightTop, lineWidth, color);
    AddLineQuad(rightTop, rightBottom, lineWidth, color);
    AddLineQuad(rightBottom, leftBottom, lineWidth, color);
    AddLineQuad(leftBottom, leftTop, lineWidth, color);
}

void NeonGridGeometry::QueueLocalGrid(const Vector3& center, float radius, float spacing, float lineWidth, const Vector4& color) {
    QueueLocalGridClipped(center, radius, spacing, lineWidth, color, center.x - radius, center.x + radius, center.y - radius, center.y + radius);
}

void NeonGridGeometry::QueueLocalGridClipped(const Vector3& center, float radius, float spacing, float lineWidth, const Vector4& color, float minX, float maxX, float minY, float maxY) {
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
    const float z = -0.03f;

    auto fadeAt = [&](float x, float y) {
        const float dx = x - center.x;
        const float dy = y - center.y;
        const float normalizedDistance = std::sqrt(dx * dx + dy * dy) / radius;
        const float fade = std::clamp(1.0f - normalizedDistance * normalizedDistance, 0.0f, 1.0f);
        return fade * fade;
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
            addFadedSegment({ x, y0, z }, { x, y1, z });
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
            addFadedSegment({ x0, y, z }, { x1, y, z });
        }
    }
}

void NeonGridGeometry::AddLineQuad(const Vector3& a, const Vector3& b, float width, const Vector4& color) {
    if (color.w <= 0.001f || vertexCount_ + 6 > vertexCapacity_) {
        return;
    }

    Vector3 dir = b - a;
    const float len = Length(dir);
    if (len <= 0.0001f) {
        return;
    }
    dir = dir / len;
    Vector3 normal = { -dir.y, dir.x, 0.0f };
    AddSoftLineQuad(a, b, normal, width, color);
}

void NeonGridGeometry::AddCameraFacingLineQuad(const Vector3& a, const Vector3& b, float width, const Vector4& color, const Vector3& cameraForward) {
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
        forward = { 0.0f, 0.0f, 1.0f };
    } else {
        forward = Normalize(forward);
    }

    Vector3 normal = Cross(forward, dir);
    if (Length(normal) <= 0.0001f) {
        normal = Cross({ 0.0f, 1.0f, 0.0f }, dir);
        if (Length(normal) <= 0.0001f) {
            normal = Cross({ 1.0f, 0.0f, 0.0f }, dir);
        }
    } else {
        normal = Normalize(normal);
    }
    AddSoftLineQuad(a, b, normal, width, color);
}

void NeonGridGeometry::AddSoftLineQuad(const Vector3& a, const Vector3& b, const Vector3& normalDir, float width, const Vector4& color) {
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

void NeonGridGeometry::PushLineStrip(
    const Vector3& a,
    const Vector3& b,
    const Vector3& normalDir,
    float offset0,
    float offset1,
    const Vector4& color0,
    const Vector4& color1) {
    if (vertexCount_ + 6 > vertexCapacity_) {
        return;
    }

    const Vector3 p0 = a + normalDir * offset0;
    const Vector3 p1 = a + normalDir * offset1;
    const Vector3 p2 = b + normalDir * offset0;
    const Vector3 p3 = b + normalDir * offset1;

    PushVertex(p0, color0, { 0.0f, 0.0f });
    PushVertex(p1, color1, { 0.0f, 1.0f });
    PushVertex(p2, color0, { 1.0f, 0.0f });
    PushVertex(p1, color1, { 0.0f, 1.0f });
    PushVertex(p3, color1, { 1.0f, 1.0f });
    PushVertex(p2, color0, { 1.0f, 0.0f });
}

void NeonGridGeometry::PushVertex(const Vector3& pos, const Vector4& color, const Vector2& uv) {
    if (vertexCount_ >= vertexCapacity_) {
        return;
    }
    vertexData_[vertexCount_].pos = pos;
    vertexData_[vertexCount_].color = color;
    vertexData_[vertexCount_].uv = uv;
    ++vertexCount_;
}

} // namespace cg2
