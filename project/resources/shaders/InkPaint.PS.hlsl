cbuffer Frame : register(b0)
{
    row_major float4x4 viewProjection;
    float4 eye;
    float4 renderOptions; // x: coverage reconstruction on/off for A/B profiling
};
Texture2D<float4> paintMask : register(t0);

struct VertexOutput
{
    float4 position : SV_Position;
    float3 worldPosition : POSITION0;
    float2 uv : TEXCOORD0;
    nointerpolation float4 normalTile : TEXCOORD1;
    nointerpolation float4 colorInkable : TEXCOORD2;
    nointerpolation float2 size : TEXCOORD3;
    nointerpolation float3 uAxis : TEXCOORD4;
    nointerpolation float3 vAxis : TEXCOORD5;
};
struct PixelOutput
{
    float4 color : SV_Target0;
    float4 normal : SV_Target1;
    float4 material : SV_Target2;
};

uint ReadTeam(int2 tileOrigin, int2 pixel)
{
    return uint(round(paintMask.Load(int3(tileOrigin + clamp(pixel, int2(0, 0), int2(511, 511)), 0)).a * 2.0f));
}

float2 TeamWeights(uint team)
{
    return float2(team == 1 ? 1.0f : 0.0f, team == 2 ? 1.0f : 0.0f);
}

// Reconstruct COVERAGE from four categorical samples, never interpolate encoded
// team IDs (which would turn enemy ink into own ink at a dry boundary). The
// persistent team grid and its exact CPU query stay entirely unchanged. Clamping
// each load to its tile also prevents neighboring surfaces bleeding together.
float2 SmoothWeights(int2 tileOrigin, float2 uv, out float2 gradient)
{
    const float2 texel = uv * 512.0f - 0.5f;
    const int2 p = int2(floor(texel));
    const float2 f = frac(texel);
    const float2 a = TeamWeights(ReadTeam(tileOrigin, p));
    const float2 b = TeamWeights(ReadTeam(tileOrigin, p + int2(1, 0)));
    const float2 c = TeamWeights(ReadTeam(tileOrigin, p + int2(0, 1)));
    const float2 d = TeamWeights(ReadTeam(tileOrigin, p + int2(1, 1)));
    const float2 dx = lerp(b - a, d - c, f.y);
    const float2 dy = lerp(c - a, d - b, f.x);
    gradient = float2(dx.x + dx.y, dy.x + dy.y);
    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

float Grid(float2 position, float lineWidth)
{
    float2 distanceToLine = abs(frac(position + 0.5f) - 0.5f);
    float2 antialias = max(fwidth(position), float2(0.0001f, 0.0001f));
    float2 lines = 1.0f - smoothstep(lineWidth, lineWidth + antialias, distanceToLine);
    return max(lines.x, lines.y);
}

PixelOutput main(VertexOutput input)
{
    const uint tile = uint(input.normalTile.w + 0.5f);
    const int2 tileOrigin = int2(tile % 4, tile / 4) * 512;
    const int2 pixel = clamp(int2(input.uv * 512.0f), int2(0, 0), int2(511, 511));
    float2 gradient = 0;
    float2 weights = 0;
    if (input.colorInkable.a > 0.5f)
    {
        if (renderOptions.x > 0.5f)
            weights = SmoothWeights(tileOrigin, input.uv, gradient);
        else
            weights = TeamWeights(ReadTeam(tileOrigin, pixel));
    }
    const float painted = saturate(weights.x + weights.y);
    const float2 local = input.uv * input.size;
    float3 normal = normalize(input.normalTile.xyz);
    const float3 view = normalize(eye.xyz - input.worldPosition + float3(0, 0.00001f, 0));
    const float3 light = normalize(float3(-0.22f, 0.66f, 0.72f));
    const float checker = fmod(floor(local.x / 2.0f) + floor(local.y / 2.0f), 2.0f);
    float3 base = input.colorInkable.rgb * (0.73f + checker * 0.13f);
    base *= 1.0f - Grid(local / 2.0f, 0.007f) * 0.32f;
    const float edgeDistance = min(min(local.x, input.size.x - local.x), min(local.y, input.size.y - local.y));
    base = lerp(base, input.colorInkable.rgb * 1.35f, 1.0f - smoothstep(0.025f, 0.06f, edgeDistance));

    if (input.colorInkable.a < 0.5f)
    {
        // Unpaintable panels have an unmistakable close metal mesh and amber
        // safety border; their material cannot acquire an ink layer.
        base = lerp(float3(0.055f, 0.08f, 0.11f), float3(0.25f, 0.32f, 0.37f), Grid(local * 3.2f, 0.06f));
        const float stripes = step(0.5f, frac((local.x + local.y) * 2.0f));
        const float3 safety = lerp(float3(0.09f, 0.1f, 0.1f), float3(0.98f, 0.56f, 0.08f), stripes);
        base = lerp(base, safety, 1.0f - smoothstep(0.08f, 0.13f, edgeDistance));
    }
    float roughness = 0.82f;
    float specular = 0;
    if (painted > 0.0001f)
    {
        // Linear-light pigment stays below the bloom threshold. A colored
        // diffuse body plus restrained neutral dielectric highlight reads wet,
        // without the previous bright mint wash or metallic reflection.
        float3 ink = (weights.x * float3(0.004f, 0.39f, 0.235f) +
            weights.y * float3(0.48f, 0.008f, 0.145f)) / max(painted, 0.0001f);
        const float2 ripple = float2(sin(local.x * 13.0f + local.y * 7.0f), cos(local.y * 15.0f - local.x * 5.0f));
        const float2 slope = -gradient * 0.09f + ripple * 0.025f;
        const float3 wetNormal = normalize(normal + input.uAxis * slope.x + input.vAxis * slope.y);
        normal = normalize(lerp(normal, wetNormal, painted));
        const float rim = saturate(length(gradient));
        ink *= 0.96f + 0.025f * sin(local.x * 3.7f + local.y * 2.4f) - rim * 0.06f;
        base = lerp(base, ink, painted);
        const float3 halfway = normalize(view + light);
        const float fresnel = 0.04f + 0.96f * pow(1.0f - saturate(dot(normal, view)), 5.0f);
        const float halfwayCosine = saturate(dot(normal, halfway));
        specular = painted * (pow(halfwayCosine, 64.0f) * 0.12f +
            pow(halfwayCosine, 18.0f) * 0.055f + fresnel * 0.018f);
        roughness = lerp(roughness, 0.30f, painted);
    }
    const float diffuse = 0.43f + 0.65f * saturate(dot(normal, light));
    PixelOutput output;
    output.color = float4(base * diffuse + specular * float3(0.80f, 0.94f, 1.0f), 1.0f);
    // Match the engine's EncodeNormalTarget and EncodeMaterialTarget layout.
    output.normal = float4(normal * 0.5f + 0.5f, painted * 0.06f);
    output.material = float4(roughness, 0.0f, 1.0f, 0.35f);
    return output;
}
