Texture2D sceneTex : register(t0);
Texture2D bloomTex : register(t1);
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
    float padding;
};

// --- ヘルパー関数：ランダム ---
float Hash(float2 p)
{
    return frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453);
}

// --- 1. 座標変換：ブラウン管の歪み ---
float2 ApplyCRTDistortion(float2 baseUV)
{
    float2 offset = baseUV.yx / 2.0;
    return baseUV + baseUV * (offset * offset) * curvature;
}

// --- 2. 座標変換：グリッチ（ブロックノイズ） ---
float2 ApplyGlitch(float2 uv)
{
    if (glitchAmount <= 0.0f)
        return uv;

    // 格子状に分割 (縦横 12x12)
    float2 gridSize = float2(12.0, 12.0);
    float2 gridIndex = floor(uv * gridSize);
    float timeStep = floor(timer * 15.0f);
    
    float seed = Hash(gridIndex + timeStep);
    
    // 一定確率で縦横に飛ばす
    if (seed > 0.9f)
    {
        float2 rOffset = float2(Hash(float2(seed, timeStep)), Hash(float2(timeStep, seed))) - 0.5f;
        uv += rOffset * glitchAmount;
    }
    return uv;
}

// --- 3. 座標変換：うねうね波 ---
float2 ApplyWave(float2 uv)
{
    uv.x += sin(uv.y * 10.0f + timer * 2.0f) * distortionAmount;
    uv.y += cos(uv.x * 10.0f + timer * 2.0f) * distortionAmount;
    return uv;
}

// --- 4. カラー変換：反転・グレースケール ---
float3 ApplyColorModifiers(float3 color)
{
    if (isInverted > 0.5f)
        color = 1.0f - color;
    if (isGrayscale > 0.5f)
    {
        float gray = dot(color, float3(0.2126, 0.7152, 0.0722));
        color = float3(gray, gray, gray);
    }
    return color;
}

// --- 5. オーバーレイ：走査線・ノイズ ---
float3 ApplyOverlays(float3 color, float2 uv)
{
    // 走査線
    float scanline = sin(uv.y * scanlineFrequency + timer * 2.0f);
    color -= scanline * 0.1f * scanlineIntensity;
    
    // フィルムノイズ
    float n = Hash(uv + frac(timer));
    color += (n - 0.5f) * noiseIntensity;
    
    return color;
}

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

// --- メイン処理 ---
float4 main(PSInput input) : SV_TARGET
{
    // A. 座標の初期化
    float2 baseUV = input.uv * 2.0 - 1.0;
    
    // B. ブラウン管歪み適用
    float2 crtUV = ApplyCRTDistortion(baseUV);
    float2 texUV = (crtUV + 1.0) / 2.0;

    // C. 画面外判定
    if (texUV.x < 0.0 || texUV.x > 1.0 || texUV.y < 0.0 || texUV.y > 1.0)
    {
        return float4(0, 0, 0, 1);
    }

    // D. グリッチ・うねうね座標確定
    float2 postGlitchUV = ApplyGlitch(texUV);
    float2 finalUV = ApplyWave(postGlitchUV);

    // E. サンプリング（色収差）
    float2 shift = (postGlitchUV - 0.5f) * chromAbAmount;
    float3 scene, bloom;
    
    scene.r = sceneTex.Sample(samp, finalUV + shift).r;
    scene.g = sceneTex.Sample(samp, finalUV).g;
    scene.b = sceneTex.Sample(samp, finalUV - shift).b;
    
    bloom.r = bloomTex.Sample(samp, finalUV + shift).r;
    bloom.g = bloomTex.Sample(samp, finalUV).g;
    bloom.b = bloomTex.Sample(samp, finalUV - shift).b;

    // F. 合成とカラー加工
    float3 result = scene + bloom * intensity;
    result = ApplyColorModifiers(result);
    result = ApplyOverlays(result, texUV);

    // G. 仕上げ（角丸・ビネット）
    float2 edge = abs(crtUV);
    float mask = pow(saturate(1.0 - edge.x), borderSharp) * pow(saturate(1.0 - edge.y), borderSharp);
    result *= saturate(1.0 - (1.0 - mask) * 10.0);

    float dist = length(baseUV * 0.5);
    result *= pow(saturate(1.0 - dist * vignetteIntensity * vignetteScale), 0.8);

    return float4(result, 1.0f);
}