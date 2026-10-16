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

float4 main(PSInput input) : SV_TARGET
{
    uint width, height;
    sceneTex.GetDimensions(width, height);

    float2 texel = 1.0f / float2(width, height);

    float4 col = 0.0f;
    if (fullScreenBoxBlurBlend > 0.0f)
    {
        // 縦方向の5タップのボックスフィルタ。横方向のパスと合わせると
        // 正しい5x5の1/25ボックスフィルタになる。
        const int kBoxRadius = 2;
        const float kBoxWeight = 1.0f / 5.0f;
        [unroll]
        for (int y = -kBoxRadius; y <= kBoxRadius; ++y)
        {
            col += sceneTex.Sample(samp, input.uv + float2(0.0f, texel.y * y)) * kBoxWeight;
        }
    }
    else
    {
        // 元の隣り合うガウスのタップ2つを、バイリニアのサンプリングでまとめる。
        // 半径・正規化・強度は保ったまま、読み込みを9回から5回に減らす。
        // 横方向のパスもまったく同じカーネルを使う。
        const float pairedWeight12 = 0.19541357f + 0.11973994f;
        const float pairedWeight34 = 0.05293135f + 0.01688015f;
        const float pairedOffset12 = (0.19541357f + 2.0f * 0.11973994f) / pairedWeight12;
        const float pairedOffset34 = (3.0f * 0.05293135f + 4.0f * 0.01688015f) / pairedWeight34;
        col = sceneTex.Sample(samp, input.uv) * 0.23006997f;
        col += sceneTex.Sample(samp, input.uv + float2(0.0f, texel.y * pairedOffset12)) * pairedWeight12;
        col += sceneTex.Sample(samp, input.uv - float2(0.0f, texel.y * pairedOffset12)) * pairedWeight12;
        col += sceneTex.Sample(samp, input.uv + float2(0.0f, texel.y * pairedOffset34)) * pairedWeight34;
        col += sceneTex.Sample(samp, input.uv - float2(0.0f, texel.y * pairedOffset34)) * pairedWeight34;
    }

    float blurIntensity = (gaussianIntensity > 0.0f || fullScreenBoxBlurBlend > 0.0f) ? 1.0f : intensity;
    return float4(col.rgb * blurIntensity, col.a);
}
