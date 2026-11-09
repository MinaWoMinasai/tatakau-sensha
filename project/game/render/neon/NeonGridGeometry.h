#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
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

/// @brief 線と塗りのディゾルブ（育つ戦車の試作の、自機の武器の切り替え）。ボスのネオンディソルブを線と塗りに置き換えたもの。
/// @note 欠片（線は pieceLength ほどの長さ、塗りはそれほどの大きさの三角形）の中点を、走査の方向（direction）に origin から
/// ±extent の範囲で 0〜1 にし、ノイズを混ぜた値が progress より小さい欠片は消し、progress + edgeWidth までの欠片は edgeColor で光らせる。
/// progress が 0 なら何も変えず、1 + edgeWidth で全部消える。
struct NeonLineDissolve {
    Vector3 origin{};
    Vector3 direction{0.70710678f, -0.70710678f, 0.0f};
    float extent = 1.0f;
    float progress = 0.0f;
    float noise = 0.35f;
    float edgeWidth = 0.12f;
    float pieceLength = 0.15f;
    Vector4 edgeColor{1.9f, 0.35f, 1.3f, 1.0f};
};

/// @brief 欠片の中点 point のディゾルブの値（0〜1）。
inline float NeonDissolveValue(const NeonLineDissolve& dissolve, const Vector3& point)
{
    const float pieceLength = (std::max)(0.01f, dissolve.pieceLength);
    const float extent = (std::max)(0.001f, dissolve.extent);
    const float rx = point.x - dissolve.origin.x;
    const float ry = point.y - dissolve.origin.y;
    const float scan = (std::clamp)(((rx * dissolve.direction.x + ry * dissolve.direction.y) / extent + 1.0f) * 0.5f, 0.0f, 1.0f);
    // 欠片のマスごとのノイズ（自機の中心からの位置で決めるので、切り替えの間は同じ欠片が同じ値になる）。
    uint32_t hash = static_cast<uint32_t>(static_cast<int>(std::floor(rx / pieceLength))) * 73856093u ^
                    static_cast<uint32_t>(static_cast<int>(std::floor(ry / pieceLength))) * 19349663u;
    hash = (hash ^ (hash >> 13)) * 0x5bd1e995u;
    hash ^= hash >> 15;
    const float noise = static_cast<float>(hash & 0xffffu) / 65535.0f;
    return scan * (1.0f - dissolve.noise) + noise * dissolve.noise;
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
    /// @brief 線を描画待ちに積む。
    void QueueLine(const Vector3& a, const Vector3& b, float lineWidth, const Vector4& color);
    /// @brief 常にカメラの方を向く線を描画待ちに積む。
    void QueueCameraFacingLine(const Vector3& a, const Vector3& b, float lineWidth, const Vector4& color, const Vector3& cameraForward);
    // 淡いHDRの芯、色付きの肩、滑らかなハローを1つの連続したプロファイルで描く。
    // 多角形の継ぎ目は共有し、外側の角は丸める。独立した線の端は丸キャップにする。
    // バッチが満杯なら、その図形は丸ごと描画しない。
    void QueueContourLine(const Vector3& a, const Vector3& b, float lineWidth, const Vector4& color, const Vector3& cameraForward,
                          const NeonContourStyle& style = {});
    /// @brief 多角形の輪郭線を描画キューに積む。
    void QueueContourPolygon(const Vector3* points, uint32_t pointCount, float lineWidth, const Vector4& color,
                             const Vector3& cameraForward, const NeonContourStyle& style = {});
    /// @brief 三角形の輪郭線を描画キューに積む。
    void QueueContourTriangle(const Vector3& a, const Vector3& b, const Vector3& c, float lineWidth, const Vector4& color,
                              const NeonContourStyle& style = {});
    /// @brief 矩形の輪郭線を描画キューに積む。
    void QueueContourRectangle(const Vector3& center, const Vector3& size, float lineWidth, const Vector4& color,
                               const NeonContourStyle& style = {});
    /// @brief カメラの方を向く三角形の輪郭線を描画キューに積む。
    void QueueBillboardContourTriangle(const Vector3& center, float radius, float rotationRad, float lineWidth, const Vector4& color,
                                       const Vector3& cameraRight, const Vector3& cameraUp, const Vector3& cameraForward,
                                       const NeonContourStyle& style = {});
    /// @brief カメラの方を向く矩形の輪郭線を描画キューに積む。
    void QueueBillboardContourRectangle(const Vector3& center, const Vector2& size, float rotationRad, float lineWidth,
                                        const Vector4& color, const Vector3& cameraRight, const Vector3& cameraUp,
                                        const Vector3& cameraForward, const NeonContourStyle& style = {});
    /// @brief 三角形を描画待ちに積む。
    void QueueTriangle(const Vector3& a, const Vector3& b, const Vector3& c, float lineWidth, const Vector4& color);
    /// @brief カメラの方を向く三角形を描画待ちに積む。
    void QueueBillboardTriangle(const Vector3& center, float radius, float rotationRad, float lineWidth, const Vector4& color,
                                const Vector3& cameraRight, const Vector3& cameraUp, const Vector3& cameraForward);
    /// @brief カメラの方を向く長方形を描画待ちに積む。
    void QueueBillboardRectangle(const Vector3& center, const Vector2& size, float rotationRad, float lineWidth, const Vector4& color,
                                 const Vector3& cameraRight, const Vector3& cameraUp, const Vector3& cameraForward);
    /// @brief カメラの方を向く円を描画待ちに積む。
    void QueueBillboardDisc(const Vector3& center, float radius, const Vector4& color, const Vector3& cameraRight, const Vector3& cameraUp,
                            int segments = 32);
    /// @brief カメラの方を向く塗りつぶした正多角形を描画待ちに積む。
    void QueueBillboardRegularPolygonFill(const Vector3& center, int segments, float radius, float rotationRad, const Vector2& scale,
                                          const Vector4& color, const Vector3& cameraRight, const Vector3& cameraUp);
    // 暗い面と光の当たった斜面を、ネオンの縁の下にあるソリッドパスで描く。
    // 本体の陰影用の視点が設定されている間は、輪郭を視点側へ持ち上げて光沢のある黒い
    // 立体にする（丸い輪郭は滑らかなドーム、多角形は面取りした錐台）。
    void QueueBeveledPolygonFill(const Vector3* points, uint32_t count, const Vector4& baseColor, const Vector4& tint,
                                 const Vector3& lightDirection);
    /// @brief 機体の立体陰影に使う視点を設定する。nullptrで従来の平面塗りへ戻す。
    void SetBodyShadingEye(const Vector3* eye);
    /// @brief 陰影つきの塗り（QueueBeveledPolygonFill・QueueShadedBox）へディゾルブを掛ける。nullptr で外す。
    /// @note 掛けている間は、塗りの三角形を pieceLength ほどの大きさまで割り、欠片ごとに消す・光らせる。
    void SetFillDissolve(const NeonLineDissolve* dissolve);
    /// @brief 視点側へ伸びる黒い箱（砲身など）を陰影つきで予約する。立体陰影の視点が必要。
    void QueueShadedBox(const Vector3& base, const Vector3& tip, const Vector3& right, float halfWidth, float height,
                        const Vector4& baseColor, const Vector4& tint);
    /// @brief ワールド全体の格子を描画待ちに積む。
    void QueueWorldGrid(float minX, float maxX, float minY, float maxY, float spacing, float lineWidth, const Vector4& color,
                        float planeZ = -0.04f);
    /// @brief 長方形の枠を描画待ちに積む。
    void QueueRectangle(const Vector3& center, const Vector3& size, float lineWidth, const Vector4& color);
    /// @brief 指定した範囲だけの格子を描画待ちに積む。
    void QueueLocalGrid(const Vector3& center, float radius, float spacing, float lineWidth, const Vector4& color);
    /// @brief 指定した範囲だけの格子を、範囲の端で切って描画待ちに積む。
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
    /// @brief カメラの方を向く線を、四角形の頂点として追加する。
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

    /// @brief ディゾルブを掛けた三角形を、欠片の大きさまで割ってから積む（depth は残りの分け方の回数）。
    void PushDissolvedTriangle(const Vector3& a, const Vector3& b, const Vector3& c, const Vector4& ca, const Vector4& cb,
                               const Vector4& cc, int depth);

    bool bodyShading_ = false;
    Vector3 bodyEye_{};
    NeonLineDissolve fillDissolve_{};
    bool fillDissolveActive_ = false;

    TrailVertex* vertexData_ = nullptr;
    uint32_t vertexCount_ = 0;
    uint32_t vertexCapacity_ = 0;
    float lineSoftEdgeRatio_ = 0.42f;
    float lineCoreIntensity_ = 1.35f;
};

} // namespace myEngine
