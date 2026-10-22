#pragma once
#include <string>
#include <wrl.h>
#include <d3d12.h>
#include "DirectXCommon.h"
#include "NeonGridGeometry.h"

namespace myEngine {

/// @brief ステージなどに重ねるネオン格子の描画資源と設定を管理する。
class NeonGridRenderer {
public:
    // 配置済みの支援アクター128体、警告リング、アクター周辺の細かいグリッド、
    // 表示中のステージ輪郭、同時に発生する撃破時の輪郭演出までを含む。
    static constexpr uint32_t kMaxVertices = NeonGridGeometry::kMaxVertices;

    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(DirectXCommon* dxCommon, const std::string& textureFilePath);

    /// @brief フレームを開始する。
    void BeginFrame()
    {
        geometry_.BeginFrame();
    }

    /// @brief 線外観を設定する。
    void SetLineStyle(float softEdgeRatio, float coreIntensity)
    {
        geometry_.SetLineStyle(softEdgeRatio, coreIntensity);
    }

    /// @brief 線を描画待ちに積む。
    void QueueLine(const Vector3& a, const Vector3& b, float lineWidth, const Vector4& color)
    {
        geometry_.QueueLine(a, b, lineWidth, color);
    }

    /// @brief 常にカメラの方を向く線を描画待ちに積む。
    void QueueCameraFacingLine(const Vector3& a, const Vector3& b, float lineWidth, const Vector4& color, const Vector3& cameraForward)
    {
        geometry_.QueueCameraFacingLine(a, b, lineWidth, color, cameraForward);
    }

    // 淡いHDRの芯、色付きの肩、滑らかなハローを1つの連続したプロファイルで描く。
    // 多角形の継ぎ目は共有し、外側の角は丸める。独立した線の端は丸キャップにする。
    // バッチが満杯なら、その図形は丸ごと描画しない。
    void QueueContourLine(const Vector3& a, const Vector3& b, float lineWidth, const Vector4& color, const Vector3& cameraForward,
                          const NeonContourStyle& style = {})
    {
        geometry_.QueueContourLine(a, b, lineWidth, color, cameraForward, style);
    }

    /// @brief 多角形の輪郭線を描画キューに積む。
    void QueueContourPolygon(const Vector3* points, uint32_t pointCount, float lineWidth, const Vector4& color,
                             const Vector3& cameraForward, const NeonContourStyle& style = {})
    {
        geometry_.QueueContourPolygon(points, pointCount, lineWidth, color, cameraForward, style);
    }

    /// @brief 三角形の輪郭線を描画キューに積む。
    void QueueContourTriangle(const Vector3& a, const Vector3& b, const Vector3& c, float lineWidth, const Vector4& color,
                              const NeonContourStyle& style = {})
    {
        geometry_.QueueContourTriangle(a, b, c, lineWidth, color, style);
    }

    /// @brief 矩形の輪郭線を描画キューに積む。
    void QueueContourRectangle(const Vector3& center, const Vector3& size, float lineWidth, const Vector4& color,
                               const NeonContourStyle& style = {})
    {
        geometry_.QueueContourRectangle(center, size, lineWidth, color, style);
    }

    /// @brief カメラの方を向く三角形の輪郭線を描画キューに積む。
    void QueueBillboardContourTriangle(const Vector3& center, float radius, float rotationRad, float lineWidth, const Vector4& color,
                                       const Vector3& cameraRight, const Vector3& cameraUp, const Vector3& cameraForward,
                                       const NeonContourStyle& style = {})
    {
        geometry_.QueueBillboardContourTriangle(center, radius, rotationRad, lineWidth, color, cameraRight, cameraUp, cameraForward, style);
    }

    /// @brief カメラの方を向く矩形の輪郭線を描画キューに積む。
    void QueueBillboardContourRectangle(const Vector3& center, const Vector2& size, float rotationRad, float lineWidth,
                                        const Vector4& color, const Vector3& cameraRight, const Vector3& cameraUp,
                                        const Vector3& cameraForward, const NeonContourStyle& style = {})
    {
        geometry_.QueueBillboardContourRectangle(center, size, rotationRad, lineWidth, color, cameraRight, cameraUp, cameraForward, style);
    }

    /// @brief 三角形を描画待ちに積む。
    void QueueTriangle(const Vector3& a, const Vector3& b, const Vector3& c, float lineWidth, const Vector4& color)
    {
        geometry_.QueueTriangle(a, b, c, lineWidth, color);
    }

    /// @brief カメラの方を向く三角形を描画待ちに積む。
    void QueueBillboardTriangle(const Vector3& center, float radius, float rotationRad, float lineWidth, const Vector4& color,
                                const Vector3& cameraRight, const Vector3& cameraUp, const Vector3& cameraForward)
    {
        geometry_.QueueBillboardTriangle(center, radius, rotationRad, lineWidth, color, cameraRight, cameraUp, cameraForward);
    }

    /// @brief カメラの方を向く長方形を描画待ちに積む。
    void QueueBillboardRectangle(const Vector3& center, const Vector2& size, float rotationRad, float lineWidth, const Vector4& color,
                                 const Vector3& cameraRight, const Vector3& cameraUp, const Vector3& cameraForward)
    {
        geometry_.QueueBillboardRectangle(center, size, rotationRad, lineWidth, color, cameraRight, cameraUp, cameraForward);
    }

    /// @brief カメラの方を向く円を描画待ちに積む。
    void QueueBillboardDisc(const Vector3& center, float radius, const Vector4& color, const Vector3& cameraRight, const Vector3& cameraUp,
                            int segments = 32)
    {
        geometry_.QueueBillboardDisc(center, radius, color, cameraRight, cameraUp, segments);
    }

    /// @brief カメラの方を向く塗りつぶした正多角形を描画待ちに積む。
    void QueueBillboardRegularPolygonFill(const Vector3& center, int segments, float radius, float rotationRad, const Vector2& scale,
                                          const Vector4& color, const Vector3& cameraRight, const Vector3& cameraUp)
    {
        geometry_.QueueBillboardRegularPolygonFill(center, segments, radius, rotationRad, scale, color, cameraRight, cameraUp);
    }

    // 暗い面と光の当たった斜面を、ネオンの縁の下にあるソリッドパスで描く。
    void QueueBeveledPolygonFill(const Vector3* points, uint32_t count, const Vector4& baseColor, const Vector4& tint,
                                 const Vector3& lightDirection)
    {
        geometry_.QueueBeveledPolygonFill(points, count, baseColor, tint, lightDirection);
    }

    /// @brief 機体の立体陰影に使う視点を設定する。nullptrで従来の平面塗りへ戻す。
    void SetBodyShadingEye(const Vector3* eye)
    {
        geometry_.SetBodyShadingEye(eye);
    }

    /// @brief 視点側へ伸びる黒い箱（砲身など）を陰影つきで予約する。
    void QueueShadedBox(const Vector3& base, const Vector3& tip, const Vector3& right, float halfWidth, float height,
                        const Vector4& baseColor, const Vector4& tint)
    {
        geometry_.QueueShadedBox(base, tip, right, halfWidth, height, baseColor, tint);
    }

    /// @brief ワールド全体の格子を描画待ちに積む。
    void QueueWorldGrid(float minX, float maxX, float minY, float maxY, float spacing, float lineWidth, const Vector4& color,
                        float planeZ = -0.04f)
    {
        geometry_.QueueWorldGrid(minX, maxX, minY, maxY, spacing, lineWidth, color, planeZ);
    }

    /// @brief 長方形の枠を描画待ちに積む。
    void QueueRectangle(const Vector3& center, const Vector3& size, float lineWidth, const Vector4& color)
    {
        geometry_.QueueRectangle(center, size, lineWidth, color);
    }

    /// @brief 指定した範囲だけの格子を描画待ちに積む。
    void QueueLocalGrid(const Vector3& center, float radius, float spacing, float lineWidth, const Vector4& color)
    {
        geometry_.QueueLocalGrid(center, radius, spacing, lineWidth, color);
    }

    /// @brief 指定した範囲だけの格子を、範囲の端で切って描画待ちに積む。
    void QueueLocalGridClipped(const Vector3& center, float radius, float spacing, float lineWidth, const Vector4& color, float minX,
                               float maxX, float minY, float maxY, float planeZ = -0.03f)
    {
        geometry_.QueueLocalGridClipped(center, radius, spacing, lineWidth, color, minX, maxX, minY, maxY, planeZ);
    }

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
    uint32_t GetVertexCount() const
    {
        return geometry_.GetVertexCount();
    }

private:
    // HDRの暗いアクター塗りには、キャプチャ用深度の有無どちらでもSceneフォーマットが必要。
    std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, 2> hdrFillPipelines_;
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

} // namespace myEngine
