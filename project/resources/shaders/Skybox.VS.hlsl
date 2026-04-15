struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexShaderInput
{
    float4 position : POSITION;
};

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float3 texcoord : TEXCOORD0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    // WVPで変換
    output.position = mul(input.position, gTransformationMatrix.WVP);
    // zをwに置き換えることで、常に遠平面(深度1.0)に描画されるようにする
    output.position.z = output.position.w;
    // 頂点のローカル座標を方向ベクトルとして使用
    output.texcoord = input.position.xyz;
    return output;
}