Texture2D sceneTex : register(t0);
SamplerState samp : register(s0);

cbuffer BloomParam : register(b0)
{
    float threshold;
    float intensity;
    float2 padding;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    float3 color = sceneTex.Sample(samp, input.uv).rgb;

    // 1. 輝度を計算
    float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
    
    // 2. 完全に真っ黒な部分での除算を防ぐための極小値
    float epsilon = 0.0001f;

    // 【ここが修正ポイント】
    // step(0か1)ではなく、「閾値を超えた分だけ」を滑らかに取り出す
    // Soft Thresholding という手法の簡易版です
    
    // 輝度から閾値を引いて、0未満なら0にする（マイナスにはしない）
    float contribution = max(0.0f, luminance - threshold);
    
    // 元の明るさに応じて正規化（色が変に白飛びするのを防ぐ）
    contribution /= max(luminance, epsilon);

    // 3. 抽出した明るさを元の色に掛ける
    // これにより、閾値ギリギリのピクセルは「うっすら」光り、
    // 閾値を大きく超えたピクセルは「強く」光るようになります。
    float3 extractColor = color * contribution;

    return float4(extractColor, 1.0f);
}