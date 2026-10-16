#pragma once
#include "DeveloperTools.h"
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
#include "DirectXCommon.h"
#include "externals/nlohmann/json.hpp"
#include <filesystem>
#include <string>

// UI描画前のエンジンの最終カラーをコピーする。読み戻しは既存のフレームフェンス通過後にだけ使う。
class NeonShowcaseCapture {
public:
    /// @brief 次のフレームの最終カラーを指定した場所へ保存するよう要求する。
    void Request(const std::filesystem::path& directory, const std::string& name, nlohmann::json metadata);
    /// @brief 要求があれば、UI描画前の最終カラーを読み戻し用バッファへコピーするコマンドを積む。
    void Record(myEngine::DirectXCommon& dx);
    /// @brief フェンス通過後に読み戻したデータをPNGとメタデータとして書き出す。
    void Resolve(myEngine::DirectXCommon& dx);

    void SetFrameMetadata(nlohmann::json metadata)
    {
        if (requested_) {
            metadata_ = std::move(metadata);
        }
    }

    bool IsBusy() const
    {
        return requested_ || pending_;
    }

    bool HasRequest() const
    {
        return requested_;
    }

    /// @brief 直前のキャプチャが成功したかを返す。
    bool WasLastCaptureSuccessful() const
    {
        return lastCaptureSucceeded_;
    }

    /// @brief 保留中のキャプチャ要求を取り消す。
    void CancelRequest()
    {
        requested_ = false;
    }

    const std::string& GetStatus() const
    {
        return status_;
    }

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
