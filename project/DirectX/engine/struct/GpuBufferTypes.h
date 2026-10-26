#pragma once
#include "MathTypes.h"

namespace myEngine {

/// @brief ワールド行列とWVPなどの頂点変換情報をGPUへ渡す。
struct TransformationMatrix {
    Matrix4x4 WVP;
    Matrix4x4 World;
    Matrix4x4 WorldInverseTranspose;
};

/// @brief 通常の頂点変換と影用の変換行列をGPUへ渡す。
struct TransformationMatrixWithShadow {
    Matrix4x4 WVP;
    Matrix4x4 World;
    Matrix4x4 WorldInverseTranspose;
    Matrix4x4 LightWVP;
};

/// @brief モデルを使う粒子の姿勢と頂点変換情報をGPUへ渡す。
struct ModelParticleTransformationMatrix {
    Matrix4x4 WVP;
    Matrix4x4 World;
    Matrix4x4 WorldInverseTranspose;
    Vector4 color;
};

/// @brief 粒子描画の行列と色をGPUへ渡す。
struct ParticleForGPU {
    Matrix4x4 WVP;
    Matrix4x4 World;
    Vector4 color;
};

/// @brief 平行光源の方向・色・強度をGPUへ渡す。
struct DirectionalLight {
    Vector4 color;     // ライトの色
    Vector3 direction; // ライトの方向
    float intensity;   // ライトの光度
};

/// @brief シェーダーで使用するカメラの位置などを表す。
struct CameraData {
    Vector3 worldPosition;
    float padding; // 16byte アラインメント用（重要）
};

/// @brief 海面に加える航跡の位置と形状をシェーダーへ渡す。
struct OceanWakeData {
    Vector4 wakePoints[16];
    Vector4 wakeDirections[16];
    Vector4 parameters;
};

/// @brief 点光源の位置・色・強度・影響範囲をGPUへ渡す。
struct PointLightData {
    Vector4 color;
    Vector3 position;
    float intensity;
    float radius;
    float decay;
    float padding[2];
};

/// @brief 影を描く視点からの頂点変換行列をGPUへ渡す。
struct ShadowTransformationMatrix {
    Matrix4x4 WVP;
    Matrix4x4 LightWVP;
};

/// @brief 影の参照に必要な行列などをGPUへ渡す。
struct ShadowData {
    Matrix4x4 lightViewProjection;
};

/// @brief 軌跡を描く頂点の位置・UV・色をGPUへ渡す。
struct TrailVertex {
    Vector3 pos;   // POSITION
    Vector4 color; // COLOR (ここを毎フレーム変えてフェードアウトさせる)
    Vector2 uv;    // TEXCOORD
};

} // namespace myEngine
