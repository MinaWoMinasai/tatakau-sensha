#include "NeonSkinned.hlsli"
#include "NeonSkinnedSurface.hlsli"
#include "NeonDissolve.hlsli"

float4 main(NeonSkinnedVertexOutput input) : SV_TARGET0
{
    float2 uvDx = ddx(input.texcoord), uvDy = ddy(input.texcoord);
    float alpha = SampleNeonSurface(input.texcoord, uvDx, uvDy).a;
    NeonDissolveEvaluation dissolve = EvaluateNeonDissolve(input.skinnedModelPosition);
    ApplyNeonAlphaCutout(alpha);
    ApplyNeonDissolveClip(dissolve);
    // HDRのままSceneへ出力。既存のGlobal Bloom / Tone Mappingが線の周囲を発光させる。
    float3 core = gSplitLineEmission != 0
        ? max(gOutlineCoreColor, 0.0f) * max(gOutlineCoreIntensity, 0.0f)
        : max(gEmissiveColor, 0.0f) * max(gEmissiveIntensity, 0.0f);
    return float4(core + dissolve.edgeEmission, 1.0f);
}
