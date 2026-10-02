#pragma once
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "SceneManager.h"
#include "ShadowMap.h"

namespace cg2 {

class Shadow {
public:
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    void PreDraw();
    void PostDraw();

    ShadowMap* GetShadowMap() { return shadowMap_.get(); }

private:
    // 便利関数：リソースバリアの切り替え
    void Transition(ID3D12Resource* res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    std::unique_ptr<ShadowMap> shadowMap_;
};

} // namespace cg2
