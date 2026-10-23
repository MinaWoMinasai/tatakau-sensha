#pragma once
#include "MathTypes.h"

namespace myEngine {

/// @brief 中心と半径で球を表す。
struct Sphere {
    Vector3 center; // 中心点
    float radius;   // 半径
};

/// @brief 基点と方向で無限直線を表す。
struct Line {
    Vector3 origin; // 始点
    Vector3 diff;   // 終点への差分ベクトル
};

/// @brief 始点と方向で半直線を表す。
struct Ray {
    Vector3 origin; // 始点
    Vector3 diff;   // 終点への差分ベクトル
};

/// @brief 始点と差分で有限線分を表す。
struct Segment {
    Vector3 origin; // 始点
    Vector3 diff;   // 終点への差分ベクトル
};

/// @brief 線分と半径でカプセル形状を表す。
struct Capsule {
    Segment segment;
    float radius;
};

/// @brief 法線と原点からの距離で平面を表す。
struct Plane {
    Vector3 normal; // 法線
    float distance; // 距離
};

/// @brief 3頂点で三角形を表す。
struct Triangle {
    Vector3 vertex[3]; // 頂点
};

/// @brief 各軸の最小・最大値で軸に平行な境界箱を表す。
struct AABB {
    Vector3 min; // 最小値
    Vector3 max; // 最大値
};

/// @brief 中心・各軸の向き・半サイズで回転可能な境界箱を表す。
struct OBB {
    Vector3 center;
    Vector3 halfExtents;
    Vector3 orientation[3]; // 正規化済み
};

/// @brief 衝突の有無と接触位置・方向などの解決情報を表す。
struct CollisionResult {
    bool hit = false;   // 衝突しているか
    Vector3 normal;     // 押し戻し方向（正規化済み）
    float depth = 0.0f; // 侵入量（押し戻す距離）
};

} // namespace myEngine
