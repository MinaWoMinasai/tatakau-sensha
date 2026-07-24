#include "OceanFFTCommon.hlsli"

Texture2D<float4> gSpectrumA : register(t0);
Texture2D<float4> gSpectrumB : register(t1);
Texture2D<float4> gSpectrumC : register(t2);
RWTexture2D<float4> gDisplacement : register(u0);
RWTexture2D<float2> gSlope : register(u1);

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x >= gFFTSize || dispatchThreadId.y >= gFFTSize)
    {
        return;
    }

    uint2 coordinate = dispatchThreadId.xy;
    float4 spectrumA = gSpectrumA.Load(int3(coordinate, 0));
    float4 spectrumB = gSpectrumB.Load(int3(coordinate, 0));
    float4 spectrumC = gSpectrumC.Load(int3(coordinate, 0));

    float height = spectrumA.x;
    float displacementX = spectrumA.z;
    float displacementZ = spectrumB.x;
    float slopeX = spectrumB.z;
    float slopeZ = spectrumC.x;
    gDisplacement[coordinate] =
        float4(displacementX, height, displacementZ, 1.0f);
    gSlope[coordinate] = float2(slopeX, slopeZ);
}
