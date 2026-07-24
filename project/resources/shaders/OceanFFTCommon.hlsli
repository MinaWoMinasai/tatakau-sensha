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
    uint gFFTPadding0;
    float2 gFFTPadding1;
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

#endif
