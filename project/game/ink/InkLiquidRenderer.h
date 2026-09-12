#pragma once

#include "Struct.h"
#include <d3d12.h>
#include <wrl.h>
#include <vector>

class DirectXCommon;
class SrvManager;

enum class InkLiquidKind : uint32_t { Blob, Droplet, Splash, Muzzle, Ripple };

// Cosmetic only: these particles neither alter trajectories nor paint either
// ownership mask. Parent scene owns aging/spawning and queues the visible set.
struct InkLiquidParticle {
    Vector3 center{};
    Vector3 velocity{};
    Vector4 color{ 0.004f, 0.39f, 0.235f, 1.0f };
    float radius = 0.08f;
    float stretch = 1.0f;
    float age = 0.0f;
    float lifetime = 0.0f; // zero: no renderer-side fade
    InkLiquidKind kind = InkLiquidKind::Blob;
};

class InkLiquidRenderer final {
public:
    static constexpr uint32_t kMaxParticles = 4096;
    InkLiquidRenderer() = default;
    ~InkLiquidRenderer();
    InkLiquidRenderer(const InkLiquidRenderer&) = delete;
    InkLiquidRenderer& operator=(const InkLiquidRenderer&) = delete;

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    void BeginFrame();
    void QueueBlob(const InkLiquidParticle& particle);
    void QueueBlob(Vector3 center, Vector3 velocity, float radius, Vector4 color,
        float stretch = 1.0f, float age = 0.0f, float lifetime = 0.0f,
        InkLiquidKind kind = InkLiquidKind::Blob);
    // normal selects the surface plane. Ring center should be slightly above
    // the hit surface; width is a fraction of the outer radius (0..1).
    void QueueRipple(Vector3 center, Vector3 normal, float radius, Vector4 color,
        float age, float lifetime, float width = 0.07f);
    void Draw(const Matrix4x4& viewProjection, const Vector3& eyePosition,
        const Vector3& cameraRight, const Vector3& cameraUp);
    uint32_t GetParticleCount() const { return static_cast<uint32_t>(particles_.size()); }
    uint32_t GetDroppedCount() const { return droppedCount_; }

private:
    struct ParticleGpu {
        Vector4 centerRadius;
        Vector4 axisStretch;
        Vector4 color;
        Vector4 parameters; // normalized age, kind, ripple width, fade enabled
    };
    void Release();
    void CreatePipeline();
    DirectXCommon* dxCommon_ = nullptr;
    std::vector<ParticleGpu> particles_;
    Microsoft::WRL::ComPtr<ID3D12Resource> instanceBuffer_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;
    ParticleGpu* mappedInstances_ = nullptr;
    uint32_t droppedCount_ = 0;
};
