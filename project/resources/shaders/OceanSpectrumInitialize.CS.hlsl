#include "OceanFFTCommon.hlsli"

RWTexture2D<float4> gInitialSpectrum : register(u0);
RWTexture2D<float4> gSpectrumDebug : register(u1);

float PhillipsSpectrum(
    float2 waveVector,
    out float radialSpectrum,
    out float directionalSpectrum)
{
    float waveNumber = length(waveVector);
    if (waveNumber < 0.0001f)
    {
        radialSpectrum = 0.0f;
        directionalSpectrum = 0.0f;
        return 0.0f;
    }

    float2 waveDirection = waveVector / waveNumber;
    float2 windDirection = OceanSafeDirection(
        gFFTWindDirection,
        float2(1.0f, 0.0f));
    float alignment = dot(waveDirection, windDirection);
    directionalSpectrum = alignment * alignment *
        lerp(0.07f, 1.0f, step(0.0f, alignment));
    float largestWave = gFFTWindSpeed * gFFTWindSpeed / kOceanGravity;
    float k2 = waveNumber * waveNumber;
    float k4 = k2 * k2;
    float dampingLength = largestWave * 0.001f;
    float lowFrequencyCutoff = exp(
        -1.0f / max(k2 * largestWave * largestWave, 0.000001f));
    float highFrequencyCutoff = exp(-k2 * dampingLength * dampingLength);
    radialSpectrum =
        kPhillipsAmplitude * gFFTAmplitude *
        lowFrequencyCutoff * highFrequencyCutoff /
        max(k4, 0.000001f);
    return radialSpectrum * directionalSpectrum;
}

float EvaluateWaveNumberSpectrum(
    float2 waveVector,
    out float radialSpectrum,
    out float directionalSpectrum,
    out float directionalNormalizationError)
{
    directionalNormalizationError = 0.0f;
    if (gFFTSpectrumModel == 0u)
    {
        return PhillipsSpectrum(
            waveVector,
            radialSpectrum,
            directionalSpectrum);
    }

    float waveNumber = length(waveVector);
    if (waveNumber < 0.0001f)
    {
        radialSpectrum = 0.0f;
        directionalSpectrum = 0.0f;
        return 0.0f;
    }

    float angularFrequency = sqrt(kOceanGravity * waveNumber);
    float absoluteAngle = atan2(waveVector.y, waveVector.x);
    radialSpectrum = OceanJonswapRadialSpectrum(angularFrequency);
    directionalSpectrum = OceanNormalizedDirectionalSpectrum(
        angularFrequency,
        absoluteAngle,
        directionalNormalizationError);
    float angularFrequencyDerivative =
        0.5f * kOceanGravity / max(angularFrequency, 0.0001f);
    float waveNumberSpectrum =
        gFFTAmplitude * radialSpectrum * directionalSpectrum *
        angularFrequencyDerivative / waveNumber;
    return isfinite(waveNumberSpectrum)
        ? max(waveNumberSpectrum, 0.0f)
        : 0.0f;
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
    float radialSpectrum;
    float directionalSpectrum;
    float directionalNormalizationError;
    float waveNumberSpectrum = EvaluateWaveNumberSpectrum(
        waveVector,
        radialSpectrum,
        directionalSpectrum,
        directionalNormalizationError);

    // A normalized 2D IFFT computes (1 / N^2) sum(H[k] exp(i k x)).
    // Store N^2 times the physical Fourier-series coefficient so the final
    // explicit normalization preserves the continuous-spectrum discretization.
    // The complex Gaussian has E[|xi|^2] = 2. The 0.25 factor gives
    // E[|h0(k)|^2] = 0.5 P(k) deltaK^2; adding the +/-k evolution pair then
    // produces exactly one physical P(k) integral in expectation.
    float inverseTransformScale = float(gFFTSize * gFFTSize);
    float amplitude =
        sqrt(max(waveNumberSpectrum, 0.0f) * 0.25f) *
        deltaK * inverseTransformScale;

    uint2 oppositeCoordinate = uint2(
        (gFFTSize - coordinate.x) % gFFTSize,
        (gFFTSize - coordinate.y) % gFFTSize);
    int2 oppositeCenteredIndex =
        int2(oppositeCoordinate) - int(gFFTSize / 2u);
    float2 oppositeWaveVector = float2(oppositeCenteredIndex) * deltaK;
    float oppositeRadialSpectrum;
    float oppositeDirectionalSpectrum;
    float oppositeDirectionalNormalizationError;
    float oppositeWaveNumberSpectrum = EvaluateWaveNumberSpectrum(
        oppositeWaveVector,
        oppositeRadialSpectrum,
        oppositeDirectionalSpectrum,
        oppositeDirectionalNormalizationError);
    float oppositeAmplitude =
        sqrt(max(oppositeWaveNumberSpectrum, 0.0f) * 0.25f) *
        deltaK * inverseTransformScale;

    float2 h0 = OceanGaussian(coordinate) * amplitude;
    float2 h0Minus = OceanGaussian(oppositeCoordinate) * oppositeAmplitude;
    if (all(centeredIndex == int2(0, 0)))
    {
        h0 = 0.0f.xx;
        h0Minus = 0.0f.xx;
    }
    gInitialSpectrum[coordinate] = float4(h0, h0Minus);
    gSpectrumDebug[coordinate] = float4(
        radialSpectrum,
        directionalSpectrum,
        directionalNormalizationError,
        waveNumberSpectrum);
}
