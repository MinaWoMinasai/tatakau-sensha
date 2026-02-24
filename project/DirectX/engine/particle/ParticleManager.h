#pragma once
#include <vector>
#include <list>
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "Model.h"
#include "ModelManager.h"
#include "Calculation.h"
#include "Camera.h"
#include "DebugCamera.h"
#include "Resource.h"

class ParticleManager {
public:
    
    // シングルトン
    static ParticleManager* GetInstance();
    
    // パーティクルの最大数
    static const uint32_t kMaxInstance = 10000;

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    void Update(float deltaTime, Camera* camera, DebugCamera* debugCamera);
    void Draw();

    // パーティクル発生
    void Emit(const Particle& particle);

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    // インスタンシング用リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_;
    ParticleForGPU* instancingData_ = nullptr;
    uint32_t srvIndex_; // SrvManagerで割り当てられたインデックス

    // 使用するモデル（plane.objなど）
    Model* model_ = nullptr;

    // パーティクルリスト
    std::list<Particle> particles_;

    Texture texture;
    Resource resource;

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource;

    TransformationMatrix* transformationMatrixData;
    DirectionalLight* directionalLightData;

    Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;
    CameraData* cameraData_ = nullptr;
};