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
    float32_t3 reflectLight = reflect(gDirectionalLight.direction, normalize(input.normal));
    
    float RdotE = dot(reflectLight, toEye);
    float speculerPow = pow(saturate(RdotE), gMaterial.shininess);
    
    PixelShaderOutput output;

    if (textureColor.a < 0.1f)
    {
        discard;
    }
    
    if (gMaterial.enableLighting != 0)
    {
        
        float32_t3 halfVector = normalize(-gDirectionalLight.direction + toEye);
        float NDotH = dot(normalize(input.normal), halfVector);
        float specularPow = pow(saturate(NDotH), gMaterial.shininess);
        
        float cosLighting = 0.0f;
        float NdotL = dot(normalize(input.normal), gDirectionalLight.direction);
        cosLighting = pow(NdotL * 0.5f + 0.5f, 2.0f);
        
        // 拡散反射
        float32_t3 diffuse = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * NdotL * gDirectionalLight.intensity;
    
        // 鏡面反射
        float32_t3 speculer = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow * float32_t3(1.0f, 1.0f, 1.0f);
        
        // 拡散反射+鏡面反射
        output.color.rgb = diffuse + speculer;
        
        // アルファ値
        output.color.a = gMaterial.color.a * textureColor.a;
        
        //if (gMaterial.lightingMode == 0)
        //{
        //    // 通常のLambertライティング
        //    cosLighting = saturate(NdotL);
        //}
        //else if (gMaterial.lightingMode == 1)
        //{
        //    // Half Lambert
        //    cosLighting = pow(NdotL * 0.5f + 0.5f, 2.0f);
        //}
        
        //output.color.rgb = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cosLighting * gDirectionalLight.intensity;
        //output.color.a = gMaterial.color.a * textureColor.a;
        
    }
    else
    {
        output.color = gMaterial.color * textureColor;
    }

    return output;
}