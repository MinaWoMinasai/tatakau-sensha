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
    // LDRへ直接送る発光を範囲内に収める。スカラーの肩1つで色相を保つ。
    float3 color = ComposeObjectBloom(bloom.rgb);
    float energy = max(max(color.r, color.g), color.b);
    if (energy <= 0.001f)
    {
        discard;
    }
    // このパスは既に光エネルギーの加算合成になっている。ぼかしたカバレッジを
    // SrcAlphaとして再利用すると細い光源が二重に減衰し、
    // 1ピクセル幅のUIの線が実質的に消えてしまう。上のエネルギー判定で
    // 空のピクセルは除外しているので、残ったBloomは重み1で加算する。
    return float4(color, 1.0f);
}
