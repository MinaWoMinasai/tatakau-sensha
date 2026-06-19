#pragma once
#include <string>
#include <wrl.h>
#include <d3d12.h>
#include "DirectXCommon.h"
#include "Struct.h"

class NeonGridRenderer {
public:
    static const uint32_t kMaxVertices = 196608;

    void Initialize(DirectXCommon* dxCommon, const std::string& textureFilePath);
    void BeginFrame();
    void SetLineStyle(float softEdgeRatio, float coreIntensity);
    void QueueLine(const Vector3& a, const Vector3& b, float lineWidth, const Vector4& color);
    void QueueCameraFacingLine(const Vector3& a, const Vector3& b, float lineWidth, const Vector4& color, const Vector3& cameraForward);
    void QueueTriangle(const Vector3& a, const Vector3& b, const Vector3& c, float lineWidth, const Vector4& color);
    void QueueBillboardTriangle(
        const Vector3& center,
        float radius,
        float rotationRad,
        float lineWidth,
        const Vector4& color,
        const Vector3& cameraRight,
        const Vector3& cameraUp,
        const Vector3& cameraForward);
    void QueueBillboardRectangle(
        const Vector3& center,
        const Vector2& size,
        float rotationRad,
        float lineWidth,
        const Vector4& color,
        const Vector3& cameraRight,
        const Vector3& cameraUp,
        const Vector3& cameraForward);
    void QueueWorldGrid(float minX, float maxX, float minY, float maxY, float spacing, float lineWidth, const Vector4& color);
    void QueueRectangle(const Vector3& center, const Vector3& size, float lineWidth, const Vector4& color);
    void QueueLocalGrid(const Vector3& center, float radius, float spacing, float lineWidth, const Vector4& color);
    void QueueLocalGridClipped(const Vector3& center, float radius, float spacing, float lineWidth, const Vector4& color, float minX, float maxX, float minY, float maxY);
    void DrawAll(const Matrix4x4& viewProjection);
    void DrawRange(uint32_t startVertex, uint32_t vertexCount, const Matrix4x4& viewProjection);
    uint32_t GetVertexCount() const { return vertexCount_; }

private:
    void AddLineQuad(const Vector3& a, const Vector3& b, float width, const Vector4& color);
    void AddCameraFacingLineQuad(const Vector3& a, const Vector3& b, float width, const Vector4& color, const Vector3& cameraForward);
    void AddSoftLineQuad(const Vector3& a, const Vector3& b, const Vector3& normalDir, float width, const Vector4& color);
    void PushLineStrip(
        const Vector3& a,
        const Vector3& b,
        const Vector3& normalDir,
        float offset0,
        float offset1,
        const Vector4& color0,
        const Vector4& color1);
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
    float lineSoftEdgeRatio_ = 0.42f;
    float lineCoreIntensity_ = 1.35f;
};
