#include "Object3d.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    int32_t lightingMode;
    float32_t2 padding;
    float32_t4x4 uvTransform;
    float32_t shininess;
};

struct Camera
{
    float32_t3 worldPosition;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<Camera> gCamera : register(b2);

struct DirectionalLight
{
    float32_t4 color; // ライトの色
    float32_t3 direction; // ライトの方向
    float intensity; // ライトの光度
};

ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};


PixelShaderOutput main(VertexShaderOutput input)
{
    
    float32_t3 toEye = normalize(gCamera.worldPosition - input.worldPosition);
    float32_t4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    PixelShaderOutput output;

    if (textureColor.a < 0.1f)
    {
        discard;
    }
    
    if (gMaterial.enableLighting != 0)
    {
        float3 N = normalize(input.normal);
        float3 L = -normalize(gDirectionalLight.direction);
        float3 V = normalize(gCamera.worldPosition - input.worldPosition);
        float3 H = normalize(L + V);

        float NdotL = saturate(dot(N, L));
        float NdotH = saturate(dot(N, H));
        
        // Ambient
        float3 ambient = gMaterial.color.rgb * textureColor.rgb *  0.1f;
        
        // Diffuse
        float3 diffuse = gMaterial.color.rgb * textureColor.rgb *
    gDirectionalLight.color.rgb *
    NdotL *
    gDirectionalLight.intensity;

// Specular（NdotLでマスク！）
        float3 specular =
    gDirectionalLight.color.rgb *
    gDirectionalLight.intensity *
    pow(NdotH, gMaterial.shininess) *
    NdotL;

        output.color.rgb = ambient + diffuse + specular;
        output.color.a = gMaterial.color.a * textureColor.a;
    }
    else
    {
        output.color = gMaterial.color * textureColor;
    }

    return output;
}