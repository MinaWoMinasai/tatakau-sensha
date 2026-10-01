#include "NeonSkinnedSkinning.hlsli"

NeonSkinnedVertexOutput main(VertexShaderInput input)
{
    NeonSkinnedVertexOutput output = SkinNeonVertex(input);
    // WorldInverseTransposeの転置はinverse(World)。既存行列のみから法線をclip方向へ変換する。
    float4 localNormal = mul(float4(output.worldNormal, 0.0f), transpose(gTransformationMatrix.WorldInverseTranspose));
    float4 clipNormal = mul(localNormal, gTransformationMatrix.WVP);
    // 透視除算後の変化方向。aspect比を補正してpixel単位の線幅を使う。
    float2 directionPixels = (clipNormal.xy * output.position.w - output.position.xy * clipNormal.w) * gViewportSize;
    float lengthSquared = dot(directionPixels, directionPixels);
    float2 direction = directionPixels * rsqrt(max(lengthSquared, 1.0e-8f));
    if (output.position.w > 1.0e-5f && gOutlineEnabled != 0)
    {
        output.position.xy += direction * (2.0f * gOutlineWidthPixels / max(gViewportSize, 1.0f)) * output.position.w;
    }
    return output;
}
