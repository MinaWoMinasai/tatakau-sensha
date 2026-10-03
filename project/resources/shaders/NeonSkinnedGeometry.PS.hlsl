#include "NeonSkinnedBody.hlsli"

// SM6.1のネイティブ入力。VSの出力や元の頂点・index・influenceを増やさない。
PixelShaderOutput main(NeonSkinnedVertexOutput input, bool isFrontFace : SV_IsFrontFace,
    noperspective float3 barycentric : SV_Barycentrics)
{
    // 全成分の微分をclip・非一様分岐より前に評価する。
    float3 baryDx = ddx(barycentric);
    float3 baryDy = ddy(barycentric);
    float2 uvDx = ddx(input.texcoord);
    float2 uvDy = ddy(input.texcoord);
    // noperspective座標の勾配長で割ると、三角形サイズ・距離によらない画面pixel距離になる。
    // 退化した微小勾配でも除算を有限に保つ。
    float3 gradient = max(sqrt(baryDx * baryDx + baryDy * baryDy), 1.0e-6f);
    float3 distances = max(barycentric, 0.0f) / gradient;
    float edgeDistance = min(distances.x, min(distances.y, distances.z));
    // Widthは共有辺を挟む全幅。各面へ半幅、境界は1pixelのsmoothstepでAAする。
    float halfWidth = gGeometryLineWidthPixels * 0.5f;
    float coverage = 1.0f - smoothstep(halfWidth - 0.5f, halfWidth + 0.5f, edgeDistance);
    float3 emission = max(gGeometryLineColor, 0.0f) * gGeometryLineIntensity
        * gSubmeshGeometryStrength * coverage;
    if (gGeometryLineEnabled == 0 || gGeometryLineWidthPixels <= 0.0f) emission = 0.0f;
    // Cutout・Depth・Stencil・Normal/Material MRTは従来Bodyと同一。
    return ShadeNeonBody(input, isFrontFace, uvDx, uvDy, emission);
}
