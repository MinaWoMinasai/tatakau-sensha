Texture2D presentTex : register(t0);
SamplerState samp : register(s0);

#include "PostEffectCommon.hlsli"

// Final presentation of the whole frame (3D composite + UI) through a vector-monitor CRT:
// barrel distortion, slight RGB convergence error, scanlines, phosphor grain, vignette and a rounded bezel.

float2 Distort(float2 centered)
{
    // Barrel distortion that grows toward the corners, like a curved glass face.
    float2 offset = centered.yx * 0.5f;
    return centered + centered * (offset * offset) * arenaCurvature;
}

float BezelMask(float2 centered, float aspect)
{
    float2 p = abs(centered) * float2(aspect, 1.0f);
    float2 halfSize = float2(aspect, 1.0f) * (1.0f - arenaBezelMargin);
    float radius = arenaBezelRadius * 2.0f;
    float2 q = p - (halfSize - radius);
    float distanceToEdge = length(max(q, 0.0f)) + min(max(q.x, q.y), 0.0f) - radius;
    return 1.0f - smoothstep(-arenaMaskSoftness, arenaMaskSoftness, distanceToEdge);
}

float4 main(PSInput input) : SV_TARGET
{
    if (arenaEnabled <= 0.5f)
    {
        return float4(presentTex.Sample(samp, input.uv).rgb, 1.0f);
    }

    uint width;
    uint height;
    presentTex.GetDimensions(width, height);
    const float aspect = (float)width / (float)height;

    float2 centered = input.uv * 2.0f - 1.0f;
    float2 warped = Distort(centered);
    float2 uv = warped * 0.5f + 0.5f;
    if (any(uv < 0.0f) || any(uv > 1.0f))
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    // Convergence error: red and blue guns land slightly apart, more at the edges.
    float2 shift = (uv - 0.5f) * arenaChromAb;
    float3 color;
    color.r = presentTex.Sample(samp, uv + shift).r;
    color.g = presentTex.Sample(samp, uv).g;
    color.b = presentTex.Sample(samp, uv - shift).b;

    // A faint halo from the neighbouring texels keeps thin vector lines soft like phosphor dots.
    float2 texel = 1.0f / float2(width, height);
    float3 halo = presentTex.Sample(samp, uv + float2(texel.x, 0.0f)).rgb +
                  presentTex.Sample(samp, uv - float2(texel.x, 0.0f)).rgb +
                  presentTex.Sample(samp, uv + float2(0.0f, texel.y)).rgb +
                  presentTex.Sample(samp, uv - float2(0.0f, texel.y)).rgb;
    color = max(color, halo * 0.25f * 0.55f);

    // Scanlines darken between beam passes; black remains black.
    float beam = 0.5f + 0.5f * sin(uv.y * arenaScanlineFrequency);
    color *= 1.0f - arenaScanlineIntensity * (1.0f - beam);

    // Phosphor grain only on lit pixels.
    if (arenaGrainIntensity > 0.0f)
    {
        float grain = Hash(floor(uv * float2(width, height) * 0.5f) + frac(timer * 7.31f) * 97.0f) - 0.5f;
        float luminance = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
        color *= 1.0f + grain * arenaGrainIntensity * saturate(luminance * 4.0f);
    }

    float vignette = pow(saturate(1.0f - length(centered * 0.5f) * arenaVignette), 0.8f);
    color *= vignette * BezelMask(warped, aspect);
    return float4(color, 1.0f);
}
