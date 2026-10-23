#pragma once
#include <d3d12.h>
#include <span>
#include <wrl.h>
#include <cassert>
#include "LogWrite.h"

namespace myEngine {

/// @brief ルートパラメーターと静的サンプラーの配置を保持する。
class Root {
public:
    /// @brief 3Dモデル（Object3d・スキニング）用の配置にする。
    void InitializeForObject();
    /// @brief 板ポリゴンのパーティクル用の配置にする。
    void InitializeForParticle();
    /// @brief モデルを使うパーティクル用の配置にする。
    void InitializeForModelParticle();
    /// @brief 全画面のポストエフェクト用の配置にする。
    void InitializeForPostEffect();
    /// @brief シャドウマップへの描画用の配置にする。
    void InitializeForShadow();
    /// @brief スキニングモデルのシャドウマップ描画用の配置にする。
    void InitializeForSkinningShadow();
    /// @brief 軌跡（トレイル）用の配置にする。
    void InitializeForTrail();
    /// @brief スカイボックス用の配置にする。
    void InitializeForSkybox();
    /// @brief GPUパーティクルのコンピュートシェーダー用の配置にする。
    void InitializeForComputeParticle();

    /// @brief InitializeFor〜で決めた配置から、ルートシグネチャを作る。
    void Create(Microsoft::WRL::ComPtr<ID3D12Device>& device);

    /// @brief ルートシグネチャの記述を返す。
    D3D12_ROOT_SIGNATURE_DESC GetDescriptionSignature()
    {
        return descriptionSignature_;
    }

    /// @brief パラメーターを返す。
    std::span<const D3D12_ROOT_PARAMETER> GetParameters() const
    {
        return Parameters_;
    }

    /// @brief ディスクリプター範囲を返す。
    std::span<const D3D12_DESCRIPTOR_RANGE> GetDescriptorRange() const
    {
        return descriptorRange_;
    }

    /// @brief シリアライズしたルートシグネチャを返す。
    ID3DBlob* GetSignatureBlob()
    {
        return signatureBlob_.Get();
    }

    /// @brief シリアライズに失敗したときのエラーメッセージを返す。
    ID3DBlob* GetErrorBlob()
    {
        return errorBlob_.Get();
    }

    /// @brief ルートシグネチャを返す。
    Microsoft::WRL::ComPtr<ID3D12RootSignature> GetSignature()
    {
        return signature_;
    }

private:
    /// @brief 決めた配置をシリアライズして、signatureBlob_ に入れる。
    /// @note 失敗したら、エラーの内容があればログへ出して assert で止める。
    void SerializeSignature();

    D3D12_ROOT_SIGNATURE_DESC descriptionSignature_{};
    D3D12_ROOT_PARAMETER Parameters_[19]{};
    D3D12_DESCRIPTOR_RANGE descriptorRange_[14] = {};
    D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing_[1] = {};
    D3D12_STATIC_SAMPLER_DESC staticSamplers_[2] = {};
    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob_;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> signature_ = nullptr;
    LogWrite log;
};

} // namespace myEngine
