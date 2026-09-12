cbuffer Stamp : register(b0)
{
    float2 center;
    float2 radius;
    float cosine;
    float sine;
    float2 surfaceSize;
    uint2 tileOrigin;
    uint2 rectOrigin;
    uint2 rectSize;
    uint team;
    uint tileSize;
};

RWTexture2D<float4> paintMask : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 thread : SV_DispatchThreadID)
{
    if (any(thread.xy >= rectSize)) { return; }
    const uint2 pixel = rectOrigin + thread.xy;
    const float2 position = (float2(pixel) + 0.5f) / float(tileSize) * surfaceSize;
    const float2 d = position - center;
    const float2 rotated = float2(cosine * d.x + sine * d.y, -sine * d.x + cosine * d.y) / radius;
    // This exact ellipse also populates the CPU query mask. Irregular edges are
    // built from shared multi-ellipse stamps, never a GPU-only noise threshold.
    if (dot(rotated, rotated) <= 1.0f)
    {
        const float3 color = team == 1 ? float3(0.035f, 0.87f, 0.65f) : float3(0.86f, 0.08f, 0.46f);
        paintMask[tileOrigin + pixel] = team == 0 ? float4(0, 0, 0, 0) : float4(color, team * 0.5f);
    }
}
