#pragma once
#include "Struct.h"
#include "Calculation.h"

namespace myEngine {

/// @brief ビュー行列・射影行列とカメラの位置・姿勢を管理する。
class Camera {
public:
    /// @brief 射影の設定（縦の視野角・縦横比・近い面と遠い面の距離）。
    struct Lens {
        float fovY = 0.45f;
        float aspectRatio = 16.0f / 9.0f;
        float nearClip = 0.1f;
        float farClip = 5000.0f;
    };
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

    /// @brief 射影の設定をまとめて決める（次の Update で射影行列に反映する）。
    void SetLens(const Lens& lens)
    {
        lens_ = lens;
    }

    /// @brief 射影の設定を返す。一部だけ変えるときは、写してから SetLens に渡す。
    const Lens& GetLens() const
    {
        return lens_;
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
    Lens lens_;
};

} // namespace myEngine
