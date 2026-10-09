#pragma once
#include <span>
#include "Struct.h"

namespace cg2 {

// Explicit per-call presentation for actor / obstacle contours. It deliberately
// does not share SetLineStyle state with grids, telegraphs, or projectile heads.
struct NeonContourStyle {
    float coreWidthRatio = 0.18f;
    float coreIntensity = 2.8f;
    float coreWhiteMix = 0.60f;
    float shoulderIntensity = 1.8f;
    float haloWidthScale = 3.4f;
    float haloIntensity = 1.1f;
    float haloAlpha = 0.18f;
    // Shared subdivision / wire endpoints can omit caps at 42 vertices per line.
    bool roundCaps = true;
};

// Actors keep a colored rim at rest; projectiles and attack flashes can use a
// brighter profile without whitening every outline in the scene.
inline NeonContourStyle ActorNeonContourStyle() {
    NeonContourStyle style;
    style.coreWidthRatio = 0.14f;
    style.coreIntensity = 2.6f;
    style.coreWhiteMix = 0.28f;
    style.shoulderIntensity = 1.55f;
    style.haloIntensity = 0.75f;
    style.haloAlpha = 0.12f;
    return style;
}

/// @brief 借用した頂点配列へネオン格子・輪郭の形状を生成する。GPUの資源・描画を扱わない。
class NeonGridGeometry {
public:
    // Includes 128 authored support actors, warning rings, dense actor-local
    // grids, visible stage outlines, and a simultaneous death-outline burst.
    static const uint32_t kMaxVertices = 786432;

    /// @brief 書き込み先を借用し、件数をリセットする。配列は次の付け替えまで有効に保つ。
    void SetVertexBuffer(std::span<TrailVertex> vertices);
    /// @brief フレームを開始する。
    void BeginFrame();
    /// @brief 線外観を設定する。
    void SetLineStyle(float softEdgeRatio, float coreIntensity);
    /// @brief 線を後で処理するために予約する。
    void QueueLine(const Vector3& a, const Vector3& b, float lineWidth, const Vector4& color);
    /// @brief カメラFacing線を後で処理するために予約する。
    void QueueCameraFacingLine(const Vector3& a, const Vector3& b, float lineWidth, const Vector4& color, const Vector3& cameraForward);
    // Pale HDR core, colored shoulder, and smooth halo in a single continuous
    // profile. Polygon joins are shared, with round exterior corners; free lines
    // have round caps. A complete primitive is skipped if the batch is full.
    void QueueContourLine(const Vector3& a, const Vector3& b, float lineWidth, const Vector4& color,
                          const Vector3& cameraForward, const NeonContourStyle& style = {});
    void QueueContourPolygon(const Vector3* points, uint32_t pointCount, float lineWidth, const Vector4& color,
                             const Vector3& cameraForward, const NeonContourStyle& style = {});
    void QueueContourTriangle(const Vector3& a, const Vector3& b, const Vector3& c, float lineWidth, const Vector4& color,
                              const NeonContourStyle& style = {});
    void QueueContourRectangle(const Vector3& center, const Vector3& size, float lineWidth, const Vector4& color,
                               const NeonContourStyle& style = {});
    void QueueBillboardContourTriangle(const Vector3& center, float radius, float rotationRad, float lineWidth, const Vector4& color,
                                       const Vector3& cameraRight, const Vector3& cameraUp, const Vector3& cameraForward,
                                       const NeonContourStyle& style = {});
    void QueueBillboardContourRectangle(const Vector3& center, const Vector2& size, float rotationRad, float lineWidth, const Vector4& color,
                                        const Vector3& cameraRight, const Vector3& cameraUp, const Vector3& cameraForward,
                                        const NeonContourStyle& style = {});
    /// @brief 三角形を後で処理するために予約する。
    void QueueTriangle(const Vector3& a, const Vector3& b, const Vector3& c, float lineWidth, const Vector4& color);
    /// @brief ビルボード三角形を後で処理するために予約する。
    void QueueBillboardTriangle(const Vector3& center, float radius, float rotationRad, float lineWidth, const Vector4& color,
                                const Vector3& cameraRight, const Vector3& cameraUp, const Vector3& cameraForward);
    /// @brief ビルボードRectangleを後で処理するために予約する。
    void QueueBillboardRectangle(const Vector3& center, const Vector2& size, float rotationRad, float lineWidth, const Vector4& color,
                                 const Vector3& cameraRight, const Vector3& cameraUp, const Vector3& cameraForward);
    /// @brief ビルボードDiscを後で処理するために予約する。
    void QueueBillboardDisc(const Vector3& center, float radius, const Vector4& color, const Vector3& cameraRight, const Vector3& cameraUp,
                            int segments = 32);
    /// @brief ビルボードRegularPolygon塗りつぶしを後で処理するために予約する。
    void QueueBillboardRegularPolygonFill(const Vector3& center, int segments, float radius, float rotationRad, const Vector2& scale,
                                          const Vector4& color, const Vector3& cameraRight, const Vector3& cameraUp);
    // A dark face and a lit bevel, drawn in the solid pass below its neon rim.
    void QueueBeveledPolygonFill(const Vector3* points, uint32_t count, const Vector4& baseColor,
                                 const Vector4& tint, const Vector3& lightDirection);
    /// @brief ワールド格子を後で処理するために予約する。
    void QueueWorldGrid(float minX, float maxX, float minY, float maxY, float spacing, float lineWidth, const Vector4& color);
    /// @brief Rectangleを後で処理するために予約する。
    void QueueRectangle(const Vector3& center, const Vector3& size, float lineWidth, const Vector4& color);
    /// @brief 局所格子を後で処理するために予約する。
    void QueueLocalGrid(const Vector3& center, float radius, float spacing, float lineWidth, const Vector4& color);
    /// @brief 局所格子Clippedを後で処理するために予約する。
    void QueueLocalGridClipped(const Vector3& center, float radius, float spacing, float lineWidth, const Vector4& color, float minX,
                               float maxX, float minY, float maxY);

    /// @brief 頂点件数を返す。
    uint32_t GetVertexCount() const
    {
        return vertexCount_;
    }

private:
    /// @brief 線四角形を追加する。
    void AddLineQuad(const Vector3& a, const Vector3& b, float width, const Vector4& color);
    /// @brief カメラFacing線四角形を追加する。
    void AddCameraFacingLineQuad(const Vector3& a, const Vector3& b, float width, const Vector4& color, const Vector3& cameraForward);
    /// @brief Soft線四角形を追加する。
    void AddSoftLineQuad(const Vector3& a, const Vector3& b, const Vector3& normalDir, float width, const Vector4& color);
    /// @brief 連続する点を描画する線列へ追加する。
    void PushLineStrip(const Vector3& a, const Vector3& b, const Vector3& normalDir, float offset0, float offset1, const Vector4& color0,
                       const Vector4& color1);
    /// @brief 描画する頂点を作成中の配列へ追加する。
    void PushVertex(const Vector3& pos, const Vector4& color, const Vector2& uv);

    TrailVertex* vertexData_ = nullptr;
    uint32_t vertexCount_ = 0;
    uint32_t vertexCapacity_ = 0;
    float lineSoftEdgeRatio_ = 0.42f;
    float lineCoreIntensity_ = 1.35f;
};

} // namespace cg2
