#pragma once

#include "RenderTexture.h"
#include "RtvManager.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

namespace cg2 {

// Reuses the scene's HDR pixels without blurring or replacing the sharp source.
// Render records commands only. The caller restores its render target, viewport
// and pipeline afterward, and keeps the shared SrvManager alive until GPU idle.
class BloomPyramid {
public:
    static constexpr uint32_t kMaxLevels = 5;
    struct LevelDimensions { uint32_t width = 1, height = 1; };

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager,
        uint32_t sourceWidth, uint32_t sourceHeight);
    uint32_t Render(uint32_t sourceSrv, const BloomParam& param,
        uint32_t materialSrv = (std::numeric_limits<uint32_t>::max)());
    uint32_t Render(D3D12_GPU_DESCRIPTOR_HANDLE sourceSrv, const BloomParam& param,
        D3D12_GPU_DESCRIPTOR_HANDLE materialSrv = {});

    // A frame-wide comparison override: -1 follows each category's parameters.
    static void SetModeOverride(int mode);
    static uint32_t EffectiveMode(const BloomParam& param);

    uint32_t GetExtractSrvIndex() const;
    uint32_t GetOutputSrvIndex() const { return outputSrv_; }
    uint32_t GetActiveLevelCount() const { return activeLevelCount_; }
    uint32_t GetSourceWidth() const { return sourceWidth_; }
    uint32_t GetSourceHeight() const { return sourceHeight_; }
    const std::array<LevelDimensions, kMaxLevels>& GetLevelDimensions() const { return dimensions_; }

private:
    struct PipelineBundle;
    // Mirrors BloomPyramidCommon.hlsli. Root constants are copied into the
    // command stream per pass, so later draws cannot overwrite their values.
    struct PassConstants {
        uint32_t sourceWidth, sourceHeight;
        float threshold, softKnee;
        float scatter, radius;
        uint32_t materialEnabled;
        float waterBloomEnabled;
        float waterDiagnosticsEnabled;
        uint32_t padding[3]{};
    };
    static_assert(sizeof(PassConstants) == 48);
    static_assert(offsetof(PassConstants, threshold) == 8);
    static_assert(offsetof(PassConstants, scatter) == 16);
    static_assert(offsetof(PassConstants, materialEnabled) == 24);
    static_assert(offsetof(PassConstants, waterDiagnosticsEnabled) == 32);
    static_assert(offsetof(PassConstants, padding) == 36);

    void CreatePipeline();
    void DrawPass(RenderTexture& output, const LevelDimensions& dimensions,
        ID3D12PipelineState* pipeline, D3D12_GPU_DESCRIPTOR_HANDLE source,
        D3D12_GPU_DESCRIPTOR_HANDLE auxiliary, const PassConstants& constants);
    void Transition(RenderTexture& target, D3D12_RESOURCE_STATES before,
        D3D12_RESOURCE_STATES after);

    static int modeOverride_;
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    // A private heap avoids exhausting the scene's already occupied RTV heap.
    std::unique_ptr<RtvManager> rtvManager_;
    std::array<std::unique_ptr<RenderTexture>, kMaxLevels> down_;
    std::array<std::unique_ptr<RenderTexture>, kMaxLevels - 1> up_;
    std::array<LevelDimensions, kMaxLevels> dimensions_{};
    std::shared_ptr<PipelineBundle> pipelines_;
    uint32_t sourceWidth_ = 1, sourceHeight_ = 1;
    uint32_t outputSrv_ = 0, activeLevelCount_ = 0;
};

} // namespace cg2
