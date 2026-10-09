#include "Trail.hlsli"

// Isolated plate path. Legacy still uses WindmillEmoji.PS.hlsl unchanged.
Texture2D<float4> glyphAtlas : register(t0); // LinearData, premultiplied RGB
SamplerState glyphSampler : register(s0);
cbuffer WindmillNeon : register(b0)
{
    float baseBrightness;
    float coreWidthPixels;
    float coreIntensity;
    float innerThreshold;
    float innerIntensity;
    float surfaceEmission;
    float haloWidthPixels;
    float haloIntensity;
    float3 coreColor;
    uint diagnostic; // 0 combined, 1 base, 2 outer core, 3 inner, 4 halo, 5 surface
    float3 haloColor;
    float recognitionAccent;
};
struct Output { float4 color : SV_TARGET0; float4 normal : SV_TARGET1; float4 material : SV_TARGET2; };

// Uniform sampling/derivatives finish before clip. Widths are screen pixels,
// independent of projected plate size, rotation, Core radiance or Bloom gain.
float4 SampleGlyph(float2 uv, float2 dx, float2 dy, float2 tile)
{
    float2 lo = tile + .5 / 576, hi = tile + 1.0 / 3 - .5 / 576;
    float4 p = glyphAtlas.SampleGrad(glyphSampler, clamp(uv, lo, hi), dx, dy);
    return p * (all(uv >= tile) && all(uv <= tile + 1.0 / 3));
}
float2 AlphaBand(float2 uv, float2 dx, float2 dy, float radius, float alpha, float2 tile)
{
    float lo = alpha, hi = alpha, sum = 0;
    const float2 offsets[8] = {
        float2(1,0),float2(-1,0),float2(0,1),float2(0,-1),
        float2(.7071,.7071),float2(-.7071,.7071),float2(.7071,-.7071),float2(-.7071,-.7071)};
    [unroll] for (uint i = 0; i < 8; ++i) {
        float a = SampleGlyph(uv + radius * (dx * offsets[i].x + dy * offsets[i].y), dx, dy, tile).a;
        lo = min(lo, a); hi = max(hi, a); sum += a;
    }
    return float2(hi - lo, abs(alpha - sum * .125));
}
float3 Signal(float4 p) { return sqrt(max(p.rgb / max(p.a, .01), 0)); }
Output main(VertexShaderOutput input)
{
    float2 dx = ddx(input.texcoord), dy = ddy(input.texcoord);
    float2 tile = floor(input.texcoord * 3) / 3;
    float4 p = SampleGlyph(input.texcoord, dx, dy, tile);
    float core = coreWidthPixels > 0 ? AlphaBand(input.texcoord, dx, dy, max(.5, coreWidthPixels * .5), p.a, tile).x : 0;
    float halo = 0;
    if (haloWidthPixels > 0) {
        halo = AlphaBand(input.texcoord, dx, dy, haloWidthPixels, p.a, tile).y * .45;
        halo += AlphaBand(input.texcoord, dx, dy, haloWidthPixels * .6, p.a, tile).y * .35;
        halo += AlphaBand(input.texcoord, dx, dy, haloWidthPixels * .3, p.a, tile).y * .20;
        halo = saturate(halo * 2) * (1 - core); // Halo never pays Core energy twice.
    }
    // Adapted from NeonSkinnedSurface: perceptual color difference, no semantic
    // eye/mouth/finger recognition. Reject alpha-edge neighborhoods so exterior
    // lines do not become duplicate internal emission. MIP-filtered color keeps
    // subpixel contrast finite rather than thresholding a level-zero distance.
    float4 xp = SampleGlyph(input.texcoord + dx, dx, dy, tile);
    float4 xm = SampleGlyph(input.texcoord - dx, dx, dy, tile);
    float4 yp = SampleGlyph(input.texcoord + dy, dx, dy, tile);
    float4 ym = SampleGlyph(input.texcoord - dy, dx, dy, tile);
    float3 difference = max(abs(Signal(xp) - Signal(xm)), abs(Signal(yp) - Signal(ym)));
    float contrast = max(difference.r, max(difference.g, difference.b));
    float interior = smoothstep(.80, .98, min(min(xp.a,xm.a), min(yp.a,ym.a))) * p.a * (1 - core);
    float inner = smoothstep(innerThreshold, max(innerThreshold + .02, innerThreshold * 1.8), contrast) * interior;
    float3 coreTint = lerp(coreColor, float3(1,.22,.34), recognitionAccent);
    float3 haloTint = lerp(haloColor, float3(1,.025,.12), recognitionAccent);
    float3 body = p.rgb * baseBrightness * (1 - core);
    float3 outerEmission = coreTint * coreIntensity * core;
    float3 innerEmission = haloTint * innerIntensity * inner;
    float3 surface = p.rgb * surfaceEmission * (1 - core) * (1 - inner);
    float3 localHalo = haloTint * haloIntensity * halo;
    float3 rgb = body + outerEmission + innerEmission + surface + localHalo;
    if (diagnostic == 1) rgb = body;
    else if (diagnostic == 2) rgb = outerEmission;
    else if (diagnostic == 3) rgb = innerEmission;
    else if (diagnostic == 4) rgb = localHalo;
    else if (diagnostic == 5) rgb = surface;
    clip(max(p.a, max(core, halo)) - .001);
    Output o;
    // Premultiplied blend ONE / INV_SRC_ALPHA. Halo has radiance but no body
    // opacity: it adds light over the room without covering it with a dark quad.
    o.color = float4(rgb * input.color.a, saturate(p.a + core) * input.color.a);
    o.normal = float4(.5,.5,0,1); o.material = float4(1,0,1,0);
    return o;
}
