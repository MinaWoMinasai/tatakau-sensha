struct TransformationMatrix
{
    float4x4 WVP;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexShaderInput
{
    float4 position : POSITION0;
};

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float3 texcoord : TEXCOORD0; // CubeMapのサンプリング用方向ベクトル
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    float4 pos = mul(input.position, gTransformationMatrix.WVP);
    // Z を W に置き換えることで、パースペクティブ除算後 Z/W = 1.0 (最奥) になる
    output.position = pos.xyww;
    // 頂点のローカル座標をそのままテクスチャ座標（方向）として使用する
    output.texcoord = input.position.xyz;
    return output;
}