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

// Particle.isActive の値。寿命が尽きたフレームだけ Dying になり、Compact で 0 に戻ってフリーリストへ返る。
static const uint kParticleInactive = 0u;
static const uint kParticleActive = 1u;
static const uint kParticleDying = 2u;

struct ComputeConfig
{
    float deltaTime;
    uint maxParticles;
    float time;
    uint itemCount;
    // 発生: このディスパッチの最初の要求が、このフレームの発生の何番目か。
    uint emitOffset;
    // 更新: このフレームに発生を要求した数（Emit と EmitBatch の合計）。
    uint emitCount;
    uint2 configPadding;
};

