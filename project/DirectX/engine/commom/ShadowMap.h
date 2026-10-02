#pragma once
#include "DirectXCommon.h"
#include "SrvManager.h"
#include <wrl.h>

namespace cg2 {

/// @brief 影描画用の深度リソースとライトからの視点を管理する。
class ShadowMap {
public:
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t width, uint32_t height);

    /// @brief リソースを返す。
    ID3D12Resource* GetResource()
    {
        return resource_.Get();
    }
    /// @brief DSVハンドルを返す。
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle()
    {
        return dsvHandle_;
    }
    /// @brief SRV添字を返す。
    uint32_t GetSrvIndex() const
    {
        return srvIndex_;
    }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_;
    uint32_t srvIndex_;
};

} // namespace cg2
