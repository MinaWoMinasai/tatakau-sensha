#include "EffectSequencer.h"
#include <algorithm>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void EffectSequencer::Initialize(ParticleManager* particleManager) {
    particleManager_ = particleManager;
    ClearEvents();

    Event hit{};
    hit.effectName = "HitSpark";
    hit.delay = 0.0f;
    hit.count = 1;
    AddEvent(hit);
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

void EffectSequencer::Update(float deltaTime) {
    if (!particleManager_) {
        return;
    }

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

void EffectSequencer::DrawImGui() {
#ifdef USE_IMGUI
    if (ImGui::Begin("Effect Sequencer")) {
        ImGui::Text("Events: %zu", events_.size());
        ImGui::Text("Active: %zu", activeEvents_.size());
        if (ImGui::Button("Play Hit Sample")) {
            PlayHitEffect({ 0.0f, 0.0f, 0.0f });
        }
    }
    ImGui::End();
#endif
}
