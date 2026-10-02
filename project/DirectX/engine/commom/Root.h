#pragma once
#include <d3d12.h>
#include <span>
#include <wrl.h>
#include <cassert>
#include "LogWrite.h"

namespace cg2 {

/// @brief ルートパラメーターと静的サンプラーの配置を保持する。
class Root {
public:
    /// @brief オブジェクトを向けに初期化する。
    void InitalizeForObject();
    /// @brief 粒子を向けに初期化する。
    void InitalizeForParticle();
    /// @brief モデル粒子を向けに初期化する。
    void InitalizeForModelParticle();
    /// @brief 用の後処理演出を初期化する。
    void InitializeForPostEffect();
    /// @brief 影を向けに初期化する。
    void InitalizeForShadow();
    /// @brief 用のスキニング影を初期化する。
    void InitializeForSkinningShadow();
    /// @brief オブジェクトBeを向けに初期化する。
    void InitalizeForObjectBe();
    /// @brief 軌跡を向けに初期化する。
    void InitalizeForTrail();
    /// @brief 用の背景を初期化する。
    void InitializeForSkybox();
    /// @brief 用の海面を初期化する。
    void InitializeForOcean();
    /// @brief 用の海面計算を初期化する。
    void InitializeForOceanCompute();
    /// @brief 用の計算粒子を初期化する。
    void InitializeForComputeParticle();

    /// @brief 指定条件でインスタンスまたはリソースを生成する。
    void Create(Microsoft::WRL::ComPtr<ID3D12Device>& device);

    /// @brief Description識別情報を返す。
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
    /// @brief 識別情報Blobを返す。
    ID3DBlob* GetSignatureBlob()
    {
        return signatureBlob_;
    }
    /// @brief エラーBlobを返す。
    ID3DBlob* GetErrorBlob()
    {
        return errorBlob_;
    }
    /// @brief 識別情報を返す。
    Microsoft::WRL::ComPtr<ID3D12RootSignature> GetSignature()
    {
        return signature_;
    }

private:
    D3D12_ROOT_SIGNATURE_DESC descriptionSignature_{};
    D3D12_ROOT_PARAMETER Parameters_[19]{};
    D3D12_DESCRIPTOR_RANGE descriptorRange_[14] = {};
    D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing_[1] = {};
    D3D12_STATIC_SAMPLER_DESC staticSamplers_[2] = {};
    ID3DBlob* signatureBlob_ = nullptr;
    ID3DBlob* errorBlob_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> signature_ = nullptr;
    LogWrite log;
};

} // namespace cg2
