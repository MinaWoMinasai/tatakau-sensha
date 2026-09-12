cbuffer BackgroundParameters : register(b0)
{
    float4 gDarkColor;
    float4 gLightColor;
    float gMode;
    float gCheckerScale;
    float gAspectRatio;
    float gPadding;
};

struct PixelShaderInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(PixelShaderInput input) : SV_TARGET0
{
    if (gMode < 0.5f)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    if (gMode < 1.5f)
    {
        return float4(0.032f, 0.032f, 0.034f, 1.0f);
    }

    const float2 checkerCoordinate = input.uv * float2(
        max(gCheckerScale * gAspectRatio, 1.0f),
        max(gCheckerScale, 1.0f));
    const float checker = fmod(
        floor(checkerCoordinate.x) + floor(checkerCoordinate.y),
        2.0f);
    return lerp(gDarkColor, gLightColor, checker);
}
