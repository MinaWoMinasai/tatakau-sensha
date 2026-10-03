#ifndef NEON_SKINNED_BODY_HLSLI
#define NEON_SKINNED_BODY_HLSLI
#include "NeonSkinned.hlsli"
#include "NeonSkinnedSurface.hlsli"
#include "NeonDissolve.hlsli"

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
    float4 normal : SV_TARGET1;
    float4 material : SV_TARGET2;
};

// Bodyの既存処理を両PSで共有。呼び出し側は微分をclip/分岐より先に評価する。
PixelShaderOutput ShadeNeonBody(NeonSkinnedVertexOutput input, bool isFrontFace,
    float2 uvDx, float2 uvDy, float3 geometryEmission)
{
    float4 surface = SampleNeonSurface(input.texcoord, uvDx, uvDy);
    float4 featureMask = SampleNeonFeatureMask(input.texcoord, uvDx, uvDy);
    float2 lineCoverage = ReconstructNeonLineCoverage(input.texcoord, uvDx, uvDy, featureMask);
    float internalLine = NeonInternalLine(input.texcoord, uvDx, uvDy, surface);
    NeonDissolveEvaluation dissolve = EvaluateNeonDissolve(input.skinnedModelPosition);
    ApplyNeonAlphaCutout(surface.a);
    ApplyNeonDissolveClip(dissolve);
    float3 normal = input.worldNormal * rsqrt(max(dot(input.worldNormal, input.worldNormal), 1.0e-8f));
    normal = isFrontFace ? normal : -normal;
    float3 toCamera = gCameraWorldPosition - input.worldPosition;
    float3 viewDirection = toCamera * rsqrt(max(dot(toCamera, toCamera), 1.0e-8f));
    float rim = pow(1.0f - saturate(dot(normal, viewDirection)), max(gRimPower, 0.001f));

    // 照明には依存せず、視線と法線で暗い面を少しだけ読めるようにする。
    // 強度0なら従来のBody色。Rimと線の色・強度は独立したまま。
    float3 body = max(gBodyColor.rgb, 0.0f);
    body += body * max(gBodyEmissionIntensity, 0.0f)
        * (0.25f + 0.75f * saturate(dot(normal, viewDirection)));
    float3 emission = max(gEmissiveColor, 0.0f) * max(gEmissiveIntensity, 0.0f)
        * max(gRimStrength, 0.0f) * rim;
    NeonLineEmission lineEmission = CompositeNeonInternalEmission(internalLine, featureMask, lineCoverage);
    emission += lineEmission.core + lineEmission.halo;
    emission += geometryEmission;
    PixelShaderOutput output;
    output.color = float4(body + emission, saturate(gBodyColor.a));
    if (gLineDiagnosticMode == 1) output.color.rgb = lineEmission.core;
    else if (gLineDiagnosticMode == 2) output.color.rgb = lineEmission.halo;
    else if (gLineDiagnosticMode == 3) output.color.rgb = body;
    if (gFeatureMaskBlend > 0.0f && gFeatureMaskDebugMode != 0)
    {
        float diagnostic = gFeatureMaskDebugMode == 1 ? featureMask.r : featureMask.g;
        output.color.rgb = diagnostic.xxx;
    }
    output.color.rgb += dissolve.edgeEmission;
    // 現在のScene MRT encoding。反射用SSR mask=0、roughness=1、metallic=0、AO=1。
    output.normal = float4(normal * 0.5f + 0.5f, 0.0f);
    output.material = float4(1.0f, 0.0f, 1.0f, 0.0f);
    return output;
}
#endif
