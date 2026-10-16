#pragma once
// 床、配置、カメラ、入力の投影で共有する、値だけを扱う計算。
#include "Calculation.h"
#include <cmath>
#include <algorithm>

namespace neondepth {

// 部屋はXY平面で、負のZ方向から見る。U x V は+Zで、意味上の高さ方向の
// 法線は-Z。下のモデルの向きは鏡映ではなく正しい回転のまま。
struct FloorBasis {
    myEngine::Vector3 origin{0, 0, 0};
    static constexpr myEngine::Vector3 u{1, 0, 0};
    static constexpr myEngine::Vector3 v{0, 1, 0};
    static constexpr myEngine::Vector3 normal{0, 0, -1};
};

/// @brief 値が有限かを返す。
inline bool Finite(float value)
{
    return std::isfinite(value);
}

/// @brief ベクトルの全成分が有限かを返す。
inline bool Finite(const myEngine::Vector2& v)
{
    return Finite(v.x) && Finite(v.y);
}

/// @brief ベクトルの全成分が有限かを返す。
inline bool Finite(const myEngine::Vector3& v)
{
    return Finite(v.x) && Finite(v.y) && Finite(v.z);
}

/// @brief 行列の全要素が有限かを返す。
inline bool Finite(const myEngine::Matrix4x4& m)
{
    for (const auto& row : m.m) {
        for (float value : row) {
            if (!Finite(value)) {
                return false;
            }
        }
    }
    return true;
}

/// @brief 値が有限で、指定範囲内にあるかを返す。
inline bool InRange(float value, float low, float high)
{
    return Finite(value) && value >= low && value <= high;
}

// エンジンの逆行列（特異行列のガードなしで割り算する）を使う前に行列式を検証する。
// 消去法では可逆かどうかを確かめるだけで、逆行列自体はエンジンの数学関数で求める。
// 問い合わせに失敗した場合は、呼び出し側の直前の行列を保つ。
inline bool TryInverseViewProjection(const myEngine::Matrix4x4& matrix, myEngine::Matrix4x4& output)
{
    if (!Finite(matrix)) {
        return false;
    }
    double values[4][4]{};
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            values[row][column] = matrix.m[row][column];
        }
    }
    double determinant = 1;
    for (int column = 0; column < 4; ++column) {
        int pivot = column;
        for (int row = column + 1; row < 4; ++row) {
            if (std::abs(values[row][column]) > std::abs(values[pivot][column])) {
                pivot = row;
            }
        }
        if (std::abs(values[pivot][column]) <= 1.0e-12) {
            return false;
        }
        if (pivot != column) {
            for (int index = 0; index < 4; ++index) {
                std::swap(values[column][index], values[pivot][index]);
            }
            determinant = -determinant;
        }
        const double diagonal = values[column][column];
        determinant *= diagonal;
        for (int row = column + 1; row < 4; ++row) {
            const double factor = values[row][column] / diagonal;
            for (int index = column + 1; index < 4; ++index) {
                values[row][index] -= factor * values[column][index];
            }
        }
    }
    if (!std::isfinite(determinant) || std::abs(determinant) <= 1.0e-12) {
        return false;
    }
    const auto candidate = myEngine::Inverse(matrix);
    if (!Finite(candidate)) {
        return false;
    }
    const auto identity = myEngine::Multiply(matrix, candidate);
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            if (!Finite(identity.m[row][column]) || std::abs(identity.m[row][column] - (row == column ? 1.0f : 0.0f)) > .02f) {
                return false;
            }
        }
    }
    output = candidate;
    return true;
}

/// @brief モデルを床に配置するための設定。
struct PlacementConfig {
    // 読み込み時に、バインドスケルトンと現在のモデルのバインド境界から1回だけ決める。
    myEngine::Vector3 stableFootLocal{};
    float boundsHeight = 1.5674443f;
    float worldHeight = 18.0f;
    // この素材の正面の候補はローカルの-Zで、初期状態ではワールドの床の-Y。
    float forwardYawOffset = myEngine::pi * 0.5f;
};

/// @brief 1フレーム分の配置の入力。
struct PlacementInput {
    myEngine::Vector3 floorAnchor{}; // 正となるEnemyのXY / 部屋の床のZ。
    myEngine::Vector2 floorOffset{}; // 見た目だけの後退。コライダーは動かさない。
    float hoverHeight = 3.0f;
    float bodyHeading = -myEngine::pi * 0.5f;
    myEngine::Vector3 localLean{}; // 毎回新しく求める範囲付きの角度で、前のポーズからの差分は使わない。
};

/// @brief 配置の計算結果。
struct PlacementResult {
    myEngine::Matrix4x4 world{};
    myEngine::Vector3 footWorld{};
    myEngine::Vector3 localUpWorld{};
    float uniformScale = 1.0f;
};

// 不正な入力では直前の有効な出力をそのまま残す。安定した足元は
// 拡縮・前傾・直立・ヨーの前に中央へ揃え、アニメーションする足で定義し直すことはない。
inline bool TryPlacement(const FloorBasis& floor, const PlacementConfig& config, const PlacementInput& input, PlacementResult& output)
{
    if (!Finite(floor.origin) || !Finite(config.stableFootLocal) || !InRange(config.boundsHeight, 0.001f, 1000.0f) ||
        !InRange(config.worldHeight, 0.1f, 80.0f) || !Finite(config.forwardYawOffset) || !Finite(input.floorAnchor) ||
        !Finite(input.floorOffset) || std::abs(input.floorAnchor.z - floor.origin.z) > 1.0e-4f ||
        !InRange(input.hoverHeight, 0.0f, 80.0f) || !Finite(input.bodyHeading) || !InRange(input.localLean.x, -0.8f, 0.8f) ||
        !InRange(input.localLean.y, -0.8f, 0.8f) || !InRange(input.localLean.z, -0.8f, 0.8f)) {
        return false;
    }
    const float scale = config.worldHeight / config.boundsHeight;
    const myEngine::Vector3 footWorld{input.floorAnchor.x + input.floorOffset.x, input.floorAnchor.y + input.floorOffset.y,
                                      floor.origin.z - input.hoverHeight};
    if (!Finite(footWorld)) {
        return false;
    }
    const auto center = myEngine::MakeTranslateMatrix({-config.stableFootLocal.x, -config.stableFootLocal.y, -config.stableFootLocal.z});
    const auto scaled = myEngine::MakeScaleMatrix({scale, scale, scale});
    const auto lean = myEngine::MakeAffineMatrix({1, 1, 1}, input.localLean, {0, 0, 0});
    const auto upright = myEngine::MakeRotateXMatrix(-myEngine::pi * 0.5f);
    const auto yaw = myEngine::MakeRotateZMatrix(input.bodyHeading + config.forwardYawOffset);
    const auto placed = myEngine::MakeTranslateMatrix(footWorld);
    PlacementResult candidate;
    candidate.world = myEngine::Multiply(
        myEngine::Multiply(myEngine::Multiply(myEngine::Multiply(myEngine::Multiply(center, scaled), lean), upright), yaw), placed);
    candidate.footWorld = footWorld;
    candidate.localUpWorld = myEngine::TransformNormal({0, 1, 0}, candidate.world);
    const float upLength = myEngine::Length(candidate.localUpWorld);
    if (!Finite(candidate.world) || !Finite(upLength) || upLength <= 1.0e-6f) {
        return false;
    }
    candidate.localUpWorld.x /= upLength;
    candidate.localUpWorld.y /= upLength;
    candidate.localUpWorld.z /= upLength;
    candidate.uniformScale = scale;
    output = candidate;
    return true;
}

/// @brief ボス戦用カメラの設定。
struct CameraProfile {
    float tiltDegrees = 20.0f; // 実際の通常ゲームプレイで15 / 20 / 25を比べる。
    float distance = 64.0f;
    float fovY = 0.55f;
    float aspect = 1280.0f / 720.0f;
    float nearClip = 0.1f;
    float farClip = 5000.0f;
};

/// @brief ボス戦用カメラの計算結果。
struct CameraFrame {
    myEngine::Vector3 position{}, rotation{};
    myEngine::Matrix4x4 world{}, view{}, projection{}, viewProjection{}, inverseViewProjection{};
};

/// @brief 注視点とカメラ設定からカメラの行列を求める。不正な入力ならfalseを返し、outputは変更しない。
inline bool TryCamera(const CameraProfile& profile, const myEngine::Vector3& focus, CameraFrame& output)
{
    if (!Finite(focus) || !InRange(profile.tiltDegrees, 0.0f, 35.0f) || !InRange(profile.distance, 5.0f, 500.0f) ||
        !InRange(profile.fovY, 0.1f, 1.5f) || !InRange(profile.aspect, 0.2f, 5.0f) || !InRange(profile.nearClip, 0.01f, 10.0f) ||
        !InRange(profile.farClip, profile.nearClip + 1.0f, 10000.0f)) {
        return false;
    }
    const float tilt = profile.tiltDegrees * myEngine::pi / 180.0f;
    CameraFrame candidate;
    candidate.position = {focus.x, focus.y - profile.distance * std::sin(tilt), focus.z - profile.distance * std::cos(tilt)};
    candidate.rotation = {-tilt, 0, 0};
    candidate.world = myEngine::MakeAffineMatrix({1, 1, 1}, candidate.rotation, candidate.position);
    candidate.view = myEngine::Inverse(candidate.world);
    candidate.projection = myEngine::MakePerspectiveForMatrix(profile.fovY, profile.aspect, profile.nearClip, profile.farClip);
    candidate.viewProjection = myEngine::Multiply(candidate.view, candidate.projection);
    if (!TryInverseViewProjection(candidate.viewProjection, candidate.inverseViewProjection)) {
        return false;
    }
    if (!Finite(candidate.world) || !Finite(candidate.view) || !Finite(candidate.projection) || !Finite(candidate.viewProjection) ||
        !Finite(candidate.inverseViewProjection)) {
        return false;
    }
    output = candidate;
    return true;
}

// 同次座標の変換では、既存のVector4変換の暗黙のw処理や、
// Vector3変換の w == 0 のassertをあえて避ける。
inline bool TryHomogeneousPoint(const myEngine::Vector3& point, const myEngine::Matrix4x4& matrix, myEngine::Vector3& output,
                                bool requirePositiveW = false)
{
    if (!Finite(point) || !Finite(matrix)) {
        return false;
    }
    const double values[4] = {
        double(point.x) * matrix.m[0][0] + double(point.y) * matrix.m[1][0] + double(point.z) * matrix.m[2][0] + matrix.m[3][0],
        double(point.x) * matrix.m[0][1] + double(point.y) * matrix.m[1][1] + double(point.z) * matrix.m[2][1] + matrix.m[3][1],
        double(point.x) * matrix.m[0][2] + double(point.y) * matrix.m[1][2] + double(point.z) * matrix.m[2][2] + matrix.m[3][2],
        double(point.x) * matrix.m[0][3] + double(point.y) * matrix.m[1][3] + double(point.z) * matrix.m[2][3] + matrix.m[3][3]};
    if (!std::isfinite(values[3]) || std::abs(values[3]) <= 1.0e-8 || (requirePositiveW && values[3] <= 0.0)) {
        return false;
    }
    const myEngine::Vector3 candidate{static_cast<float>(values[0] / values[3]), static_cast<float>(values[1] / values[3]),
                                      static_cast<float>(values[2] / values[3])};
    if (!Finite(candidate)) {
        return false;
    }
    output = candidate;
    return true;
}

// 画面外の座標も構図の決定には役立つ。正のwとクリップ深度は必須。
// 矩形範囲の扱いは描画やピッキングの方針で別に処理する。
inline bool TryProject(const myEngine::Vector3& world, const myEngine::Matrix4x4& viewProjection, const myEngine::Vector2& viewport,
                       myEngine::Vector2& output, float* clipDepth = nullptr)
{
    if (!Finite(viewport) || viewport.x <= 0 || viewport.y <= 0) {
        return false;
    }
    myEngine::Vector3 ndc{};
    if (!TryHomogeneousPoint(world, viewProjection, ndc, true)) {
        return false;
    }
    // 床への直接の問い合わせと合わせ、丸める前のクリップ深度で比較する。先にfloatへ
    // 丸めると、farをわずかに超えた点がちょうど1になり、表示中のVPによる
    // 床の問い合わせが正しく却下する点を受け入れてしまう。
    const double clipZ = double(world.x) * viewProjection.m[0][2] + double(world.y) * viewProjection.m[1][2] +
                         double(world.z) * viewProjection.m[2][2] + viewProjection.m[3][2];
    const double clipW = double(world.x) * viewProjection.m[0][3] + double(world.y) * viewProjection.m[1][3] +
                         double(world.z) * viewProjection.m[2][3] + viewProjection.m[3][3];
    const double depth = clipZ / clipW;
    if (!std::isfinite(depth) || depth < 0 || depth > 1) {
        return false;
    }
    const myEngine::Vector2 candidate{(ndc.x * 0.5f + 0.5f) * viewport.x, (0.5f - ndc.y * 0.5f) * viewport.y};
    if (!Finite(candidate)) {
        return false;
    }
    output = candidate;
    if (clipDepth) {
        *clipDepth = float(depth);
    }
    return true;
}

// 同じフレームの揺れを含めた、表示中の行列そのものの逆行列を渡す必要がある。
// 戦闘の揺れの方針はアクターの更新前に確定している。
// 床のピッキングは表示中のnear/farの区間に限るため、深度のクリップ範囲外にある
// 見えない点が攻撃の照準になることはない。
inline bool TryScreenToFloor(const myEngine::Vector2& pixel, const myEngine::Vector2& viewport,
                             const myEngine::Matrix4x4& inverseViewProjection, float floorZ, myEngine::Vector3& output)
{
    if (!Finite(pixel) || !Finite(viewport) || viewport.x <= 0 || viewport.y <= 0 || !Finite(floorZ)) {
        return false;
    }
    const float x = 2.0f * pixel.x / viewport.x - 1.0f;
    const float y = 1.0f - 2.0f * pixel.y / viewport.y;
    myEngine::Vector3 nearPoint{}, farPoint{};
    if (!TryHomogeneousPoint({x, y, 0}, inverseViewProjection, nearPoint) ||
        !TryHomogeneousPoint({x, y, 1}, inverseViewProjection, farPoint)) {
        return false;
    }
    const double dz = double(farPoint.z) - nearPoint.z;
    const double dx = double(farPoint.x) - nearPoint.x;
    const double dy = double(farPoint.y) - nearPoint.y;
    const double length = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (!std::isfinite(length) || length <= 1.0e-8 || std::abs(dz) <= length * 1.0e-6) {
        return false;
    }
    const double t = (double(floorZ) - nearPoint.z) / dz;
    if (!std::isfinite(t) || t < 0.0 || t > 1.0) {
        return false;
    }
    const myEngine::Vector3 candidate{static_cast<float>(nearPoint.x + dx * t), static_cast<float>(nearPoint.y + dy * t), floorZ};
    if (!Finite(candidate)) {
        return false;
    }
    output = candidate;
    return true;
}

// 実際の照準にはこちらを使う。描画に使ったのと同じfloatのVPから表示中の床を直接解く。
// floatの逆行列では、near 0.1/far 5000でカメラの平行移動が大きな項の差として保存され、
// 同次座標の評価をdoubleで行ってもサブピクセルの精度が失われる。このガード付きの
// 2x2の床への制限はその逆行列の丸めを避ける。myEngine::Inverseは引き続き完全な可逆性を検証する。
inline bool TryScreenToFloorFromViewProjection(const myEngine::Vector2& pixel, const myEngine::Vector2& viewport,
                                               const myEngine::Matrix4x4& viewProjection, float floorZ, myEngine::Vector3& output)
{
    if (!Finite(pixel) || !Finite(viewport) || viewport.x <= 0 || viewport.y <= 0 || !Finite(floorZ)) {
        return false;
    }
    myEngine::Matrix4x4 checkedInverse;
    if (!TryInverseViewProjection(viewProjection, checkedInverse)) {
        return false;
    }
    const double x = 2.0 * pixel.x / viewport.x - 1.0, y = 1.0 - 2.0 * pixel.y / viewport.y;
    const double a = viewProjection.m[0][0] - x * viewProjection.m[0][3];
    const double b = viewProjection.m[1][0] - x * viewProjection.m[1][3];
    const double c = viewProjection.m[0][1] - y * viewProjection.m[0][3];
    const double d = viewProjection.m[1][1] - y * viewProjection.m[1][3];
    const double constantW = double(floorZ) * viewProjection.m[2][3] + viewProjection.m[3][3];
    const double first = x * constantW - double(floorZ) * viewProjection.m[2][0] - viewProjection.m[3][0];
    const double second = y * constantW - double(floorZ) * viewProjection.m[2][1] - viewProjection.m[3][1];
    const double determinant = a * d - b * c;
    const double norm = (std::max)({std::abs(a), std::abs(b), std::abs(c), std::abs(d)});
    if (!std::isfinite(determinant) || norm <= 1.0e-12 || std::abs(determinant) <= norm * norm * 1.0e-12) {
        return false;
    }
    const myEngine::Vector3 candidate{float((first * d - b * second) / determinant), float((a * second - first * c) / determinant), floorZ};
    if (!Finite(candidate)) {
        return false;
    }
    const double clipW = double(candidate.x) * viewProjection.m[0][3] + double(candidate.y) * viewProjection.m[1][3] + constantW;
    const double clipZ = double(candidate.x) * viewProjection.m[0][2] + double(candidate.y) * viewProjection.m[1][2] +
                         double(floorZ) * viewProjection.m[2][2] + viewProjection.m[3][2];
    if (!std::isfinite(clipW) || !std::isfinite(clipZ) || clipW <= 1.0e-8) {
        return false;
    }
    const double depth = clipZ / clipW;
    if (!std::isfinite(depth) || depth < 0 || depth > 1) {
        return false;
    }
    output = candidate;
    return true;
}

/// @brief ウィンドウのクライアント座標をビューポート座標へ変換する。
inline bool TryClientToViewport(const myEngine::Vector2& clientPixel, const myEngine::Vector2& clientSize,
                                const myEngine::Vector2& viewportSize, myEngine::Vector2& output)
{
    if (!Finite(clientPixel) || !Finite(clientSize) || !Finite(viewportSize) || clientSize.x <= 0 || clientSize.y <= 0 ||
        viewportSize.x <= 0 || viewportSize.y <= 0) {
        return false;
    }
    const myEngine::Vector2 candidate{clientPixel.x * viewportSize.x / clientSize.x, clientPixel.y * viewportSize.y / clientSize.y};
    if (!Finite(candidate)) {
        return false;
    }
    output = candidate;
    return true;
}

} // namespace neondepth
