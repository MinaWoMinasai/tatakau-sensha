#include "ParticleCompute.hlsli"
#include "ParticleGroupScan.hlsli"

// Update が数えたグループごとの生存数・死亡数から、Compact の書き込み先を決める。
// 1グループだけで実行する（グループ数は kParticleScanGroupSize 以下）。

ConstantBuffer<ComputeConfig> gConfig : register(b0);
RWByteAddressBuffer gDrawArgs : register(u3);
RWStructuredBuffer<int> gFreeListIndex : register(u4);
// x: 生存数, y: 死亡数, z: 描画データの書き込み開始位置, w: フリーリストへの返却開始位置
RWStructuredBuffer<uint4> gGroupCounts : register(u6);

groupshared int gsFreeListBase;

[numthreads(kParticleScanGroupSize, 1, 1)]
void main(uint groupIndex : SV_GroupIndex)
{
    const uint groupCount = (gConfig.maxParticles + kParticleScanGroupSize - 1u) / kParticleScanGroupSize;

    if (groupIndex == 0u)
    {
        // Emit/EmitBatch はフリーリストの先頭（top）から要求の順に取った。取れた分を除いた位置から返却を積む。
        const int top = gFreeListIndex[0];
        const int consumed = int(min(gConfig.emitCount, uint(max(top + 1, 0))));
        gsFreeListBase = top - consumed;
    }

    const uint2 counts = groupIndex < groupCount ? gGroupCounts[groupIndex].xy : uint2(0u, 0u);
    uint2 total;
    const uint2 offsets = GroupExclusiveScan(counts, groupIndex, total);
    const int freeListBase = gsFreeListBase;

    if (groupIndex < groupCount)
    {
        gGroupCounts[groupIndex].zw = uint2(offsets.x, uint(freeListBase + 1) + offsets.y);
    }

    if (groupIndex == 0u)
    {
        // ExecuteIndirect の InstanceCount。
        gDrawArgs.Store(4, total.x);
        // 二重解放などが起きてもFreeListを破壊しない安全策（溢れた分は返却しない）。
        const uint pushable = uint(int(gConfig.maxParticles) - 1 - freeListBase);
        gFreeListIndex[0] = freeListBase + int(min(total.y, pushable));
    }
}
