struct Material
{
    float4 color;
};
ConstantBuffer<Material> gMaterial : register(b0);

// Texture2DではなくTextureCubeを使用する
TextureCube<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float3 texcoord : TEXCOORD0;
};

float4 main(VertexShaderOutput input) : SV_TARGET
{
    // 3Dの方向ベクトルでサンプリング
    float4 sampledColor = gTexture.Sample(gSampler, input.texcoord);
    return sampledColor * gMaterial.color;
}