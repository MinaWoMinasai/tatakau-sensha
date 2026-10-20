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

void EditorCamera::SetFromViewMatrix(const Matrix4x4& view)
{
    // カメラのワールド行列の各行が、右・上・前の向きと位置になる。
    const Matrix4x4 world = Inverse(view);
    const Vector3 up = Normalize({world.m[1][0], world.m[1][1], world.m[1][2]});
    const Vector3 forward = Normalize({world.m[2][0], world.m[2][1], world.m[2][2]});
    // 前 = (cos p sin y, -sin p, cos p cos y)
    pitch_ = std::clamp(std::asin(std::clamp(-forward.y, -1.0f, 1.0f)), -kMaxPitch, kMaxPitch);
    if (forward.x * forward.x + forward.z * forward.z > 1.0e-6f) {
        yaw_ = std::atan2(forward.x, forward.z);
    } else {
        // 真上・真下を向いているときは、上の向き = ±(sin y, 0, cos y) から yaw を決める。
        const float sign = forward.y < 0.0f ? 1.0f : -1.0f;
        yaw_ = std::atan2(up.x * sign, up.z * sign);
    }
    translate_ = {world.m[3][0], world.m[3][1], world.m[3][2]};
}

void EditorCamera::ApplyTo(Camera& camera) const
{
    camera.SetTranslate(translate_);
    camera.SetRotate(GetRotate());
}

} // namespace myEngine
