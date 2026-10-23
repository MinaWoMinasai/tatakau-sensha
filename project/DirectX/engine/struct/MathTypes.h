#pragma once

namespace myEngine {

/// @brief 2成分の座標・方向・大きさを表す値型。
struct Vector2 {
    float x;
    float y;
};

/// @brief 3成分の座標・方向・大きさを表す値型。
struct Vector3 {
    float x;
    float y;
    float z;

    /// @brief 乗算して自身へ反映する。
    inline Vector3& operator*=(float s)
    {
        x *= s;
        y *= s;
        z *= s;
        return *this;
    }

    /// @brief 減算して自身へ反映する。
    inline Vector3& operator-=(const Vector3& v)
    {
        x -= v.x;
        y -= v.y;
        z -= v.z;
        return *this;
    }

    /// @brief 加算して自身へ反映する。
    inline Vector3& operator+=(const Vector3& v)
    {
        x += v.x;
        y += v.y;
        z += v.z;
        return *this;
    }

    /// @brief 除算して自身へ反映する。
    inline Vector3& operator/=(float s)
    {
        x /= s;
        y /= s;
        z /= s;
        return *this;
    }
};

/// @brief 4成分の値を表す。色や同次座標などの用途に応じて成分を解釈する。
struct Vector4 {
    float x, y, z, w;

    /// @brief 加算して自身へ反映する。
    inline Vector4& operator+=(const Vector4& v)
    {
        x += v.x;
        y += v.y;
        z += v.z;
        w += v.w;
        return *this;
    }

    /// @brief 減算して自身へ反映する。
    inline Vector4& operator-=(const Vector4& v)
    {
        x -= v.x;
        y -= v.y;
        z -= v.z;
        w -= v.w;
        return *this;
    }

    /// @brief 乗算して自身へ反映する。
    inline Vector4& operator*=(float s)
    {
        x *= s;
        y *= s;
        z *= s;
        w *= s;
        return *this;
    }

    /// @brief 除算して自身へ反映する。
    inline Vector4& operator/=(float s)
    {
        x /= s;
        y /= s;
        z /= s;
        w /= s;
        return *this;
    }
};

/// @brief 拡大率・回転・平行移動による3Dの姿勢を表す。
struct Transform {
    Vector3 scale;
    Vector3 rotate;
    Vector3 translate;
};

/// @brief 姿勢とワールド行列をまとめた配置データを表す。
struct Actor {
    Transform transform;
    float speed;
};

// 3x3の行列
/// @brief 3行3列の行列を表す。演算規約はCalculationの行列関数に従う。
struct Matrix3x3 {
    float m[3][3];
};

// 4x4の行列
/// @brief 4行4列の行列を表す。演算規約はCalculationの行列関数に従う。
struct Matrix4x4 {
    float m[4][4];
};

/// @brief 3Dの回転をクォータニオンで表す値型。
struct Quaternion {
    float x;
    float y;
    float z;
    float w;
};

/// @brief 座標軸。
enum AxisXYZ {
    X,
    Y,
    Z,
};

} // namespace myEngine
