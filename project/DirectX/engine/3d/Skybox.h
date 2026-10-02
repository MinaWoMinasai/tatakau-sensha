#pragma once
#include "Object3d.h"
#include "TextureManager.h"
#include "DirectXCommon.h"

namespace cg2 {

class Skybox {
public:
    void Initialize(const std::string& textureFilePath);
    void Update(Camera* camera, DebugCamera* debugCamera);
    void Draw();
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
