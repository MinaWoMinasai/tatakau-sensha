#include "NeonProjectileRenderer.h"
#include "game/player/actor/Bullet.h"
#include "Calculation.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
bool Finite(myEngine::Vector3 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

float Bounded(float value, float fallback, float low, float high)
{
    return std::isfinite(value) ? (std::clamp)(value, low, high) : fallback;
}

/// @brief 色と強さから発光色を作る。
myEngine::Vector4 Emission(myEngine::Vector3 color, float intensity, float alpha)
{
    return {color.x * intensity, color.y * intensity, color.z * intensity, alpha};
}
} // namespace

void NeonProjectileRenderer::Initialize(myEngine::DirectXCommon* dx)
{
    if (!dx || dx_) {
        throw std::logic_error("Initialize projectile presentation once with a valid device.");
    }
    dx_ = dx;
    geometry_.Initialize(dx, "resources/white512x512.png");
}

void NeonProjectileRenderer::SetParams(const NeonProjectileParams& params)
{
    const NeonProjectileParams defaults{};
    params_ = params;
    params_.headLength = Bounded(params.headLength, defaults.headLength, 0.05f, 3.0f);
    params_.headWidth = Bounded(params.headWidth, defaults.headWidth, 0.03f, 1.5f);
    params_.coreLengthScale = Bounded(params.coreLengthScale, defaults.coreLengthScale, 0.05f, 1.0f);
    params_.coreWidthScale = Bounded(params.coreWidthScale, defaults.coreWidthScale, 0.05f, 0.75f);
    params_.headIntensity = Bounded(params.headIntensity, defaults.headIntensity, 0.0f, 8.0f);
    params_.coreIntensity = Bounded(params.coreIntensity, defaults.coreIntensity, 0.0f, 12.0f);
    params_.haloWidthScale = Bounded(params.haloWidthScale, defaults.haloWidthScale, 1.0f, 4.0f);
    params_.haloIntensity = Bounded(params.haloIntensity, defaults.haloIntensity, 0.0f, 4.0f);
    params_.haloAlpha = Bounded(params.haloAlpha, defaults.haloAlpha, 0.0f, 0.4f);
}

void NeonProjectileRenderer::BeginFrame(const std::vector<Bullet*>& bullets, const myEngine::Vector3& cameraForward)
{
    if (!dx_) {
        throw std::logic_error("Initialize projectile presentation before preparing a frame.");
    }
    const uint64_t fence = dx_->GetFenceValue();
    if ((prepared_ && preparedFence_ == fence) || dx_->GetFence()->GetCompletedValue() < lastDrawFence_) {
        throw std::logic_error("Prepare projectile presentation once after the previous frame Fence.");
    }
    prepared_ = true;
    preparedFence_ = fence;
    drawn_ = false;
    stats_ = {};
    geometry_.BeginFrame();
    if (!params_.enabled) {
        return;
    }
    for (const Bullet* bullet : bullets) {
        if (!bullet || bullet->IsDead() || bullet->GetSpecialKind() == Bullet::SpecialKind::Rail ||
            bullet->GetSpecialKind() == Bullet::SpecialKind::SlashWave) {
            ++stats_.skipped;
            continue;
        }
        // 柔らかい帯3本（各18頂点）と12角形のキャップ2つ（各36頂点）。
        constexpr uint32_t verticesPerHead = 126;
        if (geometry_.GetVertexCount() + verticesPerHead > myEngine::NeonGridRenderer::kMaxVertices) {
            ++stats_.truncated;
            continue;
        }
        QueueHead(*bullet, cameraForward);
    }
    stats_.vertices = geometry_.GetVertexCount();
}

void NeonProjectileRenderer::QueueHead(const Bullet& bullet, const myEngine::Vector3& cameraForward)
{
    const myEngine::Vector3 center = bullet.GetWorldPosition();
    if (!Finite(center)) {
        ++stats_.skipped;
        return;
    }
    myEngine::Vector3 direction = bullet.GetMove();
    direction.z = 0.0f; // ゲームの既存のXY平面での弾の見せ方に合わせる。
    const float speed = Finite(direction) ? myEngine::Length(direction) : 0.0f;
    direction = std::isfinite(speed) && speed > 0.0001f ? direction / speed : myEngine::Vector3{1.0f, 0.0f, 0.0f};
    myEngine::Vector3 forward = cameraForward;
    const float forwardLength = Finite(forward) ? myEngine::Length(forward) : 0.0f;
    forward = std::isfinite(forwardLength) && forwardLength > 0.0001f ? forward / forwardLength : myEngine::Vector3{0, 0, 1};
    myEngine::Vector3 side = myEngine::Cross(forward, direction);
    if (myEngine::Length(side) <= 0.0001f) {
        side = {-direction.y, direction.x, 0};
    }
    side = myEngine::Normalize(side);

    const myEngine::Vector4 source = bullet.GetVisualColor();
    myEngine::Vector3 color{Bounded(source.x, 0, 0, 4), Bounded(source.y, 0, 0, 4), Bounded(source.z, 0, 0, 4)};
    const float peak = (std::max)({color.x, color.y, color.z});
    const float alpha = Bounded(source.w, 1, 0, 1);
    if (peak <= 0.0001f || alpha <= 0.001f) {
        ++stats_.skipped;
        return;
    }
    color = color / peak;
    const myEngine::Vector3 pale = color * 0.16f + myEngine::Vector3{0.84f, 0.84f, 0.84f};
    const myEngine::Vector3 tip = center + direction * (params_.headLength * 0.2f);
    const myEngine::Vector3 back = center - direction * (params_.headLength * 0.8f);

    geometry_.SetLineStyle(0.86f, 1.0f);
    geometry_.QueueCameraFacingLine(back - direction * (params_.headLength * 0.12f), tip, params_.headWidth * params_.haloWidthScale,
                                    Emission(color, params_.haloIntensity, params_.haloAlpha * alpha), forward);
    geometry_.SetLineStyle(0.3f, 1.0f);
    geometry_.QueueCameraFacingLine(back, tip, params_.headWidth, Emission(color, params_.headIntensity, alpha), forward);
    geometry_.QueueBillboardDisc(tip, params_.headWidth * 0.35f, Emission(color, params_.headIntensity, alpha), direction, side, 12);
    geometry_.SetLineStyle(0.5f, 1.0f);
    geometry_.QueueCameraFacingLine(tip - direction * (params_.headLength * params_.coreLengthScale), tip,
                                    params_.headWidth * params_.coreWidthScale, Emission(pale, params_.coreIntensity, alpha), forward);
    geometry_.QueueBillboardDisc(tip, params_.headWidth * params_.coreWidthScale * 0.25f, Emission(pale, params_.coreIntensity, alpha),
                                 direction, side, 12);
    ++stats_.heads;
}

void NeonProjectileRenderer::Draw(const myEngine::Matrix4x4& viewProjection)
{
    if (!prepared_ || drawn_ || stats_.vertices == 0) {
        return;
    }
    if (dx_->GetFenceValue() != preparedFence_) {
        throw std::logic_error("Prepare projectile geometry for the current frame.");
    }
    geometry_.DrawAll(viewProjection);
    drawn_ = true;
    stats_.drawCalls = 1;
    lastDrawFence_ = preparedFence_ + 1;
}
