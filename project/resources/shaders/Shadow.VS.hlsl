struct ShadowTransformationMatrix
{
    float32_t4x4 WVP; // ライト視点のViewProjection * モデルのWorld
};
ConstantBuffer<ShadowTransformationMatrix> gMatrix : register(b0);

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
};

float32_t4 main(VertexShaderInput input) : SV_POSITION
{
    return mul(input.position, gMatrix.WVP);
}