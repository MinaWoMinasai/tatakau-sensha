#pragma once
#include <vector>
#include <string>
#include <wrl.h>
#include <d3d12.h>
#include "DirectXCommon.h"
#include "Calculation.h"
#include "Struct.h"

struct CylinderEffectConfig {
    float lifeTime = 0.85f;
    float startRadius = 2.0f;
    float endRadius = 12.0f;
    float startHeight = 2.0f;
    float endHeight = 28.0f;
    Vector3 rotate = { 0.0f, 0.0f, 0.0f };
    Vector4 startColor = { 0.1f, 0.85f, 1.0f, 0.95f };
    Vector4 endColor = { 0.3f, 0.15f, 1.0f, 0.0f };
    uint32_t divisions = 96;
};

class CylinderManager {
public:
    static const uint32_t kMaxVertices = 12288;

    void Initialize(DirectXCommon* dxCommon, const std::string& textureFilePath);
    void Emit(const Vector3& position, const CylinderEffectConfig& config = {});
    void Update(float deltaTime);
    void DrawAll(const Matrix4x4& viewProjection);
    void Clear() { cylinders_.clear(); }

private:
    struct ActiveCylinder {
        Vector3 position = {};
        CylinderEffectConfig config = {};
        float currentTime = 0.0f;
    };

    static float EaseOut(float t);
    static Vector4 Lerp(const Vector4& start, const Vector4& end, float t);
    static Vector3 TransformPoint(const Vector3& point, const Matrix4x4& matrix);

    DirectXCommon* dxCommon_ = nullptr;
    std::string textureFilePath_;
    std::vector<ActiveCylinder> cylinders_;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    TrailVertex* vertexData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> viewProjectionResource_;
    Matrix4x4* viewProjectionData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;
};
