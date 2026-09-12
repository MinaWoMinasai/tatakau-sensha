cbuffer ReticleConstants : register(b0) {
    float4 centerViewport;
    float4 shape;
    float4 quad;
    float4 feedback;
    float4 charge;
    float4 detail;
};
struct ReticleVertex {
    float4 position : SV_POSITION;
    float2 local : TEXCOORD0;
};
