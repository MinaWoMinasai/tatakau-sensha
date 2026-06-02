Texture2D sceneTex : register(t0);
SamplerState samp : register(s0);

cbuffer BloomParam : register(b0)
{
    float threshold;
    float intensity;
    float vignetteIntensity;
    float vignetteScale;
    float timer;
    float distortionAmount;
    float chromAbAmount;
    float isGrayscale;
    float isInverted;
    float noiseIntensity;
    float scanlineIntensity;
    float scanlineFrequency;
    float curvature;
    float borderSharp;
    float glitchAmount;
    float gaussianIntensity;
    float dissolveThreshold;
    float outlineWidth;
    float outlineThreshold;
    float boxBlurIntensity;
    float3 outlineColor;
    float outlineBloomIntensity;
    float outlineBloomWidth;
    float boxBlurRadius;
    float fullScreenBoxBlurBlend;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float CalculateGaussianWeight(int offset, float sigma)
{
    float x = (float)offset;
    return exp(-(x * x) / (2.0f * sigma * sigma));
}

float4 main(PSInput input) : SV_TARGET
{
    uint width, height;
    sceneTex.GetDimensions(width, height);

    float2 texel = 1.0f / float2(width, height);

    float4 col = 0.0f;
    if (fullScreenBoxBlurBlend > 0.0f)
    {
        // Horizontal 5-tap box filter. The vertical pass applies the same
        // 1/5 average, so the combined result is a true 5x5 1/25 box filter.
        const int kBoxRadius = 2;
        const float kBoxWeight = 1.0f / 5.0f;
        [unroll]
        for (int x = -kBoxRadius; x <= kBoxRadius; ++x)
        {
            col += sceneTex.Sample(samp, input.uv + float2(texel.x * x, 0.0f)) * kBoxWeight;
        }
    }
    else
    {
        // Gaussian kernel: calculate weights from the Gaussian function,
        // then normalize them so the convolution preserves brightness.
        const int kGaussianRadius = 4;
        const float kGaussianSigma = 1.75f;
        float weightSum = CalculateGaussianWeight(0, kGaussianSigma);
        [unroll]
        for (int i = 1; i <= kGaussianRadius; ++i)
        {
            weightSum += CalculateGaussianWeight(i, kGaussianSigma) * 2.0f;
        }

        col = sceneTex.Sample(samp, input.uv) * (CalculateGaussianWeight(0, kGaussianSigma) / weightSum);
        [unroll]
        for (int i = 1; i <= kGaussianRadius; ++i)
        {
            float weight = CalculateGaussianWeight(i, kGaussianSigma) / weightSum;
            col += sceneTex.Sample(samp, input.uv + float2(texel.x * i, 0.0f)) * weight;
            col += sceneTex.Sample(samp, input.uv - float2(texel.x * i, 0.0f)) * weight;
        }
    }

    float blurIntensity = (gaussianIntensity > 0.0f || fullScreenBoxBlurBlend > 0.0f) ? 1.0f : intensity;
    return float4(col.rgb * blurIntensity, col.a);
}
