#include "NeonDissolvePreviewController.h"
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)

bool NeonDissolvePreviewController::Trigger(cg2::SkinnedModel& model, const cg2::Matrix4x4& world,
    const cg2::Matrix4x4& cameraWorld, cg2::NeonDissolveDirection direction,
    const cg2::NeonDissolveParams& settings, float waitDuration, float duration) {
    // Repeated Trigger restarts the first snapshot rather than capturing a partly dissolved state.
    if (active_) return Restart(model);
    if (!std::isfinite(waitDuration) || waitDuration < 0 || waitDuration > 60 ||
        !std::isfinite(duration) || duration < 0.01f || duration > 60 ||
        !std::isfinite(settings.noiseStrength) || !std::isfinite(settings.noiseScale) ||
        !std::isfinite(settings.edgeWidth) || !std::isfinite(settings.edgeIntensity) ||
        !cg2::IsFiniteNeonPosition(settings.edgeColor)) {
        error_ = "Dissolve timing and settings must be finite and duration must be positive.";
        return false;
    }
    auto next = cg2::SanitizeNeonDissolveParams(settings);
    if (!cg2::MakeNeonDissolveDirection(world,cameraWorld,direction,next.direction,&error_)) return false;
    if (!cg2::ComputeNeonSkinnedProjectionBounds(model,next.direction,next.scanMin,next.scanMax)) {
        error_ = "Cannot compute a finite scan range from the current skinned Palette.";
        return false;
    }
    // Snapshot allocation/validation completes before playback is paused or state is committed.
    auto playback = model.CaptureAnimationPlaybackState();
    std::vector<cg2::QuaternionTransform> pose;
    pose.reserve(model.GetSkeleton().joints.size());
    for (const auto& joint : model.GetSkeleton().joints) pose.push_back(joint.transform);
    next.enabled = 1; next.progress = 0;
    beforeTriggerParams_ = params_;
    playback_ = std::move(playback); frozenPose_ = std::move(pose); params_ = next;
    world_ = world; cameraWorld_ = cameraWorld; direction_ = direction;
    elapsed_ = 0; waitDuration_ = waitDuration; duration_ = duration; speed_ = 1;
    active_ = playing_ = true; error_.clear();
    model.SetAnimationPlaying(false);
    return true;
}

void NeonDissolvePreviewController::Update(float deltaTime) {
    if (!active_ || !playing_ || !std::isfinite(deltaTime) || deltaTime <= 0) return;
    elapsed_ = (std::min)(waitDuration_ + duration_, elapsed_ + deltaTime * speed_);
    params_.progress = (std::clamp)((elapsed_ - waitDuration_) / duration_,0.0f,1.0f);
    if (params_.progress >= 1) playing_ = false;
}

bool NeonDissolvePreviewController::Restart(cg2::SkinnedModel& model) {
    if (!active_) return false;
    if (!model.RestoreAnimationPlaybackState(playback_)) {
        error_ = "Cannot restore the original frozen animation snapshot."; return false;
    }
    model.SetAnimationPlaying(false);
    elapsed_ = 0; params_.enabled = 1; params_.progress = 0; playing_ = true; error_.clear();
    return true;
}

bool NeonDissolvePreviewController::Reset(cg2::SkinnedModel& model) {
    if (!active_) return true;
    if (!model.RestoreAnimationPlaybackState(playback_)) {
        error_ = "Cannot restore playback; dissolve remains active to protect its snapshot."; return false;
    }
    active_ = playing_ = false; params_ = beforeTriggerParams_; elapsed_ = 0; error_.clear();
    return true;
}

bool NeonDissolvePreviewController::Seek(float progress) {
    if (!active_ || !std::isfinite(progress)) return false;
    params_.progress = (std::clamp)(progress,0.0f,1.0f);
    elapsed_ = waitDuration_ + duration_ * params_.progress;
    playing_ = false;
    return true;
}

bool NeonDissolvePreviewController::SetPlaybackSpeed(float speed) {
    if (!std::isfinite(speed) || speed < 0.05f || speed > 4.0f) return false;
    speed_ = speed; return true;
}
#endif
