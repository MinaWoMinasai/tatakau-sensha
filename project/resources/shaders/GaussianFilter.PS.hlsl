Texture2D sourceTexture : register(t0);
SamplerState textureSampler : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

// 課題確認用の独立したGaussian Filter。
// 現在のゲーム描画には未接続だが、起動時にPSOを生成してコンパイルを検証する。
float CalculateGaussianWeight(float2 offset, float sigma)
{
    const float kPi = 3.14159265f;
    float squaredDistance = dot(offset, offset);
    return exp(-squaredDistance / (2.0f * sigma * sigma)) /
        (2.0f * kPi * sigma * sigma);
}

float4 main(PSInput input) : SV_TARGET
{
    uint textureWidth;
    uint textureHeight;
    sourceTexture.GetDimensions(textureWidth, textureHeight);
    float2 texelSize = 1.0f / float2(textureWidth, textureHeight);

    const int kKernelRadius = 2;
    const float kSigma = 1.0f;
    float4 weightedColorSum = 0.0f;
    float gaussianWeightSum = 0.0f;

    [unroll]
    for (int y = -kKernelRadius; y <= kKernelRadius; ++y)
    {
        [unroll]
        for (int x = -kKernelRadius; x <= kKernelRadius; ++x)
        {
            float2 sampleOffset = float2((float)x, (float)y);
            float gaussianWeight = CalculateGaussianWeight(sampleOffset, kSigma);
            weightedColorSum += sourceTexture.Sample(
                textureSampler,
                input.uv + sampleOffset * texelSize) * gaussianWeight;
            gaussianWeightSum += gaussianWeight;
        }
    }

    return weightedColorSum / gaussianWeightSum;
}
