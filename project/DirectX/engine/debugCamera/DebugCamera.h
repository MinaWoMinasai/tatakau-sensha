#pragma once
#define NOMINMAX
#include "Calculation.h"
#include "Input.h"
#include "algorithm"
#include <DirectXMath.h>
#include "externals/imgui/imgui.h"
#include "WinApp.h"

namespace cg2 {

/// @brief 開発時の入力で動かせるカメラの姿勢と行列を管理する。
class DebugCamera {

public:
    /// @brief インスタンスの初期値と利用先を設定する。
    DebugCamera();

    /// @brief 初期化
    void Initialize();

    /// @brief 更新
    void Update(const DIMOUSESTATE& mousestate, const std::span<const BYTE>& key, Vector2 leftStick);

    /// @brief ビュー行列を返す。
    Matrix4x4 GetViewMatrix()
    {
        return viewMatrix_;
    }

    /// @brief 距離を返す。
    float GetDistance() const
    {
        return distance;
    }

    /// @brief Eye位置を返す。
    Vector3 GetEyePosition()
    {
        return eye_;
    }

    /// @brief 射影行列を返す。
    const Matrix4x4& GetProjectionMatrix() const
    {
        return projectionMatrix_;
    }
    /// @brief 視点射影行列を返す。
    const Matrix4x4& GetViewProjectionMatrix() const
    {
        return viewProjectionMatrix_;
    }
    /// @brief ジッター適用前の射影行列を返す。
    const Matrix4x4& GetUnjitteredProjectionMatrix() const
    {
        return unjitteredProjectionMatrix_;
    }
    /// @brief ジッター適用前の視点射影行列を返す。
    const Matrix4x4& GetUnjitteredViewProjectionMatrix() const
    {
        return unjitteredViewProjectionMatrix_;
    }
    /// @brief 射影射影ジッターを返す。
    const Vector2& GetProjectionJitter() const
    {
        return projectionJitter_;
    }
    /// @brief 近クリップ距離を返す。
    float GetNearClip() const
    {
        return nearClip_;
    }
    /// @brief 遠クリップ距離を返す。
    float GetFarClip() const
    {
        return farClip_;
    }
    /// @brief 近クリップ距離を設定する。
    void SetNearClip(float nearClip)
    {
        nearClip_ = nearClip;
    }
    /// @brief 遠クリップ距離を設定する。
    void SetFarClip(float farClip)
    {
        farClip_ = farClip;
    }
    /// @brief 射影射影ジッターを設定する。
    void SetProjectionJitter(const Vector2& jitter);

private:
    /// @brief 射影行列を更新する。
    void UpdateProjectionMatrices();

    // X,Y,Z軸回りのローカル回転角
    Vector3 rotation_ = {0, 0, 0};
    // ローカル座標
    Vector3 translation_ = {19.0f, 12.3f, 0.15f};
    // ビュー行列
    Matrix4x4 viewMatrix_ = MakeIdentity4x4();
    // 射影行列
    Matrix4x4 projectionMatrix_;
    Matrix4x4 viewProjectionMatrix_;
    Matrix4x4 unjitteredProjectionMatrix_;
    Matrix4x4 unjitteredViewProjectionMatrix_;
    Vector2 projectionJitter_ = {0.0f, 0.0f};
    float fovY_;
    float aspectRatio_;
    float nearClip_;
    float farClip_;
    // カメラの移動速度
    // カメラ
    Matrix4x4 WorldMatrix_;

    Input input_;

    // 移動・回転速度調整用
    Vector3 velocity_ = {0.01f, 0.01f, 1.0f}; // x=横回転速度, y=縦回転速度, z=ズーム速度
    float distance = 100.0f;                  // 注視点との距離（ズーム）
    Vector3 target = {0.01f, 0.01f, 0.01f};   // 注視点のワールド座標
    Vector3 eye_;                             // カメラのワールド位置
};

} // namespace cg2
