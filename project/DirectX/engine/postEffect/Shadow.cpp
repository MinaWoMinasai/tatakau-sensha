#include "Shadow.h"

void Shadow::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {

    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    
    // シャドウマップの初期化
    shadowMap_ = std::make_unique<ShadowMap>();
    shadowMap_->Initialize(dxCommon_, srvManager_, 2048, 2048);

}

void Shadow::PreDraw() {

    // ==========================================
    // 1. シャドウマップへの描き込み (Depth Only Pass)
    // ==========================================
    // リソースを DEPTH_WRITE に変更
    Transition(shadowMap_->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    
    // シャドウ用のDSVをセットし、レンダーターゲットはNULLにする
    auto dsvHandle = shadowMap_->GetDSVHandle(); // DSVハンドル
    dxCommon_->GetList()->OMSetRenderTargets(0, nullptr, false, &dsvHandle);
    dxCommon_->GetList()->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    
    // シャドウ用ビューポート設定 (2048x2048など)
    dxCommon_->SetViewport(2048, 2048);
    
    // 影用PSOを使用して描画 (PSなしの軽量パス)
    // 内部で SetPipelineState(shadowPSO.graphicsState_) を呼ぶ
    SceneManager::GetInstance()->DrawShadow();
    
    // 書き込み終わったので SHADER_RESOURCE に戻す
    Transition(shadowMap_->GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

}

void Shadow::Transition(ID3D12Resource* res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {

    assert(res != nullptr);
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = res;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    dxCommon_->GetList()->ResourceBarrier(1, &barrier);

}