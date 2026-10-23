#pragma once
#include "BloomConstantBuffer.h"
#include "BloomPyramid.h"
#include "PostEffect.h"
#include "RenderTexture.h"
#include "RtvManager.h"
#include <memory>

namespace myEngine {

/// @brief オブジェクト用の輪郭線・発光の抽出と合成を管理する。
class ObjectPostEffect {
public:
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, RtvManager* rtvManager, float renderScale = 1.0f,
                    float qualityCaptureScale = 0.0f);
    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Update(float deltaTime = 1.0f / 60.0f);

    /// @brief ここから描くオブジェクトを、専用の描画先へキャプチャし始める。
    void BeginCapture();
    /// @brief 今の深度バッファを使ったまま、キャプチャを始める。
    void BeginCaptureWithCurrentDepth(bool preserveFullResolution = false);
    /// @brief キャプチャを終え、輪郭と発光を合成してシーン（HDR）へ加える。
    void EndCapture();
    /// @brief キャプチャを終え、輪郭と発光を合成してバックバッファへ加える。
    void EndCaptureToBackBuffer();
    /// @brief キャプチャを終え、加算の合成だけを行う。
    void EndCaptureAdditiveOnly();
    /// @brief キャプチャを終え、ブルームだけをシーン（HDR）へ加える。
    void EndCaptureBloomOnly();
    /// @brief キャプチャを終え、ブルームだけをバックバッファへ加える。
    void EndCaptureBloomOnlyToBackBuffer();
    /// @brief キャプチャを終え、ブルームの結果をDrawCachedBloom用に取っておく。
    void EndCaptureBloomOnlyToCache();
    /// @brief キャッシュ済みブルームを描画する。
    /// @note qualityCaptureScale を指定した場合、Legacy とほかのモードを切り替えたらキャプチャを
    /// 取り直す必要がある。元の解像度は BeginCapture の時点で決まる。
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
    /// @brief キャプチャ結果を最後にどう合成するか。
    enum class FinishMode {
        CompositeAndAdd,
        AdditiveOnly,
        BloomOnly,
        BloomOnlyCache,
    };
    /// @brief キャプチャを終え、modeに応じて合成する。outputToHdrならシーン（HDR）へ、そうでなければバックバッファへ出す。
    void FinishCapture(FinishMode mode, bool outputToHdr);
    /// @brief キャプチャした画像からBloomを作成し、結果のSRVインデックスを返す。
    uint32_t RenderBloom(const BloomParam& drawParam);
    /// @brief 描画時に使うBloomパラメータを作る。
    BloomParam MakeDrawParam(bool outputToHdr) const;
    /// @brief キャプチャに使う描画先を選ぶ。
    void SelectCaptureTarget(bool preserveFullResolution = false);
    /// @brief GPUリソースを次の利用に必要な状態へ遷移させる。
    void Transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);
    /// @brief 透明を消去する。
    void ClearTransparent(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle);

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    RtvManager* rtvManager_ = nullptr;

    std::unique_ptr<RenderTexture> objectRT_;
    std::unique_ptr<RenderTexture> qualityObjectRT_;
    std::unique_ptr<RenderTexture> bloomRT_A_;
    std::unique_ptr<RenderTexture> bloomRT_B_;
    std::unique_ptr<RenderTexture> bloomRT_Half_;
    std::unique_ptr<BloomPyramid> bloomPyramid_;
    std::unique_ptr<BloomPyramid> qualityBloomPyramid_;
    RenderTexture* activeCaptureRT_ = nullptr;

    std::unique_ptr<PostEffect> postEffect_;
    std::unique_ptr<BloomConstantBuffer> cb_;
    std::unique_ptr<RtvManager> ownedRtvManager_;
    BloomParam param_{};
    BloomParam cachedFilterParam_{};
    uint32_t cachedBloomSrv_ = 0;
    uint32_t cachedPresentationGeneration_ = 0;
    float timer_ = 0.0f;
    uint32_t renderWidth_ = WinApp::kRenderWidth;
    uint32_t renderHeight_ = WinApp::kRenderHeight;
    uint32_t qualityWidth_ = WinApp::kRenderWidth, qualityHeight_ = WinApp::kRenderHeight;
    uint32_t activeWidth_ = WinApp::kRenderWidth, activeHeight_ = WinApp::kRenderHeight;
    uint32_t halfWidth_ = WinApp::kRenderWidth / 2;
    uint32_t halfHeight_ = WinApp::kRenderHeight / 2;
    uint32_t bloomWidth_ = WinApp::kRenderWidth / 2;
    uint32_t bloomHeight_ = WinApp::kRenderHeight / 2;
    D3D12_CPU_DESCRIPTOR_HANDLE restoreRtvHandle_{};
    D3D12_CPU_DESCRIPTOR_HANDLE restoreDsvHandle_{};
    bool restoreHasDsv_ = false;
    DirectXCommon::RenderTargetState restoreTargets_{};
    bool preserveFullResolutionCapture_ = false;
};

} // namespace myEngine
