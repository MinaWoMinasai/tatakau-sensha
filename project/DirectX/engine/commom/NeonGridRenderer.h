#pragma once
#include <string>
#include <wrl.h>
#include <d3d12.h>
#include "DirectXCommon.h"
#include "Struct.h"

namespace cg2 {

/// @brief ステージなどに重ねるネオン格子の描画資源と設定を管理する。
class NeonGridRenderer {
public:
    static const uint32_t kMaxVertices = 196608;

    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(DirectXCommon* dxCommon, const std::string& textureFilePath);
    /// @brief フレームを開始する。
    void BeginFrame();
    /// @brief 線外観を設定する。
    void SetLineStyle(float softEdgeRatio, float coreIntensity);
    /// @brief 線を後で処理するために予約する。
    void QueueLine(const Vector3& a, const Vector3& b, float lineWidth, const Vector4& color);
    /// @brief カメラFacing線を後で処理するために予約する。
    void QueueCameraFacingLine(const Vector3& a, const Vector3& b, float lineWidth, const Vector4& color, const Vector3& cameraForward);
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
    /// @brief ワールド格子を後で処理するために予約する。
    void QueueWorldGrid(float minX, float maxX, float minY, float maxY, float spacing, float lineWidth, const Vector4& color);
    /// @brief Rectangleを後で処理するために予約する。
    void QueueRectangle(const Vector3& center, const Vector3& size, float lineWidth, const Vector4& color);
    /// @brief 局所格子を後で処理するために予約する。
    void QueueLocalGrid(const Vector3& center, float radius, float spacing, float lineWidth, const Vector4& color);
    /// @brief 局所格子Clippedを後で処理するために予約する。
    void QueueLocalGridClipped(const Vector3& center, float radius, float spacing, float lineWidth, const Vector4& color, float minX,
                               float maxX, float minY, float maxY);
    /// @brief 全体を描画する。
    void DrawAll(const Matrix4x4& viewProjection);
    /// @brief 範囲を描画する。
    void DrawRange(uint32_t startVertex, uint32_t vertexCount, const Matrix4x4& viewProjection);
    /// @brief 範囲単色を描画する。
    void DrawRangeSolid(uint32_t startVertex, uint32_t vertexCount, const Matrix4x4& viewProjection);
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

} // namespace cg2
