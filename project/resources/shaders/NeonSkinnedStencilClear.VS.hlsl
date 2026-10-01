// Color / Depthは変更せず、PSOのWriteMaskでStencil bit 0x80だけを消去する。
float4 main(uint vertexId : SV_VertexID) : SV_POSITION
{
    float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    return float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
}
