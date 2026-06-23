#include "ParticleCompute.hlsli"

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeListIndex : register(u1);
RWStructuredBuffer<uint> gFreeList : register(u2);
ConstantBuffer<ComputeConfig> gConfig : register(b0);

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    const uint particleIndex = DTid.x;
    if (particleIndex == 0)
    {
        gFreeListIndex[0] = int(gConfig.maxParticles) - 1;
    }
    if (particleIndex < gConfig.maxParticles)
    {
        gParticles[particleIndex] = (Particle)0;
        gFreeList[particleIndex] = particleIndex;
    }
}

