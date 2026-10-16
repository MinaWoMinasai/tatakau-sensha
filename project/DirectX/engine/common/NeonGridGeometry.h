#pragma once
#include <span>
#include "Struct.h"

namespace myEngine {

// アクターや障害物の輪郭を呼び出しごとに明示的に描画する。グリッド、予兆表示、
// 弾の先端とはあえてSetLineStyleの状態を共有しない。
struct NeonContourStyle {
    float coreWidthRatio = 0.18f;
    float coreIntensity = 2.8f;
    float coreWhiteMix = 0.60f;
    float shoulderIntensity = 1.8f;
    float haloWidthScale = 3.4f;
    float haloIntensity = 1.1f;
    float haloAlpha = 0.18f;
    // 細分化した線や端点を共有する線は、キャップを省いて1本あたり42頂点に収められる。
    bool roundCaps = true;
};

// アクターは静止時も色付きの縁を保つ。弾や攻撃の閃光は、シーン内の輪郭をすべて
// 白飛びさせることなく、より明るいプロファイルを使える。
inline NeonContourStyle ActorNeonContourStyle()
{
    // 白熱した輪と締まった色付きのハローで、内側の黒い本体を読み取りやすく保つ。
    NeonContourStyle style;
    style.coreWidthRatio = 0.20f;
    style.coreIntensity = 2.8f;
    style.coreWhiteMix = 0.45f;
    style.shoulderIntensity = 1.55f;
    style.haloWidthScale = 2.1f;
    style.haloIntensity = 0.50f;
    style.haloAlpha = 0.08f;
    return style;
}

/// @brief 借用した頂点配列へネオン格子・輪郭の形状を生成する。GPUの資源・描画を扱わない。
class NeonGridGeometry {
public:
    // 配置済みの支援アクター128体、警告リング、アクター周辺の細かいグリッド、
    // 表示中のステージ輪郭、同時に発生する撃破時の輪郭演出までを含む。
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
    // 淡いHDRの芯、色付きの肩、滑らかなハローを1つの連続したプロファイルで描く。
    // 多角形の継ぎ目は共有し、外側の角は丸める。独立した線の端は丸キャップにする。
    // バッチが満杯なら、その図形は丸ごと描画しない。
    void QueueContourLine(const Vector3& a, const Vector3& b, float lineWidth, const Vector4& color, const Vector3& cameraForward,
                          const NeonContourStyle& style = {});
    void QueueContourPolygon(const Vector3* points, uint32_t pointCount, float lineWidth, const Vector4& color,
                             const Vector3& cameraForward, const NeonContourStyle& style = {});
    void QueueContourTriangle(const Vector3& a, const Vector3& b, const Vector3& c, float lineWidth, const Vector4& color,
                              const NeonContourStyle& style = {});
    void QueueContourRectangle(const Vector3& center, const Vector3& size, float lineWidth, const Vector4& color,
                               const NeonContourStyle& style = {});
    void QueueBillboardContourTriangle(const Vector3& center, float radius, float rotationRad, float lineWidth, const Vector4& color,
                                       const Vector3& cameraRight, const Vector3& cameraUp, const Vector3& cameraForward,
                                       const NeonContourStyle& style = {});
    void QueueBillboardContourRectangle(const Vector3& center, const Vector2& size, float rotationRad, float lineWidth,
                                        const Vector4& color, const Vector3& cameraRight, const Vector3& cameraUp,
                                        const Vector3& cameraForward, const NeonContourStyle& style = {});
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
    // 暗い面と光の当たった斜面を、ネオンの縁の下にあるソリッドパスで描く。
    // 本体の陰影用の視点が設定されている間は、輪郭を視点側へ持ち上げて光沢のある黒い
    // 立体にする（丸い輪郭は滑らかなドーム、多角形は面取りした錐台）。
    void QueueBeveledPolygonFill(const Vector3* points, uint32_t count, const Vector4& baseColor, const Vector4& tint,
                                 const Vector3& lightDirection);
    /// @brief 機体の立体陰影に使う視点を設定する。nullptrで従来の平面塗りへ戻す。
    void SetBodyShadingEye(const Vector3* eye);
    /// @brief 視点側へ伸びる黒い箱（砲身など）を陰影つきで予約する。立体陰影の視点が必要。
    void QueueShadedBox(const Vector3& base, const Vector3& tip, const Vector3& right, float halfWidth, float height,
                        const Vector4& baseColor, const Vector4& tint);
    /// @brief ワールド格子を後で処理するために予約する。
    void QueueWorldGrid(float minX, float maxX, float minY, float maxY, float spacing, float lineWidth, const Vector4& color,
                        float planeZ = -0.04f);
    /// @brief Rectangleを後で処理するために予約する。
    void QueueRectangle(const Vector3& center, const Vector3& size, float lineWidth, const Vector4& color);
    /// @brief 局所格子を後で処理するために予約する。
    void QueueLocalGrid(const Vector3& center, float radius, float spacing, float lineWidth, const Vector4& color);
    /// @brief 局所格子Clippedを後で処理するために予約する。
    void QueueLocalGridClipped(const Vector3& center, float radius, float spacing, float lineWidth, const Vector4& color, float minX,
                               float maxX, float minY, float maxY, float planeZ = -0.03f);

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
    /// @brief 黒い艶のある機体表面の色を求める（拡散・機体色のフレネル反射・白いハイライト）。
    Vector4 ShadeBody(const Vector3& position, const Vector3& normal, const Vector4& baseColor, const Vector4& tint) const;
    /// @brief 視点側を向く三角形だけを頂点ごとの色で積む。
    void PushShadedTriangle(const Vector3& a, const Vector3& b, const Vector3& c, const Vector4& ca, const Vector4& cb, const Vector4& cc);
    /// @brief 輪郭を視点側へ持ち上げた立体を積む。
    void QueueShadedBody(const Vector3* points, uint32_t count, const Vector4& baseColor, const Vector4& tint);

    bool bodyShading_ = false;
    Vector3 bodyEye_{};

    TrailVertex* vertexData_ = nullptr;
    uint32_t vertexCount_ = 0;
    uint32_t vertexCapacity_ = 0;
    float lineSoftEdgeRatio_ = 0.42f;
    float lineCoreIntensity_ = 1.35f;
};

} // namespace myEngine
