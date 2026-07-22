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
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
ConstantBuffer<ShadowData> gShadowData : register(b1);
ConstantBuffer<Material> gMaterial : register(b2);

struct OceanWakeData
{
    float32_t4 wakePoints[16];     // x: local sea x, y: local sea z, z: age, w: strength
    float32_t4 wakeDirections[16]; // xz: flow direction in local sea space
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
        float displacement =
            sin(phaseA) * 0.46f +
            sin(phaseB) * 0.20f +
            sin(phaseC) * 0.08f +
            sin(phaseD) * 0.12f;
        float oceanDisplacement = displacement * lerp(0.54f + centerCurrent * 0.30f, 2.28f, navalOceanMode);

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
        grad *= lerp(1.0f, 1.92f, navalOceanMode);

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
            float2 wakeDir = normalize(wakeDirectionData.xz + float2(0.0001f, 0.0f));
            float2 wakeSide = float2(-wakeDir.y, wakeDir.x);
            float2 rel = p - wakeOrigin;
            float along = dot(rel, wakeDir);
            float side = dot(rel, wakeSide);
            float behindMask = smoothstep(-2.0f, 8.0f, along);
            float lengthFade = 1.0f - smoothstep(36.0f + age * 18.0f, 260.0f + age * 32.0f, along);
            float ageFade = 1.0f - smoothstep(1.0f, 10.0f, age);
            float vWidth = 4.5f + along * 0.13f + age * 1.35f;
            float vArm = abs(abs(side) - along * 0.265f);
            float vMask = exp(-(vArm * vArm) / max(vWidth * vWidth, 0.001f));
            float centerMask = exp(-(side * side) / max((5.0f + along * 0.045f) * (5.0f + along * 0.045f), 0.001f));
            float hollowCenter = 1.0f - exp(-(side * side) / 18.0f);
            float wakeMask = behindMask * lengthFade * ageFade * strength;
            float wakePhase = along * 0.31f - age * 4.9f;
            float vWave = sin(wakePhase) * vMask * hollowCenter * 1.24f;
            float centerWave = sin(along * 0.18f - age * 2.8f) * centerMask * 0.46f;
            float wakeHeight = (vWave + centerWave) * wakeMask * navalOceanMode;
            wakeDisplacement += wakeHeight;

            float gradPhase = cos(wakePhase) * wakeMask * navalOceanMode;
            wakeGrad += wakeDir * gradPhase * (vMask * 0.72f + centerMask * 0.22f);
            wakeGrad += wakeSide * (-sign(side)) * wakeMask * navalOceanMode * (vMask * 0.115f + centerMask * 0.060f);
        }

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
