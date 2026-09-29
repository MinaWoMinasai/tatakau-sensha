#include "Audio.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <type_traits>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "Mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "Mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace {
static_assert(std::is_nothrow_move_assignable_v<Audio::AudioData>);
float SafeGain(float value) {
    return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
}
float SafePitch(float value) {
    return std::isfinite(value) ? std::clamp(value, 0.25f, 2.0f) : 1.0f;
}
uint16_t WaveU16(const BYTE* bytes) {
    return static_cast<uint16_t>(bytes[0] | (static_cast<uint16_t>(bytes[1]) << 8));
}
uint32_t WaveU32(const BYTE* bytes) {
    return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
        (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
}

// This bounded parser has no device/COM dependency. Offsets are validated before
// reading bytes; a WAVE struct is never cast over potentially unaligned input.
bool ParsePcm16Wave(const std::vector<BYTE>& bytes, WAVEFORMATEX& format,
                   size_t& dataOffset, size_t& dataSize) {
    if (bytes.size() < 44 || bytes.size() > Audio::kMaxPcmWaveBytes ||
        std::memcmp(bytes.data(), "RIFF", 4) != 0 ||
        std::memcmp(bytes.data() + 8, "WAVE", 4) != 0) return false;
    const size_t riffSize = WaveU32(bytes.data() + 4);
    if (riffSize != bytes.size() - 8) return false;
    bool hasFormat = false, hasData = false;
    WAVEFORMATEX candidate{};
    size_t candidateOffset = 0, candidateSize = 0;
    for (size_t position = 12; position < bytes.size();) {
        if (bytes.size() - position < 8) return false;
        const BYTE* header = bytes.data() + position;
        const size_t chunkSize = WaveU32(header + 4);
        position += 8;
        if (chunkSize > bytes.size() - position) return false;
        const BYTE* payload = bytes.data() + position;
        if (std::memcmp(header, "fmt ", 4) == 0) {
            if (hasFormat || (chunkSize != 16 && chunkSize != 18) ||
                (chunkSize == 18 && WaveU16(payload + 16) != 0)) return false;
            candidate.wFormatTag = WaveU16(payload);
            candidate.nChannels = WaveU16(payload + 2);
            candidate.nSamplesPerSec = WaveU32(payload + 4);
            candidate.nAvgBytesPerSec = WaveU32(payload + 8);
            candidate.nBlockAlign = WaveU16(payload + 12);
            candidate.wBitsPerSample = WaveU16(payload + 14);
            if (candidate.wFormatTag != WAVE_FORMAT_PCM || candidate.wBitsPerSample != 16 ||
                candidate.nChannels < 1 || candidate.nChannels > 2 ||
                candidate.nSamplesPerSec < 8000 || candidate.nSamplesPerSec > 192000 ||
                candidate.nBlockAlign != candidate.nChannels * 2 ||
                candidate.nAvgBytesPerSec != candidate.nSamplesPerSec * candidate.nBlockAlign) return false;
            hasFormat = true;
        } else if (std::memcmp(header, "data", 4) == 0) {
            if (hasData || chunkSize == 0) return false;
            candidateOffset = position; candidateSize = chunkSize; hasData = true;
        }
        position += chunkSize;
        if (chunkSize & 1u) {
            if (position >= bytes.size()) return false;
            ++position; // RIFF chunks of odd length include a pad byte.
        }
    }
    if (!hasFormat || !hasData || candidateSize % candidate.nBlockAlign != 0) return false;
    format = candidate; dataOffset = candidateOffset; dataSize = candidateSize;
    return true;
}

void ReleaseAudioData(Audio::AudioData& data) {
    // XAudio2 retains pAudioData until consumption or voice destruction. Destroy
    // every source before freeing/replacing the backing sample vector.
    for (auto* voice : data.voicePool) if (voice) voice->DestroyVoice();
    data.voicePool.clear(); data.voiceGenerations.clear(); data.voiceStarted.clear();
    data.bufferData.clear(); data.bufferSize = 0; data.nextVoiceIndex = 0;
    if (data.waveFormat) CoTaskMemFree(data.waveFormat);
    data.waveFormat = nullptr; data.pSourceVoice = nullptr;
}
struct AudioDataGuard {
    Audio::AudioData* data;
    ~AudioDataGuard() { if (data) ReleaseAudioData(*data); }
};
}

Audio* Audio::GetInstance() {
    static Audio instance;
    return &instance;
}

Audio::~Audio() {
    for (auto& item : audioMap) ReleaseAudioData(item.second);
    if (masterVoice_) { masterVoice_->DestroyVoice(); masterVoice_ = nullptr; }
    xAudio2_.Reset();
    if (mediaFoundationStarted_) MFShutdown();
}

void Audio::Initialize() {
    if (IsReady()) return;
    if (!mediaFoundationStarted_)
        mediaFoundationStarted_ = SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET));
    Microsoft::WRL::ComPtr<IXAudio2> candidate;
    if (FAILED(XAudio2Create(&candidate, 0, XAUDIO2_DEFAULT_PROCESSOR))) return;
    IXAudio2MasteringVoice* mastering = nullptr;
    if (FAILED(candidate->CreateMasteringVoice(&mastering))) return;
    xAudio2_ = std::move(candidate); masterVoice_ = mastering;
}

bool Audio::InstallAudioData(const std::wstring& soundName, std::vector<BYTE> samples,
                            WAVEFORMATEX* ownedFormat, size_t maxConcurrency) {
    AudioData candidate;
    candidate.waveFormat = ownedFormat;
    candidate.bufferData = std::move(samples);
    AudioDataGuard cleanup{&candidate};
    if (!IsReady() || soundName.empty() || !ownedFormat || candidate.bufferData.empty() ||
        candidate.bufferData.size() > XAUDIO2_MAX_BUFFER_BYTES ||
        maxConcurrency == 0 || maxConcurrency > kMaxVoicesPerSound) return false;
    try {
        size_t otherVoices = 0;
        for (const auto& item : audioMap)
            if (item.first != soundName) otherVoices += item.second.voicePool.size();
        if (otherVoices > kMaxLoadedVoices - maxConcurrency) return false;
        candidate.bufferSize = static_cast<UINT32>(candidate.bufferData.size());
        candidate.voicePool.resize(maxConcurrency, nullptr);
        candidate.voiceGenerations.resize(maxConcurrency, 0);
        candidate.voiceStarted.resize(maxConcurrency, 0);
        for (auto& voice : candidate.voicePool)
            if (FAILED(xAudio2_->CreateSourceVoice(&voice, ownedFormat, 0, 2.0f)) || !voice) return false;
        // Map allocation and all new voices succeed before touching an old bank.
        // Moving these default-allocator vectors cannot throw.
        auto destination = audioMap.try_emplace(soundName).first;
        ReleaseAudioData(destination->second);
        destination->second = std::move(candidate);
        cleanup.data = nullptr;
        return true;
    } catch (...) {
        return false;
    }
}

bool Audio::TryLoadPcmWave(const std::wstring& soundName, const std::wstring& soundPath,
                          size_t maxConcurrency) {
    if (!IsReady() || soundName.empty() || maxConcurrency == 0 || maxConcurrency > kMaxVoicesPerSound)
        return false;
    try {
        std::ifstream file(std::filesystem::path(soundPath), std::ios::binary | std::ios::ate);
        if (!file) return false;
        const auto length = file.tellg();
        if (length < 44 || length > static_cast<std::streamoff>(kMaxPcmWaveBytes)) return false;
        std::vector<BYTE> bytes(static_cast<size_t>(length));
        file.seekg(0);
        if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) return false;
        WAVEFORMATEX format{};
        size_t offset = 0, size = 0;
        if (!ParsePcm16Wave(bytes, format, offset, size)) return false;
        std::vector<BYTE> samples(bytes.begin() + static_cast<ptrdiff_t>(offset),
                                  bytes.begin() + static_cast<ptrdiff_t>(offset + size));
        auto* ownedFormat = static_cast<WAVEFORMATEX*>(CoTaskMemAlloc(sizeof(WAVEFORMATEX)));
        if (!ownedFormat) return false;
        *ownedFormat = format;
        return InstallAudioData(soundName, std::move(samples), ownedFormat, maxConcurrency);
    } catch (...) {
        return false;
    }
}

// Existing MP3/other Media Foundation callers remain supported. PCM16 WAV
// sounds use TryLoadPcmWave instead, so missing codecs cannot affect that path.
void Audio::LoadAudio(const std::wstring soundName, const std::wstring filePath, size_t maxConcurrency) {
    if (!IsReady() || !mediaFoundationStarted_ || soundName.empty()) return;
    constexpr DWORD audioStream = static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM);
    Microsoft::WRL::ComPtr<IMFSourceReader> reader;
    if (FAILED(MFCreateSourceReaderFromURL(filePath.c_str(), nullptr, &reader))) return;
    Microsoft::WRL::ComPtr<IMFMediaType> requested;
    if (FAILED(MFCreateMediaType(&requested)) ||
        FAILED(requested->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio)) ||
        FAILED(requested->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM)) ||
        FAILED(reader->SetCurrentMediaType(audioStream, nullptr, requested.Get()))) return;
    Microsoft::WRL::ComPtr<IMFMediaType> actual;
    if (FAILED(reader->GetCurrentMediaType(audioStream, &actual))) return;
    WAVEFORMATEX* format = nullptr;
    if (FAILED(MFCreateWaveFormatExFromMFMediaType(actual.Get(), &format, nullptr))) return;
    AudioData decoded;
    decoded.waveFormat = format;
    AudioDataGuard cleanup{&decoded};
    try {
        while (true) {
            Microsoft::WRL::ComPtr<IMFSample> sample;
            DWORD flags = 0;
            if (FAILED(reader->ReadSample(audioStream, 0, nullptr, &flags, nullptr, &sample)) ||
                (flags & MF_SOURCE_READERF_ERROR)) return;
            if (sample) {
                Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
                if (FAILED(sample->ConvertToContiguousBuffer(&buffer))) return;
                BYTE* source = nullptr; DWORD length = 0;
                if (FAILED(buffer->Lock(&source, nullptr, &length))) return;
                const size_t previous = decoded.bufferData.size();
                if (length > XAUDIO2_MAX_BUFFER_BYTES - previous) { buffer->Unlock(); return; }
                // Ensure a failed allocation does not leave the MF buffer locked.
                try {
                    decoded.bufferData.resize(previous + length);
                    if (length) std::memcpy(decoded.bufferData.data() + previous, source, length);
                } catch (...) { buffer->Unlock(); return; }
                if (FAILED(buffer->Unlock())) return;
            }
            if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
        }
        decoded.waveFormat = nullptr; // InstallAudioData takes ownership, including failures.
        InstallAudioData(soundName, std::move(decoded.bufferData), format,
            std::clamp(maxConcurrency, size_t{1}, kMaxVoicesPerSound));
    } catch (...) {
        return;
    }
}

Audio::VoiceHandle Audio::StartVoice(const std::wstring& soundName, AudioData& data,
                                    size_t index, bool loop, float volume, float pitch) {
    if (!IsReady() || index >= data.voicePool.size() || index >= data.voiceGenerations.size() ||
        index >= data.voiceStarted.size() || data.bufferData.empty()) return {};
    auto* voice = data.voicePool[index];
    if (!voice) return {};
    VoiceHandle result;
    try { result.soundName = soundName; } catch (...) { return {}; }
    data.voiceGenerations[index] = 0; data.voiceStarted[index] = 0;
    if (FAILED(voice->Stop()) || FAILED(voice->FlushSourceBuffers()) ||
        FAILED(voice->SetVolume(SafeGain(volume))) || FAILED(voice->SetFrequencyRatio(SafePitch(pitch)))) return {};
    XAUDIO2_BUFFER buffer{};
    buffer.AudioBytes = data.bufferSize; buffer.pAudioData = data.bufferData.data();
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    if (loop) {
        if (!data.waveFormat || !data.waveFormat->nBlockAlign) return {};
        buffer.LoopLength = data.bufferSize / data.waveFormat->nBlockAlign;
        buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
    }
    if (FAILED(voice->SubmitSourceBuffer(&buffer))) return {};
    if (FAILED(voice->Start())) { voice->Stop(); voice->FlushSourceBuffers(); return {}; }
    if (++nextGeneration_ == 0) ++nextGeneration_;
    result.voiceIndex = index; result.generation = nextGeneration_;
    data.voiceGenerations[index] = result.generation; data.voiceStarted[index] = 1;
    return result;
}

Audio::VoiceHandle Audio::TryPlayAudio(const std::wstring& soundName, bool loop, float volume, float pitch) {
    auto found = audioMap.find(soundName);
    if (!IsReady() || found == audioMap.end() || found->second.voicePool.empty()) return {};
    auto& data = found->second;
    const size_t index = data.nextVoiceIndex % data.voicePool.size();
    auto result = StartVoice(soundName, data, index, loop, volume, pitch);
    if (result.IsValid()) data.nextVoiceIndex = (index + 1) % data.voicePool.size();
    return result;
}

Audio::VoiceHandle Audio::PlayAudioSE(const std::wstring soundName, float volume) {
    // The legacy default -1 was passed straight to XAudio2 (phase inversion).
    // Treat that sentinel as unity gain; the new API uses an explicit 0..1 gain.
    return TryPlayAudio(soundName, false, volume < 0 ? 1.0f : volume, 1.0f);
}

void Audio::PlayAudio(const std::wstring soundName, bool loop, float volume) {
    auto found = audioMap.find(soundName);
    if (found == audioMap.end()) return;
    // Preserve the old BGM API's restart-on-voice-zero behavior.
    StartVoice(soundName, found->second, 0, loop, volume < 0 ? 1.0f : volume, 1.0f);
}

IXAudio2SourceVoice* Audio::ResolveVoice(const VoiceHandle& handle) const {
    const auto found = audioMap.find(handle.soundName);
    if (!handle.IsValid() || found == audioMap.end()) return nullptr;
    const auto& data = found->second;
    if (handle.voiceIndex >= data.voicePool.size() || handle.voiceIndex >= data.voiceGenerations.size() ||
        data.voiceGenerations[handle.voiceIndex] != handle.generation) return nullptr;
    return data.voicePool[handle.voiceIndex];
}

bool Audio::IsVoicePlaying(const VoiceHandle& handle) const {
    auto* voice = ResolveVoice(handle);
    if (!voice) return false;
    const auto& data = audioMap.find(handle.soundName)->second;
    if (handle.voiceIndex >= data.voiceStarted.size() || !data.voiceStarted[handle.voiceIndex]) return false;
    XAUDIO2_VOICE_STATE state{};
    voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
    return state.BuffersQueued != 0;
}

void Audio::StopAudio(const std::wstring& soundName) {
    const auto found = audioMap.find(soundName);
    if (found == audioMap.end()) return;
    auto& data = found->second;
    for (auto* voice : data.voicePool) if (voice) { voice->Stop(); voice->FlushSourceBuffers(); }
    std::fill(data.voiceGenerations.begin(), data.voiceGenerations.end(), 0);
    std::fill(data.voiceStarted.begin(), data.voiceStarted.end(), uint8_t{0});
}

void Audio::UnloadAudio(const std::wstring& soundName) {
    const auto found = audioMap.find(soundName);
    if (found == audioMap.end()) return;
    ReleaseAudioData(found->second);
    audioMap.erase(found);
}

void Audio::StopAudio(const VoiceHandle& handle) {
    if (auto* voice = ResolveVoice(handle)) {
        voice->Stop(); voice->FlushSourceBuffers();
        auto& data = audioMap.find(handle.soundName)->second;
        data.voiceGenerations[handle.voiceIndex] = 0; data.voiceStarted[handle.voiceIndex] = 0;
    }
}

void Audio::PauseAudio(const VoiceHandle& handle) {
    if (auto* voice = ResolveVoice(handle)) {
        if (SUCCEEDED(voice->Stop())) audioMap.find(handle.soundName)->second.voiceStarted[handle.voiceIndex] = 0;
    }
}

void Audio::ResumeAudio(const VoiceHandle& handle) {
    if (auto* voice = ResolveVoice(handle)) {
        if (SUCCEEDED(voice->Start())) audioMap.find(handle.soundName)->second.voiceStarted[handle.voiceIndex] = 1;
    }
}

void Audio::StopAll() {
    for (auto& item : audioMap) StopAudio(item.first);
}

void Audio::SetVolume(const VoiceHandle& handle, float volume) {
    if (auto* voice = ResolveVoice(handle)) voice->SetVolume(SafeGain(volume));
}

bool Audio::SetPitch(const VoiceHandle& handle, float pitch) {
    auto* voice = ResolveVoice(handle);
    return voice && SUCCEEDED(voice->SetFrequencyRatio(SafePitch(pitch)));
}

void Audio::SetMasterVolume(float volume) {
    if (masterVoice_) masterVoice_->SetVolume(SafeGain(volume));
}
