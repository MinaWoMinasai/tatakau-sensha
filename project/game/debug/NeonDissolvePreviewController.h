#pragma once
#include "DeveloperTools.h"
#if MYENGINE_DEVELOPER_TOOLS && !defined(NDEBUG)
#include "DirectX/engine/3d/neon/NeonDissolve.h"

// CPU preview state only; model/GPU resources remain owned by NeonSkinnedPreview.
class NeonDissolvePreviewController {
public:
    bool Trigger(myEngine::SkinnedModel& model, const myEngine::Matrix4x4& world,
        const myEngine::Matrix4x4& cameraWorld, myEngine::NeonDissolveDirection direction,
        const myEngine::NeonDissolveParams& settings, float waitDuration = 0.15f, float duration = 2.0f);
    void Update(float deltaTime);
    bool Restart(myEngine::SkinnedModel& model);
    bool Reset(myEngine::SkinnedModel& model);
    void SetPlaying(bool playing) { if (active_) playing_ = playing && params_.progress < 1.0f; }
    bool Seek(float progress);
    bool SetPlaybackSpeed(float speed);
    bool IsActive() const { return active_; }
    bool IsPlaying() const { return playing_; }
    float GetElapsed() const { return elapsed_; }
    float GetWaitDuration() const { return waitDuration_; }
    float GetDuration() const { return duration_; }
    float GetPlaybackSpeed() const { return speed_; }
    const myEngine::NeonDissolveParams& GetParams() const { return params_; }
    const myEngine::SkinnedModel::AnimationPlaybackState& GetSavedPlayback() const { return playback_; }
    const std::vector<myEngine::QuaternionTransform>& GetFrozenPose() const { return frozenPose_; }
    const myEngine::Matrix4x4& GetStartWorld() const { return world_; }
    const myEngine::Matrix4x4& GetStartCameraWorld() const { return cameraWorld_; }
    myEngine::NeonDissolveDirection GetDirectionPreset() const { return direction_; }
    const std::string& GetError() const { return error_; }
private:
    myEngine::SkinnedModel::AnimationPlaybackState playback_;
    std::vector<myEngine::QuaternionTransform> frozenPose_;
    myEngine::NeonDissolveParams params_{};
    myEngine::NeonDissolveParams beforeTriggerParams_{};
    myEngine::Matrix4x4 world_{}, cameraWorld_{};
    myEngine::NeonDissolveDirection direction_ = myEngine::NeonDissolveDirection::UpperLeftToLowerRight;
    float elapsed_ = 0.0f, waitDuration_ = 0.15f, duration_ = 2.0f, speed_ = 1.0f;
    bool active_ = false, playing_ = false;
    std::string error_;
};
#endif
