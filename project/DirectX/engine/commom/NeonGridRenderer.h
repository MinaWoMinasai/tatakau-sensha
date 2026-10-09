#pragma once
#include <string>
#include <wrl.h>
#include <d3d12.h>
#include "DirectXCommon.h"
#include "NeonGridGeometry.h"

namespace cg2 {

/// @brief ステージなどに重ねるネオン格子の描画資源と設定を管理する。
class NeonGridRenderer {
public:
    // Includes 128 authored support actors, warning rings, dense actor-local
    // grids, visible stage outlines, and a simultaneous death-outline burst.
    static constexpr uint32_t kMaxVertices = NeonGridGeometry::kMaxVertices;

    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(DirectXCommon* dxCommon, const std::string& textureFilePath);
    /// @brief フレームを開始する。
    void BeginFrame() { geometry_.BeginFrame(); }
    /// @brief 線外観を設定する。
    void SetLineStyle(float softEdgeRatio, float coreIntensity) { geometry_.SetLineStyle(softEdgeRatio, coreIntensity); }
    /// @brief 線を後で処理するために予約する。
    void QueueLine(const Vector3& a, const Vector3& b, float lineWidth, const Vector4& color) { geometry_.QueueLine(a, b, lineWidth, color); }
    /// @brief カメラFacing線を後で処理するために予約する。
    void QueueCameraFacingLine(const Vector3& a, const Vector3& b, float lineWidth, const Vector4& color, const Vector3& cameraForward) { geometry_.QueueCameraFacingLine(a, b, lineWidth, color, cameraForward); }
    // Pale HDR core, colored shoulder, and smooth halo in a single continuous
    // profile. Polygon joins are shared, with round exterior corners; free lines
    // have round caps. A complete primitive is skipped if the batch is full.
    void QueueContourLine(const Vector3& a, const Vector3& b, float lineWidth, const Vector4& color,
                          const Vector3& cameraForward, const NeonContourStyle& style = {}) { geometry_.QueueContourLine(a, b, lineWidth, color, cameraForward, style); }
    void QueueContourPolygon(const Vector3* points, uint32_t pointCount, float lineWidth, const Vector4& color,
                             const Vector3& cameraForward, const NeonContourStyle& style = {}) { geometry_.QueueContourPolygon(points, pointCount, lineWidth, color, cameraForward, style); }
    void QueueContourTriangle(const Vector3& a, const Vector3& b, const Vector3& c, float lineWidth, const Vector4& color,
                              const NeonContourStyle& style = {}) { geometry_.QueueContourTriangle(a, b, c, lineWidth, color, style); }
    void QueueContourRectangle(const Vector3& center, const Vector3& size, float lineWidth, const Vector4& color,
                               const NeonContourStyle& style = {}) { geometry_.QueueContourRectangle(center, size, lineWidth, color, style); }
    void QueueBillboardContourTriangle(const Vector3& center, float radius, float rotationRad, float lineWidth, const Vector4& color,
                                       const Vector3& cameraRight, const Vector3& cameraUp, const Vector3& cameraForward,
                                       const NeonContourStyle& style = {}) { geometry_.QueueBillboardContourTriangle(center, radius, rotationRad, lineWidth, color, cameraRight, cameraUp, cameraForward, style); }
    void QueueBillboardContourRectangle(const Vector3& center, const Vector2& size, float rotationRad, float lineWidth, const Vector4& color,
                                        const Vector3& cameraRight, const Vector3& cameraUp, const Vector3& cameraForward,
                                        const NeonContourStyle& style = {}) { geometry_.QueueBillboardContourRectangle(center, size, rotationRad, lineWidth, color, cameraRight, cameraUp, cameraForward, style); }
    /// @brief 三角形を後で処理するために予約する。
    void QueueTriangle(const Vector3& a, const Vector3& b, const Vector3& c, float lineWidth, const Vector4& color) { geometry_.QueueTriangle(a, b, c, lineWidth, color); }
    /// @brief ビルボード三角形を後で処理するために予約する。
    void QueueBillboardTriangle(const Vector3& center, float radius, float rotationRad, float lineWidth, const Vector4& color,
                                const Vector3& cameraRight, const Vector3& cameraUp, const Vector3& cameraForward) { geometry_.QueueBillboardTriangle(center, radius, rotationRad, lineWidth, color, cameraRight, cameraUp, cameraForward); }
    /// @brief ビルボードRectangleを後で処理するために予約する。
    void QueueBillboardRectangle(const Vector3& center, const Vector2& size, float rotationRad, float lineWidth, const Vector4& color,
                                 const Vector3& cameraRight, const Vector3& cameraUp, const Vector3& cameraForward) { geometry_.QueueBillboardRectangle(center, size, rotationRad, lineWidth, color, cameraRight, cameraUp, cameraForward); }
    /// @brief ビルボードDiscを後で処理するために予約する。
    void QueueBillboardDisc(const Vector3& center, float radius, const Vector4& color, const Vector3& cameraRight, const Vector3& cameraUp,
                            int segments = 32) { geometry_.QueueBillboardDisc(center, radius, color, cameraRight, cameraUp, segments); }
    /// @brief ビルボードRegularPolygon塗りつぶしを後で処理するために予約する。
    void QueueBillboardRegularPolygonFill(const Vector3& center, int segments, float radius, float rotationRad, const Vector2& scale,
                                          const Vector4& color, const Vector3& cameraRight, const Vector3& cameraUp) { geometry_.QueueBillboardRegularPolygonFill(center, segments, radius, rotationRad, scale, color, cameraRight, cameraUp); }
    // A dark face and a lit bevel, drawn in the solid pass below its neon rim.
    void QueueBeveledPolygonFill(const Vector3* points, uint32_t count, const Vector4& baseColor,
                                 const Vector4& tint, const Vector3& lightDirection) { geometry_.QueueBeveledPolygonFill(points, count, baseColor, tint, lightDirection); }
    /// @brief ワールド格子を後で処理するために予約する。
    void QueueWorldGrid(float minX, float maxX, float minY, float maxY, float spacing, float lineWidth, const Vector4& color) { geometry_.QueueWorldGrid(minX, maxX, minY, maxY, spacing, lineWidth, color); }
    /// @brief Rectangleを後で処理するために予約する。
    void QueueRectangle(const Vector3& center, const Vector3& size, float lineWidth, const Vector4& color) { geometry_.QueueRectangle(center, size, lineWidth, color); }
    /// @brief 局所格子を後で処理するために予約する。
    void QueueLocalGrid(const Vector3& center, float radius, float spacing, float lineWidth, const Vector4& color) { geometry_.QueueLocalGrid(center, radius, spacing, lineWidth, color); }
    /// @brief 局所格子Clippedを後で処理するために予約する。
    void QueueLocalGridClipped(const Vector3& center, float radius, float spacing, float lineWidth, const Vector4& color, float minX,
                               float maxX, float minY, float maxY) { geometry_.QueueLocalGridClipped(center, radius, spacing, lineWidth, color, minX, maxX, minY, maxY); }
    /// @brief 全体を描画する。
    void DrawAll(const Matrix4x4& viewProjection, bool noDepth = false);
    /// @brief 範囲を描画する。
    void DrawRange(uint32_t startVertex, uint32_t vertexCount, const Matrix4x4& viewProjection, bool noDepth = false);
    /// @brief 範囲単色を描画する。
    void DrawRangeSolid(uint32_t startVertex, uint32_t vertexCount, const Matrix4x4& viewProjection);
    /// @brief HDRシーン用の不透明・深度書き込みパイプラインを必要時に作成する。
    bool InitializeSceneSolidPipeline();
    /// @brief HDRシーン内で不透明な立体面を描画し、後続のネオン線を深度で遮蔽する。
    void DrawRangeSceneSolid(uint32_t startVertex, uint32_t vertexCount, const Matrix4x4& viewProjection);
    /// @brief 頂点件数を返す。
    uint32_t GetVertexCount() const { return geometry_.GetVertexCount(); }

private:
    DirectXCommon* dxCommon_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> sceneSolidPipeline_;
    std::string textureFilePath_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    TrailVertex* vertexData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> viewProjectionResource_;
    Matrix4x4* viewProjectionData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;
    NeonGridGeometry geometry_;
};

} // namespace cg2
