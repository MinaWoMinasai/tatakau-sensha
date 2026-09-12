#pragma once

#include "InkTypes.h"
#include "Struct.h"
#include <d3d12.h>
#include <wrl.h>
#include <vector>

class DirectXCommon;
class SrvManager;

// Persistent GPU paint. Each rectangular surface owns one atlas tile. Only the
// bounding rectangle of each incoming ellipse is dispatched; no paint readback
// or per-frame texture upload is needed. Call Draw in the HDR scene MRT pass.
class InkPaintRenderer final {
public:
    static constexpr uint32_t kTileSize = 512;
    static constexpr uint32_t kTilesPerAxis = 4;
    static constexpr uint32_t kAtlasSize = kTileSize * kTilesPerAxis;
    static constexpr uint32_t kMaxSurfaces = kTilesPerAxis * kTilesPerAxis;

    InkPaintRenderer() = default;
    ~InkPaintRenderer();
    InkPaintRenderer(const InkPaintRenderer&) = delete;
    InkPaintRenderer& operator=(const InkPaintRenderer&) = delete;

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager,
        const std::vector<ink::Surface>& surfaces);
    void QueueStamps(const std::vector<ink::PaintStamp>& stamps);
    void Clear();
    void Draw(const Matrix4x4& viewProjection, const Vector3& eyePosition);

    // RGBA UNORM SRV, usable directly by ImGui::Image via SrvManager's GPU handle.
    // RGB is a debug ink color; alpha stores team / 2 (0 = unpainted).
    uint32_t GetMaskSrvIndex() const { return maskSrvIndex_; }
    uint32_t GetLastStampCount() const { return lastStampCount_; }
    uint32_t GetSurfaceCount() const { return static_cast<uint32_t>(surfaces_.size()); }

private:
    void Release();
    void CreatePipelines();
    void FlushStamps();
    void TransitionMask(D3D12_RESOURCE_STATES state);

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    std::vector<ink::Surface> surfaces_;
    std::vector<ink::PaintStamp> pendingStamps_;
    Microsoft::WRL::ComPtr<ID3D12Resource> mask_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> clearUavHeap_;
    Microsoft::WRL::ComPtr<ID3D12Resource> surfaceBuffer_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> paintRoot_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> surfaceRoot_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> paintPso_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> surfacePso_;
    uint32_t maskSrvIndex_ = 0;
    uint32_t maskUavIndex_ = 0;
    uint32_t lastStampCount_ = 0;
    bool clearPending_ = true;
    D3D12_RESOURCE_STATES maskState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
};
