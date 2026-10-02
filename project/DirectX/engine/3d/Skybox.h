#pragma once
#include "Object3d.h"
#include "TextureManager.h"
#include "DirectXCommon.h"

namespace cg2 {

/// @brief 背景のキューブマップをカメラの周囲に描画する。
class Skybox {
public:
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(const std::string& textureFilePath);
    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    void Update(Camera* camera, DebugCamera* debugCamera);
    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    void Draw();
    /// @brief 色を設定する。
    void SetColor(const Vector4& color);

private:
    std::unique_ptr<Object3d> object_;
    uint32_t textureIndex_ = 0;
    std::string filePath_;

    Object3dCommon* object3dCommon_;
    DirectXCommon* dxCommon_;
    SrvManager* srvManager_;
};

} // namespace cg2
