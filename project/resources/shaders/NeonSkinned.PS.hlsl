#include "NeonSkinned.hlsli"

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
    float4 normal : SV_TARGET1;
    float4 material : SV_TARGET2;
};

PixelShaderOutput main(NeonSkinnedVertexOutput input, bool isFrontFace : SV_IsFrontFace)
{
    float3 normal = input.worldNormal * rsqrt(max(dot(input.worldNormal, input.worldNormal), 1.0e-8f));
    normal = isFrontFace ? normal : -normal;
    float3 toCamera = gCameraWorldPosition - input.worldPosition;
    float3 viewDirection = toCamera * rsqrt(max(dot(toCamera, toCamera), 1.0e-8f));
    float rim = pow(1.0f - saturate(dot(normal, viewDirection)), max(gRimPower, 0.001f));

    // 面全体の固定発光は行わない。Rimは任意の弱い補助で、外周は専用パスで描く。
    float3 emission = max(gEmissiveColor, 0.0f) * max(gEmissiveIntensity, 0.0f)
        * max(gRimStrength, 0.0f) * rim;
    PixelShaderOutput output;
    output.color = float4(max(gBodyColor.rgb, 0.0f) + emission, saturate(gBodyColor.a));
    // 現在のScene MRT encoding。反射用SSR mask=0、roughness=1、metallic=0、AO=1。
    output.normal = float4(normal * 0.5f + 0.5f, 0.0f);
    output.material = float4(1.0f, 0.0f, 1.0f, 0.0f);
    return output;
}
