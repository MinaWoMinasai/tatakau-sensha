#pragma once
#include "Audio.h"
#include "InkSimulation.h"
#include <array>
#include <string>

// Scene-owned bank on the engine's XAudio2 device. Gameplay emits facts;
// this consumer alone chooses samples, gain, looping, and presentation limits.
class InkAudioDirector {
public:
    void Initialize(); // first scene Update, after Game initializes Audio
    void Update(float dt,ink::Simulation& simulation,bool gameplayInput);
    void Stop();
    void Shutdown();
    void DrawEditor();
    void WriteDiagnostics(const char* path) const;
private:
    enum Clip { Shooter,Tap,Mid,Full,ChargeLoop,ChargeFirst,ChargeFull,Stick,Burst,Count };
    bool Play(Clip clip,float gain=1.0f,bool loop=false,float pitch=1.0f);
    void StopCharge();
    bool SaveSettings();
    bool initialized_=false,enabled_=true,chargeWanted_=false;
    float masterGain_=0.55f,shotGain_=0.80f,chargeGain_=0.38f,impactGain_=0.55f;
    float chargeFade_=0;
    std::array<bool,Count> loaded_{};
    std::array<uint64_t,Count> playCount_{};
    uint64_t failedPlays_=0;
    Audio::VoiceHandle chargeVoice_{};
    std::string status_;
};
