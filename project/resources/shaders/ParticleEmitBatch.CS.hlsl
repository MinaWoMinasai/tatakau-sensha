#include "ParticleCompute.hlsli"

ConstantBuffer<ComputeConfig> gConfig : register(b0);
RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeListIndex : register(u1);
RWStructuredBuffer<uint> gFreeList : register(u2);
StructuredBuffer<Particle> gInputParticles : register(t0);

[numthreads(64, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= gConfig.itemCount)
    {
        return;
    }

    int freeListIndex;
    InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
    if (freeListIndex < 0 || freeListIndex >= int(gConfig.maxParticles))
    {
        InterlockedAdd(gFreeListIndex[0], 1);
        return;
    }

    const uint particleIndex = gFreeList[freeListIndex];
    Particle particle = gInputParticles[DTid.x];
    particle.isActive = 1u;
    gParticles[particleIndex] = particle;
}

