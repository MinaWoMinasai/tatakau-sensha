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

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    float32_t4 localPosition = input.position;
    float32_t3 localNormal = input.normal;

    // Naval water prototype:
    // environmentCoefficient >= 1.5 means this mesh is the sea.
    // The sea mesh must be subdivided; otherwise this only moves the large plane corners.
    if (gMaterial.environmentCoefficient >= 1.5f)
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
    
    output.worldPosition = mul(localPosition, gTransformationMatrix.World).xyz;
    output.shadowMapPosition = mul(localPosition, gTransformationMatrix.LightWVP);
    
    return output;
}
