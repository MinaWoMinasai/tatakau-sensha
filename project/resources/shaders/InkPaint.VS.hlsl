struct Surface
{
    float4 originWidth;
    float4 uHeight;
    float4 vInkable;
    float4 normalTile;
    float4 color;
};

cbuffer Frame : register(b0)
{
    row_major float4x4 viewProjection;
    float4 eye;
    float4 renderOptions;
};
StructuredBuffer<Surface> surfaces : register(t1);

struct VertexOutput
{
    float4 position : SV_Position;
    float3 worldPosition : POSITION0;
    float2 uv : TEXCOORD0;
    nointerpolation float4 normalTile : TEXCOORD1;
    nointerpolation float4 colorInkable : TEXCOORD2;
    nointerpolation float2 size : TEXCOORD3;
    nointerpolation float3 uAxis : TEXCOORD4;
    nointerpolation float3 vAxis : TEXCOORD5;
};

VertexOutput main(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    const float2 corners[6] = {
        float2(0, 0), float2(1, 0), float2(0, 1),
        float2(0, 1), float2(1, 0), float2(1, 1)
    };
    const Surface s = surfaces[instanceId];
    VertexOutput output;
    output.uv = corners[vertexId];
    output.worldPosition = s.originWidth.xyz +
        s.uHeight.xyz * (output.uv.x * s.originWidth.w) +
        s.vInkable.xyz * (output.uv.y * s.uHeight.w);
    output.position = mul(float4(output.worldPosition, 1.0f), viewProjection);
    output.normalTile = s.normalTile;
    output.colorInkable = float4(s.color.rgb, s.vInkable.w);
    output.size = float2(s.originWidth.w, s.uHeight.w);
    output.uAxis = s.uHeight.xyz;
    output.vAxis = s.vInkable.xyz;
    return output;
}
