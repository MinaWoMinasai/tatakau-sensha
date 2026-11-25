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
    /// @brief 自機の弾を細い針で描く（計画書 p10 の16章の M2）。敵の管と同じ芯・にじみの線2本（淡い尾と、明るい先）。
    /// 線の太さ、先の長さ（中心から前後）、尾の長さ・太さ（先に対する倍）・明るさ、色の明るさ（倍）。当たりは変えない。
    struct Needle {
        bool enabled = false;
        float width = 0.06f;
        float back = 0.7f;
        float front = 0.3f;
        float tail = 1.5f;
        float tailWidth = 0.55f;
        float tailAlpha = 0.5f;
        float brightness = 1.0f;
        float nearGlow = 0.0f;
        float nearGlowRadius = 0.4f;
        myEngine::NeonContourStyle style{};
    } needle;
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
    /// @brief 自機の弾1発分の針を描画キューに積む。
    void QueueNeedle(const Bullet& bullet, const myEngine::Vector3& center, const myEngine::Vector3& direction, const myEngine::Vector3& forward);
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
