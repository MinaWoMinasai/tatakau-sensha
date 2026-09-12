cbuffer Frame : register(b0)
{
    row_major float4x4 viewProjection;
    float4 eye;
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
    const uint team = input.colorInkable.a > 0.5f ? ReadTeam(tileOrigin, pixel) : 0;
    const float painted = team > 0 ? 1.0f : 0.0f;
    const float2 local = input.uv * input.size;
    float3 normal = normalize(input.normalTile.xyz);
    const float3 view = normalize(eye.xyz - input.worldPosition + float3(0, 0.00001f, 0));
    const float3 light = normalize(float3(-0.38f, 0.88f, -0.30f));
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
    if (team > 0)
    {
        base = team == 1 ? float3(0.012f, 0.78f, 0.53f) : float3(0.82f, 0.025f, 0.35f);
        // Subtle wet surface shading affects normals, never the team footprint.
        const float2 ripple = float2(sin(local.x * 21.0f + local.y * 8.0f), cos(local.y * 19.0f - local.x * 6.0f));
        const float left = ReadTeam(tileOrigin, pixel + int2(-1, 0)) == team ? 1.0f : 0.0f;
        const float right = ReadTeam(tileOrigin, pixel + int2(1, 0)) == team ? 1.0f : 0.0f;
        const float down = ReadTeam(tileOrigin, pixel + int2(0, -1)) == team ? 1.0f : 0.0f;
        const float up = ReadTeam(tileOrigin, pixel + int2(0, 1)) == team ? 1.0f : 0.0f;
        const float2 slope = float2(left - right, down - up) * 0.24f + ripple * 0.013f;
        normal = normalize(normal + input.uAxis * slope.x + input.vAxis * slope.y);
        const float rim = 1.0f - min(min(left, right), min(down, up));
        base *= 1.0f - rim * 0.12f;
        const float3 halfway = normalize(view + light);
        specular = pow(saturate(dot(normal, halfway)), 76.0f) * 1.25f;
        const float fresnel = pow(1.0f - saturate(dot(normal, view)), 4.0f);
        specular += fresnel * 0.20f;
        roughness = 0.2f;
    }
    const float diffuse = 0.43f + 0.65f * saturate(dot(normal, light));
    PixelOutput output;
    output.color = float4(base * diffuse + specular * float3(0.80f, 0.94f, 1.0f), 1.0f);
    // Match the engine's EncodeNormalTarget and EncodeMaterialTarget layout.
    output.normal = float4(normal * 0.5f + 0.5f, painted * 0.35f);
    output.material = float4(roughness, 0.0f, 1.0f, 0.35f);
    return output;
}
