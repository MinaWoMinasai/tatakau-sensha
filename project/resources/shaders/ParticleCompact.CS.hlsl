#include "ParticleCompute.hlsli"
#include "ParticleGroupScan.hlsli"

// GPUパーティクルの更新（3/3）: 生存しているものの描画データをパーティクルの番号順に詰め、
// 寿命が尽きたものを番号順にフリーリストへ返す。書き込み先は ParticleScan が決めた位置。

struct RenderData
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
    float4 color;
};

// バッファ
RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<RenderData> gRenderData : register(u1); // 描画用

// 描画対象のインデックスを保持するバッファ
RWStructuredBuffer<uint> gAliveIndices : register(u2);
RWStructuredBuffer<uint> gFreeList : register(u5);
// z: 描画データの書き込み開始位置, w: フリーリストへの返却開始位置
RWStructuredBuffer<uint4> gGroupCounts : register(u6);

// 定数
struct SceneConfig
{
    float4x4 viewProjection;
    float3 cameraPosition;  // ビルボード用カメラ位置
    float scenePadding;
};

ConstantBuffer<ComputeConfig> gConfig : register(b0);
ConstantBuffer<SceneConfig> gScene : register(b1);

// --- 行列生成関数 ---
float4x4 MakeAffineMatrix(float3 scale, float3 rotate, float3 translate)
{
    // スケール行列
    float4x4 mScale =
    {
        scale.x, 0, 0, 0,
        0, scale.y, 0, 0,
        0, 0, scale.z, 0,
        0, 0, 0, 1
    };

    // 回転行列 (XYZ順)
    float3 s = sin(rotate);
    float3 c = cos(rotate);
    
    float4x4 mRotateX =
    {
        1, 0, 0, 0,
        0, c.x, s.x, 0,
        0, -s.x, c.x, 0,
        0, 0, 0, 1
    };
    float4x4 mRotateY =
    {
        c.y, 0, -s.y, 0,
        0, 1, 0, 0,
        s.y, 0, c.y, 0,
        0, 0, 0, 1
    };
    float4x4 mRotateZ =
    {
        c.z, s.z, 0, 0,
        -s.z, c.z, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
    float4x4 mRotate = mul(mRotateX, mul(mRotateY, mRotateZ));

    // 平行移動行列
    float4x4 mTranslate =
    {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        translate.x, translate.y, translate.z, 1
    };

    return mul(mScale, mul(mRotate, mTranslate));
}

// --- 新機能: イージング関数 ---
float ApplyEasing(float t, uint easingType)
{
    if (easingType == 1) // EaseIn (二次)
        return t * t;
    if (easingType == 2) // EaseOut (二次)
        return 1.0 - (1.0 - t) * (1.0 - t);
    return t; // 線形
}

// --- 新機能: ビルボード行列生成 ---
float4x4 MakeBillboardMatrix(float3 scale, float3 position, float3 cameraPos, float rotation)
{
    float3 forward = normalize(cameraPos - position);
    
    // forward がほぼ上方向の場合の対策
    float3 up = float3(0, 1, 0);
    if (abs(dot(forward, up)) > 0.999)
    {
        up = float3(0, 0, 1);
    }
    
    float3 right = normalize(cross(up, forward));
    up = cross(forward, right);

    float rotationCos = cos(rotation);
    float rotationSin = sin(rotation);
    float3 spunRight = right * rotationCos + up * rotationSin;
    float3 spunUp = up * rotationCos - right * rotationSin;
    
    float4x4 billboard =
    {
        spunRight.x * scale.x, spunRight.y * scale.x, spunRight.z * scale.x, 0,
        spunUp.x * scale.y, spunUp.y * scale.y, spunUp.z * scale.y, 0,
        forward.x * scale.z, forward.y * scale.z, forward.z * scale.z, 0,
        position.x,          position.y,          position.z,          1
    };
    return billboard;
}

[numthreads(kParticleScanGroupSize, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    // 1. 生存・死亡をスレッド番号順に数え、ParticleScan が決めたグループの開始位置に足す
    const uint state = DTid.x < gConfig.maxParticles ? gParticles[DTid.x].isActive : kParticleInactive;
    uint2 groupTotal;
    const uint2 localIndex = GroupExclusiveScan(
        uint2(state == kParticleActive ? 1u : 0u, state == kParticleDying ? 1u : 0u), groupIndex, groupTotal);
    const uint4 groupCounts = gGroupCounts[groupId.x];

    // 2. 寿命が尽きたものをフリーリストへ返す
    if (state == kParticleDying)
    {
        gParticles[DTid.x].isActive = kParticleInactive;
        const uint freeListIndex = groupCounts.w + localIndex.y;
        // 二重解放などが起きてもFreeListを破壊しない安全策。
        if (freeListIndex < gConfig.maxParticles)
        {
            gFreeList[freeListIndex] = DTid.x;
        }
        return;
    }
    if (state != kParticleActive)
        return;

    Particle p = gParticles[DTid.x];

    // 3. 寿命と物理は ParticleUpdate で進めてある
    float rawT = p.currentTime / p.lifeTime;
    // --- 新機能: イージング適用 ---
    float t = ApplyEasing(rawT, p.easingType);

    // 5. 演出パラメータ計算（イージング適用済みのtを使用）
    const bool neonRadiance = p.padding1 > 0.5f;
    float currentScale = lerp(p.startScale, p.endScale, neonRadiance ? rawT : t);
    float4 currentColor = lerp(p.startColor, p.endColor, neonRadiance ? rawT : t);
    if (neonRadiance)
    {
        // CPU経路と合わせる。以前のEaseOutのアルファ * 元のアルファの積では、
        // ネオンの破片が寿命よりかなり早く消え、Bloomの発光元が失われていた。
        const float hold = p.padding2 == 4.0f ? 0.0f : p.padding2 == 5.0f ? 0.08f : p.padding2 == 0.0f ? 0.10f : 0.28f;
        const float fadeT = saturate((rawT - hold) / (1.0f - hold));
        const float fade = fadeT * fadeT * (3.0f - 2.0f * fadeT);
        currentColor.a = lerp(p.startColor.a, p.endColor.a, fade);
    }
    else
    {
        currentColor.a *= (1.0f - rawT); // 通常のGPUパーティクルはそのまま保つ。
    }

    // 6. 行列生成
    // --- 新機能: ビルボード制御 ---
    float4x4 world;
    if (p.isBillboard == 1)
    {
        if (neonRadiance && p.padding2 == 3.0f && dot(p.velocity, p.velocity) > 1e-8f)
        {
            // MakeBillboardMatrixと同じ基底で、火花を投影後の進行方向に揃える。
            // ワールドでのatan2では、正面のカメラで左右が反転していた。
            const float3 facing = normalize(gScene.cameraPosition - p.position);
            const float3 referenceUp = abs(facing.y) > 0.999f ? float3(0,0,1) : float3(0,1,0);
            const float3 right = normalize(cross(referenceUp, facing));
            const float3 up = cross(facing, right);
            p.rotate.z = atan2(dot(p.velocity, up), dot(p.velocity, right)) - 1.57079632679f;
        }
        world = MakeBillboardMatrix(
            float3(currentScale, currentScale, currentScale),
            p.position,
            gScene.cameraPosition,
            p.rotate.z
        );
    }
    else
    {
        world = MakeAffineMatrix(float3(currentScale, currentScale, currentScale), p.rotate, p.position);
    }
    
    // --- ここからが「間接描画」のための重要処理 ---

    // 7. 書き込み先はパーティクルの番号順。InstanceCount は ParticleScan が書いてある
    const uint drawIndex = groupCounts.z + localIndex.x;

    // 8. 取得した drawIndex 番目に描画データを詰めて書き込む
    // 注意：DTid.x ではなく drawIndex を使うことで、バッファの先頭から生存分が並ぶ
    gRenderData[drawIndex].World = world;
    gRenderData[drawIndex].WVP = mul(world, gScene.viewProjection);
    gRenderData[drawIndex].WorldInverseTranspose = transpose(world);
    // 同次座標の要素は、法線の3x3変換では使われない。
    gRenderData[drawIndex].WorldInverseTranspose[3][3] = neonRadiance ? 2.0f + p.padding2 : 1.0f;
    gRenderData[drawIndex].color = currentColor;

    // (オプション) 生存している元のパーティクルIDを保持しておきたい場合
    gAliveIndices[drawIndex] = DTid.x;

    // 9. パーティクル状態を保存
    gParticles[DTid.x] = p;
}
