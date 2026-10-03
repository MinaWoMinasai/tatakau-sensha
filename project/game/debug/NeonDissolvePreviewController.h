#pragma once
#include "DeveloperTools.h"
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
#include "DirectX/engine/3d/neon/NeonDissolve.h"

// CPU preview state only; model/GPU resources remain owned by NeonSkinnedPreview.
class NeonDissolvePreviewController {
public:
    bool Trigger(cg2::SkinnedModel& model, const cg2::Matrix4x4& world,
        const cg2::Matrix4x4& cameraWorld, cg2::NeonDissolveDirection direction,
        const cg2::NeonDissolveParams& settings, float waitDuration = 0.15f, float duration = 2.0f);
    void Update(float deltaTime);
    bool Restart(cg2::SkinnedModel& model);
    bool Reset(cg2::SkinnedModel& model);
    void SetPlaying(bool playing) { if (active_) playing_ = playing && params_.progress < 1.0f; }
    bool Seek(float progress);
    bool SetPlaybackSpeed(float speed);
    bool IsActive() const { return active_; }
    bool IsPlaying() const { return playing_; }
    float GetElapsed() const { return elapsed_; }
    float GetWaitDuration() const { return waitDuration_; }
    float GetDuration() const { return duration_; }
    float GetPlaybackSpeed() const { return speed_; }
    const cg2::NeonDissolveParams& GetParams() const { return params_; }
    const cg2::SkinnedModel::AnimationPlaybackState& GetSavedPlayback() const { return playback_; }
    const std::vector<cg2::QuaternionTransform>& GetFrozenPose() const { return frozenPose_; }
    const cg2::Matrix4x4& GetStartWorld() const { return world_; }
    const cg2::Matrix4x4& GetStartCameraWorld() const { return cameraWorld_; }
    cg2::NeonDissolveDirection GetDirectionPreset() const { return direction_; }
    const std::string& GetError() const { return error_; }
private:
    cg2::SkinnedModel::AnimationPlaybackState playback_;
    std::vector<cg2::QuaternionTransform> frozenPose_;
    cg2::NeonDissolveParams params_{};
    cg2::NeonDissolveParams beforeTriggerParams_{};
    cg2::Matrix4x4 world_{}, cameraWorld_{};
    cg2::NeonDissolveDirection direction_ = cg2::NeonDissolveDirection::UpperLeftToLowerRight;
    float elapsed_ = 0.0f, waitDuration_ = 0.15f, duration_ = 2.0f, speed_ = 1.0f;
    bool active_ = false, playing_ = false;
    std::string error_;
};
#endif
