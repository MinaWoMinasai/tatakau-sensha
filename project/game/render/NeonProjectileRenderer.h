#pragma once

#include "NeonGridRenderer.h"
#include <cstdint>
#include <vector>

class Bullet;

// Presentation dimensions are world units and never alter a collider radius.
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

// Adds a compact pale tip and colored halo beside the existing history ribbon.
// The caller retains the Trail pass and rebinds the following renderer's state.
class NeonProjectileRenderer {
public:
    struct DrawStats {
        uint32_t heads = 0;
        uint32_t skipped = 0;
        uint32_t truncated = 0;
        uint32_t vertices = 0;
        uint32_t drawCalls = 0;
    };

    void Initialize(cg2::DirectXCommon* dx);
    void SetParams(const NeonProjectileParams& params);
    const NeonProjectileParams& GetParams() const { return params_; }
    // Once per rendered frame after the existing frame Fence is complete.
    // Frozen gameplay still calls this from Draw; no animation/time is advanced.
    void BeginFrame(const std::vector<Bullet*>& bullets, const cg2::Vector3& cameraForward);
    // Once per frame in the existing Trail capture or compatible Scene pass.
    void Draw(const cg2::Matrix4x4& viewProjection);
    const DrawStats& GetStats() const { return stats_; }

private:
    void QueueHead(const Bullet& bullet, const cg2::Vector3& cameraForward);

    cg2::DirectXCommon* dx_ = nullptr;
    cg2::NeonGridRenderer geometry_;
    NeonProjectileParams params_{};
    DrawStats stats_{};
    uint64_t preparedFence_ = 0;
    uint64_t lastDrawFence_ = 0;
    bool prepared_ = false;
    bool drawn_ = false;
};
