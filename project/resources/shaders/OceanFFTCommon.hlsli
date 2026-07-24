#ifndef OCEAN_FFT_COMMON_HLSLI
#define OCEAN_FFT_COMMON_HLSLI

cbuffer OceanFFTParameters : register(b0)
{
    uint gFFTSize;
    float gFFTPatchLength;
    float gFFTTime;
    float gFFTAmplitude;
    float2 gFFTWindDirection;
    float gFFTWindSpeed;
    float gFFTChoppiness;
    uint gFFTSeed;
    uint gFFTSpectrumModel;
    float gFFTFetch;
    float gFFTGamma;
    float gFFTLowFrequencyDamping;
    float gFFTHighFrequencyDamping;
    float2 gFFTSwellDirection;
    float gFFTSwellAmount;
    float gFFTOppositeWaveSuppression;
    float2 gFFTPadding;
};

cbuffer OceanFFTDispatch : register(b1)
{
    uint gFFTStage;
    uint gFFTDirection;
    uint gFFTDispatchSize;
    uint gFFTDispatchPadding;
};

static const float kOceanPi = 3.14159265358979323846f;
static const float kOceanGravity = 9.81f;
static const float kPhillipsAmplitude = 0.0008f;
static const uint kDirectionIntegrationSamples = 64u;
static const uint kDirectionValidationSamples = 128u;

float2 ComplexMultiply(float2 a, float2 b)
{
    return float2(
        a.x * b.x - a.y * b.y,
        a.x * b.y + a.y * b.x);
}

uint OceanHash(uint value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

float OceanHash01(uint2 coordinate, uint salt)
{
    uint value = OceanHash(
        coordinate.x * 0x9e3779b9u ^
        coordinate.y * 0x85ebca6bu ^
        gFFTSeed ^
        salt);
    return (float(value) + 0.5f) / 4294967296.0f;
}

float2 OceanGaussian(uint2 coordinate)
{
    float u1 = max(OceanHash01(coordinate, 0x68bc21ebu), 0.000001f);
    float u2 = OceanHash01(coordinate, 0x02e5be93u);
    float radius = sqrt(-2.0f * log(u1));
    float angle = 2.0f * kOceanPi * u2;
    return radius * float2(cos(angle), sin(angle));
}

uint OceanBitReverse(uint value)
{
    uint log2Size = firstbithigh(gFFTSize);
    return reversebits(value) >> (32u - log2Size);
}

float2 OceanSafeDirection(float2 direction, float2 fallbackDirection)
{
    float lengthSquared = dot(direction, direction);
    return lengthSquared > 0.000001f
        ? direction * rsqrt(lengthSquared)
        : fallbackDirection;
}

float OceanWrapAngle(float angle)
{
    return atan2(sin(angle), cos(angle));
}

float OceanSechSquared(float value)
{
    float clampedValue = clamp(value, -20.0f, 20.0f);
    float inverseCosh = rcp(cosh(clampedValue));
    return inverseCosh * inverseCosh;
}

float OceanJonswapAlpha()
{
    float windSpeed = max(gFFTWindSpeed, 0.1f);
    float fetch = max(gFFTFetch, 1.0f);
    float nondimensionalFetch =
        max(windSpeed * windSpeed / (fetch * kOceanGravity), 0.00000001f);
    return 0.076f * pow(nondimensionalFetch, 0.22f);
}

float OceanJonswapPeakAngularFrequency()
{
    float windSpeed = max(gFFTWindSpeed, 0.1f);
    float fetch = max(gFFTFetch, 1.0f);
    return 22.0f * pow(
        kOceanGravity * kOceanGravity / (windSpeed * fetch),
        1.0f / 3.0f);
}

float OceanJonswapRadialSpectrum(float angularFrequency)
{
    float omega = max(angularFrequency, 0.0001f);
    float omegaPeak = max(OceanJonswapPeakAngularFrequency(), 0.0001f);
    float sigma = omega <= omegaPeak ? 0.07f : 0.09f;
    float difference = omega - omegaPeak;
    float peakExponent = exp(
        -(difference * difference) /
        max(2.0f * sigma * sigma * omegaPeak * omegaPeak, 0.000001f));
    float peakEnhancement = pow(max(gFFTGamma, 1.0f), peakExponent);
    float frequencyRatio = omegaPeak / omega;
    float baseCutoff = exp(-1.25f * pow(frequencyRatio, 4.0f));
    float lowDamping = exp(
        -max(gFFTLowFrequencyDamping, 0.0f) * pow(frequencyRatio, 4.0f));
    float highRatio = omega / omegaPeak;
    float highDamping = exp(
        -max(gFFTHighFrequencyDamping, 0.0f) * highRatio * highRatio);
    float spectrum =
        OceanJonswapAlpha() * kOceanGravity * kOceanGravity /
        pow(omega, 5.0f) *
        baseCutoff * peakEnhancement * lowDamping * highDamping;
    return isfinite(spectrum) ? max(spectrum, 0.0f) : 0.0f;
}

float OceanDonelanBeta(float frequencyRatio)
{
    float ratio = max(frequencyRatio, 0.0001f);
    if (ratio < 0.95f)
    {
        return 2.61f * pow(ratio, 1.3f);
    }
    if (ratio < 1.6f)
    {
        return 2.28f * pow(ratio, -1.3f);
    }

    float epsilon =
        0.8393f * exp(-0.567f * log(ratio * ratio)) - 0.4f;
    return pow(10.0f, epsilon);
}

float OceanDonelanBanner(
    float angularFrequency,
    float directionOffset)
{
    float omegaPeak = max(OceanJonswapPeakAngularFrequency(), 0.0001f);
    float beta = max(
        OceanDonelanBeta(angularFrequency / omegaPeak),
        0.0001f);
    float normalization = max(tanh(beta * kOceanPi), 0.0001f);
    return 0.5f * beta / normalization *
        OceanSechSquared(beta * OceanWrapAngle(directionOffset));
}

float OceanOppositeWeight(float directionOffset)
{
    float forwardWeight =
        saturate(0.5f + 0.5f * cos(OceanWrapAngle(directionOffset)));
    return lerp(
        1.0f,
        forwardWeight * forwardWeight,
        saturate(gFFTOppositeWaveSuppression));
}

float OceanDirectionalRaw(
    float angularFrequency,
    float absoluteAngle)
{
    float2 windDirection = OceanSafeDirection(
        gFFTWindDirection,
        float2(1.0f, 0.0f));
    float2 swellDirection = OceanSafeDirection(
        gFFTSwellDirection,
        windDirection);
    float windAngle = atan2(windDirection.y, windDirection.x);
    float swellAngle = atan2(swellDirection.y, swellDirection.x);
    float windOffset = OceanWrapAngle(absoluteAngle - windAngle);
    float swellOffset = OceanWrapAngle(absoluteAngle - swellAngle);
    float frequencyRatio = angularFrequency /
        max(OceanJonswapPeakAngularFrequency(), 0.0001f);
    float swellExponent =
        16.0f * pow(tanh(rcp(max(frequencyRatio, 0.0001f))), 2.0f) *
        saturate(gFFTSwellAmount);
    float swellShape = pow(
        max(abs(cos(0.5f * swellOffset)), 0.000001f),
        2.0f * swellExponent);

    float windLobe =
        OceanDonelanBanner(angularFrequency, windOffset) *
        OceanOppositeWeight(windOffset);
    float swellLobe =
        OceanDonelanBanner(angularFrequency, swellOffset) *
        swellShape *
        OceanOppositeWeight(swellOffset);
    return lerp(windLobe, swellLobe, saturate(gFFTSwellAmount));
}

float OceanDirectionalIntegral(
    float angularFrequency,
    uint sampleCount)
{
    float angleStep = 2.0f * kOceanPi / float(sampleCount);
    float integral = 0.0f;
    for (uint sampleIndex = 0u; sampleIndex < sampleCount; ++sampleIndex)
    {
        float angle = -kOceanPi +
            (float(sampleIndex) + 0.5f) * angleStep;
        integral += OceanDirectionalRaw(angularFrequency, angle);
    }
    return max(integral * angleStep, 0.000001f);
}

float OceanNormalizedDirectionalSpectrum(
    float angularFrequency,
    float absoluteAngle,
    out float normalizationError)
{
    float integral = OceanDirectionalIntegral(
        angularFrequency,
        kDirectionIntegrationSamples);
    float directional =
        OceanDirectionalRaw(angularFrequency, absoluteAngle) / integral;
    float validationIntegral = OceanDirectionalIntegral(
        angularFrequency,
        kDirectionValidationSamples) / integral;
    normalizationError = abs(validationIntegral - 1.0f);
    return isfinite(directional) ? max(directional, 0.0f) : 0.0f;
}

#endif
