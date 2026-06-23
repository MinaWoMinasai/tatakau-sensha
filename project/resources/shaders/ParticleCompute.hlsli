struct Particle
{
    float3 position;
    float currentTime;
    float3 velocity;
    float lifeTime;
    float3 acceleration;
    float startScale;
    float4 startColor;
    float4 endColor;
    float endScale;
    uint isActive;
    uint easingType;
    uint isBillboard;
    float3 rotate;
    float padding1;
    float3 angularVelocity;
    float padding2;
};

struct ComputeConfig
{
    float deltaTime;
    uint maxParticles;
    float time;
    uint itemCount;
};

