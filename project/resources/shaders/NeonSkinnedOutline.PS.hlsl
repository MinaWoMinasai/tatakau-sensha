#include "NeonSkinned.hlsli"
#include "NeonSkinnedSurface.hlsli"

float4 main(NeonSkinnedVertexOutput input) : SV_TARGET0
{
    ApplyNeonAlphaCutout(SampleNeonSurface(input.texcoord, ddx(input.texcoord), ddy(input.texcoord)).a);
    // HDRのままSceneへ出力。既存のGlobal Bloom / Tone Mappingが線の周囲を発光させる。
    return float4(max(gEmissiveColor, 0.0f) * max(gEmissiveIntensity, 0.0f), 1.0f);
}
