#include "EditorCamera.h"
#include "Camera.h"

#include <algorithm>
#include <cmath>

namespace myEngine {

namespace {

// 真上・真下を向くと左右の向きが決まらなくなるので、少し手前で止める。
constexpr float kMaxPitch = 1.55f;

} // namespace

// 回転は Rx(pitch) * Ry(yaw)（行ベクトル）。各行が右・上・前の向きになる。
Vector3 EditorCamera::GetRight() const
{
    return {std::cos(yaw_), 0.0f, -std::sin(yaw_)};
}

Vector3 EditorCamera::GetUp() const
{
    return {std::sin(pitch_) * std::sin(yaw_), std::cos(pitch_), std::sin(pitch_) * std::cos(yaw_)};
}

Vector3 EditorCamera::GetForward() const
{
    return {std::cos(pitch_) * std::sin(yaw_), -std::sin(pitch_), std::cos(pitch_) * std::cos(yaw_)};
}

void EditorCamera::Apply(const EditorCameraInput& input)
{
    if (input.rotating) {
        // 右へドラッグすると右を向き、下へドラッグすると下を向く。
        yaw_ += input.mouseDelta.x * settings_.rotateSpeed;
        pitch_ = std::clamp(pitch_ + input.mouseDelta.y * settings_.rotateSpeed, -kMaxPitch, kMaxPitch);
        const float speed = settings_.moveSpeed * (input.fast ? settings_.fastMultiplier : 1.0f) * input.deltaTime;
        translate_ = translate_ + (GetRight() * input.move.x + GetUp() * input.move.y + GetForward() * input.move.z) * speed;
    }
    if (input.panning) {
        // つかんだ景色が手についてくるように、カメラはマウスと逆へ動く。
        translate_ = translate_ + (GetRight() * -input.mouseDelta.x + GetUp() * input.mouseDelta.y) * settings_.panSpeed;
    }
    if (input.wheel != 0.0f) {
        translate_ = translate_ + GetForward() * (input.wheel * settings_.wheelStep);
    }
}

void EditorCamera::Focus(const Vector3& target, float distance)
{
    translate_ = target - GetForward() * distance;
}

void EditorCamera::SetPose(const Vector3& translate, const Vector3& rotate)
{
    translate_ = translate;
    pitch_ = std::clamp(rotate.x, -kMaxPitch, kMaxPitch);
    yaw_ = rotate.y;
}

void EditorCamera::ApplyTo(Camera& camera) const
{
    camera.SetTranslate(translate_);
    camera.SetRotate(GetRotate());
}

} // namespace myEngine
