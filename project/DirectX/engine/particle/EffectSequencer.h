#pragma once
#include "ParticleManager.h"
#include <string>
#include <vector>

class EffectSequencer {
public:
    struct Event {
        std::string effectName;
        float delay = 0.0f;
        uint32_t count = 1;
        Vector3 offset = { 0.0f, 0.0f, 0.0f };
    };

    void Initialize(ParticleManager* particleManager = ParticleManager::GetInstance());
    void AddEvent(const Event& event);
    void ClearEvents();
    void Play(const Vector3& position);
    void PlayHitEffect(const Vector3& position);
    void Update(float deltaTime);
    void DrawImGui();

private:
    struct ActiveEvent {
        Event event;
        Vector3 origin = { 0.0f, 0.0f, 0.0f };
        float timer = 0.0f;
        bool emitted = false;
    };

    ParticleManager* particleManager_ = nullptr;
    std::vector<Event> events_;
    std::vector<ActiveEvent> activeEvents_;
};
