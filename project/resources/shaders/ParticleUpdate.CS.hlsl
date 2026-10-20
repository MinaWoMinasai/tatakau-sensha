#include "ParticleCompute.hlsli"

// GPUパーティクルの更新（1/3）: 寿命と物理を進め、グループごとの生存数・死亡数を数える。
// 描画データの書き込みとフリーリストへの返却は、ParticleScan で位置を決めてから ParticleCompact が行う。
// 不可分操作で書き込み先を取ると、ウェーブの実行順で描画順が実行ごとに変わり、加算合成の丸めが揺れるため。

RWStructuredBuffer<Particle> gParticles : register(u0);
// x: 生存数, y: 死亡数（z, w は ParticleScan が書く）
RWStructuredBuffer<uint4> gGroupCounts : register(u6);

ConstantBuffer<ComputeConfig> gConfig : register(b0);

groupshared uint gsAliveCount;
groupshared uint gsDyingCount;

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    if (groupIndex == 0u)
    {
        gsAliveCount = 0u;
        gsDyingCount = 0u;
    }
    GroupMemoryBarrierWithGroupSync();

    // 1. 範囲外・非アクティブは数えない
    if (DTid.x < gConfig.maxParticles && gParticles[DTid.x].isActive != kParticleInactive)
    {
        Particle p = gParticles[DTid.x];

        // 2. 寿命更新と終了判定
        p.currentTime += gConfig.deltaTime;
        if (p.currentTime >= p.lifeTime)
        {
            p.isActive = kParticleDying;
            p.currentTime = p.lifeTime;
            p.startScale = 0.0f;
            p.endScale = 0.0f;
            InterlockedAdd(gsDyingCount, 1u);
        }
        else
        {
            // 3. 物理挙動の計算
            p.velocity += p.acceleration * gConfig.deltaTime;
            if (p.padding1 > 0.5f && (p.padding2 == 1.0f || p.padding2 == 2.0f))
                p.velocity *= exp(-2.8f * gConfig.deltaTime);
            p.position += p.velocity * gConfig.deltaTime;
            p.rotate += p.angularVelocity * gConfig.deltaTime;
            InterlockedAdd(gsAliveCount, 1u);
        }

        gParticles[DTid.x] = p;
    }

    GroupMemoryBarrierWithGroupSync();
    if (groupIndex == 0u)
    {
        gGroupCounts[groupId.x] = uint4(gsAliveCount, gsDyingCount, 0u, 0u);
    }
}
