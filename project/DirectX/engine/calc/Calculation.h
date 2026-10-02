#pragma once
#include "Struct.h"
#include <cassert>
#include <random>

namespace cg2 {

extern std::mt19937 rng;

const float pi = 3.14159265f;

// 加算
Vector2 Add(const Vector2& v1, const Vector2& v2);
/// @brief 指定した値や要素を加える。
Vector3 Add(const Vector3& v1, const Vector3& v2);
/// @brief 指定した値や要素を加える。
Vector4 Add(const Vector4& v1, const Vector4& v2);

// 減算
Vector2 Subtract(const Vector2& v1, const Vector2& v2);
/// @brief 2つの入力の差を返す。
Vector3 Subtract(const Vector3& v1, const Vector3& v2);
/// @brief 2つの入力の差を返す。
Vector4 Subtract(const Vector4& v1, const Vector4& v2);

// スカラー倍
Vector2 Multiply(float scalar, const Vector2& v);
/// @brief 2つの入力の積を返す。
Vector3 Multiply(float scalar, const Vector3& v);

// 内積
float Dot(const Vector3& v1, const Vector3& v2);

// 長さ(ノルム)
float Length(const Vector3& v);

// クロス積
Vector3 Cross(const Vector3& v1, const Vector3& v2);

// 正規化
Vector3 Normalize(const Vector3& v);

/// @brief クォータニオンの内積を返す。
float DotQuaternion(const Quaternion& q0, const Quaternion& q1);
/// @brief クォータニオンを正規化する。
Quaternion NormalizeQuaternion(const Quaternion& quaternion);
/// @brief 2つのクォータニオンを球面線形補間して返す。
Quaternion SlerpQuaternion(const Quaternion& q0, const Quaternion& q1, float t);
/// @brief 回転Axis角度クォータニオンを作成して返す。
Quaternion MakeRotateAxisAngleQuaternion(const Vector3& axis, float angle);

// ベクトル変換
Vector3 TransformNormal(const Vector3& v, const Matrix4x4& m);

// 行列の加算
Matrix4x4 Add(const Matrix4x4& m1, const Matrix4x4& m2);
// 行列の減産
Matrix4x4 Subtract(const Matrix4x4& m1, const Matrix4x4& m2);
// 行列の積
Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2);
// 逆行列
Matrix4x4 Inverse(const Matrix4x4& m);
// 転置行列
Matrix4x4 Transpose(const Matrix4x4& m);
// 単位行列の作成
Matrix4x4 MakeIdentity4x4();

// 平行移動行列
Matrix4x4 MakeTranslateMatrix(const Vector3& translate);
// 拡大縮小行列
Matrix4x4 MakeScaleMatrix(const Vector3& scale);
// 座標変換
Vector3 TransformMatrix(const Vector3& vector, const Matrix4x4& matrix);

/// @brief 行列を座標変換する。
Vector4 TransformMatrix(const Vector4& vector, const Matrix4x4& matrix);

// X軸回転行列
Matrix4x4 MakeRotateXMatrix(float radian);
// Y軸回転行列
Matrix4x4 MakeRotateYMatrix(float radian);
// Z軸回転行列
Matrix4x4 MakeRotateZMatrix(float radian);
/// @brief 回転行列を作成して返す。
Matrix4x4 MakeRotateMatrix(const Quaternion& quaternion);

// 3次元アフィン変換行列
Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);
/// @brief Affine行列を作成して返す。
Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Quaternion& rotate, const Vector3& translate);

// 透視投影行列
Matrix4x4 MakePerspectiveForMatrix(float fovY, float aspectRatio, float nearClip, float farClip);
// 正射影行列
Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);
// ビューポート変換行列
Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth);
// LookAt行列
Matrix4x4 MakeLookAtMatrix(const Vector3& eye, const Vector3& target, const Vector3& up);

/// @brief 指定範囲の乱数を返す。
int Rand(int min, int max);
/// @brief 指定範囲の乱数を返す。
float Rand(float min, float max);
/// @brief 指定範囲の乱数を返す。
Vector2 Rand(const Vector2& min, const Vector2& max);
/// @brief 指定範囲の乱数を返す。
Vector3 Rand(const Vector3& min, const Vector3& max);
/// @brief 指定範囲の乱数を返す。
Vector4 Rand(const Vector4& min = {0.0f, 0.0f, 0.0f, 1.0f}, const Vector4& max = {1.0f, 1.0f, 1.0f, 1.0f});
/// @brief 粒子を作成して返す。
Particle MakeParticle(const Vector3& position, const Vector4& baseColor);
/// @brief Tornado粒子を作成して返す。
TornadoParticle MakeTornadoParticle(const Vector3& center);

// 球と平面の衝突判定
bool IsCollision(const Sphere& sphere, const Plane& plane);

// 球と線分の衝突判定
bool IsCollision(const Segment& segment, const Plane& plane);

// 三角形と線のあたり判定
bool IsCollision(const Segment& segment, const Triangle& triangle);

// 直方体と直方体の当たり判定
bool IsCollision(const AABB& aabb1, const AABB& aabb2);

// 球と直方体のあたり判定
bool IsCollision(const AABB& aabb, const Sphere& sphere);

// 直方体と線の当たり判定
bool IsCollision(const AABB& aabb, const Segment& segmrnt);

/// @brief 点から有限線分までの最短距離を返す。
float DistancePointToSegment(const Vector3& point, const Segment& segment);

// 線と球の当たり判定
bool IsCollision(const Segment& segment, const Sphere& sphere);

// カプセルと線の球の当たり判定
bool IsCollision(const Segment& seg, const Sphere& sphere, float capsuleRadius);

//* 演算子オーバーロード
//---------------------------------------------

// Vector2
inline Vector2 operator+(const Vector2& v1, const Vector2& v2)
{
    return Add(v1, v2);
}
/// @brief 値を減算または符号反転する。
inline Vector2 operator-(const Vector2& v1, const Vector2& v2)
{
    return Subtract(v1, v2);
}
/// @brief 積を求める。
inline Vector2 operator*(float s, const Vector2& v)
{
    return Multiply(s, v);
}
/// @brief 積を求める。
inline Vector2 operator*(const Vector2& v, float s)
{
    return Multiply(s, v);
}
/// @brief 値を除算する。
inline Vector2 operator/(const Vector2& v, float s)
{
    return Multiply(1.0f / s, v);
}

// Vector3
/// @brief 値を加算する。
inline Vector3 operator+(const Vector3& v1, const Vector3& v2)
{
    return Add(v1, v2);
}
/// @brief 値を減算または符号反転する。
inline Vector3 operator-(const Vector3& v1, const Vector3& v2)
{
    return Subtract(v1, v2);
}
/// @brief 積を求める。
inline Vector3 operator*(float s, const Vector3& v)
{
    return Multiply(s, v);
}
/// @brief 積を求める。
inline Vector3 operator*(const Vector3& v, float s)
{
    return Multiply(s, v);
}
/// @brief 値を除算する。
inline Vector3 operator/(const Vector3& v, float s)
{
    return Multiply(1.0f / s, v);
}

// Vector4
/// @brief 値を加算する。
inline Vector4 operator+(const Vector4& v1, const Vector4& v2)
{
    return {v1.x + v2.x, v1.y + v2.y, v1.z + v2.z, v1.w + v2.w};
}
/// @brief 値を減算または符号反転する。
inline Vector4 operator-(const Vector4& v1, const Vector4& v2)
{
    return {v1.x - v2.x, v1.y - v2.y, v1.z - v2.z, v1.w - v2.w};
}
/// @brief 積を求める。
inline Vector4 operator*(const Vector4& v, float s)
{
    return {v.x * s, v.y * s, v.z * s, v.w * s};
}
/// @brief 値を除算する。
inline Vector4 operator/(const Vector4& v, float s)
{
    return {v.x / s, v.y / s, v.z / s, v.w / s};
}
/// @brief 値を減算または符号反転する。
inline Vector4 operator-(const Vector4& v)
{
    return {-v.x, -v.y, -v.z, -v.w};
}

// Matrix4x4
/// @brief 値を加算する。
inline Matrix4x4 operator+(const Matrix4x4& m1, const Matrix4x4& m2)
{
    return Add(m1, m2);
}
/// @brief 値を減算または符号反転する。
inline Matrix4x4 operator-(const Matrix4x4& m1, const Matrix4x4& m2)
{
    return Subtract(m1, m2);
}
/// @brief 積を求める。
inline Matrix4x4 operator*(const Matrix4x4& m1, const Matrix4x4& m2)
{
    return Multiply(m1, m2);
}

// 単項演算子
inline Vector3 operator-(const Vector3& v)
{
    return {-v.x, -v.y, -v.z};
}
/// @brief 値を加算する。
inline Vector3 operator+(const Vector3& v)
{
    return v;
}

/// @brief 画面上の2D座標をワールド座標へ変換する。
Vector3 ScreenToWorld2D(const Vector2& screenPos, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix, float windowWidth,
                        float windowHeight);
/// @brief 画面上の座標をワールドのZ=0平面へ投影する。
Vector3 ScreenToWorldOnZ0(const Vector2& screenPos, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix, float windowWidth,
                          float windowHeight);
/// @brief 画面上の座標と深度をワールド座標へ変換する。
Vector3 ScreenToWorld3D(const Vector2& screenPos, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix, float windowWidth,
                        float windowHeight, float distanceFromCamera);

/// @brief 乱数を使って単位方向ベクトルを生成する。
Vector3 RandomUnitVector();

// ワールドトランスフォームの初期化
Transform InitWorldTransform();

/// @brief 指定した値を左方向への動きに変換する。
Vector3 SlideLeft(const Vector3& dir);

/// @brief 指定した値を右方向への動きに変換する。
Vector3 SlideRight(const Vector3& dir);

/// @brief 球VsOBBを確認する。
CollisionResult CheckSphereVsOBB(const Sphere& s, const OBB& o);

// 影用の行列を計算する関数例
Matrix4x4 CalculateLightViewProjection();

/// @brief 制御点列からCatmull-Rom補間で位置を求めて返す。
Vector3 CatmullRom(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t);

} // namespace cg2
