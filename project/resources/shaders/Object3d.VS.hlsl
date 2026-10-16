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

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
ConstantBuffer<ShadowData> gShadowData : register(b1);

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
