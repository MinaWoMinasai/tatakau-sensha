TextureCube<float4> gSkyboxTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

float4 main(float3 texcoord : TEXCOORD0) : SV_TARGET
{
    return gSkyboxTexture.Sample(gSampler, texcoord);
}