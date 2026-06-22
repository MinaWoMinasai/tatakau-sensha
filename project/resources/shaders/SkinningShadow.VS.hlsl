struct ShadowTransformationMatrix
{
    float32_t4x4 WVP;
    float32_t4x4 World;
    float32_t4x4 WorldInverseTranspose;
    float32_t4x4 LightWVP;
};

struct Well
{
    float32_t4x4 skeletonSpaceMatrix;
    float32_t4x4 skeletonSpaceInverseTransposeMatrix;
};

ConstantBuffer<ShadowTransformationMatrix> gMatrix : register(b0);
StructuredBuffer<Well> gMatrixPalette : register(t3);

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t4 weight : WEIGHT0;
    int32_t4 index : INDEX0;
};

float32_t4 main(VertexShaderInput input) : SV_POSITION
{
    float32_t4 skinnedPosition = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);
    [unroll]
    for (uint32_t influence = 0; influence < 4; ++influence)
    {
        skinnedPosition += mul(input.position, gMatrixPalette[input.index[influence]].skeletonSpaceMatrix) * input.weight[influence];
    }
    skinnedPosition.w = 1.0f;
    return mul(skinnedPosition, gMatrix.LightWVP);
}
