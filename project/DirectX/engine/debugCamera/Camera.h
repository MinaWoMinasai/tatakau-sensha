#pragma once
#include "Struct.h"
#include "Calculation.h"

namespace cg2 {

/// @brief ビュー行列・射影行列とカメラの位置・姿勢を管理する。
class Camera {

public:
    /// @brief インスタンスの初期値と利用先を設定する。
    Camera();

    // 更新
    void Update();

    // setter
    /// @brief 回転角を設定する。
    void SetRotate(const Vector3& rotate)
    {
        transform_.rotate = rotate;
    }
    /// @brief 平行移動を設定する。
    void SetTranslate(const Vector3& translate)
    {
        transform_.translate = translate;
    }
    /// @brief 縦方向の視野角を設定する。
    void SetFovY(float fovY)
    {
        fovY_ = fovY;
    }
    /// @brief 画面の縦横比を設定する。
    void SetAspectRatio(float aspectRatio)
    {
        aspectRatio_ = aspectRatio;
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

    // getter
    /// @brief ワールド行列を返す。
    const Matrix4x4& GetWorldMatrix() const
    {
        return worldMatrix_;
    }
    /// @brief ビュー行列を返す。
    const Matrix4x4& GetViewMatrix() const
    {
        return viewMatrix_;
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
    /// @brief 回転角を返す。
    const Vector3& GetRotate() const
    {
        return transform_.rotate;
    }
    /// @brief 平行移動を返す。
    const Vector3& GetTranslate() const
    {
        return transform_.translate;
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

private:
    /// @brief 射影行列を更新する。
    void UpdateProjectionMatrices();

    Transform transform_;
    Matrix4x4 worldMatrix_;
    Matrix4x4 viewMatrix_;
    Matrix4x4 projectionMatrix_;
    Matrix4x4 viewProjectionMatrix_;
    Matrix4x4 unjitteredProjectionMatrix_;
    Matrix4x4 unjitteredViewProjectionMatrix_;
    Vector2 projectionJitter_ = {0.0f, 0.0f};
    float fovY_;
    float aspectRatio_;
    float nearClip_;
    float farClip_;
};

} // namespace cg2
