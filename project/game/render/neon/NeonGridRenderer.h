#pragma once
#include <algorithm>
#include <cmath>
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

    /// @brief 線のディゾルブ（育つ戦車の試作の、自機の武器の切り替え）。ボスのネオンディソルブを線に置き換えたもの。
    /// @note 有効な間に積んだ線・輪郭の線・多角形は、pieceLength ほどの欠片に分ける。欠片の中点を走査の方向（direction）に
    /// origin から ±extent の範囲で 0〜1 にし、ノイズを混ぜた値が progress より小さい欠片は消し、progress + edgeWidth までの欠片は
    /// edgeColor で光らせる。progress が 0 なら何も変えず、1 + edgeWidth で全部消える。
    struct LineDissolve {
        Vector3 origin{};
        Vector3 direction{0.70710678f, -0.70710678f, 0.0f};
        float extent = 1.0f;
        float progress = 0.0f;
        float noise = 0.35f;
        float edgeWidth = 0.12f;
        float pieceLength = 0.15f;
        Vector4 edgeColor{1.9f, 0.35f, 1.3f, 1.0f};
    };
    /// @brief ここから EndLineDissolve までに積む線へ、ディゾルブを掛ける。
    void BeginLineDissolve(const LineDissolve& dissolve)
    {
        dissolve_ = dissolve;
        dissolveActive_ = true;
    }
    void EndLineDissolve()
    {
        dissolveActive_ = false;
    }

    /// @brief 線を描画待ちに積む。
    void QueueLine(const Vector3& a, const Vector3& b, float lineWidth, const Vector4& color)
    {
        if (dissolveActive_) {
            ForEachDissolvedPiece(a, b, [&](const Vector3& p0, const Vector3& p1, bool edge) {
                geometry_.QueueLine(p0, p1, edge ? lineWidth * 1.3f : lineWidth, edge ? EdgeColor(color) : color);
            });
            return;
        }
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
        if (dissolveActive_) {
            QueueDissolvedContour(a, b, lineWidth, color, cameraForward, style);
            return;
        }
        geometry_.QueueContourLine(a, b, lineWidth, color, cameraForward, style);
    }

    /// @brief 多角形の輪郭線を描画キューに積む。
    void QueueContourPolygon(const Vector3* points, uint32_t pointCount, float lineWidth, const Vector4& color,
                             const Vector3& cameraForward, const NeonContourStyle& style = {})
    {
        if (dissolveActive_) {
            for (uint32_t i = 0; i < pointCount; ++i) {
                QueueDissolvedContour(points[i], points[(i + 1) % pointCount], lineWidth, color, cameraForward, style);
            }
            return;
        }
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
    /// @brief a から b の線を欠片に分け、残る欠片ごとに emit(始点, 終点, 境目か) を呼ぶ。
    template <class Emit>
    void ForEachDissolvedPiece(const Vector3& a, const Vector3& b, Emit&& emit) const
    {
        const Vector3 delta{b.x - a.x, b.y - a.y, b.z - a.z};
        const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
        const float pieceLength = (std::max)(0.01f, dissolve_.pieceLength);
        const int pieces = (std::clamp)(static_cast<int>(std::ceil(length / pieceLength)), 1, 24);
        const float extent = (std::max)(0.001f, dissolve_.extent);
        for (int i = 0; i < pieces; ++i) {
            const float t0 = static_cast<float>(i) / static_cast<float>(pieces);
            const float t1 = static_cast<float>(i + 1) / static_cast<float>(pieces);
            const Vector3 p0{a.x + delta.x * t0, a.y + delta.y * t0, a.z + delta.z * t0};
            const Vector3 p1{a.x + delta.x * t1, a.y + delta.y * t1, a.z + delta.z * t1};
            const float rx = (p0.x + p1.x) * 0.5f - dissolve_.origin.x;
            const float ry = (p0.y + p1.y) * 0.5f - dissolve_.origin.y;
            const float scan = (std::clamp)(((rx * dissolve_.direction.x + ry * dissolve_.direction.y) / extent + 1.0f) * 0.5f, 0.0f, 1.0f);
            // 欠片のマスごとのノイズ（自機の中心からの位置で決めるので、切り替えの間は同じ欠片が同じ値になる）。
            uint32_t hash = static_cast<uint32_t>(static_cast<int>(std::floor(rx / pieceLength))) * 73856093u ^
                            static_cast<uint32_t>(static_cast<int>(std::floor(ry / pieceLength))) * 19349663u;
            hash = (hash ^ (hash >> 13)) * 0x5bd1e995u;
            hash ^= hash >> 15;
            const float noise = static_cast<float>(hash & 0xffffu) / 65535.0f;
            const float value = scan * (1.0f - dissolve_.noise) + noise * dissolve_.noise;
            if (value < dissolve_.progress) {
                continue;
            }
            emit(p0, p1, dissolve_.progress > 0.001f && value < dissolve_.progress + dissolve_.edgeWidth);
        }
    }
    /// @brief 輪郭の線をディゾルブで欠片にして積む。
    void QueueDissolvedContour(const Vector3& a, const Vector3& b, float lineWidth, const Vector4& color, const Vector3& cameraForward,
                               const NeonContourStyle& style)
    {
        ForEachDissolvedPiece(a, b, [&](const Vector3& p0, const Vector3& p1, bool edge) {
            geometry_.QueueContourLine(p0, p1, edge ? lineWidth * 1.3f : lineWidth, edge ? EdgeColor(color) : color, cameraForward, style);
        });
    }
    /// @brief 境目の欠片の色（元の線の透明度を残す）。
    Vector4 EdgeColor(const Vector4& color) const
    {
        return {dissolve_.edgeColor.x, dissolve_.edgeColor.y, dissolve_.edgeColor.z, dissolve_.edgeColor.w * color.w};
    }

    LineDissolve dissolve_{};
    bool dissolveActive_ = false;
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
