struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOut main(uint id : SV_VertexID)
{
    float2 pos[3] =
    {
        { -1.0, -1.0 },
        { -1.0, 3.0 },
        { 3.0, -1.0 }
    };

    float2 uv[3] =
    {
        { 0.0, 1.0 },
        { 0.0, -1.0 },
        { 2.0, 1.0 }
    };

    VSOut o;
    o.pos = float4(pos[id], 0, 1);
    o.uv = uv[id];
    return o;
}