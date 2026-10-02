#pragma once
#include "DirectXCommon.h"
#include "BloomConstantBuffer.h"

namespace cg2 {

/// @brief 全画面描画で使用するパイプラインの共通操作を提供する。
class PostEffect {
public:
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(DirectXCommon* dxCommon, BloomConstantBuffer* bloomCB);
    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    void Draw(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV, BlendMode blendMode, bool outputToHdr = true);
    /// @brief ブルームExtractを描画する。
    void DrawBloomExtract(D3D12_GPU_DESCRIPTOR_HANDLE sceneSRV, D3D12_GPU_DESCRIPTOR_HANDLE materialSRV);
    /// @brief 合成を描画する。
    void DrawComposite(D3D12_GPU_DESCRIPTOR_HANDLE sceneSRV, D3D12_GPU_DESCRIPTOR_HANDLE bloomSRV, D3D12_GPU_DESCRIPTOR_HANDLE depthSRV,
                       D3D12_GPU_DESCRIPTOR_HANDLE normalSRV, D3D12_GPU_DESCRIPTOR_HANDLE ssrSRV, D3D12_GPU_DESCRIPTOR_HANDLE materialSRV,
                       D3D12_GPU_DESCRIPTOR_HANDLE ssaoSRV, D3D12_GPU_DESCRIPTOR_HANDLE motionVectorSRV);
    /// @brief 動きベクトル解決を描画する。
    void DrawMotionVectorResolve(D3D12_GPU_DESCRIPTOR_HANDLE depthSRV);
    /// @brief 時間方向解決を描画する。
    void DrawTemporalResolve(D3D12_GPU_DESCRIPTOR_HANDLE currentSceneSRV, D3D12_GPU_DESCRIPTOR_HANDLE historySceneSRV,
                             D3D12_GPU_DESCRIPTOR_HANDLE depthSRV, D3D12_GPU_DESCRIPTOR_HANDLE normalSRV,
                             D3D12_GPU_DESCRIPTOR_HANDLE materialSRV, D3D12_GPU_DESCRIPTOR_HANDLE motionVectorSRV);
    /// @brief SSAO解決を描画する。
    void DrawSSAOResolve(D3D12_GPU_DESCRIPTOR_HANDLE depthSRV, D3D12_GPU_DESCRIPTOR_HANDLE normalSRV,
                         D3D12_GPU_DESCRIPTOR_HANDLE materialSRV);
    /// @brief SSAODenoiseを描画する。
    void DrawSSAODenoise(D3D12_GPU_DESCRIPTOR_HANDLE ssaoSRV, D3D12_GPU_DESCRIPTOR_HANDLE depthSRV, D3D12_GPU_DESCRIPTOR_HANDLE normalSRV,
                         D3D12_GPU_DESCRIPTOR_HANDLE materialSRV);
    /// @brief SSR解決を描画する。
    void DrawSSRResolve(D3D12_GPU_DESCRIPTOR_HANDLE sceneSRV, D3D12_GPU_DESCRIPTOR_HANDLE depthSRV, D3D12_GPU_DESCRIPTOR_HANDLE normalSRV,
                        D3D12_GPU_DESCRIPTOR_HANDLE materialSRV);
    /// @brief SSRDenoiseを描画する。
    void DrawSSRDenoise(D3D12_GPU_DESCRIPTOR_HANDLE ssrSRV, D3D12_GPU_DESCRIPTOR_HANDLE depthSRV, D3D12_GPU_DESCRIPTOR_HANDLE normalSRV,
                        D3D12_GPU_DESCRIPTOR_HANDLE materialSRV);
    /// @brief オブジェクト合成を描画する。
    void DrawObjectComposite(D3D12_GPU_DESCRIPTOR_HANDLE objectSRV, D3D12_GPU_DESCRIPTOR_HANDLE bloomSRV, bool outputToHdr = true);
    /// @brief オブジェクト輪郭追加を描画する。
    void DrawObjectOutlineAdd(D3D12_GPU_DESCRIPTOR_HANDLE objectSRV, D3D12_GPU_DESCRIPTOR_HANDLE bloomSRV, bool outputToHdr = true);
    /// @brief オブジェクトブルーム追加を描画する。
    void DrawObjectBloomAdd(D3D12_GPU_DESCRIPTOR_HANDLE bloomSRV, bool outputToHdr = true);

private:
    DirectXCommon* dxCommon_ = nullptr;
    BloomConstantBuffer* bloomCB_;
};

} // namespace cg2
