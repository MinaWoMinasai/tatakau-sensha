#include "InkReticle.hlsli"
ReticleVertex main(uint vertexId : SV_VertexID) {
    const float2 corners[6] = {
        float2(-1, -1), float2(1, -1), float2(-1, 1),
        float2(-1, 1), float2(1, -1), float2(1, 1)
    };
    ReticleVertex output;
    output.local = corners[vertexId] * quad.xy;
    const float2 pixel = centerViewport.xy + output.local;
    output.position = float4(pixel / centerViewport.zw * float2(2, -2) + float2(-1, 1), 0, 1);
    return output;
}
