#pragma once
#include <string>
#include <wrl.h>
#include <d3d12.h>
#include "DirectXCommon.h"
#include "Struct.h"

class NeonGridRenderer {
public:
    static const uint32_t kMaxVertices = 65536;

    void Initialize(DirectXCommon* dxCommon, const std::string& textureFilePath);
    void BeginFrame();
    void QueueWorldGrid(float minX, float maxX, float minY, float maxY, float spacing, float lineWidth, const Vector4& color);
    void QueueLocalGrid(const Vector3& center, float radius, float spacing, float lineWidth, const Vector4& color);
    void QueueLocalGridClipped(const Vector3& center, float radius, float spacing, float lineWidth, const Vector4& color, float minX, float maxX, float minY, float maxY);
    void DrawAll(const Matrix4x4& viewProjection);

private:
    void AddLineQuad(const Vector3& a, const Vector3& b, float width, const Vector4& color);
    void PushVertex(const Vector3& pos, const Vector4& color, const Vector2& uv);

    DirectXCommon* dxCommon_ = nullptr;
    std::string textureFilePath_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    TrailVertex* vertexData_ = nullptr;
    uint32_t vertexCount_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> viewProjectionResource_;
    Matrix4x4* viewProjectionData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;
};
