#pragma once
#include <vector>
#include <string>
#include <wrl.h>
#include <d3d12.h>
#include "DirectXCommon.h"
#include "Calculation.h"
#include "Struct.h"

struct RingEffectConfig {
    float lifeTime = 0.7f;
    float startRadius = 1.0f;
    float endRadius = 16.0f;
    float startWidth = 0.8f;
    float endWidth = 2.2f;
    Vector3 rotate = { pi * 0.5f, 0.0f, 0.0f };
    Vector4 startColor = { 0.2f, 0.85f, 1.0f, 1.0f };
    Vector4 endColor = { 0.2f, 0.85f, 1.0f, 0.0f };
    uint32_t divisions = 96;
};

class RingManager {
public:
    static const uint32_t kMaxVertices = 12288;

    void Initialize(DirectXCommon* dxCommon, const std::string& textureFilePath);
    void Emit(const Vector3& position, const RingEffectConfig& config = {});
    void Update(float deltaTime);
    void DrawAll(const Matrix4x4& viewProjection);
    void Clear() { rings_.clear(); }

private:
    struct ActiveRing {
        Vector3 position = {};
        RingEffectConfig config = {};
        float currentTime = 0.0f;
    };

    static float EaseOut(float t);
    static Vector4 Lerp(const Vector4& start, const Vector4& end, float t);
    static Vector3 TransformPoint(const Vector3& point, const Matrix4x4& matrix);

    DirectXCommon* dxCommon_ = nullptr;
    std::string textureFilePath_;
    std::vector<ActiveRing> rings_;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    TrailVertex* vertexData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> viewProjectionResource_;
    Matrix4x4* viewProjectionData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;
};
