#pragma once
#include "Audio.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>

// Scene-owned original audio. Update during reward/pause screens as well so the
// music ducks smoothly without restarting its phrase at every room boundary.
// No engine master gain, unrelated bank, or AudioManager BGM is changed here.
class TankExpeditionAudio {
public:
    TankExpeditionAudio() {
        const auto prefix=L"__tank_expedition_"+std::to_wstring(++nextInstance_)+L"_";
        for(size_t n=0;n<Count;++n) keys_[n]=prefix+Files[n];
    }
    ~TankExpeditionAudio() { Shutdown(); }
    TankExpeditionAudio(const TankExpeditionAudio&)=delete;
    TankExpeditionAudio& operator=(const TankExpeditionAudio&)=delete;

    void Initialize() {
        if(initialized_) return;
        auto* audio=Audio::GetInstance();
        if(!audio->IsReady()) return; // Game initializes Audio after its first scene.
        initialized_=true;
        for(size_t n=0;n<Count;++n)
            loaded_[n]=audio->TryLoadPcmWave(keys_[n],
                (std::filesystem::path(L"resources/audio/tank_expedition")/Files[n]).wstring(),Voices[n]);
        // Equal-length stems share the device playback clock. They are started
        // consecutively, not sample-accurate scheduled, and never restart for UI.
        if(loaded_[MusicBase]) baseVoice_=audio->TryPlayAudio(keys_[MusicBase],true,0);
        if(loaded_[MusicIntensity]) intensityVoice_=audio->TryPlayAudio(keys_[MusicIntensity],true,0);
    }

    void Shutdown() {
        if(initialized_) {
            auto* audio=Audio::GetInstance();
            for(size_t n=0;n<Count;++n) if(loaded_[n]) {
                audio->StopAudio(keys_[n]);
                audio->UnloadAudio(keys_[n]);
            }
        }
        loaded_.fill(false); cooldowns_.fill(0);
        initialized_=false; baseVoice_={}; intensityVoice_={};
        baseGain_=0; intensityGain_=0; variant_=0;
        combat_=false; boss_=false; ducked_=false;
    }

    void SetCombat(bool combat) { combat_=combat; }
    void SetBoss(bool boss) { boss_=boss; }
    void SetDucked(bool ducked) { ducked_=ducked; }
    void SetMusicVolume(float volume) { musicVolume_=Gain(volume); }
    void SetEffectsVolume(float volume) { effectsVolume_=Gain(volume); }

    void Update(float dt) {
        Initialize();
        const float seconds=std::isfinite(dt)?std::clamp(dt,0.0f,0.1f):0.0f;
        for(auto& cooldown:cooldowns_) cooldown=(std::max)(0.0f,cooldown-seconds);
        if(!initialized_) return;
        const float duck=ducked_?0.22f:1.0f;
        const float baseGoal=musicVolume_*(combat_?0.75f:0.32f)*duck;
        const float intensityGoal=musicVolume_*(combat_?(boss_?0.75f:0.30f):0.04f)*duck;
        const float blend=1.0f-std::exp(-seconds/0.28f);
        baseGain_+=(baseGoal-baseGain_)*blend;
        intensityGain_+=(intensityGoal-intensityGain_)*blend;
        auto* audio=Audio::GetInstance();
        audio->SetVolume(baseVoice_,baseGain_);
        audio->SetVolume(intensityVoice_,intensityGain_);
    }

    // One scene call per volley/impact is preferred. These independent global
    // limits also protect the mix when scatter or chains emit many callbacks.
    void Shot() { Play(ShotClip,0.052f,0.48f,NextPitch()); }
    void Hit() { Play(HitClip,0.055f,0.36f,NextPitch()); }
    void Kill(int chain=1) {
        Play(KillClip,0.075f,0.76f,1.0f+0.022f*static_cast<float>(std::clamp(chain,1,9)-1));
    }
    void EnemyWarning() { Play(WarningClip,0.28f,0.64f); }
    void Dash() { Play(DashClip,0.12f,0.56f); }
    void Upgrade() { Play(UpgradeClip,0.18f,0.68f); }
    void ArmorBreak() { Play(ArmorBreakClip,0.10f,0.67f); }
    void UiHover() { Play(HitClip,0.09f,0.13f,1.65f); }
    void UiConfirm() { Play(DashClip,0.14f,0.32f,1.25f); }
    void UiDenied() { Play(ArmorBreakClip,0.20f,0.25f,0.75f); }
    void Collect(int amount) { Play(HitClip,0.045f,0.23f,1.35f+0.035f*static_cast<float>(std::clamp(amount,1,8))); }
    void Slash() { Play(DashClip,0.12f,0.38f,1.45f); }
    // Keep the enemy warning voice/cooldown free while the player charges.
    void RailCharge(bool full) { Play(UpgradeClip,0.25f,full?0.25f:0.14f,full?1.6f:1.15f); }
    void RailShot() { Play(ArmorBreakClip,0.12f,0.60f,0.78f); }
    void Parry(bool perfect) { Play(ArmorBreakClip,0.10f,perfect?0.58f:0.34f,perfect?1.65f:1.30f); }
    void Slam(bool finisher=false) { Play(ArmorBreakClip,0.12f,finisher?0.80f:0.58f,finisher?0.72f:0.92f); }

    size_t LoadedClipCount() const {
        return static_cast<size_t>(std::count(loaded_.begin(),loaded_.end(),true));
    }
    bool IsMusicPlaying() const {
        auto* audio=Audio::GetInstance();
        return audio->IsVoicePlaying(baseVoice_) || audio->IsVoicePlaying(intensityVoice_);
    }
    uint64_t PlayCount() const { return playCount_; }
    static constexpr size_t ClipCount() { return Count; }

private:
    enum Clip:size_t { ShotClip,HitClip,KillClip,WarningClip,DashClip,UpgradeClip,
        ArmorBreakClip,MusicBase,MusicIntensity,Count };
    inline static constexpr const wchar_t* Files[Count]={L"shot.wav",L"hit.wav",L"kill.wav",
        L"warning.wav",L"dash.wav",L"upgrade.wav",L"armor_break.wav",L"music_base.wav",L"music_intensity.wav"};
    inline static constexpr size_t Voices[Count]={3,3,3,2,2,2,2,1,1};
    inline static std::atomic<uint64_t> nextInstance_{0};

    static float Gain(float value) { return std::isfinite(value)?std::clamp(value,0.0f,1.0f):0.0f; }
    float NextPitch() {
        constexpr float pitches[]={1.00f,0.97f,1.025f,0.985f,1.015f};
        return pitches[variant_++%5]; // Presentation never consumes gameplay RNG.
    }
    void Play(Clip clip,float cooldown,float gain,float pitch=1.0f) {
        if(!loaded_[clip] || cooldowns_[clip]>0 || effectsVolume_<=0) return;
        const auto handle=Audio::GetInstance()->TryPlayAudio(keys_[clip],false,Gain(gain*effectsVolume_),pitch);
        if(handle.IsValid()) { cooldowns_[clip]=cooldown; ++playCount_; }
    }

    std::array<std::wstring,Count> keys_{};
    std::array<bool,Count> loaded_{};
    std::array<float,Count> cooldowns_{};
    Audio::VoiceHandle baseVoice_{},intensityVoice_{};
    bool initialized_=false,combat_=false,boss_=false,ducked_=false;
    float musicVolume_=0.55f,effectsVolume_=0.80f,baseGain_=0,intensityGain_=0;
    uint64_t variant_=0,playCount_=0;
};
