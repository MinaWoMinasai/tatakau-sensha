#ifndef NEON_DISSOLVE_HLSLI
#define NEON_DISSOLVE_HLSLI
#include "NeonSkinned.hlsli"

// CPU reference: NeonDissolve.h. Integer overflow is intentional and seed is frozen.
uint NeonDissolveHash(uint3 cell)
{
    uint h = cell.x * 0x8da6b343u ^ cell.y * 0xd8163841u ^ cell.z * 0xcb1ab31fu ^ gDissolveSeed;
    h ^= h >> 16; h *= 0x7feb352du; h ^= h >> 15; h *= 0x846ca68bu; h ^= h >> 16;
    return h;
}

float NeonDissolveLattice(int3 cell)
{
    return float(NeonDissolveHash(uint3(cell)) & 0x00ffffffu) / 16777215.0f;
}

float NeonDissolveNoise(float3 position)
{
    float3 scaled = clamp(position * gDissolveNoiseScale, -1048576.0f, 1048576.0f);
    int3 cell = int3(floor(scaled));
    float3 f = scaled - float3(cell);
    f = f * f * (3.0f - 2.0f * f);
    float a = lerp(lerp(NeonDissolveLattice(cell), NeonDissolveLattice(cell + int3(1,0,0)), f.x),
        lerp(NeonDissolveLattice(cell + int3(0,1,0)), NeonDissolveLattice(cell + int3(1,1,0)), f.x), f.y);
    float b = lerp(lerp(NeonDissolveLattice(cell + int3(0,0,1)), NeonDissolveLattice(cell + int3(1,0,1)), f.x),
        lerp(NeonDissolveLattice(cell + int3(0,1,1)), NeonDissolveLattice(cell + int3(1,1,1)), f.x), f.y);
    return lerp(a, b, f.z);
}

struct NeonDissolveEvaluation
{
    float signedDistance;
    float3 edgeEmission;
};

// Call before ALL clips. It computes derivatives but never discards a pixel itself.
// Width and noise strength are distances along the normalized model-space plane;
// noise scale is lattice cells per model-space unit, independent of camera/time.
NeonDissolveEvaluation EvaluateNeonDissolve(float3 skinnedModelPosition)
{
    NeonDissolveEvaluation result;
    result.signedDistance = 1.0f;
    result.edgeEmission = 0.0f;
    if (gDissolveEnabled == 0 || gDissolveProgress <= 0.0f) return result;
    if (gDissolveProgress >= 1.0f)
    {
        result.signedDistance = -1.0f;
        return result;
    }
    float strength = gDissolveNoiseScale > 0.0f ? gDissolveNoiseStrength : 0.0f;
    float low = gDissolveScanMin - strength * 0.5f;
    float high = gDissolveScanMax + strength * 0.5f;
    float threshold = low + gDissolveProgress * max(high - low, 1.0e-5f);
    float noise = strength > 0.0f ? (NeonDissolveNoise(skinnedModelPosition) - 0.5f) * strength : 0.0f;
    result.signedDistance = dot(skinnedModelPosition, gDissolveDirection) + noise - threshold;
    float aa = max(length(float2(ddx(result.signedDistance), ddy(result.signedDistance))), 1.0e-6f);
    if (gDissolveEdgeEnabled != 0 && gDissolveEdgeWidth > 0.0f && gDissolveEdgeIntensity > 0.0f)
    {
        float survivingDistance = max(result.signedDistance, 0.0f);
        float width = gDissolveEdgeWidth;
        float coreWidth = width * 0.22f;
        float core = 1.0f - smoothstep(coreWidth, coreWidth + aa, survivingDistance);
        float halo = 1.0f - smoothstep(width, width + aa, survivingDistance);
        float3 color = max(gDissolveEdgeColor, 0.0f);
        float3 whiteCore = lerp(color, float3(1.0f, 0.8f, 0.9f), 0.65f);
        result.edgeEmission = (whiteCore * core + color * halo * 0.35f) * gDissolveEdgeIntensity;
    }
    return result;
}

void ApplyNeonDissolveClip(NeonDissolveEvaluation dissolve)
{
    clip(dissolve.signedDistance);
}
#endif
