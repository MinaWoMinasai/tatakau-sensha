Texture2D bloomTex : register(t0);
SamplerState samp : register(s0);

#include "PostEffectCommon.hlsli"

float4 main(PSInput input) : SV_TARGET
{
    float2 uv = input.uv - float2(boxBlurRadius, fullScreenBoxBlurBlend);
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
    {
        discard;
    }

    float4 bloom = bloomTex.Sample(samp, uv);
    // Bound glow sent directly to LDR; preserve hue using one scalar shoulder.
    float3 color = ComposeObjectBloom(bloom.rgb);
    float energy = max(max(color.r, color.g), color.b);
    if (energy <= 0.001f)
    {
        discard;
    }
    // This pass is already an additive light-energy composite.  Reusing the
    // blurred coverage as SrcAlpha attenuates thin emitters a second time and
    // makes one-pixel UI lines effectively disappear.  The energy test above
    // keeps empty pixels out, so surviving bloom must be added at full weight.
    return float4(color, 1.0f);
}
