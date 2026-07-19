Texture2D sceneTex : register(t0);
Texture2D bloomTex : register(t1);
Texture2D<float> depthTex : register(t2);
Texture2D normalTex : register(t3);
Texture2D ssrTex : register(t4);
Texture2D materialTex : register(t5);
Texture2D ssaoTex : register(t6);
Texture2D motionVectorTex : register(t7);
SamplerState samp : register(s0);

#include "PostEffectCommon.hlsli"

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

float GetLinearDepth(float2 uv)
{
    float depth = depthTex.Sample(samp, uv);
    return depth >= 0.99999f ? depthFarClip : RestoreViewSpaceZ(depth);
}

float GetDepthEdgeIntensity(float2 uv)
{
    if (outlineWidth <= 0.0f)
    {
        return 0.0f;
    }

    uint width;
    uint height;
    depthTex.GetDimensions(width, height);
    float2 texelSize = 1.0f / float2(width, height);
    float2 offset = outlineWidth * texelSize;
    float centerDepth = depthTex.Sample(samp, uv);
    if (centerDepth >= 0.99999f)
    {
        return 0.0f;
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

    return maxViewZDifference * depthOutlineScale;
}

// DepthBufferからView空間Zを復元し、奥行きの不連続を輪郭として抽出する。
float3 GetDepthOutline(float2 uv)
{
    if (depthOutlineEnabled <= 0.5f)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    float edge = GetDepthEdgeIntensity(uv);
    return edge > outlineThreshold ? outlineColor : float3(0.0f, 0.0f, 0.0f);
}

float3 GetDepthDerivedNormal(float2 uv)
{
    float centerDepth = depthTex.Sample(samp, uv);
    if (centerDepth >= 0.99999f)
    {
        return float3(0.5f, 0.5f, 1.0f);
    }

    uint width;
    uint height;
    depthTex.GetDimensions(width, height);
    float2 texelSize = 1.0f / float2(width, height);

    float leftZ = GetLinearDepth(uv - float2(texelSize.x, 0.0f));
    float rightZ = GetLinearDepth(uv + float2(texelSize.x, 0.0f));
    float upZ = GetLinearDepth(uv - float2(0.0f, texelSize.y));
    float downZ = GetLinearDepth(uv + float2(0.0f, texelSize.y));

    float2 slope = float2(rightZ - leftZ, downZ - upZ) * depthNormalScale;
    float3 normal = normalize(float3(-slope.x, slope.y, 1.0f));
    return normal * 0.5f + 0.5f;
}

float2 GetSSAOPoissonOffset(int index)
{
    if (index == 0) return float2(-0.94201624f, -0.39906216f);
    if (index == 1) return float2( 0.94558609f, -0.76890725f);
    if (index == 2) return float2(-0.09418410f, -0.92938870f);
    if (index == 3) return float2( 0.34495938f,  0.29387760f);
    if (index == 4) return float2(-0.91588581f,  0.45771432f);
    if (index == 5) return float2(-0.81544232f, -0.87912464f);
    if (index == 6) return float2(-0.38277543f,  0.27676845f);
    if (index == 7) return float2( 0.97484398f,  0.75648379f);
    if (index == 8) return float2( 0.44323325f, -0.97511554f);
    if (index == 9) return float2( 0.53742981f, -0.47373420f);
    if (index == 10) return float2(-0.26496911f, -0.41893023f);
    if (index == 11) return float2( 0.79197514f,  0.19090188f);
    if (index == 12) return float2(-0.24188840f,  0.99706507f);
    if (index == 13) return float2(-0.81409955f,  0.91437590f);
    if (index == 14) return float2( 0.19984126f,  0.78641367f);
    return float2(0.14383161f, -0.14100790f);
}

float GetSSAO(float2 uv)
{
    if (ssaoEnabled <= 0.5f)
    {
        return 1.0f;
    }

    float centerDepth = depthTex.Sample(samp, uv);
    if (centerDepth >= 0.99999f)
    {
        return 1.0f;
    }

    float4 centerNormalSample = normalTex.Sample(samp, uv);

    uint width;
    uint height;
    depthTex.GetDimensions(width, height);
    float2 texelSize = 1.0f / float2(width, height);

    float centerViewZ = RestoreViewSpaceZ(centerDepth);
    float3 centerNormal = DecodeNormalTarget(centerNormalSample);
    float radiusPixels = max(ssaoRadius, 0.0f);
    float distanceFalloff = max(ssaoDistanceFalloff, 0.001f);
    int sampleCount = clamp((int)ssaoSampleCount, 1, 16);

    float randomAngle = Hash(floor(uv * float2(width, height)) * 0.25f) * 6.2831853f;
    float s = sin(randomAngle);
    float c = cos(randomAngle);
    float2x2 rotation = float2x2(c, -s, s, c);

    float occlusion = 0.0f;
    float validSamples = 0.0f;

    [unroll]
    for (int i = 0; i < 16; ++i)
    {
        if (i >= sampleCount)
        {
            continue;
        }

        float sampleStep = ((float)i + 0.5f) / (float)sampleCount;
        float2 offset = mul(GetSSAOPoissonOffset(i), rotation) * (0.35f + sampleStep * 0.65f);
        float2 sampleUV = uv + offset * texelSize * radiusPixels;
        if (any(sampleUV < 0.0f) || any(sampleUV > 1.0f))
        {
            continue;
        }

        float sampleDepth = depthTex.Sample(samp, sampleUV);
        if (sampleDepth >= 0.99999f)
        {
            continue;
        }

        float sampleViewZ = RestoreViewSpaceZ(sampleDepth);
        float depthDelta = centerViewZ - sampleViewZ;
        float frontWeight = smoothstep(ssaoBias, ssaoBias + distanceFalloff * 0.18f, depthDelta);
        float rangeWeight = 1.0f - saturate(abs(depthDelta) / distanceFalloff);

        float4 sampleNormalSample = normalTex.Sample(samp, sampleUV);
        float3 sampleNormal = DecodeNormalTarget(sampleNormalSample);
        float normalDifference = 1.0f - saturate(dot(centerNormal, sampleNormal));
        float normalWeight = lerp(1.0f, normalDifference, saturate(ssaoNormalInfluence));

        occlusion += frontWeight * rangeWeight * normalWeight;
        validSamples += 1.0f;
    }

    if (validSamples <= 0.0f)
    {
        return 1.0f;
    }

    float ao = 1.0f - saturate((occlusion / validSamples) * max(ssaoIntensity, 0.0f));
    return pow(saturate(ao), max(ssaoPower, 0.001f));
}

float3 ApplyDepthFog(float3 color, float2 uv)
{
    if (depthFogEnabled <= 0.5f)
    {
        return color;
    }

    float depth = depthTex.Sample(samp, uv);
    if (depth >= 0.99999f)
    {
        return color;
    }

    float viewZ = RestoreViewSpaceZ(depth);
    float fogRange = max(depthFogEnd - depthFogStart, 0.001f);
    float fogDistance = max(viewZ - depthFogStart, 0.0f);
    float linearFog = saturate(fogDistance / fogRange);
    float expFog = 1.0f - exp(-fogDistance * max(depthFogDensity, 0.0f));
    float fogFactor = saturate(max(linearFog, expFog) * saturate(depthFogMaxOpacity));
    return lerp(color, depthFogColor, fogFactor);
}

float3 GetRenderDebugColor(float2 uv)
{
    if (renderDebugMode < 1.5f)
    {
        return ApplyToneMapping(sceneTex.Sample(samp, uv).rgb);
    }

    if (renderDebugMode < 2.5f)
    {
        return ApplyToneMapping(bloomTex.Sample(samp, uv).rgb);
    }

    if (renderDebugMode < 3.5f)
    {
        float viewZ = GetLinearDepth(uv);
        float normalizedDepth = saturate(viewZ / max(linearDepthDebugRange, 0.001f));
        return normalizedDepth.xxx;
    }

    if (renderDebugMode < 4.5f)
    {
        float edge = saturate(GetDepthEdgeIntensity(uv));
        return edge.xxx;
    }

    if (renderDebugMode < 5.5f)
    {
        return GetDepthDerivedNormal(uv);
    }

    if (renderDebugMode < 6.5f)
    {
        return normalTex.Sample(samp, uv).rgb;
    }

    if (renderDebugMode < 7.5f)
    {
        return ssaoTex.Sample(samp, uv).r.xxx;
    }

    if (renderDebugMode < 8.5f)
    {
        float4 ssr = ssrTex.Sample(samp, uv);
        return ApplyToneMapping(ssr.rgb * ssr.a * max(ssrIntensity, 0.0f));
    }

    if (renderDebugMode < 9.5f)
    {
        return normalTex.Sample(samp, uv).a.xxx;
    }

    float4 materialSample = materialTex.Sample(samp, uv);
    if (renderDebugMode < 10.5f)
    {
        return materialSample.r.xxx;
    }

    if (renderDebugMode < 11.5f)
    {
        return materialSample.g.xxx;
    }

    if (renderDebugMode < 12.5f)
    {
        return materialSample.b.xxx;
    }

    if (renderDebugMode < 13.5f)
    {
        return materialSample.a.xxx;
    }

    if (renderDebugMode < 14.5f)
    {
        if (motionVectorEnabled <= 0.5f)
        {
            return float3(1.0f, 0.12f, 0.04f);
        }

        float2 motionVector = motionVectorTex.Sample(samp, uv).rg;
        float2 debugMotion = motionVector * max(motionVectorDebugScale, 1.0f);
        return saturate(float3(0.5f + debugMotion.x, 0.5f - debugMotion.y, 0.5f + length(debugMotion)));
    }

    if (renderDebugMode < 15.5f)
    {
        if (motionVectorEnabled <= 0.5f)
        {
            return float3(1.0f, 0.12f, 0.04f);
        }

        float2 motionVector = motionVectorTex.Sample(samp, uv).rg;
        float motionSignal = max(abs(motionVector.x), abs(motionVector.y));
        if (motionSignal < 0.0001f)
        {
            return float3(1.0f, 0.0f, 1.0f);
        }

        return float3(saturate(motionVector), 1.0f);
    }

    return ApplyToneMapping(sceneTex.Sample(samp, uv).rgb);
}
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

    if (renderDebugMode > 0.5f)
    {
        return float4(GetRenderDebugColor(input.uv), 1.0f);
    }

    // D. グリッチ・うねうね座標確定
    float2 postGlitchUV = ApplyGlitch(texUV);
    float2 shockwaveUV = ApplyShockwave(postGlitchUV);
    float2 finalUV = ApplyWave(shockwaveUV);
    float screenSpaceAO = ssaoTex.Sample(samp, finalUV).r;
    float4 screenSpaceReflection = ssrTex.Sample(samp, finalUV);
    float screenSpaceReflectionBlend = saturate(screenSpaceReflection.a * max(ssrIntensity, 0.0f));
    float4 materialSample = materialTex.Sample(samp, finalUV);
    float materialDrivenRoughnessFade = 1.0f - smoothstep(0.58f, 0.94f, materialSample.r);
    screenSpaceReflectionBlend *= lerp(1.0f, materialDrivenRoughnessFade, saturate(materialSample.a));

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
    sceneColor *= screenSpaceAO;
    sceneColor = lerp(sceneColor, screenSpaceReflection.rgb, screenSpaceReflectionBlend);
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
        sceneColor *= screenSpaceAO;
        sceneColor = lerp(sceneColor, screenSpaceReflection.rgb, screenSpaceReflectionBlend);
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

    result = ApplyDepthFog(result, finalUV);
    
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

    result = ApplyToneMapping(result);
    return float4(result, 1.0f);
}
