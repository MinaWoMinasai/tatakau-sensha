
struct ShadowTransformationMatrix
{
    
    float32_t4x4 WVP;
    float32_t4x4 World;
    float32_t4x4 WorldInverseTranspose;
    float32_t4x4 LightWVP;
};

ConstantBuffer<ShadowTransformationMatrix> gMatrix : register(b0);

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
};

float32_t4 main(VertexShaderInput input) : SV_POSITION
{
    return mul(input.position, gMatrix.LightWVP);
}