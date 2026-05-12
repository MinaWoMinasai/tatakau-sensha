#pragma once
#include "ParticleManager.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>

class Object3d;
class Object3dCommon;
class DirectXCommon;
class Camera;
class TrailManager;
class TrailInstance;

struct ProjectileProfile {
    std::string modelPath = "ball.obj";
    Vector3 scale = { 0.3f, 0.3f, 0.3f };
    Vector3 rotationSpeed = { 0.0f, 5.0f, 0.0f };

    nlohmann::json ToJson() const;
    void FromJson(const nlohmann::json& j);
};

struct TrailProfile {
    Vector4 startColor = { 0.5f, 0.0f, 1.0f, 1.0f };
    Vector4 endColor = { 0.5f, 0.0f, 1.0f, 0.0f };
    uint32_t maxPoints = 100;
    uint32_t interpolationSteps = 6;
    Vector3 tipOffset = { 0.0f, 0.3f, 0.0f };
    Vector3 baseOffset = { 0.0f, -0.3f, 0.0f };
    float lifetime = 0.5f;

    nlohmann::json ToJson() const;
    void FromJson(const nlohmann::json& j);
};

struct EffectProfile {
    ProjectileProfile projectile;
    std::string flyParticle = "HitSpark";
    std::string hitParticle = "HitSpark";
    TrailProfile trail;
    float duration = 1.0f;
    float hitDuration = 0.5f;
    uint32_t flyParticleCount = 1;
    uint32_t hitParticleCount = 1;
    bool enableTrail = true;

    nlohmann::json ToJson() const;
    void FromJson(const nlohmann::json& j);
};

class EffectSequencer {
public:
    enum class State {
        Idle,
        Firing,
        Flying,
        Hit,
        Finished,
    };

    struct Event {
        std::string effectName;
        float delay = 0.0f;
        uint32_t count = 1;
        Vector3 offset = { 0.0f, 0.0f, 0.0f };
    };

    void Initialize(ParticleManager* particleManager = ParticleManager::GetInstance());
    void Initialize(Object3dCommon* objCommon, DirectXCommon* dx, Camera* camera, ParticleManager* particleManager, TrailManager* trailManager);
    void AddEvent(const Event& event);
    void ClearEvents();
    void Play(const Vector3& position);
    void PlayHitEffect(const Vector3& position);
    void Fire(const EffectProfile& profile, const Vector3& startPos, const Vector3& targetPos);
    void Update(float deltaTime);
    void Draw();
    void DrawImGui();
#ifdef USE_IMGUI
    void DrawImGuiEditor(const Vector3& defaultStartPos, const Vector3& defaultTargetPos);
#endif

    bool IsFinished() const { return state_ == State::Finished || state_ == State::Idle; }
    bool IsActive() const { return state_ != State::Idle && state_ != State::Finished; }
    State GetState() const { return state_; }
    void Reset();
    void SetOnHitCallback(std::function<void()> callback) { onHitCallback_ = std::move(callback); }

    static void SaveProfile(const std::string& path, const EffectProfile& profile);
    static bool LoadProfile(const std::string& path, EffectProfile& profile);

    const EffectProfile& GetProfile() const { return profile_; }
    void SetProfile(const EffectProfile& profile) { profile_ = profile; }

private:
    struct ActiveEvent {
        Event event;
        Vector3 origin = { 0.0f, 0.0f, 0.0f };
        float timer = 0.0f;
        bool emitted = false;
    };

    void UpdateFiring(float deltaTime);
    void UpdateFlying(float deltaTime);
    void UpdateHit(float deltaTime);
    static Vector3 LerpVec3(const Vector3& a, const Vector3& b, float t);

    ParticleManager* particleManager_ = nullptr;
    Object3dCommon* objCommon_ = nullptr;
    DirectXCommon* dx_ = nullptr;
    Camera* camera_ = nullptr;
    TrailManager* trailManager_ = nullptr;

    std::vector<Event> events_;
    std::vector<ActiveEvent> activeEvents_;

    State state_ = State::Idle;
    EffectProfile profile_;
    EffectProfile editingProfile_;
    Vector3 startPos_ = {};
    Vector3 targetPos_ = {};
    Vector3 currentPos_ = {};
    Vector3 projectileRotation_ = {};
    float elapsedTime_ = 0.0f;

    std::unique_ptr<Object3d> projectile_;
    TrailInstance* trail_ = nullptr;
    std::function<void()> onHitCallback_;
    char profileFilename_[128] = "effect_default.json";
};
