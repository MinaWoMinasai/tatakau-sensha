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

struct Well
{
    float32_t4x4 skeletonSpaceMatrix;
    float32_t4x4 skeletonSpaceInverseTransposeMatrix;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
ConstantBuffer<ShadowData> gShadowData : register(b1);
StructuredBuffer<Well> gMatrixPalette : register(t3);

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t4 weight : WEIGHT0;
    int32_t4 index : INDEX0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    float32_t4 skinnedPosition = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);
    float32_t3 skinnedNormal = float32_t3(0.0f, 0.0f, 0.0f);
    [unroll]
    for (uint32_t influence = 0; influence < 4; ++influence)
    {
        skinnedPosition += mul(input.position, gMatrixPalette[input.index[influence]].skeletonSpaceMatrix) * input.weight[influence];
        skinnedNormal += mul(input.normal, (float32_t3x3)gMatrixPalette[input.index[influence]].skeletonSpaceInverseTransposeMatrix) * input.weight[influence];
    }
    skinnedPosition.w = 1.0f;

    VertexShaderOutput output;
    output.position = mul(skinnedPosition, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(skinnedNormal, (float32_t3x3)gTransformationMatrix.WorldInverseTranspose));
    output.worldPosition = mul(skinnedPosition, gTransformationMatrix.World).xyz;
    output.shadowMapPosition = mul(skinnedPosition, gTransformationMatrix.LightWVP);
    return output;
}
