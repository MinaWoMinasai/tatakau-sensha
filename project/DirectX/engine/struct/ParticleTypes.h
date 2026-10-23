#pragma once
#include "GeometryTypes.h"
#include <cstdint>

namespace myEngine {

/// @brief 1粒子の姿勢・速度・色・寿命などの状態を表す。
struct Particle {
    Transform transform;
    Vector3 velocity;
    Vector3 acceleration;
    Vector3 angularVelocity;
    Vector3 kVelocity;
    Vector4 color;
    float lifeTime;
    float currentTime;
};

/// @brief 粒子の発生位置・範囲・頻度を表す。
struct Emitter {
    Transform transform; // エミッタの位置
    uint32_t count;      // 発生数
    float frequency;     // 発生頻度
    float frequencyTime; // 頻度用時刻
};

/// @brief 粒子などへ加える加速度と、その作用範囲を表す。
struct AccelerationField {
    Vector3 acceleration; // 加速度
    AABB area;            // 効果範囲
};

} // namespace myEngine
