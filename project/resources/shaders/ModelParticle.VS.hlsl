#include "ModelParticle.hlsli"

// インスタンスごとのデータを保持する構造体
struct TransformationMatrix
{
    float32_t4x4 WVP;
    float32_t4x4 World;
    float32_t4x4 WorldInverseTranspose;
    float32_t4 color;
};

// インスタンシング用のバッファ (registerは任意ですが、ここではt1に設定)
StructuredBuffer<TransformationMatrix> gTransformationMatrices : register(t1);

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input, uint32_t instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    
    // インスタンスIDを使用してデータを取得
    TransformationMatrix gData = gTransformationMatrices[instanceId];

    // 座標変換
    output.position = mul(input.position, gData.WVP);
    output.worldPosition = mul(input.position, gData.World).xyz;
    
    // 法線の変換 (逆転置行列を使用)
    output.normal = normalize(mul(input.normal, (float32_t3x3) gData.WorldInverseTranspose));
    
    output.texcoord = input.texcoord;
    output.color = gData.color;
    
    return output;
}