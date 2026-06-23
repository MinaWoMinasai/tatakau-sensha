#include "ParticleCompute.hlsli"

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
// ExecuteIndirect用の引数バッファ (InstanceCountを書き換える)
RWByteAddressBuffer gDrawArgs : register(u3);
RWStructuredBuffer<int> gFreeListIndex : register(u4);
RWStructuredBuffer<uint> gFreeList : register(u5);

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
    return t; // Linear
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

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // 1. 範囲外チェック
    if (DTid.x >= gConfig.maxParticles)
        return;

    // 2. アクティブチェック
    if (gParticles[DTid.x].isActive == 0)
        return;

    Particle p = gParticles[DTid.x];

    // 3. 寿命更新と終了判定
    p.currentTime += gConfig.deltaTime;
    if (p.currentTime >= p.lifeTime)
    {
        p.isActive = 0;
        p.currentTime = p.lifeTime;
        p.startScale = 0.0f;
        p.endScale = 0.0f;
        gParticles[DTid.x] = p;

        int freeListIndex;
        InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
        if ((freeListIndex + 1) < int(gConfig.maxParticles))
        {
            gFreeList[freeListIndex + 1] = DTid.x;
        }
        else
        {
            // 二重解放などが起きてもFreeListを破壊しない安全策。
            InterlockedAdd(gFreeListIndex[0], -1);
        }
        return;
    }

    // 4. 物理挙動の計算
    float rawT = p.currentTime / p.lifeTime;
    // --- 新機能: イージング適用 ---
    float t = ApplyEasing(rawT, p.easingType);
    p.velocity += p.acceleration * gConfig.deltaTime;
    p.position += p.velocity * gConfig.deltaTime;
    p.rotate += p.angularVelocity * gConfig.deltaTime;

    // 5. 演出パラメータ計算（イージング適用済みのtを使用）
    float currentScale = lerp(p.startScale, p.endScale, t);
    float4 currentColor = lerp(p.startColor, p.endColor, t);
    currentColor.a *= (1.0f - rawT); // フェードアウトは rawT で

    // 6. 行列生成
    // --- 新機能: ビルボード制御 ---
    float4x4 world;
    if (p.isBillboard == 1)
    {
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

    // 7. 生存カウンタをインクリメントして、書き込み先のインデックスを取得
    // gDrawArgs (RWByteAddressBuffer) の 4バイト目 (InstanceCount) を +1 する
    uint drawIndex;
    gDrawArgs.InterlockedAdd(4, 1, drawIndex);

    // 8. 取得した drawIndex 番目に描画データを詰めて書き込む
    // 注意：DTid.x ではなく drawIndex を使うことで、バッファの先頭から生存分が並ぶ
    gRenderData[drawIndex].World = world;
    gRenderData[drawIndex].WVP = mul(world, gScene.viewProjection);
    gRenderData[drawIndex].WorldInverseTranspose = transpose(world);
    gRenderData[drawIndex].color = currentColor;

    // (オプション) 生存している元のパーティクルIDを保持しておきたい場合
    gAliveIndices[drawIndex] = DTid.x;

    // 9. パーティクル状態を保存
    gParticles[DTid.x] = p;
}
