#pragma once
#include <vector>
#include <string>
#include <map>
#include <random>
#include <wrl.h>
#include <d3d12.h>
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "ModelManager.h"
#include "Camera.h"
#include <nlohmann/json.hpp>

class DebugCamera;

// エミッター形状
enum class EmitterShape : uint32_t { Point = 0, Sphere = 1, Box = 2 };
// イージングタイプ
enum class EasingType : uint32_t { Linear = 0, EaseIn = 1, EaseOut = 2 };

// エフェクト設定構造体
struct ParticleEmitterConfig {
    Vector3 position = { 0, 0, 0 };
    float speedMin = 0.0f;
    float speedMax = 1.0f;
    float lifeTimeMin = 2.0f;
    float lifeTimeMax = 3.0f;
    Vector3 gravity = { 0.0f, -0.08f, 0.0f };
    float startScale = 1.0f;
    float endScale = 0.0f;
    Vector4 startColor = { 1, 1, 1, 1 };
    Vector4 endColor = { 1, 1, 1, 0 };
    std::string modelPath = "triangleParticle.obj";
    EmitterShape emitterShape = EmitterShape::Point;
    Vector3 shapeSize = { 1.0f, 1.0f, 1.0f };
    EasingType easingType = EasingType::Linear;
    bool isBillboard = false;

    nlohmann::json ToJson() const;
    void FromJson(const nlohmann::json& j);
};

class ParticleManager {
public:
    // GPUに送るパーティクル1粒のデータ
    struct ParticleGPU {
        Vector3 position;    float currentTime;
        Vector3 velocity;    float lifeTime;
        Vector3 acceleration; float startScale;
        Vector4 startColor;
        Vector4 endColor;
        float   endScale;    uint32_t isActive;
        uint32_t easingType; uint32_t isBillboard;
        Vector3 rotate;      float padding1;
        Vector3 angularVelocity; float padding2;
    };

    struct Particle {
        Transform transform;
        Vector3 velocity;
        Vector3 acceleration;
        Vector3 angularVelocity;
        float lifeTime;
        float currentTime;
        float startScale;
        float endScale;
        Vector4 startColor;
        Vector4 endColor;
        EasingType easingType;
        bool isBillboard;
    };

    // シェーダー用定数バッファ構造体
    struct ModelParticleTransformationMatrix {
        Matrix4x4 WVP;
        Matrix4x4 World;
        Matrix4x4 WorldInverseTranspose;
        Vector4 color;
    };

    struct GlobalConfig { float deltaTime; uint32_t maxParticles; };
    struct SceneConfig { Matrix4x4 viewProjection; Vector3 cameraPosition; float scenePadding; };

    static ParticleManager* GetInstance();
    static const uint32_t kMaxInstance = 100000; // 実行環境に合わせて調整

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    void Update(float deltaTime, Camera* camera, DebugCamera* debugCamera = nullptr);
    void Dispatch(float deltaTime, Camera* camera);
    void Draw();

    void RegisterEffect(const std::string& effectName, const std::string& jsonPath);
    void Emit(const std::string& effectName, const Vector3& position, uint32_t count);
    void Emit(const ::Particle& particle);

private:
    ParticleManager() = default;
    ~ParticleManager() = default;
    void EmitBatch(const std::vector<Particle>& particles);
    Particle MakeParticle(const ParticleEmitterConfig& config);

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    std::map<std::string, ParticleEmitterConfig> effectLibrary_;
    Model* model_ = nullptr;

    // リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> particleResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> drawArgsResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> aliveIndicesResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> computeConfigResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> computeSceneResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> emitStagingResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> resetResource_;
    Microsoft::WRL::ComPtr<ID3D12CommandSignature> commandSignature_;

    uint32_t srvIndexInstancing_ = 0;
    uint32_t uavIndexParticles_ = 0;
    uint32_t uavIndexRenderData_ = 0;
    uint32_t uavIndexAliveIndices_ = 0;
    uint32_t uavIndexDrawArgs_ = 0;

    uint32_t freeIndex_ = 0;
};
