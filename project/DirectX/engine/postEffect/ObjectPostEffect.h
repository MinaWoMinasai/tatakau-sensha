#pragma once
#include "BloomConstantBuffer.h"
#include "PostEffect.h"
#include "RenderTexture.h"
#include "RtvManager.h"
#include <memory>

namespace cg2 {

/// @brief オブジェクト用の輪郭線・発光の抽出と合成を管理する。
class ObjectPostEffect {
public:
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, RtvManager* rtvManager, float renderScale = 1.0f);
    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Update(float deltaTime = 1.0f / 60.0f);

    /// @brief 計測を開始する。
    void BeginCapture();
    /// @brief 計測を含む現在深度を開始する。
    void BeginCaptureWithCurrentDepth();
    /// @brief 計測を終了する。
    void EndCapture();
    /// @brief 計測への背面バッファを終了する。
    void EndCaptureToBackBuffer();
    /// @brief 計測Additive専用を終了する。
    void EndCaptureAdditiveOnly();
    /// @brief 計測ブルーム専用を終了する。
    void EndCaptureBloomOnly();
    /// @brief 計測ブルーム専用への背面バッファを終了する。
    void EndCaptureBloomOnlyToBackBuffer();
    /// @brief 計測ブルーム専用へのキャッシュを終了する。
    void EndCaptureBloomOnlyToCache();
    /// @brief キャッシュ済みブルームを描画する。
    void DrawCachedBloom(const Vector2& uvOffset);

    /// @brief パラメーターを返す。
    BloomParam& GetParam()
    {
        return param_;
    }
    /// @brief パラメーターを返す。
    const BloomParam& GetParam() const
    {
        return param_;
    }
    /// @brief パラメーターを設定する。
    void SetParam(const BloomParam& param);

private:
    enum class FinishMode {
        CompositeAndAdd,
        AdditiveOnly,
        BloomOnly,
        BloomOnlyCache,
    };
    /// @brief 計測を終了する。
    void FinishCapture(FinishMode mode, bool outputToHdr);
    /// @brief GPUリソースを次の利用に必要な状態へ遷移させる。
    void Transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);
    /// @brief 透明を消去する。
    void ClearTransparent(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle);

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    RtvManager* rtvManager_ = nullptr;

    std::unique_ptr<RenderTexture> objectRT_;
    std::unique_ptr<RenderTexture> bloomRT_A_;
    std::unique_ptr<RenderTexture> bloomRT_B_;
    std::unique_ptr<RenderTexture> bloomRT_Half_;

    std::unique_ptr<PostEffect> postEffect_;
    std::unique_ptr<BloomConstantBuffer> cb_;
    std::unique_ptr<RtvManager> ownedRtvManager_;
    BloomParam param_{};
    float timer_ = 0.0f;
    uint32_t renderWidth_ = WinApp::kClientWidth;
    uint32_t renderHeight_ = WinApp::kClientHeight;
    uint32_t halfWidth_ = WinApp::kClientWidth / 2;
    uint32_t halfHeight_ = WinApp::kClientHeight / 2;
    uint32_t bloomWidth_ = WinApp::kClientWidth / 2;
    uint32_t bloomHeight_ = WinApp::kClientHeight / 2;
    D3D12_CPU_DESCRIPTOR_HANDLE restoreRtvHandle_{};
    D3D12_CPU_DESCRIPTOR_HANDLE restoreDsvHandle_{};
    bool restoreHasDsv_ = false;
};

} // namespace cg2
