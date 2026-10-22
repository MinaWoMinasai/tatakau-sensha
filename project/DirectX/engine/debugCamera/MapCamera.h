#pragma once
#include "Calculation.h"
#include "Struct.h"

namespace myEngine {

class Camera;

/// @brief 地図カメラへの、1フレーム分の入力。ゲームの入力から作るが、テストでは直接作って渡せる。
/// 画面の座標は左上を原点とし、screenSize と同じ座標系（ピクセルなど）で渡す。
struct MapCameraInput {
    // 画面の大きさ。カーソルを使う操作（ドラッグ・カーソル位置へのズーム・画面端での移動）に使う。
    Vector2 screenSize = {1.0f, 1.0f};
    // カーソルの位置と、前のフレームからの移動量。hasCursor が false ならカーソルを使う操作はしない。
    Vector2 cursor = {0.0f, 0.0f};
    Vector2 cursorDelta = {0.0f, 0.0f};
    bool hasCursor = false;
    // 地図をつかんで動かしている途中か（ドラッグ）。
    bool dragging = false;
    // ホイールの回転量（奥へ回すと正で、近づく）。
    float wheel = 0.0f;
    // キーでの移動の向き（x: 右、y: 奥）。各成分は -1〜1。
    Vector2 move = {0.0f, 0.0f};
    float deltaTime = 0.0f;
};

/// @brief 地面（y = 0 の平面）を見下ろして動かすカメラ（俯瞰の視点で地面を動かして見るゲーム向け）。
/// 注視点（地面上の x, z）と高さで位置を決め、北（+z）を上にして見る。
/// 高いほど真上に近い向きになり、近づくほど斜めに傾く。地面のつかみ移動と、カーソル位置へのズームでは、
/// カーソルの下の地面の点が動かないようにする。
class MapCamera {
public:
    /// @brief 操作の速さと、見え方。
    struct Settings {
        float minHeight = 5.0f;
        float maxHeight = 150.0f;
        // 最も近いとき・最も高いときの見下ろす角度（ラジアン。π/2 で真下）。高さに応じて間を補間する。
        float nearPitch = 0.9f;
        float farPitch = 1.45f;
        // ホイール1目盛りで高さを変える割合。
        float zoomStep = 0.15f;
        // キーでの移動の速さ（1秒あたり、高さの何倍動くか）。
        float panSpeed = 1.0f;
        // 画面の端から何ピクセル以内で、その向きへ動かすか。0 なら端での移動をしない。
        float edgeScrollMargin = 0.0f;
        // 射影（Camera と同じ意味）。
        float fovY = 0.45f;
        float aspectRatio = 16.0f / 9.0f;
        float nearClip = 0.1f;
        float farClip = 5000.0f;
    };

    MapCamera() = default;

    /// @brief 1フレーム分の入力を反映する。
    void Apply(const MapCameraInput& input);

    /// @brief 注視点（地面上の x, z）を設定する。範囲を設定していれば、その中に収める。
    void SetTarget(const Vector2& target);
    /// @brief 高さを設定する（Settings の範囲に収める）。
    void SetHeight(float height);
    /// @brief 注視点が動ける範囲（地面上の x, z）を設定する。
    void SetBounds(const Vector2& min, const Vector2& max);
    /// @brief 範囲の制限をなくす。
    void ClearBounds()
    {
        hasBounds_ = false;
    }

    const Vector2& GetTarget() const
    {
        return target_;
    }

    float GetHeight() const
    {
        return height_;
    }

    /// @brief 見下ろす角度（ラジアン）。
    float GetPitch() const;
    /// @brief カメラの位置。
    Vector3 GetPosition() const;
    /// @brief カメラの向き（Camera と同じ Rx * Ry * Rz の角度。x: 見下ろす角度、y・z: 0）。
    Vector3 GetRotate() const
    {
        return {GetPitch(), 0.0f, 0.0f};
    }

    Matrix4x4 GetViewMatrix() const;
    Matrix4x4 GetProjectionMatrix() const;

    /// @brief 画面上の点の下にある、地面（y = 0）の点を求める。
    /// @return 光線が地面と交わればtrue。
    bool GroundPointAt(const Vector2& screenPos, const Vector2& screenSize, Vector3& groundPoint) const;

    /// @brief Camera へ位置・向き・射影の設定を書き込む（Camera::Update は呼び出し側で行う）。
    void ApplyTo(Camera& camera) const;

    Settings& GetSettings()
    {
        return settings_;
    }

    const Settings& GetSettings() const
    {
        return settings_;
    }

private:
    /// @brief 注視点を範囲の中に収める。
    void ClampTarget();
    /// @brief 画面上の点の下にある地面の点が、移動の前後で同じになるよう注視点をずらす。
    void KeepGroundPointUnder(const Vector3& groundPointBefore, const Vector2& screenPos, const Vector2& screenSize);

    Settings settings_;
    Vector2 target_ = {0.0f, 0.0f};
    float height_ = 50.0f;
    bool hasBounds_ = false;
    Vector2 boundsMin_ = {0.0f, 0.0f};
    Vector2 boundsMax_ = {0.0f, 0.0f};
};

} // namespace myEngine
