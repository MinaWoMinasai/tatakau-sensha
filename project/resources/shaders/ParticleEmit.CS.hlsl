#include "ParticleCompute.hlsli"

struct EmitterRequest
{
    float3 position;
    float radius;
    float3 boxSize;
    uint shape;
    float speedMin;
    float speedMax;
    float lifeTimeMin;
    float lifeTimeMax;
    float3 acceleration;
    float startScaleMin;
    float startScaleMax;
    float endScaleMin;
    float endScaleMax;
    uint easingType;
    float4 startColor;
    float4 endColor;
    uint isBillboard;
    float seed;
    float2 padding;
};

ConstantBuffer<ComputeConfig> gConfig : register(b0);
RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeListIndex : register(u1);
RWStructuredBuffer<uint> gFreeList : register(u2);
StructuredBuffer<EmitterRequest> gEmitters : register(t0);

uint Hash(uint value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

float Random01(inout uint state)
{
    state = Hash(state);
    return (float(state & 0x00ffffffu) + 0.5f) / 16777216.0f;
}

float3 RandomUnitVector(inout uint state)
{
    const float z = Random01(state) * 2.0f - 1.0f;
    const float angle = Random01(state) * 6.28318530718f;
    const float radius = sqrt(max(0.0f, 1.0f - z * z));
    return float3(radius * cos(angle), radius * sin(angle), z);
}

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
    const EmitterRequest emitter = gEmitters[DTid.x];
    uint randomState = asuint(emitter.seed + gConfig.time * 17.0f) ^ Hash(DTid.x + 1u);

    float3 offset = 0.0f;
    if (emitter.shape == 1u)
    {
        offset = RandomUnitVector(randomState) * (emitter.radius * pow(Random01(randomState), 1.0f / 3.0f));
    }
    else if (emitter.shape == 2u)
    {
        offset = (float3(Random01(randomState), Random01(randomState), Random01(randomState)) - 0.5f) * emitter.boxSize;
    }

    Particle particle = (Particle)0;
    particle.position = emitter.position + offset;
    particle.velocity = RandomUnitVector(randomState) * lerp(emitter.speedMin, emitter.speedMax, Random01(randomState));
    particle.acceleration = emitter.acceleration;
    particle.lifeTime = max(0.001f, lerp(emitter.lifeTimeMin, emitter.lifeTimeMax, Random01(randomState)));
    particle.startScale = lerp(emitter.startScaleMin, emitter.startScaleMax, Random01(randomState));
    particle.endScale = lerp(emitter.endScaleMin, emitter.endScaleMax, Random01(randomState));
    particle.startColor = emitter.startColor;
    particle.endColor = emitter.endColor;
    particle.rotate.z = Random01(randomState) * 6.28318530718f;
    particle.angularVelocity = (float3(Random01(randomState), Random01(randomState), Random01(randomState)) * 2.0f - 1.0f) * 5.0f;
    particle.isActive = 1u;
    particle.easingType = emitter.easingType;
    particle.isBillboard = emitter.isBillboard;
    gParticles[particleIndex] = particle;
}

