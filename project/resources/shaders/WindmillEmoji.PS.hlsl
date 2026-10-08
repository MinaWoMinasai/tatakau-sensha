#include "Trail.hlsli"
Texture2D<float4> glyphAtlas : register(t0);
SamplerState glyphSampler : register(s0);
struct Output {
    float4 color : SV_TARGET0;
    float4 normal : SV_TARGET1;
    float4 material : SV_TARGET2;
};
Output main(VertexShaderOutput input)
{
    float4 glyph = glyphAtlas.Sample(glyphSampler, input.texcoord);
    // 透明な四角形を深度へ書き込まず、絵文字のシルエットだけを立体空間へ置く。
    clip(glyph.a - 0.03);
    Output output;
    // SRGBテクスチャはロード時のSRVで線形化される。元の顔・指の色を保持する。
    output.color = float4(glyph.rgb * 0.95 * input.color.a, glyph.a * input.color.a);
    output.normal = float4(0.5, 0.5, 0.0, 1.0);
    output.material = float4(1.0, 0.0, 1.0, 0.0);
    return output;
}
