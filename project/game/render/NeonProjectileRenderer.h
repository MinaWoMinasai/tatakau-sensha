#pragma once

#include "game/render/neon/NeonGridRenderer.h"
#include <cstdint>
#include <vector>

class Bullet;

// 演出の寸法はワールド単位で、コライダーの半径は変えない。
struct NeonProjectileParams {
    bool enabled = true;
    float headLength = 0.8f;
    float headWidth = 0.4f;
    float coreLengthScale = 0.55f;
    float coreWidthScale = 0.3f;
    float headIntensity = 2.8f;
    float coreIntensity = 4.5f;
    float haloWidthScale = 2.8f;
    float haloIntensity = 1.2f;
    float haloAlpha = 0.18f;
};

// 既存の履歴リボンの横に、小さく淡い先端と色付きのハローを加える。
// 呼び出し側はTrailパスを保ち、後続の描画の状態を設定し直す。
class NeonProjectileRenderer {
public:
    /// @brief 弾の先端の描画数。検証で使う。
    struct DrawStats {
        uint32_t heads = 0;
        uint32_t skipped = 0;
        uint32_t truncated = 0;
        uint32_t vertices = 0;
        uint32_t drawCalls = 0;
    };

    /// @brief 描画リソースを作成する。
    void Initialize(myEngine::DirectXCommon* dx);
    void SetParams(const NeonProjectileParams& params);

    const NeonProjectileParams& GetParams() const
    {
        return params_;
    }

    // 既存のフレームフェンスの完了後、描画フレームごとに1回呼ぶ。
    // ゲームプレイの停止中もDrawから呼ばれるが、アニメーションや時間は進めない。
    void BeginFrame(const std::vector<Bullet*>& bullets, const myEngine::Vector3& cameraForward);
    // 既存のTrailキャプチャか互換のシーンパスで、フレームごとに1回呼ぶ。
    void Draw(const myEngine::Matrix4x4& viewProjection);

    const DrawStats& GetStats() const
    {
        return stats_;
    }

private:
    /// @brief 弾1発分の先端を描画キューに積む。
    void QueueHead(const Bullet& bullet, const myEngine::Vector3& cameraForward);

    myEngine::DirectXCommon* dx_ = nullptr;
    myEngine::NeonGridRenderer geometry_;
    NeonProjectileParams params_{};
    DrawStats stats_{};
    uint64_t preparedFence_ = 0;
    uint64_t lastDrawFence_ = 0;
    bool prepared_ = false;
    bool drawn_ = false;
};
