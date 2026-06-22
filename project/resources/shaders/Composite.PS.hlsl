Texture2D sceneTex : register(t0);
Texture2D bloomTex : register(t1);
Texture2D<float> depthTex : register(t2);
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
    float dissolveThreshold; // ディゾルブの進行度 (0~1)
    float outlineWidth; // アウトラインの太さ
    float outlineThreshold; // エッジ検出のしきい値
    float boxBlurIntensity;
    float3 outlineColor; // アウトラインの色
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

float2 ApplyShockwave(float2 uv)
{
    if (shockwaveStrength <= 0.0f || shockwaveWidth <= 0.0f)
    {
        return uv;
    }

    const float aspect = 1280.0f / 720.0f;
    float2 delta = uv - shockwaveCenter;
    float2 aspectDelta = float2(delta.x * aspect, delta.y);
    float distanceFromCenter = length(aspectDelta);
    float ring = 1.0f - saturate(abs(distanceFromCenter - shockwaveRadius) / shockwaveWidth);
    ring = ring * ring * (3.0f - 2.0f * ring);
    float2 direction = distanceFromCenter > 0.0001f ? aspectDelta / distanceFromCenter : float2(0.0f, 0.0f);
    direction.x /= aspect;
    return uv + direction * ring * shockwaveStrength;
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

float3 SampleBoxBlur(Texture2D tex, float2 uv, float radiusPixels)
{
    if (boxBlurIntensity <= 0.0f || radiusPixels <= 0.0f)
    {
        return tex.Sample(samp, uv).rgb;
    }

    float2 texelSize = float2(1.0f / 1280.0f, 1.0f / 720.0f);
    float2 offset = texelSize * radiusPixels;

    float3 sum = 0.0f;
    sum += tex.Sample(samp, uv + offset * float2(-1.0f, -1.0f)).rgb;
    sum += tex.Sample(samp, uv + offset * float2( 0.0f, -1.0f)).rgb;
    sum += tex.Sample(samp, uv + offset * float2( 1.0f, -1.0f)).rgb;
    sum += tex.Sample(samp, uv + offset * float2(-1.0f,  0.0f)).rgb;
    sum += tex.Sample(samp, uv).rgb;
    sum += tex.Sample(samp, uv + offset * float2( 1.0f,  0.0f)).rgb;
    sum += tex.Sample(samp, uv + offset * float2(-1.0f,  1.0f)).rgb;
    sum += tex.Sample(samp, uv + offset * float2( 0.0f,  1.0f)).rgb;
    sum += tex.Sample(samp, uv + offset * float2( 1.0f,  1.0f)).rgb;

    return sum / 9.0f;
}

float3 SampleRadialBlur(Texture2D tex, float2 uv)
{
	float3 original = tex.Sample(samp, uv).rgb;
	if (radialBlurIntensity <= 0.0f || radialBlurWidth <= 0.0f)
	{
		return original;
	}

	const int sampleCount = 10;
	float2 direction = uv - radialBlurCenter;
	float3 sum = 0.0f;
	[unroll]
	for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
	{
		float sampleStep = (float)sampleIndex / (float)(sampleCount - 1);
		float2 sampleUV = saturate(uv + direction * radialBlurWidth * sampleStep);
		sum += tex.Sample(samp, sampleUV).rgb;
	}
	return lerp(original, sum / (float)sampleCount, saturate(radialBlurIntensity));
}

float RestoreViewSpaceZ(float depth)
{
    return depthNearClip * depthFarClip /
        max(depthFarClip - depth * (depthFarClip - depthNearClip), 0.0001f);
}

// DepthBufferからView空間Zを復元し、奥行きの不連続を輪郭として抽出する。
float3 GetDepthOutline(float2 uv)
{
    if (depthOutlineEnabled <= 0.5f || outlineWidth <= 0.0f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    uint width;
    uint height;
    depthTex.GetDimensions(width, height);
    float2 texelSize = 1.0f / float2(width, height);
    float2 offset = outlineWidth * texelSize;
    float centerDepth = depthTex.Sample(samp, uv);
    if (centerDepth >= 0.99999f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    float centerViewZ = RestoreViewSpaceZ(centerDepth);
    float maxViewZDifference = 0.0f;
    const float2 directions[8] = {
        float2(-1.0f, -1.0f), float2(0.0f, -1.0f), float2(1.0f, -1.0f),
        float2(-1.0f,  0.0f),                       float2(1.0f,  0.0f),
        float2(-1.0f,  1.0f), float2(0.0f,  1.0f), float2(1.0f,  1.0f)
    };

    [unroll]
    for (int i = 0; i < 8; ++i)
    {
        float sampleDepth = depthTex.Sample(samp, uv + directions[i] * offset);
        float sampleViewZ = sampleDepth >= 0.99999f ? depthFarClip : RestoreViewSpaceZ(sampleDepth);
        maxViewZDifference = max(maxViewZDifference, abs(sampleViewZ - centerViewZ));
    }

    float edge = maxViewZDifference * depthOutlineScale;
    return edge > outlineThreshold ? outlineColor : float3(0.0f, 0.0f, 0.0f);
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
    float2 shockwaveUV = ApplyShockwave(postGlitchUV);
    float2 finalUV = ApplyWave(shockwaveUV);

    // E. サンプリング（色収差）
    float2 shift = (shockwaveUV - 0.5f) * chromAbAmount;
    float3 scene, bloom;
    
    scene.r = sceneTex.Sample(samp, finalUV + shift).r;
    scene.g = sceneTex.Sample(samp, finalUV).g;
    scene.b = sceneTex.Sample(samp, finalUV - shift).b;
    
    bloom.r = bloomTex.Sample(samp, finalUV + shift).r;
    bloom.g = bloomTex.Sample(samp, finalUV).g;
    bloom.b = bloomTex.Sample(samp, finalUV - shift).b;
    
    // 1. ディゾルブ処理。Hashをノイズマスクとして扱い、時間で任意に流せる。
	float2 dissolveUV = texUV * max(dissolveNoiseScale, 1.0f);
	float dissolveNoise = Hash(dissolveUV + timer * dissolveNoiseSpeed);
    if (dissolveNoise < dissolveThreshold)
    {
        discard; // しきい値より低いピクセルは描画をスキップ
    }
    
    // F. 合成とカラー加工
	float3 sceneColor = SampleRadialBlur(sceneTex, finalUV);
    float3 boxBlurredScene = SampleBoxBlur(sceneTex, finalUV, boxBlurRadius);
    sceneColor = lerp(sceneColor, boxBlurredScene, saturate(boxBlurIntensity));
    float3 blurredColor = bloomTex.Sample(samp, texUV).rgb; // PostDrawでシーン全体をぼかして渡したもの
    
    float3 result;

    if (fullScreenBoxBlurBlend > 0.0f)
    {
    // 【5x5 Box Filterモード】
    // シーン全体を5x5平均でぼかしたものをlerpで混ぜる
        result = lerp(sceneColor, blurredColor, fullScreenBoxBlurBlend);
    }
    else if (gaussianIntensity > 0.0f)
    {
    // 【Gaussian Blurモード】
    // シーン全体をガウシアンぼかししたものをlerpで混ぜる
        result = lerp(sceneColor, blurredColor, gaussianIntensity);
    }
    else
    {
		sceneColor = radialBlurIntensity > 0.0f ? SampleRadialBlur(sceneTex, finalUV) : scene;
        sceneColor = lerp(sceneColor, boxBlurredScene, saturate(boxBlurIntensity));
        blurredColor = bloom;
    // 【ブルームモード】
    // 元の絵に、高輝度部分をぼかしたものを「加算」する
    // ここで intensity をかけることで、光の強さを制御できます
        result = sceneColor + (blurredColor * intensity);
    
        // 2. アウトライン処理 (エッジ検出)
        // 改良版アウトラインの適用
        float3 outline = GetDepthOutline(finalUV);
        if (any(outline)) // outlineが黒でない場合
        {
            result = outline;
        }
    }
    
    // ブルームとしても使いたい場合は、加算なども考慮
    // result += blurredColor * intensity;
    result = ApplyColorModifiers(result);
    result = ApplyOverlays(result, texUV);
	if (dissolveThreshold > 0.0f && dissolveEdgeWidth > 0.0f)
	{
		float dissolveEdge = 1.0f - smoothstep(
			dissolveThreshold,
			dissolveThreshold + dissolveEdgeWidth,
			dissolveNoise);
		result += dissolveEdge * dissolveEdgeColor;
	}

    // G. 仕上げ（角丸・ビネット）
    float2 edge = abs(crtUV);
    float mask = pow(saturate(1.0 - edge.x), borderSharp) * pow(saturate(1.0 - edge.y), borderSharp);
    result *= saturate(1.0 - (1.0 - mask) * 10.0);

    float dist = length(baseUV * 0.5);
    result *= pow(saturate(1.0 - dist * vignetteIntensity * vignetteScale), 0.8);

    return float4(result, 1.0f);
}
