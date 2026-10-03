#pragma once
#include "DeveloperTools.h"
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
#include "DirectXCommon.h"
#include "externals/nlohmann/json.hpp"
#include <filesystem>
#include <string>

// Copies the engine's final color before UI. Readback is consumed only after the existing frame fence.
class NeonShowcaseCapture {
public:
    void Request(const std::filesystem::path& directory, const std::string& name, nlohmann::json metadata);
    void Record(cg2::DirectXCommon& dx);
    void Resolve(cg2::DirectXCommon& dx);
    void SetFrameMetadata(nlohmann::json metadata) { if (requested_) metadata_ = std::move(metadata); }
    bool IsBusy() const { return requested_ || pending_; }
    bool HasRequest() const { return requested_; }
    bool WasLastCaptureSuccessful() const { return lastCaptureSucceeded_; }
    void CancelRequest() { requested_ = false; }
    const std::string& GetStatus() const { return status_; }
private:
    Microsoft::WRL::ComPtr<ID3D12Resource> source_, readback_;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint_{};
    UINT64 fence_ = 0, bytes_ = 0;
    UINT width_ = 0, height_ = 0;
    bool requested_ = false, pending_ = false;
    bool lastCaptureSucceeded_ = false;
    std::filesystem::path path_;
    nlohmann::json metadata_;
    std::string status_;
};
#endif
