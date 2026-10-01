#include "NeonSkinned.hlsli"

// Object3dの既存TransformationMatrix(WithShadow) CBVの先頭3行列を利用する。
struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};

// SkinningPaletteEntryと同じStructuredBuffer配置。
struct SkinningPaletteEntry
{
    float4x4 skeletonSpaceMatrix;
    float4x4 skeletonSpaceInverseTransposeMatrix;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
StructuredBuffer<SkinningPaletteEntry> gMatrixPalette : register(t3);

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 tangent : TANGENT0;
    float4 weight : WEIGHT0;
    int4 index : INDEX0;
};

NeonSkinnedVertexOutput main(VertexShaderInput input)
{
    // 既存SkinningObject3d.VSと同じ4 influence / row-major行列によるGPU Skinning。
    // 通常Shaderは独立したまま保持し、Neonでは不要なPBR / tangent処理を行わない。
    float4 skinnedPosition = 0.0f;
    float3 skinnedNormal = 0.0f;
    [unroll]
    for (uint influence = 0; influence < 4; ++influence)
    {
        skinnedPosition += mul(input.position,
            gMatrixPalette[input.index[influence]].skeletonSpaceMatrix) * input.weight[influence];
        skinnedNormal += mul(input.normal,
            (float3x3)gMatrixPalette[input.index[influence]].skeletonSpaceInverseTransposeMatrix) * input.weight[influence];
    }
    skinnedPosition.w = 1.0f;

    NeonSkinnedVertexOutput output;
    output.position = mul(skinnedPosition, gTransformationMatrix.WVP);
    output.worldPosition = mul(skinnedPosition, gTransformationMatrix.World).xyz;
    output.worldNormal = normalize(mul(skinnedNormal, (float3x3)gTransformationMatrix.WorldInverseTranspose));
    return output;
}
