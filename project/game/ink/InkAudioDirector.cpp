#include "InkAudioDirector.h"
#include "externals/nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace {
constexpr const wchar_t* Keys[]={L"__ink_shooter",L"__ink_stringer_tap",L"__ink_stringer_mid",
    L"__ink_stringer_full",L"__ink_charge_loop",L"__ink_charge_first",L"__ink_charge_full",L"__ink_arrow_stick",L"__ink_arrow_burst"};
constexpr const wchar_t* Files[]={L"shooter_shot.wav",L"stringer_tap.wav",L"stringer_mid.wav",
    L"stringer_full.wav",L"charge_loop.wav",L"charge_first.wav",L"charge_full.wav",L"arrow_stick.wav",L"arrow_burst.wav"};
constexpr const char* Names[]={"シューター発射","短押し発射","1段階発射","最大チャージ発射",
    "チャージ持続","1段階到達","最大チャージ到達","冷却矢の着弾","矢の爆発"};
constexpr size_t Voices[]={4,3,3,3,1,1,1,4,4};
constexpr const char* SettingsPath="resources/configs/ink_audio.json";
float ClampGain(float gain) { return std::isfinite(gain)?std::clamp(gain,0.0f,1.0f):0.0f; }
}

void InkAudioDirector::Initialize() {
    if(initialized_) return;
    initialized_=true;
    try {
        std::ifstream input(SettingsPath);
        if(input) {
            nlohmann::json data; input>>data;
            if(data.at("schemaVersion").get<int>()!=1) throw std::runtime_error("未対応の音設定です。");
            const bool enabled=data.at("enabled").get<bool>();
            std::array<float,4> values{data.at("master").get<float>(),data.at("shot").get<float>(),
                data.at("charge").get<float>(),data.at("impact").get<float>()};
            for(float value:values) if(!std::isfinite(value)||value<0||value>1) throw std::runtime_error("音量は0〜1で指定してください。");
            enabled_=enabled; masterGain_=values[0]; shotGain_=values[1]; chargeGain_=values[2]; impactGain_=values[3];
        }
    } catch(const std::exception&) { status_="音量設定を読めなかったため、初期音量を使います。"; }
    auto* audio=Audio::GetInstance();
    if(!audio->IsReady()) { status_="音声デバイスを利用できません。ゲームの操作は続けられます。"; return; }
    int count=0;
    for(int n=0;n<Count;++n) {
        loaded_[n]=audio->TryLoadPcmWave(Keys[n],(std::filesystem::path(L"resources/audio/ink")/Files[n]).wstring(),Voices[n]);
        if(loaded_[n]) ++count;
    }
    if(count!=Count) status_="SE読込："+std::to_string(count)+" / "+std::to_string(Count)+"。不足した音は無音になります。";
    else if(status_.empty()) status_="原作の音を参考にした独自の合成SEを読み込みました。";
}

bool InkAudioDirector::Play(Clip clip,float gain,bool loop,float pitch) {
    if(!enabled_||!loaded_[clip]) return false;
    const auto handle=Audio::GetInstance()->TryPlayAudio(Keys[clip],loop,ClampGain(masterGain_*gain),pitch);
    if(!handle.IsValid()) { ++failedPlays_; return false; }
    ++playCount_[clip];
    if(loop) chargeVoice_=handle;
    return true;
}

void InkAudioDirector::StopCharge() {
    chargeWanted_=false;
    if(chargeVoice_.IsValid()) Audio::GetInstance()->StopAudio(chargeVoice_);
    chargeVoice_={}; chargeFade_=0;
}

void InkAudioDirector::Stop() {
    StopCharge();
    if(initialized_) for(int n=0;n<Count;++n) if(loaded_[n]) Audio::GetInstance()->StopAudio(Keys[n]);
}

void InkAudioDirector::Shutdown() {
    Stop();
    if(initialized_) for(int n=0;n<Count;++n) if(loaded_[n]) Audio::GetInstance()->UnloadAudio(Keys[n]);
    loaded_.fill(false); initialized_=false;
}

void InkAudioDirector::Update(float dt,ink::Simulation& simulation,bool gameplayInput) {
    Initialize();
    const auto events=simulation.TakeAudioEvents();
    if(!enabled_) { Stop(); return; }
    const auto listener=simulation.Player().position;
    // One closely grouped volley makes one impact sound per displayed frame.
    // Separate distant impacts remain separate; no audio choice feeds physics.
    std::vector<ink::AudioEvent> impacts;
    for(const auto& event:events) {
        switch(event.cue) {
        case ink::AudioCue::ShooterShot: Play(Shooter,shotGain_); break;
        case ink::AudioCue::StringerShot:
            chargeWanted_=false;
            Play(event.chargeLevel>=2?Full:event.chargeLevel==1?Mid:Tap,shotGain_); break;
        case ink::AudioCue::ChargeStart: chargeWanted_=gameplayInput; break;
        case ink::AudioCue::ChargeFirst: if(gameplayInput) Play(ChargeFirst,chargeGain_); break;
        case ink::AudioCue::ChargeFull: if(gameplayInput) Play(ChargeFull,chargeGain_); break;
        case ink::AudioCue::ChargeCancel:
        case ink::AudioCue::Empty: chargeWanted_=false; break;
        case ink::AudioCue::ArrowStick:
        case ink::AudioCue::ArrowBurst: {
            bool grouped=false;
            for(const auto& prior:impacts) if(prior.cue==event.cue && ink::Length(prior.position-event.position)<1.5f) { grouped=true; break; }
            if(!grouped) {
                impacts.push_back(event);
                const float distance=ink::Length(listener-event.position);
                const float attenuation=1.0f/(1.0f+distance*distance*0.025f);
                Play(event.cue==ink::AudioCue::ArrowStick?Stick:Burst,impactGain_*attenuation);
            }
            break;
        }
        }
    }
    chargeWanted_=gameplayInput && simulation.IsCharging();
    const float seconds=std::clamp(dt,0.0f,0.1f);
    const float goal=chargeWanted_?1.0f:0.0f;
    chargeFade_+=std::clamp(goal-chargeFade_,-seconds/0.035f,seconds/0.055f);
    if(chargeWanted_ && !chargeVoice_.IsValid()) Play(ChargeLoop,0,true,0.8f);
    if(chargeVoice_.IsValid()) {
        auto* audio=Audio::GetInstance();
        audio->SetVolume(chargeVoice_,ClampGain(masterGain_*chargeGain_*chargeFade_));
        // Pitch is presentation only; charge stages come from the simulation,
        // including slower airborne charging and live custom weapon timings.
        audio->SetPitch(chargeVoice_,0.80f+0.35f*simulation.ChargeProfile().normalizedCharge);
        if(!chargeWanted_ && chargeFade_<=0) StopCharge();
    }
}

bool InkAudioDirector::SaveSettings() {
    const auto temp=std::filesystem::path("resources/configs/ink_audio.json.tmp");
    const nlohmann::json data={{"schemaVersion",1},{"enabled",enabled_},{"master",masterGain_},
        {"shot",shotGain_},{"charge",chargeGain_},{"impact",impactGain_}};
    try {
        { std::ofstream out(temp,std::ios::binary|std::ios::trunc); out<<data.dump(2)<<'\n'; out.flush(); if(!out) throw std::runtime_error("write"); }
        if(!MoveFileExW(temp.c_str(),std::filesystem::path(SettingsPath).c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("replace");
        status_="音量を保存しました。"; return true;
    } catch(const std::exception&) {
        std::error_code ec; std::filesystem::remove(temp,ec);
        status_="音量を保存できませんでした。現在の音量と既存ファイルを保持します。"; return false;
    }
}

void InkAudioDirector::DrawEditor() {
#ifdef USE_IMGUI
    if(!ImGui::CollapsingHeader("SE・音量###InkAudio")) return;
    if(ImGui::Checkbox("効果音を鳴らす###EnableInkAudio",&enabled_) && !enabled_) Stop();
    if(ImGui::SliderFloat("全体###InkMasterGain",&masterGain_,0,1,"%.2f") && masterGain_<=0) Stop();
    if(ImGui::SliderFloat("発射###InkShotGain",&shotGain_,0,1,"%.2f") && shotGain_<=0)
        for(int n=Shooter;n<=Full;++n) Audio::GetInstance()->StopAudio(Keys[n]);
    if(ImGui::SliderFloat("チャージ###InkChargeGain",&chargeGain_,0,1,"%.2f") && chargeGain_<=0) {
        StopCharge();
        Audio::GetInstance()->StopAudio(Keys[ChargeFirst]); Audio::GetInstance()->StopAudio(Keys[ChargeFull]);
    }
    if(ImGui::SliderFloat("着弾・爆発###InkImpactGain",&impactGain_,0,1,"%.2f") && impactGain_<=0) {
        Audio::GetInstance()->StopAudio(Keys[Stick]); Audio::GetInstance()->StopAudio(Keys[Burst]);
    }
    ImGui::TextWrapped("弦の響き・インクの飛散・音程変化を組み合わせた独自の合成SEです。元動画の音声は使っていません。音量を0にすると無音です。");
    for(int n=0;n<Count;++n) {
        ImGui::PushID(n);
        ImGui::BeginDisabled(!loaded_[n]||n==ChargeLoop);
        if(ImGui::Button(Names[n])) Play(static_cast<Clip>(n),n<ChargeLoop?shotGain_:n<Stick?chargeGain_:impactGain_);
        ImGui::EndDisabled(); ImGui::PopID();
    }
    if(ImGui::Button("音量を保存###SaveInkAudio")) SaveSettings();
    if(!status_.empty()) ImGui::TextWrapped("%s",status_.c_str());
#endif
}

void InkAudioDirector::WriteDiagnostics(const char* path) const {
    std::ofstream out(path);
    out<<"Audio initialized: "<<initialized_<<"\nXAudio2 ready: "<<Audio::GetInstance()->IsReady()
       <<"\nPlayback failures: "<<failedPlays_<<"\nCharge loop active: "<<Audio::GetInstance()->IsVoicePlaying(chargeVoice_)<<'\n';
    for(int n=0;n<Count;++n) out<<Names[n]<<": loaded="<<loaded_[n]<<", started="<<playCount_[n]<<", voices="<<Voices[n]<<'\n';
}
