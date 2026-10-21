#pragma once
#include "Calculation.h"
#include "Struct.h"

namespace myEngine {

class Camera;

/// @brief エディタ用カメラへの、1フレーム分の入力。画面の操作（ImGui）から作るが、テストでは直接作って渡せる。
struct EditorCameraInput {
    // マウスの移動量（ピクセル）。
    Vector2 mouseDelta = {0.0f, 0.0f};
    // ホイールの回転量（手前が負）。
    float wheel = 0.0f;
    // 右ドラッグ中（向きを変える。WASD / QE での移動もこの間だけ）。
    bool rotating = false;
    // 中ドラッグ中（平行移動）。
    bool panning = false;
    // カメラから見た移動の向き（x: 右、y: 上、z: 前）。各成分は -1〜1。
    Vector3 move = {0.0f, 0.0f, 0.0f};
    // 速く動かす（Shift）。
    bool fast = false;
    float deltaTime = 0.0f;
};

/// @brief 編集中にシーンを見るためのカメラ（Unity の Scene ビューのカメラ）。シーンファイルには保存しない。
/// 向きは pitch（rotate.x）と yaw（rotate.y）だけで表し、Camera と同じく Rx * Ry * Rz の回転と +Z を前とする。
class EditorCamera {
public:
    /// @brief 操作の速さ。
    struct Settings {
        // 右ドラッグ1ピクセルあたりの回転（ラジアン）。
        float rotateSpeed = 0.004f;
        // WASD / QE の移動の速さ（1秒あたり）。
        float moveSpeed = 8.0f;
        // Shift を押したときの倍率。
        float fastMultiplier = 4.0f;
        // ホイール1目盛りで前後に動く距離。
        float wheelStep = 1.0f;
        // 中ドラッグ1ピクセルあたりの平行移動。
        float panSpeed = 0.02f;
    };

    /// @brief 1フレーム分の入力を反映する。
    void Apply(const EditorCameraInput& input);

    /// @brief target が画面の中央に来るように、今の向きのまま distance だけ離れた位置へ移る。
    void Focus(const Vector3& target, float distance);

    /// @brief 位置と向き（ラジアン。x: pitch、y: yaw。z は使わない）を直接設定する。
    void SetPose(const Vector3& translate, const Vector3& rotate);

    /// @brief ビュー行列（カメラのワールド行列の逆行列）から、位置と向きを設定する。z 軸まわりの回転は捨てる。
    /// 視点キューブで変えた向きを取り込むときに使う。
    void SetFromViewMatrix(const Matrix4x4& view);

    /// @brief Camera へ位置と向きを設定する（Camera::Update は呼び出し側で行う）。
    void ApplyTo(Camera& camera) const;

    const Vector3& GetTranslate() const
    {
        return translate_;
    }

    /// @brief 向き（x: pitch、y: yaw、z: 0）を返す。
    Vector3 GetRotate() const
    {
        return {pitch_, yaw_, 0.0f};
    }

    /// @brief カメラの前・右・上の向き（ワールド座標の単位ベクトル）。
    Vector3 GetForward() const;
    Vector3 GetRight() const;
    Vector3 GetUp() const;

    Settings& GetSettings()
    {
        return settings_;
    }

private:
    Vector3 translate_ = {0.0f, 0.0f, -20.0f};
    float pitch_ = 0.0f;
    float yaw_ = 0.0f;
    Settings settings_;
};

} // namespace myEngine
