#pragma once
#include "Calculation.h"
#include <wrl.h>
#include <d3d12.h>
#include "Texture.h"

namespace cg2 {

/// @brief GPUバッファの作成とマップ領域を管理する。
class Resource {

public:
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize();

    /// @brief VBVを生成する。
    D3D12_VERTEX_BUFFER_VIEW CreateVBV(const ModelData& modelData, Texture texture, Microsoft::WRL::ComPtr<ID3D12Device>& device,
                                       Microsoft::WRL::ComPtr<ID3D12Resource>& vertexResource);

    /// @brief 材質を生成する。
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateMaterial(Texture texture, Microsoft::WRL::ComPtr<ID3D12Device>& device);

    /// @brief WVPを生成する。
    void CreateWVP(Texture texture, Microsoft::WRL::ComPtr<ID3D12Device>& device, Microsoft::WRL::ComPtr<ID3D12Resource>& wvpResource,
                   TransformationMatrix*& wvpData);

    /// @brief 平行光のライトを生成する。
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateDirectionalLight(Texture texture, Microsoft::WRL::ComPtr<ID3D12Device>& device);

private:
};

} // namespace cg2
