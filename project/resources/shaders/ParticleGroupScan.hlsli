// スレッドグループ内の排他的プレフィックス和。
// 不可分操作の順序（ウェーブの実行順）に頼らず、スレッドの番号順に並べるために使う。
// 1回のディスパッチで1度だけ呼ぶ。グループ内の全スレッドが呼ぶこと（中で同期する）。

#define kParticleScanGroupSize 1024

groupshared uint2 gsParticleScan[kParticleScanGroupSize];

uint2 GroupExclusiveScan(uint2 value, uint groupIndex, out uint2 total)
{
    gsParticleScan[groupIndex] = value;
    GroupMemoryBarrierWithGroupSync();

    for (uint offset = 1u; offset < kParticleScanGroupSize; offset <<= 1u)
    {
        uint2 addend = uint2(0u, 0u);
        if (groupIndex >= offset)
        {
            addend = gsParticleScan[groupIndex - offset];
        }
        GroupMemoryBarrierWithGroupSync();
        gsParticleScan[groupIndex] += addend;
        GroupMemoryBarrierWithGroupSync();
    }

    total = gsParticleScan[kParticleScanGroupSize - 1u];
    return gsParticleScan[groupIndex] - value;
}
