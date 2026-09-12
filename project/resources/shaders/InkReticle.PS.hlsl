#include "InkReticle.hlsli"

float Coverage(float distance) {
    return saturate(0.5 - distance / max(fwidth(distance), 0.7));
}
float SegmentDistance(float2 p, float2 a, float2 b) {
    const float2 segment = b - a;
    const float t = saturate(dot(p - a, segment) / max(dot(segment, segment), 0.00001));
    return length(p - a - segment * t);
}
void Over(inout float4 destination, float3 color, float alpha) {
    destination = float4(color * alpha, alpha) + destination * (1 - alpha);
}
void Stroke(inout float4 color, float distance, float3 tint, float alpha, float scale) {
    Over(color, float3(0.015, 0.023, 0.030), Coverage(distance - scale * 0.8) * alpha * 0.65);
    Over(color, tint, Coverage(distance) * alpha);
}
float ArcCoverage(float2 p, float radius, float thickness, float progress) {
    if (progress <= 0) return 0;
    const float ring = Coverage(abs(length(p) - radius) - thickness * 0.5);
    if (progress >= 1) return ring;
    float angle = atan2(p.x, -p.y);
    if (angle < 0) angle += 6.28318530718;
    return ring * saturate((progress * 6.28318530718 - angle) * radius + 0.5);
}

float4 main(ReticleVertex input) : SV_TARGET0 {
    const float2 p = input.local;
    const float scale = detail.z;
    const float halfThickness = shape.w * 0.5;
    float4 result = 0;
    // Folding into one quadrant gives four diagonal corner marks. Their
    // vertical band remains fixed for the shooter; jump spread changes X.
    const float2 diagonal = float2(0.70710678, -0.70710678) * quad.z;
    const float corner = SegmentDistance(abs(p), shape.xy - diagonal, shape.xy + diagonal) - halfThickness;
    Stroke(result, corner, feedback.rgb, 0.98, scale);

    if (charge.x > 0.5) {
        if (charge.x > 1.5) {
            const float2 axis = charge.y > 0.5 ? float2(0, 1) : float2(1, 0);
            const float offset = detail.x * (charge.x - 1.0) * 0.5;
            const float sideDot = min(length(p - axis * offset), length(p + axis * offset)) - 1.8 * scale;
            Stroke(result, sideDot, feedback.rgb, 0.88, scale);
        }
        const float firstRadius = 10.5 * scale;
        const float secondRadius = 14.5 * scale;
        const float tracks = max(Coverage(abs(length(p) - firstRadius) - 0.65 * scale),
            Coverage(abs(length(p) - secondRadius) - 0.65 * scale));
        Over(result, float3(0.04, 0.08, 0.10), tracks * 0.55);
        Over(result, float3(0.03, 0.78, 0.52), ArcCoverage(p, firstRadius, 2.0 * scale, charge.z) * 0.95);
        Over(result, float3(0.65, 1.0, 0.84), ArcCoverage(p, secondRadius, 2.0 * scale, charge.w) * 0.95);
    }

    // The center aim ring/dot do not breathe with spread or visual weapon kick.
    const float ring = abs(length(p) - shape.z) - 0.65 * scale;
    Stroke(result, ring, feedback.rgb, detail.y, scale);
    Stroke(result, length(p) - quad.w, feedback.rgb, 0.95, scale);
    if (detail.w > 0.5) {
        const float2 crossP = abs(p);
        const float targetMark = SegmentDistance(crossP, float2(2.6, 2.6) * scale,
            float2(4.8, 4.8) * scale) - 0.6 * scale;
        Over(result, feedback.rgb, Coverage(targetMark) * 0.8);
    }
    return result * feedback.w;
}
