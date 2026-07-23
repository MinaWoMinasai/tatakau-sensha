#include "Object3d.hlsli"

struct TransformationMatrix
{
    
    float32_t4x4 WVP;
    float32_t4x4 World;
    float32_t4x4 WorldInverseTranspose;
    float32_t4x4 LightWVP;
};

struct ShadowData
{
    float32_t4x4 lightViewProjection;
};

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    int32_t lightingMode;
    float32_t environmentCoefficient;
    float32_t padding;
    float32_t4x4 uvTransform;
    float32_t shininess;
    float32_t waterDiagnosticsEnabled;
    float32_t waterSunPathEnabled;
    float32_t waterAtmosphereEnabled;
    float32_t waterFarFlattenEnabled;
    float32_t waterProceduralCloudReflectionEnabled;
    float32_t waterDebugMode;
    float32_t waterAtmosphereStrength;
    float32_t waterFarFlattenStrength;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
ConstantBuffer<ShadowData> gShadowData : register(b1);
ConstantBuffer<Material> gMaterial : register(b2);

struct OceanWakeData
{
    float32_t4 wakePoints[16];     // x: local sea x, y: local sea z, z: age, w: strength
    float32_t4 wakeDirections[16]; // xz: flow direction in local sea space, y: wake type
    float32_t4 parameters;        // x: active count, yzw: reserved
};

ConstantBuffer<OceanWakeData> gOceanWake : register(b3);

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t4 tangent : TANGENT0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    float32_t4 localPosition = input.position;
    float32_t3 localNormal = input.normal;
    float32_t4 localTangent = input.tangent;

    // Graphics lab water: dedicated high-density river mesh.
    if (gMaterial.environmentCoefficient >= 2.5f)
    {
        float time = gMaterial.shininess;
        float2 p = input.position.xz;

        float flow = p.y * 0.030f + time * 0.86f;
        float cross = p.x * 0.095f;
        float2 dirA = normalize(float2(0.18f, 0.98f));
        float2 dirB = normalize(float2(-0.42f, 0.91f));
        float2 dirC = normalize(float2(0.72f, 0.69f));

        float phaseA = dot(p, dirA) * 0.040f + time * 0.92f;
        float phaseB = dot(p, dirB) * 0.078f - time * 1.34f;
        float phaseC = dot(p, dirC) * 0.150f + time * 2.05f;
        float phaseD = flow + sin(cross + time * 0.35f) * 0.55f;

        float edgeCalm = saturate((45.0f - abs(p.x)) / 18.0f);
        float centerCurrent = smoothstep(0.0f, 1.0f, edgeCalm);
        float navalOceanMode = step(2.85f, gMaterial.environmentCoefficient);
        float arcBlancMode = step(3.10f, gMaterial.environmentCoefficient);
        float2 dirD = normalize(float2(-0.88f, 0.47f));
        float2 dirE = normalize(float2(0.36f, 0.93f));
        float2 dirF = normalize(float2(0.08f, 0.997f));
        float2 dirG = normalize(float2(-0.31f, 0.95f));
        float phaseE = dot(p, dirD) * 0.245f - time * 2.90f;
        float phaseF = dot(p, dirE) * 0.360f + time * 3.70f + sin(dot(p, dirB) * 0.018f + time * 0.28f) * 0.85f;
        float phaseG = dot(p, dirF) * 0.0155f + sin(dot(p, dirG) * 0.0065f + time * 0.055f) * 1.55f + time * 0.30f;
        float phaseH = dot(p, dirG) * 0.0260f - sin(dot(p, dirF) * 0.0080f - time * 0.045f) * 1.10f - time * 0.42f;
        float spectrumPatch = 0.72f + 0.28f * sin(dot(p, normalize(float2(0.53f, 0.85f))) * 0.010f + time * 0.16f);
        float swellPatch = 0.68f + 0.32f * sin(dot(p, normalize(float2(-0.18f, 0.98f))) * 0.0042f + time * 0.035f);
        float displacement =
            sin(phaseA) * lerp(0.46f, 0.34f, arcBlancMode) +
            sin(phaseB) * lerp(0.20f, 0.16f, arcBlancMode) +
            sin(phaseC) * lerp(0.08f, 0.11f, arcBlancMode) +
            sin(phaseD) * lerp(0.12f, 0.09f, arcBlancMode);
        displacement += arcBlancMode * spectrumPatch * (
            sin(phaseE) * 0.070f +
            sin(phaseF) * 0.044f);
        displacement += arcBlancMode * swellPatch * (
            sin(phaseG) * 0.165f +
            sin(phaseH) * 0.095f);
        float diagnosticFarFlatten = saturate(gMaterial.waterFarFlattenEnabled) *
            saturate(gMaterial.waterFarFlattenStrength);
        float farFlattenStrength = lerp(1.0f, diagnosticFarFlatten, saturate(gMaterial.waterDiagnosticsEnabled));
        float farFlatten = lerp(
            1.0f,
            1.0f - smoothstep(360.0f, 900.0f, abs(p.y)) * 0.58f * farFlattenStrength,
            arcBlancMode);
        float oceanDisplacement = displacement * lerp(0.54f + centerCurrent * 0.30f, lerp(2.28f, 1.32f, arcBlancMode), navalOceanMode);
        oceanDisplacement *= farFlatten;

        float2 grad;
        grad.x =
            cos(phaseA) * 0.46f * 0.040f * dirA.x +
            cos(phaseB) * 0.20f * 0.078f * dirB.x +
            cos(phaseC) * 0.08f * 0.150f * dirC.x +
            cos(phaseD) * 0.12f * cos(cross + time * 0.35f) * 0.095f * 0.55f;
        grad.y =
            cos(phaseA) * 0.46f * 0.040f * dirA.y +
            cos(phaseB) * 0.20f * 0.078f * dirB.y +
            cos(phaseC) * 0.08f * 0.150f * dirC.y +
            cos(phaseD) * 0.12f * 0.030f;
        grad += arcBlancMode * spectrumPatch * (
            dirD * cos(phaseE) * 0.052f * 0.245f +
            dirE * cos(phaseF) * 0.030f * 0.360f);
        grad += arcBlancMode * swellPatch * (
            dirF * cos(phaseG) * 0.150f * 0.0155f +
            dirG * cos(phaseH) * 0.080f * 0.0260f);
        grad *= lerp(1.0f, 1.92f, navalOceanMode);
        grad *= lerp(1.0f, 1.52f, arcBlancMode);
        grad *= lerp(1.0f, farFlatten, arcBlancMode);

        float wakeCount = min(gOceanWake.parameters.x, 16.0f);
        float wakeDisplacement = 0.0f;
        float2 wakeGrad = float2(0.0f, 0.0f);
        [unroll]
        for (int index = 0; index < 16; ++index)
        {
            if (index >= (int)wakeCount)
            {
                continue;
            }
            float4 wakePoint = gOceanWake.wakePoints[index];
            float4 wakeDirectionData = gOceanWake.wakeDirections[index];
            float2 wakeOrigin = wakePoint.xy;
            float age = wakePoint.z;
            float strength = wakePoint.w;
            float wakeType = wakeDirectionData.y;
            float2 wakeDir = normalize(wakeDirectionData.xz + float2(0.0001f, 0.0f));
            float2 wakeSide = float2(-wakeDir.y, wakeDir.x);
            float2 rel = p - wakeOrigin;
            float along = dot(rel, wakeDir);
            float side = dot(rel, wakeSide);

            float sternMode = 1.0f - step(0.5f, wakeType);
            float bowMode = step(0.5f, wakeType) * (1.0f - step(1.5f, wakeType));
            float sideMode = step(1.5f, wakeType);

            float behindMask = smoothstep(-2.0f, 8.0f, along);
            float lengthFade = 1.0f - smoothstep(36.0f + age * 18.0f, 260.0f + age * 32.0f, along);
            float ageFade = 1.0f - smoothstep(1.0f, 10.0f, age);
            float vWidth = 4.5f + along * 0.15f + age * 1.55f;
            float vArm = abs(abs(side) - along * 0.285f);
            float vMask = exp(-(vArm * vArm) / max(vWidth * vWidth, 0.001f));
            float centerMask = exp(-(side * side) / max((5.0f + along * 0.045f) * (5.0f + along * 0.045f), 0.001f));
            float hollowCenter = 1.0f - exp(-(side * side) / 18.0f);
            float wakeMask = behindMask * lengthFade * ageFade * strength;
            float wakePhase = along * 0.31f - age * 4.9f;
            float wakeRuffle = sin(along * 0.92f + abs(side) * 0.33f - age * 6.6f);
            float vWave = (sin(wakePhase) * 1.12f + wakeRuffle * 0.20f) * vMask * hollowCenter * 1.30f;
            float centerWave = sin(along * 0.18f - age * 2.8f) * centerMask * 0.38f;

            float bowFront = exp(-(along * along) / 80.0f) * exp(-(side * side) / 42.0f);
            float bowShoulder = exp(-((abs(side) - 4.8f) * (abs(side) - 4.8f)) / 24.0f) * exp(-(along * along) / 120.0f);
            float bowRipple = sin(along * 0.62f - age * 5.6f) * bowFront * 0.38f + sin(abs(side) * 0.72f - age * 3.8f) * bowShoulder * 0.24f;
            float bowHeight = (bowFront * 1.05f + bowShoulder * 0.22f + bowRipple) * ageFade * strength * navalOceanMode;

            float sideBehind = smoothstep(-3.0f, 6.0f, along);
            float sideLengthFade = 1.0f - smoothstep(14.0f + age * 10.0f, 115.0f + age * 18.0f, along);
            float sideBand = exp(-(side * side) / max((3.0f + along * 0.055f) * (3.0f + along * 0.055f), 0.001f));
            float sideRipple = sin(along * 0.42f - age * 4.2f) * sideBand * sideBehind * sideLengthFade * ageFade * strength * 0.55f * navalOceanMode;

            float sternHeight = (vWave + centerWave) * wakeMask * navalOceanMode;
            float wakeHeight = sternHeight * sternMode + bowHeight * bowMode + sideRipple * sideMode;
            wakeDisplacement += wakeHeight;

            float gradPhase = cos(wakePhase) * wakeMask * navalOceanMode;
            wakeGrad += wakeDir * gradPhase * (vMask * 0.72f + centerMask * 0.22f) * sternMode;
            wakeGrad += wakeSide * (-sign(side)) * wakeMask * navalOceanMode * (vMask * 0.115f + centerMask * 0.060f) * sternMode;
            wakeGrad += wakeDir * (bowFront * 0.12f + bowShoulder * 0.05f) * bowMode * strength * navalOceanMode;
            wakeGrad += wakeSide * (-sign(side)) * (bowShoulder * 0.10f + sideBand * 0.07f * sideMode) * strength * navalOceanMode;
        }

        float2 choppyOffset = -grad * (3.10f * arcBlancMode);
        localPosition.xz += choppyOffset;
        localPosition.y += oceanDisplacement + wakeDisplacement;
        grad += wakeGrad * 0.36f;
        localNormal = normalize(float32_t3(-grad.x, 1.0f, -grad.y));
    }
    // Naval water prototype:
    // environmentCoefficient >= 1.5 means this mesh is the sea.
    // The sea mesh must be subdivided; otherwise this only moves the large plane corners.
    else if (gMaterial.environmentCoefficient >= 1.5f)
    {
        float time = gMaterial.shininess;
        float2 p = input.position.xz;

        float2 dirA = normalize(float2(0.86f, 0.51f));
        float2 dirB = normalize(float2(-0.42f, 0.91f));
        float2 dirC = normalize(float2(0.62f, -0.78f));
        float phaseA = dot(p, dirA) * 0.014f + time * 0.66f;
        float phaseB = dot(p, dirB) * 0.021f - time * 0.54f;
        float phaseC = dot(p, dirC) * 0.038f + time * 1.05f;
        float phaseD = p.x * 0.068f + p.y * -0.041f + time * 1.55f;

        float waveA = sin(phaseA);
        float waveB = sin(phaseB);
        float waveC = sin(phaseC);
        float waveD = sin(phaseD);
        float swellFocus = 0.88f + 0.18f * sin(dot(p, normalize(float2(0.27f, 0.96f))) * 0.006f + time * 0.18f);
        float displacement = (waveA * 1.75f + waveB * 1.05f + waveC * 0.50f + waveD * 0.28f) * swellFocus;
        localPosition.y += displacement;

        float2 grad;
        grad.x =
            cos(phaseA) * 1.75f * 0.014f * dirA.x +
            cos(phaseB) * 1.05f * 0.021f * dirB.x +
            cos(phaseC) * 0.50f * 0.038f * dirC.x +
            cos(phaseD) * 0.28f * 0.068f;
        grad.y =
            cos(phaseA) * 1.75f * 0.014f * dirA.y +
            cos(phaseB) * 1.05f * 0.021f * dirB.y +
            cos(phaseC) * 0.50f * 0.038f * dirC.y +
            cos(phaseD) * 0.28f * -0.041f;
        localNormal = normalize(float32_t3(-grad.x, 1.0f, -grad.y));
    }

    output.position = mul(localPosition, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    
    // 2. 法線の変換に逆転置行列を使用する
    // float32_t3x3 にキャストして、平行移動成分を無視します
    output.normal = normalize(mul(localNormal, (float32_t3x3) gTransformationMatrix.WorldInverseTranspose));
    output.tangent = float32_t4(
        normalize(mul(localTangent.xyz, (float32_t3x3) gTransformationMatrix.WorldInverseTranspose)),
        localTangent.w);
    
    output.worldPosition = mul(localPosition, gTransformationMatrix.World).xyz;
    output.shadowMapPosition = mul(localPosition, gTransformationMatrix.LightWVP);
    
    return output;
}
