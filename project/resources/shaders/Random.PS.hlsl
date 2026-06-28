Texture2D inputTexture : register(t0);
SamplerState textureSampler : register(s0);

// BloomParamと同じ定数バッファを使い、末尾のRandom課題用パラメータを参照する。
cbuffer BloomParam : register(b0)
{
    float threshold;
    float intensity;
    float vignetteIntensity;
    float vignetteScale;
    float time;
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
    float depthOutlineEnabled;
    float depthNearClip;
    float depthFarClip;
    float depthOutlineScale;
    float2 shockwaveCenter;
    float shockwaveRadius;
    float shockwaveWidth;
    float shockwaveStrength;
    float3 shockwavePadding;
    float2 radialBlurCenter;
    float radialBlurWidth;
    float radialBlurIntensity;
    float3 dissolveEdgeColor;
    float dissolveEdgeWidth;
    float dissolveNoiseScale;
    float dissolveNoiseSpeed;
    float2 postEffectPadding;
    float randomIntensity;
    float randomScale;
    float randomTimeScale;
    float randomGrayscalePreview;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

// 2次元のSeedをGPU上で0～1の疑似乱数へ変換する。
// 課題資料で求められるrand2dTo1dを明示的な独立関数として実装する。
float rand2dTo1d(float2 seed)
{
    const float2 randomVector = float2(12.9898f, 78.233f);
    return frac(sin(dot(seed, randomVector)) * 43758.5453f);
}

float4 main(PSInput input) : SV_TARGET
{
    // timeをSeedへ加えることで、毎フレーム異なるGPU乱数を生成する。
    float timeSeed = time * randomTimeScale;
    float2 pixelSeed = floor(input.uv * max(randomScale, 1.0f));
    float randomValue = rand2dTo1d(pixelSeed + float2(timeSeed, timeSeed * 1.6180339f));

    // 確認用：生成した乱数を白黒（グレースケール）で直接出力する。
    if (randomGrayscalePreview > 0.5f)
    {
        return float4(randomValue.xxx, 1.0f);
    }

    // 最終課題：乱数結果を入力画像へ乗算する。
    float4 inputColor = inputTexture.Sample(textureSampler, input.uv);
    float randomMultiplier = lerp(1.0f, randomValue, saturate(randomIntensity));
    return float4(inputColor.rgb * randomMultiplier, inputColor.a);
}
