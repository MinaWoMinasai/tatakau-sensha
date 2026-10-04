#ifndef BLOOM_PYRAMID_COMMON_HLSLI
#define BLOOM_PYRAMID_COMMON_HLSLI

Texture2D<float4> sourceTex : register(t0);
Texture2D<float4> auxiliaryTex : register(t1);
SamplerState linearClamp : register(s0);

cbuffer BloomPyramidPass : register(b0)
{
    uint sourceWidth;
    uint sourceHeight;
    float threshold;
    float softKnee;
    float scatter;
    float radius;
    uint materialEnabled;
    float waterBloomEnabled;
    float waterDiagnosticsEnabled;
    uint3 passPadding;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

#endif
