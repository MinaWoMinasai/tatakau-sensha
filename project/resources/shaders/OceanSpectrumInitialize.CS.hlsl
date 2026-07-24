#include "OceanFFTCommon.hlsli"

RWTexture2D<float4> gInitialSpectrum : register(u0);

float PhillipsSpectrum(float2 waveVector)
{
    float waveNumber = length(waveVector);
    if (waveNumber < 0.0001f)
    {
        return 0.0f;
    }

    float2 waveDirection = waveVector / waveNumber;
    float2 windDirection = normalize(gFFTWindDirection + float2(0.0001f, 0.0001f));
    float alignment = dot(waveDirection, windDirection);
    float directionalWeight = alignment * alignment *
        lerp(0.07f, 1.0f, step(0.0f, alignment));
    float largestWave = gFFTWindSpeed * gFFTWindSpeed / kOceanGravity;
    float k2 = waveNumber * waveNumber;
    float k4 = k2 * k2;
    float dampingLength = largestWave * 0.001f;
    float lowFrequencyCutoff = exp(
        -1.0f / max(k2 * largestWave * largestWave, 0.000001f));
    float highFrequencyCutoff = exp(-k2 * dampingLength * dampingLength);
    return gFFTAmplitude * lowFrequencyCutoff * highFrequencyCutoff *
        directionalWeight / max(k4, 0.000001f);
}

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
    float amplitude = sqrt(max(PhillipsSpectrum(waveVector), 0.0f) * 0.5f) * deltaK;

    uint2 oppositeCoordinate = uint2(
        (gFFTSize - coordinate.x) % gFFTSize,
        (gFFTSize - coordinate.y) % gFFTSize);
    int2 oppositeCenteredIndex =
        int2(oppositeCoordinate) - int(gFFTSize / 2u);
    float2 oppositeWaveVector = float2(oppositeCenteredIndex) * deltaK;
    float oppositeAmplitude =
        sqrt(max(PhillipsSpectrum(oppositeWaveVector), 0.0f) * 0.5f) * deltaK;

    float2 h0 = OceanGaussian(coordinate) * amplitude;
    float2 h0Minus = OceanGaussian(oppositeCoordinate) * oppositeAmplitude;
    if (all(centeredIndex == int2(0, 0)))
    {
        h0 = 0.0f.xx;
        h0Minus = 0.0f.xx;
    }
    gInitialSpectrum[coordinate] = float4(h0, h0Minus);
}
