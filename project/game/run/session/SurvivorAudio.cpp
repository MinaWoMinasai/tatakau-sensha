#include "game/run/session/SurvivorAudio.h"
#include <algorithm>
#include <cmath>
#include <filesystem>

namespace gameplay {
namespace {
constexpr const char* kAudioRoot = "resources/audio/";

std::wstring Widen(const std::string& text)
{
    // 場所は英数字と記号だけを想定する（設定ファイルに日本語のファイル名を書かない）。
    return std::filesystem::path(text).wstring();
}
} // namespace

void SurvivorAudio::Initialize(const survivor::AudioConfig& config, bool muted)
{
    if (initialized_) {
        Shutdown();
    }
    config_ = config;
    muted_ = muted;
    if (!myEngine::Audio::GetInstance()->IsReady()) {
        return;
    }
    initialized_ = true;
    // 同じファイルは1回だけ読み、重ねて鳴らせる数は、そのファイルを使う音の中で一番多いものに合わせる。
    std::map<std::string, int> voices;
    for (const auto& [name, cue] : config_.cues) {
        voices[cue.file] = (std::max)(voices[cue.file], (std::clamp)(cue.voices, 1, 8));
    }
    for (const auto& [name, track] : config_.music) {
        voices[track.file] = (std::max)(voices[track.file], 2);
    }
    for (const auto& [file, count] : voices) {
        if (file.empty()) {
            continue;
        }
        const std::wstring key = LoadFile(file, count);
        if (key.empty()) {
            ++missingFiles_;
        } else {
            fileKeys_[file] = key;
            ++loadedFiles_;
        }
    }
}

std::wstring SurvivorAudio::LoadFile(const std::string& file, int voices)
{
    const std::filesystem::path path(std::string(kAudioRoot) + file);
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        return {};
    }
    const std::wstring key = L"__survivor_" + Widen(file);
    auto* audio = myEngine::Audio::GetInstance();
    std::wstring extension = path.extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);
    if (extension == L".wav") {
        // 16bit の PCM はそのまま読み、ほかの形の wav は Media Foundation に任せる。
        if (audio->TryLoadPcmWave(key, path.wstring(), static_cast<size_t>(voices))) {
            return key;
        }
    }
    audio->LoadAudio(key, path.wstring(), static_cast<size_t>(voices));
    return key;
}

void SurvivorAudio::Shutdown()
{
    if (initialized_) {
        auto* audio = myEngine::Audio::GetInstance();
        for (const auto& [file, key] : fileKeys_) {
            audio->StopAudio(key);
            audio->UnloadAudio(key);
        }
    }
    fileKeys_.clear();
    cooldowns_.clear();
    initialized_ = false;
    current_ = {};
    previous_ = {};
    currentTrack_.clear();
}

bool SurvivorAudio::Play(const std::string& cue)
{
    if (!initialized_) {
        return false;
    }
    auto found = config_.cues.find(cue);
    if (found == config_.cues.end()) {
        return false;
    }
    const survivor::AudioCue& entry = found->second;
    float& cooldown = cooldowns_[cue];
    if (cooldown > 0.0f) {
        return false;
    }
    const auto key = fileKeys_.find(entry.file);
    if (key == fileKeys_.end()) {
        return false;
    }
    // 連続で鳴らすときは、高さを少しずつ揺らして同じ音に聞こえにくくする（ゲームの乱数は使わない）。
    static constexpr float kSteps[] = {0.0f, -1.0f, 0.8f, -0.5f, 0.4f};
    const float pitch = entry.pitch * (1.0f + entry.pitchJitter * kSteps[variant_++ % 5]);
    const float volume = muted_ ? 0.0f : (std::clamp)(entry.volume * config_.effectsVolume, 0.0f, 1.0f);
    myEngine::Audio::GetInstance()->TryPlayAudio(key->second, false, volume, pitch);
    cooldown = entry.cooldown;
    ++playCounts_[cue];
    return true;
}

void SurvivorAudio::StartTrack(Track& track, const std::string& name)
{
    track = {};
    track.name = name;
    const auto found = config_.music.find(name);
    if (found == config_.music.end()) {
        return;
    }
    const auto key = fileKeys_.find(found->second.file);
    if (key == fileKeys_.end()) {
        return;
    }
    track.voice = myEngine::Audio::GetInstance()->TryPlayAudio(key->second, true, 0.0f, found->second.pitch);
    track.target = muted_ ? 0.0f : (std::clamp)(found->second.volume * config_.musicVolume, 0.0f, 1.0f);
}

void SurvivorAudio::SetMusic(const std::string& track)
{
    if (!initialized_ || track == currentTrack_) {
        return;
    }
    currentTrack_ = track;
    // 下げている途中の前の曲は止め、今の曲を前の曲として下げていく。
    if (previous_.voice.IsValid()) {
        myEngine::Audio::GetInstance()->StopAudio(previous_.voice);
    }
    previous_ = current_;
    previous_.target = 0.0f;
    StartTrack(current_, track);
    ++playCounts_["music_" + track];
}

void SurvivorAudio::Update(float dt, bool ducked)
{
    if (!initialized_) {
        return;
    }
    const float seconds = std::isfinite(dt) ? (std::clamp)(dt, 0.0f, 0.1f) : 0.0f;
    for (auto& [name, cooldown] : cooldowns_) {
        cooldown = (std::max)(0.0f, cooldown - seconds);
    }
    auto* audio = myEngine::Audio::GetInstance();
    const float step = seconds / (std::max)(0.05f, config_.fadeSeconds);
    auto approach = [&](Track& track, float target) {
        if (!track.voice.IsValid()) {
            return;
        }
        track.gain = track.gain < target ? (std::min)(target, track.gain + step) : (std::max)(target, track.gain - step);
        audio->SetVolume(track.voice, track.gain);
    };
    approach(current_, current_.target * (ducked ? config_.duckRatio : 1.0f));
    approach(previous_, 0.0f);
    if (previous_.voice.IsValid() && previous_.gain <= 0.0f) {
        audio->StopAudio(previous_.voice);
        previous_ = {};
    }
}
} // namespace gameplay
