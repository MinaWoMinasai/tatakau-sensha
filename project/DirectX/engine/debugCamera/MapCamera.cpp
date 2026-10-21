#include "MapCamera.h"
#include "Camera.h"

#include <algorithm>
#include <cmath>

namespace myEngine {

void MapCamera::Apply(const MapCameraInput& input)
{
    // キーでの移動。高いほど速く動かし、見えている範囲に対する速さをそろえる。
    Vector2 move = input.move;
    if (input.hasCursor && settings_.edgeScrollMargin > 0.0f) {
        if (input.cursor.x <= settings_.edgeScrollMargin) {
            move.x -= 1.0f;
        } else if (input.cursor.x >= input.screenSize.x - settings_.edgeScrollMargin) {
            move.x += 1.0f;
        }
        // 画面の上端は奥（+z）。
        if (input.cursor.y <= settings_.edgeScrollMargin) {
            move.y += 1.0f;
        } else if (input.cursor.y >= input.screenSize.y - settings_.edgeScrollMargin) {
            move.y -= 1.0f;
        }
    }
    move.x = std::clamp(move.x, -1.0f, 1.0f);
    move.y = std::clamp(move.y, -1.0f, 1.0f);
    const float distance = height_ * settings_.panSpeed * input.deltaTime;
    target_ = {target_.x + move.x * distance, target_.y + move.y * distance};
    ClampTarget();

    // ドラッグ: 前のフレームでカーソルの下にあった地面の点を、今のカーソルの下へ持ってくる。
    if (input.dragging && input.hasCursor) {
        Vector3 grabbed;
        const Vector2 previous = input.cursor - input.cursorDelta;
        if (GroundPointAt(previous, input.screenSize, grabbed)) {
            KeepGroundPointUnder(grabbed, input.cursor, input.screenSize);
        }
    }

    // ズーム: カーソルの下の地面の点を保ったまま、高さを変える。
    if (input.wheel != 0.0f) {
        Vector3 underCursor;
        const bool keepCursor = input.hasCursor && GroundPointAt(input.cursor, input.screenSize, underCursor);
        SetHeight(height_ * std::pow(1.0f - settings_.zoomStep, input.wheel));
        if (keepCursor) {
            KeepGroundPointUnder(underCursor, input.cursor, input.screenSize);
        }
    }
}

void MapCamera::SetTarget(const Vector2& target)
{
    target_ = target;
    ClampTarget();
}

void MapCamera::SetHeight(float height)
{
    height_ = std::clamp(height, settings_.minHeight, settings_.maxHeight);
}

void MapCamera::SetBounds(const Vector2& min, const Vector2& max)
{
    hasBounds_ = true;
    boundsMin_ = {(std::min)(min.x, max.x), (std::min)(min.y, max.y)};
    boundsMax_ = {(std::max)(min.x, max.x), (std::max)(min.y, max.y)};
    ClampTarget();
}

void MapCamera::ClampTarget()
{
    if (!hasBounds_) {
        return;
    }
    target_.x = std::clamp(target_.x, boundsMin_.x, boundsMax_.x);
    target_.y = std::clamp(target_.y, boundsMin_.y, boundsMax_.y);
}

void MapCamera::KeepGroundPointUnder(const Vector3& groundPointBefore, const Vector2& screenPos, const Vector2& screenSize)
{
    // 注視点を動かすとカメラは平行に動くので、地面の点も同じだけずれる。差の分だけ戻せばよい。
    Vector3 groundPointNow;
    if (!GroundPointAt(screenPos, screenSize, groundPointNow)) {
        return;
    }
    target_ = {target_.x + groundPointBefore.x - groundPointNow.x, target_.y + groundPointBefore.z - groundPointNow.z};
    ClampTarget();
}

float MapCamera::GetPitch() const
{
    const float range = settings_.maxHeight - settings_.minHeight;
    const float t = range > 0.0f ? std::clamp((height_ - settings_.minHeight) / range, 0.0f, 1.0f) : 1.0f;
    return settings_.nearPitch + (settings_.farPitch - settings_.nearPitch) * t;
}

Vector3 MapCamera::GetPosition() const
{
    // 前の向きは (0, -sin p, cos p)。高さ h から地面の注視点を見るには、z を h / tan p だけ手前に引く。
    const float pitch = GetPitch();
    return {target_.x, height_, target_.y - height_ * std::cos(pitch) / std::sin(pitch)};
}

Matrix4x4 MapCamera::GetViewMatrix() const
{
    return Inverse(MakeAffineMatrix(Vector3{1.0f, 1.0f, 1.0f}, GetRotate(), GetPosition()));
}

Matrix4x4 MapCamera::GetProjectionMatrix() const
{
    return MakePerspectiveForMatrix(settings_.fovY, settings_.aspectRatio, settings_.nearClip, settings_.farClip);
}

bool MapCamera::GroundPointAt(const Vector2& screenPos, const Vector2& screenSize, Vector3& groundPoint) const
{
    const Ray ray = ScreenPointToRay(screenPos, GetViewMatrix(), GetProjectionMatrix(), screenSize.x, screenSize.y);
    return IntersectRayPlane(ray, Plane{{0.0f, 1.0f, 0.0f}, 0.0f}, groundPoint);
}

void MapCamera::ApplyTo(Camera& camera) const
{
    camera.SetTranslate(GetPosition());
    camera.SetRotate(GetRotate());
    camera.SetFovY(settings_.fovY);
    camera.SetAspectRatio(settings_.aspectRatio);
    camera.SetNearClip(settings_.nearClip);
    camera.SetFarClip(settings_.farClip);
}

} // namespace myEngine
