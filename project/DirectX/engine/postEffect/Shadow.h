#pragma once
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "SceneManager.h"
#include "ShadowMap.h"

namespace cg2 {

/// @brief 影描画用のパイプラインと描画準備を管理する。
class Shadow {
public:
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    /// @brief 後続の描画で使う描画先・パイプラインを準備する。対応するPostDrawと組にして使う。
    void PreDraw();
    /// @brief 描画後の処理を行い、次の描画または合成に必要な状態へ進める。
    void PostDraw();

    /// @brief 影マップを返す。
    ShadowMap* GetShadowMap()
    {
        return shadowMap_.get();
    }

private:
    // 便利関数：リソースバリアの切り替え
    void Transition(ID3D12Resource* res, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    std::unique_ptr<ShadowMap> shadowMap_;
};

} // namespace cg2
