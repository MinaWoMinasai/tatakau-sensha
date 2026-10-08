#include "ModelParticle.hlsli"

#ifndef SCENE_NORMAL_TARGET
#define SCENE_NORMAL_TARGET 0
#endif

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    int32_t lightingMode;
    float32_t environmentCoefficient;
    float32_t proceduralContour; // Material.padding at byte 28, opt in only
    float32_t4x4 uvTransform;
    float32_t shininess;
};

struct Camera
{
    float32_t3 worldPosition;
};

struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float intensity;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
#if SCENE_NORMAL_TARGET
    float32_t4 normal : SV_TARGET1;
    float32_t4 material : SV_TARGET2;
#endif
};

float SegmentDistance(float2 p, float2 a, float2 b)
{
    const float2 delta = b - a;
    const float along = saturate(dot(p - a, delta) / max(dot(delta, delta), 1e-6f));
    return length(p - a - delta * along);
}

float TriangleContourDistance(float2 p)
{
    const float2 a = float2(0.0f, 1.0f);
    const float2 b = float2(-0.8660254f, -0.5f);
    const float2 c = float2(0.8660254f, -0.5f);
    return min(SegmentDistance(p, a, b), min(SegmentDistance(p, b, c), SegmentDistance(p, c, a)));
}

float CombatContourDistance(float2 p, uint shape)
{
    if (shape == 1u) // asymmetric shard
    {
        const float2 a = float2(0.0f, 0.95f), b = float2(-0.42f, -0.40f);
        const float2 c = float2(-0.12f, -0.62f), d = float2(0.45f, -0.12f);
        return min(min(SegmentDistance(p, a, b), SegmentDistance(p, b, c)),
                   min(SegmentDistance(p, c, d), SegmentDistance(p, d, a)));
    }
    if (shape == 2u) // narrow triangular sliver
    {
        const float2 a = float2(0.12f, 1.0f), b = float2(-0.17f, -0.50f), c = float2(0.10f, -0.58f);
        return min(SegmentDistance(p, a, b), min(SegmentDistance(p, b, c), SegmentDistance(p, c, a)));
    }
    if (shape == 3u) return SegmentDistance(p, float2(0.0f, -0.65f), float2(0.0f, 0.75f));
    if (shape == 5u) return abs(length(p) - 0.52f);
    return TriangleContourDistance(p);
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
#if SCENE_NORMAL_TARGET
    output.normal = float32_t4(0.5f, 0.5f, 1.0f, 0.0f);
    output.material = float32_t4(1.0f, 0.0f, 1.0f, 0.0f);
#endif
    
    // UVトランスフォームの適用
    float32_t4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);

    if (gMaterial.proceduralContour > 0.5f)
    {
        // A single distance field closes all three corners without drawing the
        // shared vertices twice. Derivatives integrate the edge at pixel width.
        const uint shape = (uint)clamp(input.neonRadiance - 1.0f, 0.0f, 5.0f);
        float distance = CombatContourDistance(input.localPosition, shape);
        if (shape == 5u)
        {
            // Expansion changes radius, while the shockwave's line stays thin
            // in world space rather than becoming a thick donut at its edge.
            const float scaleX = length(ddx(input.worldPosition)) / max(length(ddx(input.localPosition)), 1e-5f);
            const float scaleY = length(ddy(input.worldPosition)) / max(length(ddy(input.localPosition)), 1e-5f);
            distance *= max(scaleX, scaleY);
        }
        // The maximum filter support remains inside the quad's 0.35 padding,
        // including a nearly edge-on or subpixel fragment.
        const float aa = clamp(fwidth(distance) * 0.5f, 0.0001f, 0.16f);
        const float shoulder = 1.0f - smoothstep(0.055f - aa, 0.075f + aa, distance);
        const float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
        const float4 source = gMaterial.color * textureColor * input.color;
        if (input.neonRadiance > 0.5f && shape == 4u)
        {
            // Round, spatially soft flash. A short lifetime makes this the
            // impact peak, while the separate ring and debris remain readable.
            const float radius = length(input.localPosition);
            const float edge = 1.0f - smoothstep(0.42f, 0.64f, radius);
            const float light = exp2(-5.0f * radius * radius) * edge;
            output.color = float4(source.rgb * (4.0f * light), saturate(source.a));
            if (light <= 0.0001f) discard;
            return output;
        }
        if (input.neonRadiance <= 0.5f)
        {
            // Keep the original authored RGB and alpha for ordinary emitters.
            output.color = float4(source.rgb, source.a * shoulder);
            if (output.color.a <= 0.0001f) discard;
            return output;
        }

        const float peak = max(source.r, max(source.g, source.b));
        if (peak <= 0.0001f || source.a <= 0.0001f) discard;
        const float3 hue = max(source.rgb, 0.0f) / peak;
        const float3 pale = lerp(float3(1.0f, 1.0f, 1.0f), hue, shape == 0u ? 0.16f : 0.52f);
        const float core = 1.0f - smoothstep((shape == 0u ? 0.014f : 0.008f) - aa,
                                            (shape == 0u ? 0.032f : 0.020f) + aa, distance);
        const float halo = exp2(-2.0f * distance * distance / (0.125f * 0.125f)) *
            (1.0f - smoothstep(0.24f, 0.29f, distance));
        // Same source structure as the projectile heads: pale HDR core,
        // saturated shoulder, and a low-energy halo outside the crisp contour.
        // Bound already-HDR flashes to the projectile source range while
        // preserving the authored energy of small, dimmer fragments.
        const float energy = min(peak, 1.0f);
        output.color.rgb = energy * (hue * ((shape == 0u ? 2.8f : 2.2f) * shoulder * (1.0f - core * 0.70f) +
            (shape == 0u ? 0.22f : 0.16f) * halo) + pale * ((shape == 0u ? 4.5f : 3.4f) * core));
        output.color.a = saturate(source.a);
        if (max(shoulder, halo) <= 0.0001f) discard;
        return output;
    }

    if (gMaterial.enableLighting != 0)
    {
        // 1. 視線ベクトルの計算
        float3 V = normalize(gCamera.worldPosition - input.worldPosition);
        
        // 2. 奥行き感（擬似屈折）
        float2 distortion = V.xy * 0.05f;
        float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy + distortion);
        
        if (textureColor.a < 0.1f)
        {
            discard;
        }

        // 3. フラットシェーディング法線の計算 (宝石の面を作る)
        float3 dx = ddx(input.worldPosition);
        float3 dy = ddy(input.worldPosition);
        float3 N = normalize(cross(dx, dy));

        // 4. ライティング計算
        float3 L = -normalize(gDirectionalLight.direction);
        float3 H = normalize(L + V);
        float NdotL = saturate(dot(N, L));
        float NdotH = saturate(dot(N, H));

        // A. 鋭いスペキュラ
        float3 jewelSpecular = gDirectionalLight.color.rgb * pow(NdotH, 300.0f) * gDirectionalLight.intensity * 2.0f;
        
        // B. フレネル発光
        float fresnel = pow(1.0f - saturate(dot(N, V)), 4.0f);
        float3 jewelGlow = input.color.rgb * fresnel * 3.0f;

        // C. ベースカラー
        float3 baseColor = textureColor.rgb * gMaterial.color.rgb * input.color.rgb * 0.3f;

        // 最終色合成
        output.color.rgb = baseColor + jewelSpecular + jewelGlow;
        
        // 1. まずベースとなる不透明度を計算（輝き成分を足す）
        float baseAlpha = (0.5f * gMaterial.color.a * textureColor.a) + fresnel + pow(NdotH, 100.0f);
        // 2. その全体に対して input.color.a (フェードアウト係数) を掛ける
        output.color.a = saturate(baseAlpha * input.color.a);
    }
    else
    {
        // ライティングなし
        float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
        if (textureColor.a < 0.1f)
        {
            discard;
        }
        output.color = gMaterial.color * textureColor * input.color;
    }

    return output;
}
