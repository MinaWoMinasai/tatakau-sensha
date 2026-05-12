#include "EffectSequencer.h"
#include "Object3d.h"
#include "Object3dCommon.h"
#include "DirectXCommon.h"
#include "Camera.h"
#include "TrailManager.h"
#include "TrailInstance.h"
#include "ModelManager.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <cstdio>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

nlohmann::json ProjectileProfile::ToJson() const {
    return {
        {"modelPath", modelPath},
        {"scale", {scale.x, scale.y, scale.z}},
        {"rotationSpeed", {rotationSpeed.x, rotationSpeed.y, rotationSpeed.z}},
    };
}

void ProjectileProfile::FromJson(const nlohmann::json& j) {
    modelPath = j.value("modelPath", modelPath);
    if (j.contains("scale") && j["scale"].is_array() && j["scale"].size() >= 3) {
        scale = { j["scale"][0].get<float>(), j["scale"][1].get<float>(), j["scale"][2].get<float>() };
    }
    if (j.contains("rotationSpeed") && j["rotationSpeed"].is_array() && j["rotationSpeed"].size() >= 3) {
        rotationSpeed = { j["rotationSpeed"][0].get<float>(), j["rotationSpeed"][1].get<float>(), j["rotationSpeed"][2].get<float>() };
    }
}

nlohmann::json TrailProfile::ToJson() const {
    return {
        {"startColor", {startColor.x, startColor.y, startColor.z, startColor.w}},
        {"endColor", {endColor.x, endColor.y, endColor.z, endColor.w}},
        {"maxPoints", maxPoints},
        {"interpolationSteps", interpolationSteps},
        {"tipOffset", {tipOffset.x, tipOffset.y, tipOffset.z}},
        {"baseOffset", {baseOffset.x, baseOffset.y, baseOffset.z}},
        {"lifetime", lifetime},
    };
}

void TrailProfile::FromJson(const nlohmann::json& j) {
    if (j.contains("startColor") && j["startColor"].is_array() && j["startColor"].size() >= 4) {
        startColor = { j["startColor"][0].get<float>(), j["startColor"][1].get<float>(), j["startColor"][2].get<float>(), j["startColor"][3].get<float>() };
    }
    if (j.contains("endColor") && j["endColor"].is_array() && j["endColor"].size() >= 4) {
        endColor = { j["endColor"][0].get<float>(), j["endColor"][1].get<float>(), j["endColor"][2].get<float>(), j["endColor"][3].get<float>() };
    }
    maxPoints = j.value("maxPoints", maxPoints);
    interpolationSteps = j.value("interpolationSteps", interpolationSteps);
    if (j.contains("tipOffset") && j["tipOffset"].is_array() && j["tipOffset"].size() >= 3) {
        tipOffset = { j["tipOffset"][0].get<float>(), j["tipOffset"][1].get<float>(), j["tipOffset"][2].get<float>() };
    }
    if (j.contains("baseOffset") && j["baseOffset"].is_array() && j["baseOffset"].size() >= 3) {
        baseOffset = { j["baseOffset"][0].get<float>(), j["baseOffset"][1].get<float>(), j["baseOffset"][2].get<float>() };
    }
    lifetime = j.value("lifetime", lifetime);
}

nlohmann::json EffectProfile::ToJson() const {
    return {
        {"projectile", projectile.ToJson()},
        {"flyParticle", flyParticle},
        {"hitParticle", hitParticle},
        {"trail", trail.ToJson()},
        {"duration", duration},
        {"hitDuration", hitDuration},
        {"flyParticleCount", flyParticleCount},
        {"hitParticleCount", hitParticleCount},
        {"enableTrail", enableTrail},
    };
}

void EffectProfile::FromJson(const nlohmann::json& j) {
    if (j.contains("projectile")) {
        projectile.FromJson(j["projectile"]);
    }
    flyParticle = j.value("flyParticle", flyParticle);
    hitParticle = j.value("hitParticle", hitParticle);
    if (j.contains("trail")) {
        trail.FromJson(j["trail"]);
    }
    duration = j.value("duration", duration);
    hitDuration = j.value("hitDuration", hitDuration);
    flyParticleCount = j.value("flyParticleCount", flyParticleCount);
    hitParticleCount = j.value("hitParticleCount", hitParticleCount);
    enableTrail = j.value("enableTrail", enableTrail);
}

Vector3 EffectSequencer::LerpVec3(const Vector3& a, const Vector3& b, float t) {
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
    };
}

void EffectSequencer::Initialize(ParticleManager* particleManager) {
    particleManager_ = particleManager;
    ClearEvents();

    Event hit{};
    hit.effectName = "HitSpark";
    hit.delay = 0.0f;
    hit.count = 1;
    AddEvent(hit);
    editingProfile_ = EffectProfile{};
}

void EffectSequencer::Initialize(Object3dCommon* objCommon, DirectXCommon* dx, Camera* camera, ParticleManager* particleManager, TrailManager* trailManager) {
    Initialize(particleManager);
    objCommon_ = objCommon;
    dx_ = dx;
    camera_ = camera;
    trailManager_ = trailManager;
}

void EffectSequencer::AddEvent(const Event& event) {
    events_.push_back(event);
}

void EffectSequencer::ClearEvents() {
    events_.clear();
    activeEvents_.clear();
}

void EffectSequencer::Play(const Vector3& position) {
    for (const Event& event : events_) {
        activeEvents_.push_back({ event, position, 0.0f, false });
    }
}

void EffectSequencer::PlayHitEffect(const Vector3& position) {
    if (particleManager_) {
        particleManager_->EmitHitEffect(position);
    }
}

void EffectSequencer::Fire(const EffectProfile& profile, const Vector3& startPos, const Vector3& targetPos) {
    if (IsActive()) {
        Reset();
    }

    profile_ = profile;
    startPos_ = startPos;
    targetPos_ = targetPos;
    currentPos_ = startPos;
    elapsedTime_ = 0.0f;
    projectileRotation_ = { 0.0f, 0.0f, 0.0f };
    state_ = State::Firing;
}

void EffectSequencer::Update(float deltaTime) {
    switch (state_) {
    case State::Firing:
        UpdateFiring(deltaTime);
        break;
    case State::Flying:
        UpdateFlying(deltaTime);
        break;
    case State::Hit:
        UpdateHit(deltaTime);
        break;
    case State::Idle:
    case State::Finished:
    default:
        break;
    }

    if (particleManager_) {
        for (ActiveEvent& active : activeEvents_) {
            active.timer += deltaTime;
            if (!active.emitted && active.timer >= active.event.delay) {
                Vector3 emitPosition = active.origin + active.event.offset;
                if (active.event.effectName == "HitSpark") {
                    particleManager_->EmitHitEffect(emitPosition);
                } else {
                    particleManager_->Emit(active.event.effectName, emitPosition, active.event.count);
                }
                active.emitted = true;
            }
        }

        std::erase_if(activeEvents_, [](const ActiveEvent& active) {
            return active.emitted;
        });
    }
}

void EffectSequencer::UpdateFiring(float deltaTime) {
    if (!objCommon_ || !dx_ || !camera_) {
        state_ = State::Finished;
        return;
    }

    ModelManager::GetInstance()->LoadModel(profile_.projectile.modelPath);

    projectile_ = std::make_unique<Object3d>();
    projectile_->Initialize();
    projectile_->SetModel(profile_.projectile.modelPath);
    projectile_->SetScale(profile_.projectile.scale);
    projectile_->SetTranslate(startPos_);
    projectile_->SetLighting(false);
    projectile_->SetCamera(camera_);
    projectile_->Update();

    if (profile_.enableTrail && trailManager_) {
        trail_ = trailManager_->CreateInstance();
        if (trail_) {
            trail_->SetIsPermanent(false);
            trail_->SetActive(true);
            TrailConfig config;
            config.startColor = profile_.trail.startColor;
            config.endColor = profile_.trail.endColor;
            config.maxPoints = profile_.trail.maxPoints;
            config.interpolationSteps = profile_.trail.interpolationSteps;
            config.lifetime = profile_.trail.lifetime;
            trail_->SetConfig(config);
        }
    }

    state_ = State::Flying;
    UpdateFlying(deltaTime);
}

void EffectSequencer::UpdateFlying(float deltaTime) {
    elapsedTime_ += deltaTime;

    float duration = (std::max)(profile_.duration, 0.001f);
    float t = std::clamp(elapsedTime_ / duration, 0.0f, 1.0f);
    float eased = t * t * (3.0f - 2.0f * t);
    currentPos_ = LerpVec3(startPos_, targetPos_, eased);

    projectileRotation_ += profile_.projectile.rotationSpeed * deltaTime;
    if (projectile_) {
        projectile_->SetTranslate(currentPos_);
        projectile_->SetRotate(projectileRotation_);
        projectile_->Update();
    }

    if (particleManager_ && !profile_.flyParticle.empty()) {
        if (profile_.flyParticle == "HitSpark") {
            particleManager_->EmitHitEffect(currentPos_);
        } else {
            particleManager_->Emit(profile_.flyParticle, currentPos_, profile_.flyParticleCount);
        }
    }

    if (trail_ && trail_->IsActive()) {
        Vector3 tipPos = currentPos_ + profile_.trail.tipOffset;
        Vector3 basePos = currentPos_ + profile_.trail.baseOffset;
        TrailConfig config;
        config.startColor = profile_.trail.startColor;
        config.endColor = profile_.trail.endColor;
        config.maxPoints = profile_.trail.maxPoints;
        config.interpolationSteps = profile_.trail.interpolationSteps;
        config.lifetime = profile_.trail.lifetime;
        trail_->Update(deltaTime, tipPos, basePos, config);
    }

    if (t >= 1.0f) {
        state_ = State::Hit;
        elapsedTime_ = 0.0f;
        projectile_.reset();
        if (trail_) {
            trail_->SetActive(false);
        }

        if (particleManager_ && !profile_.hitParticle.empty()) {
            if (profile_.hitParticle == "HitSpark") {
                for (uint32_t i = 0; i < profile_.hitParticleCount; ++i) {
                    particleManager_->EmitHitEffect(targetPos_);
                }
            } else {
                particleManager_->Emit(profile_.hitParticle, targetPos_, profile_.hitParticleCount);
            }
        }

        if (onHitCallback_) {
            onHitCallback_();
        }
    }
}

void EffectSequencer::UpdateHit(float deltaTime) {
    elapsedTime_ += deltaTime;
    if (elapsedTime_ >= profile_.hitDuration) {
        state_ = State::Finished;
        trail_ = nullptr;
    }
}

void EffectSequencer::Draw() {
    if (state_ == State::Flying && projectile_) {
        projectile_->Draw();
    }
}

void EffectSequencer::Reset() {
    state_ = State::Idle;
    elapsedTime_ = 0.0f;
    projectile_.reset();
    if (trail_) {
        trail_->SetActive(false);
        trail_ = nullptr;
    }
    currentPos_ = {};
    projectileRotation_ = {};
}

void EffectSequencer::SaveProfile(const std::string& path, const EffectProfile& profile) {
    std::ofstream file(path);
    if (!file) {
        return;
    }
    file << std::setw(4) << profile.ToJson() << std::endl;
}

bool EffectSequencer::LoadProfile(const std::string& path, EffectProfile& profile) {
    std::ifstream file(path);
    if (!file) {
        return false;
    }

    nlohmann::json json;
    file >> json;
    profile.FromJson(json);
    return true;
}

void EffectSequencer::DrawImGui() {
#ifdef USE_IMGUI
    if (ImGui::Begin("Effect Sequencer")) {
        ImGui::Text("Events: %zu", events_.size());
        ImGui::Text("Active Events: %zu", activeEvents_.size());
        ImGui::Text("Projectile State: %d", static_cast<int>(state_));
        if (ImGui::Button("Play Hit Sample")) {
            PlayHitEffect({ 0.0f, 0.0f, 0.0f });
        }
    }
    ImGui::End();
#endif
}

#ifdef USE_IMGUI
void EffectSequencer::DrawImGuiEditor(const Vector3& defaultStartPos, const Vector3& defaultTargetPos) {
    if (ImGui::Begin("Attack Effect Editor")) {
        static char modelBuffer[256] = "ball.obj";
        static char flyBuffer[128] = "HitSpark";
        static char hitBuffer[128] = "HitSpark";

        ImGui::Text("State: %d", static_cast<int>(state_));
        if (ImGui::InputText("Model Path", modelBuffer, sizeof(modelBuffer))) {
            editingProfile_.projectile.modelPath = modelBuffer;
        }
        ImGui::DragFloat3("Projectile Scale", &editingProfile_.projectile.scale.x, 0.01f, 0.01f, 10.0f);
        ImGui::DragFloat3("Rotation Speed", &editingProfile_.projectile.rotationSpeed.x, 0.1f);

        if (ImGui::InputText("Fly Particle", flyBuffer, sizeof(flyBuffer))) {
            editingProfile_.flyParticle = flyBuffer;
        }
        int flyCount = static_cast<int>(editingProfile_.flyParticleCount);
        if (ImGui::SliderInt("Fly Count", &flyCount, 0, 20)) {
            editingProfile_.flyParticleCount = static_cast<uint32_t>(flyCount);
        }

        if (ImGui::InputText("Hit Particle", hitBuffer, sizeof(hitBuffer))) {
            editingProfile_.hitParticle = hitBuffer;
        }
        int hitCount = static_cast<int>(editingProfile_.hitParticleCount);
        if (ImGui::SliderInt("Hit Count", &hitCount, 1, 20)) {
            editingProfile_.hitParticleCount = static_cast<uint32_t>(hitCount);
        }

        ImGui::Checkbox("Enable Trail", &editingProfile_.enableTrail);
        ImGui::ColorEdit4("Trail Start", &editingProfile_.trail.startColor.x);
        ImGui::ColorEdit4("Trail End", &editingProfile_.trail.endColor.x);
        ImGui::DragFloat3("Tip Offset", &editingProfile_.trail.tipOffset.x, 0.01f);
        ImGui::DragFloat3("Base Offset", &editingProfile_.trail.baseOffset.x, 0.01f);
        ImGui::DragFloat("Duration", &editingProfile_.duration, 0.05f, 0.1f, 10.0f);
        ImGui::DragFloat("Hit Duration", &editingProfile_.hitDuration, 0.05f, 0.1f, 5.0f);

        if (ImGui::Button("Test Fire")) {
            Fire(editingProfile_, defaultStartPos, defaultTargetPos);
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop")) {
            Reset();
        }

        ImGui::InputText("Filename", profileFilename_, sizeof(profileFilename_));
        if (ImGui::Button("Save Profile")) {
            SaveProfile(std::string("resources/effects/") + profileFilename_, editingProfile_);
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Profile")) {
            LoadProfile(std::string("resources/effects/") + profileFilename_, editingProfile_);
            std::snprintf(modelBuffer, sizeof(modelBuffer), "%s", editingProfile_.projectile.modelPath.c_str());
            std::snprintf(flyBuffer, sizeof(flyBuffer), "%s", editingProfile_.flyParticle.c_str());
            std::snprintf(hitBuffer, sizeof(hitBuffer), "%s", editingProfile_.hitParticle.c_str());
        }
    }
    ImGui::End();
}
#endif
