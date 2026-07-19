struct Material
{
    float4 color;
};
ConstantBuffer<Material> gMaterial : register(b0);

// Texture2DではなくTextureCubeを使用する
TextureCube<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float3 texcoord : TEXCOORD0;
};

#ifndef SCENE_NORMAL_TARGET
#define SCENE_NORMAL_TARGET 0
#endif

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
#if SCENE_NORMAL_TARGET
    float4 normal : SV_TARGET1;
    float4 material : SV_TARGET2;
#endif
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
#if SCENE_NORMAL_TARGET
    output.normal = float4(0.5f, 0.5f, 1.0f, 0.0f);
    output.material = float4(1.0f, 0.0f, 1.0f, 0.0f);
#endif
    if (gMaterial.color.a > 1.5f)
    {
        float3 dir = normalize(input.texcoord);
        float horizon = saturate(dir.y * 0.5f + 0.5f);
        float3 sunDir = normalize(float3(0.36f, 0.30f, 0.88f));
        float sun = pow(saturate(dot(dir, sunDir)), 420.0f);
        float sunGlow = pow(saturate(dot(dir, sunDir)), 18.0f);
        float lowMist = pow(1.0f - horizon, 3.0f);
        float zenithFade = pow(horizon, 1.35f);

        float warmHaze = pow(saturate(dot(dir, sunDir)), 4.0f);
        float3 zenith = float3(0.070f, 0.150f, 0.285f);
        float3 midSky = float3(0.185f, 0.410f, 0.570f);
        float3 horizonSky = float3(0.650f, 0.690f, 0.620f);
        float3 color = lerp(horizonSky, midSky, smoothstep(0.05f, 0.55f, horizon));
        color = lerp(color, zenith, zenithFade * 0.72f);
        color += float3(1.0f, 0.72f, 0.46f) * warmHaze * 0.16f;
        color += float3(0.98f, 0.84f, 0.52f) * sunGlow * 0.60f;
        color += float3(1.0f, 0.92f, 0.66f) * sun * 8.0f;
        color = lerp(color, float3(0.48f, 0.56f, 0.56f), lowMist * 0.16f);

        output.color = float4(max(color * gMaterial.color.rgb, 0.0f), 1.0f);
        return output;
    }

    // 3Dの方向ベクトルでサンプリング
    float4 sampledColor = gTexture.Sample(gSampler, input.texcoord);
    output.color = sampledColor * gMaterial.color;
    return output;
}
