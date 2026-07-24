#include "OceanFFTCommon.hlsli"

Texture2D<float4> gInputA : register(t0);
Texture2D<float4> gInputB : register(t1);
Texture2D<float4> gInputC : register(t2);
RWTexture2D<float4> gOutputA : register(u0);
RWTexture2D<float4> gOutputB : register(u1);
RWTexture2D<float4> gOutputC : register(u2);

float4 ApplyTwiddle(float4 value, float2 twiddle)
{
    return float4(
        ComplexMultiply(value.xy, twiddle),
        ComplexMultiply(value.zw, twiddle));
}

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x >= gFFTDispatchSize ||
        dispatchThreadId.y >= gFFTDispatchSize)
    {
        return;
    }

    uint2 outputCoordinate = dispatchThreadId.xy;
    uint transformIndex =
        gFFTDirection == 0u ? outputCoordinate.x : outputCoordinate.y;
    uint span = 1u << (gFFTStage + 1u);
    uint halfSpan = span >> 1u;
    uint blockStart = (transformIndex / span) * span;
    uint butterflyIndex = transformIndex % halfSpan;
    uint sourceIndex0 = blockStart + butterflyIndex;
    uint sourceIndex1 = sourceIndex0 + halfSpan;

    uint2 sourceCoordinate0 = outputCoordinate;
    uint2 sourceCoordinate1 = outputCoordinate;
    if (gFFTDirection == 0u)
    {
        sourceCoordinate0.x = sourceIndex0;
        sourceCoordinate1.x = sourceIndex1;
    }
    else
    {
        sourceCoordinate0.y = sourceIndex0;
        sourceCoordinate1.y = sourceIndex1;
    }

    float angle = 2.0f * kOceanPi * float(butterflyIndex) / float(span);
    float2 twiddle = float2(cos(angle), sin(angle));
    bool upperHalf = (transformIndex % span) >= halfSpan;

    float4 a0 = gInputA.Load(int3(sourceCoordinate0, 0));
    float4 a1 = ApplyTwiddle(gInputA.Load(int3(sourceCoordinate1, 0)), twiddle);
    float4 b0 = gInputB.Load(int3(sourceCoordinate0, 0));
    float4 b1 = ApplyTwiddle(gInputB.Load(int3(sourceCoordinate1, 0)), twiddle);
    float4 c0 = gInputC.Load(int3(sourceCoordinate0, 0));
    float4 c1 = ApplyTwiddle(gInputC.Load(int3(sourceCoordinate1, 0)), twiddle);

    gOutputA[outputCoordinate] = upperHalf ? a0 - a1 : a0 + a1;
    gOutputB[outputCoordinate] = upperHalf ? b0 - b1 : b0 + b1;
    gOutputC[outputCoordinate] = upperHalf ? c0 - c1 : c0 + c1;
}
