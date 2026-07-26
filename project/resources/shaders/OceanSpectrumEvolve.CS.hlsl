#include "OceanFFTCommon.hlsli"

Texture2D<float4> gInitialSpectrum : register(t0);
RWTexture2D<float4> gSpectrumA : register(u0);
RWTexture2D<float4> gSpectrumB : register(u1);
RWTexture2D<float4> gSpectrumC : register(u2);
RWTexture2D<float4> gSpectrumD : register(u3);
RWTexture2D<float4> gEvolvedSpectrumDebug : register(u4);

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x >= gFFTSize || dispatchThreadId.y >= gFFTSize)
    {
        return;
    }

    uint2 coordinate = dispatchThreadId.xy;
    int2 centeredIndex = int2(coordinate) - int(gFFTSize / 2u);
    float deltaK = 2.0f * kOceanPi / gFFTPatchLength;
    float2 waveVector = float2(centeredIndex) * deltaK;
    float waveNumber = length(waveVector);
    float omega = sqrt(kOceanGravity * waveNumber);
    float phase = omega * gFFTTime;
    float2 positivePhase = float2(cos(phase), sin(phase));
    float2 negativePhase = float2(positivePhase.x, -positivePhase.y);

    float4 initial = gInitialSpectrum.Load(int3(coordinate, 0));
    float2 h0 = initial.xy;
    float2 conjugateH0Minus = float2(initial.z, -initial.w);
    float2 heightSpectrum =
        ComplexMultiply(h0, positivePhase) +
        ComplexMultiply(conjugateH0Minus, negativePhase);

    float inverseWaveNumber = waveNumber > 0.0001f ? rcp(waveNumber) : 0.0f;
    float2 minusIHeight = float2(heightSpectrum.y, -heightSpectrum.x);
    float2 iHeight = float2(-heightSpectrum.y, heightSpectrum.x);
    float2 displacementX =
        minusIHeight * waveVector.x * inverseWaveNumber * gFFTChoppiness;
    float2 displacementZ =
        minusIHeight * waveVector.y * inverseWaveNumber * gFFTChoppiness;
    float2 slopeX = iHeight * waveVector.x;
    float2 slopeZ = iHeight * waveVector.y;

    // Intrinsic derivatives of the potential displacement. Choppiness is
    // applied once, alongside the rendered horizontal displacement.
    float2 dDxDx = heightSpectrum * (waveVector.x * waveVector.x * inverseWaveNumber);
    float2 dDxDz = heightSpectrum * (waveVector.x * waveVector.y * inverseWaveNumber);
    float2 dDzDz = heightSpectrum * (waveVector.y * waveVector.y * inverseWaveNumber);

    // IFFT input is ifftshifted and bit-reversed here. Both axes are
    // reversed up front; horizontal and vertical DIT stages can then share CS.
    uint2 unshiftedCoordinate =
        (coordinate + uint2(gFFTSize / 2u, gFFTSize / 2u)) % gFFTSize;
    uint2 fftCoordinate = uint2(
        OceanBitReverse(unshiftedCoordinate.x),
        OceanBitReverse(unshiftedCoordinate.y));

    gSpectrumA[fftCoordinate] = float4(heightSpectrum, displacementX);
    gSpectrumB[fftCoordinate] = float4(displacementZ, slopeX);
    gSpectrumC[fftCoordinate] = float4(slopeZ, dDxDx);
    gSpectrumD[fftCoordinate] = float4(dDxDz, dDzDz);
    gEvolvedSpectrumDebug[coordinate] =
        float4(heightSpectrum, length(heightSpectrum), waveNumber);
}
