#pragma once
#include <Windows.h>
#include <cstdint>
#include <format>
#include <cassert>
#include <unordered_map>
#include <string>
#include <vector>

// 必須h
#include <filesystem>
#include <wrl.h>
#include <xaudio2.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

namespace cg2 {

/// @brief 音声データと再生ボイスのプールを管理する。制御には世代付きVoiceHandleを使う。
class Audio {
public:
    // シングルトン
    static Audio* GetInstance();

    /// @brief 1音源のPCMデータ・フォーマット・再生ボイスのプールを所有する。
    struct AudioData {
        std::vector<BYTE> bufferData;
        WAVEFORMATEX* waveFormat = nullptr;
        IXAudio2SourceVoice* pSourceVoice = nullptr; // retained legacy member; pool owns voices
        UINT32 bufferSize = 0;

        // この音専用の再生用ボイスリスト
        std::vector<IXAudio2SourceVoice*> voicePool;
        std::vector<uint64_t> voiceGenerations;
        std::vector<uint8_t> voiceStarted;
        size_t nextVoiceIndex = 0; // 次に使うボイスの番号
    };

    /// @brief 音源・ボイス番号・世代で1回の再生を識別する。再利用されたボイスを誤って制御しないために使う。
    struct VoiceHandle {
        std::wstring soundName;
        size_t voiceIndex = 0;
        uint64_t generation = 0;
        /// @brief 有効であるか判定する。
        bool IsValid() const
        {
            return !soundName.empty() && generation != 0;
        }
    };

    // デストラクタ
    ~Audio();

    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize();
    /// @brief 準備完了であるか判定する。
    bool IsReady() const
    {
        return xAudio2_ != nullptr && masterVoice_ != nullptr;
    }

    static constexpr size_t kMaxPcmWaveBytes = 16 * 1024 * 1024;
    static constexpr size_t kMaxVoicesPerSound = 16;
    static constexpr size_t kMaxLoadedVoices = 128;
    // Generated RIFF/WAVE PCM16 only (mono/stereo, 8-192 kHz), no Media Foundation
    // decoder. A failed load preserves the previous bank and its active voices.
    /// @brief 生成したRIFF/WAVEのPCM16音源を読む。失敗時は既存の音源と再生を保つ。
    /// @param maxConcurrency 同時に使うボイス数。
    /// @return 対応する音源を読み込んで登録できた場合true。
    /// @note モノラル／ステレオ、8〜192kHzが対象。Media Foundationのデコーダーは使わない。
    bool TryLoadPcmWave(const std::wstring& soundName, const std::wstring& soundPath, size_t maxConcurrency = 1);
    // Gain is clamped to 0..1, pitch ratio to 0.25..2 (changes playback speed too).
    // Pool saturation steals its next voice. Old handles cannot control the reuse.
    /// @brief 音源を再生し、この再生の世代付きハンドルを返す。
    /// @param volume 音量。0〜1へ制限する。
    /// @param pitch 再生速度も変わる周波数比。0.25〜2へ制限する。
    /// @note プールが満杯なら次のボイスを再利用する。旧ハンドルからは制御できない。
    VoiceHandle TryPlayAudio(const std::wstring& soundName, bool loop = false, float volume = 1.0f, float pitch = 1.0f);
    /// @brief 再生ボイス再生中であるか判定する。
    bool IsVoicePlaying(const VoiceHandle& handle) const;
    /// @brief ピッチを設定する。
    bool SetPitch(const VoiceHandle& handle, float pitch);

    /// @brief 音声の読み込み
    void LoadAudio(const std::wstring& soundName, const std::wstring& soundPath, size_t maxConcurrency = 1);

    /// @brief 音声再生
    void PlayAudio(const std::wstring& soundName, bool loop, float volume = -1.0f);

    /// @brief SE再生
    Audio::VoiceHandle PlayAudioSE(const std::wstring& soundName, float volume = -1.0f);

    /// @brief 特定の音をすべて停止（BGMの切り替えやエディターでの停止用）
    void StopAudio(const std::wstring& soundName);

    /// @brief 音声データの解放（エディターで項目を削除した時や、メモリ節約用）
    void UnloadAudio(const std::wstring& soundName);

    /// @brief 音声を停止する。
    void StopAudio(const VoiceHandle& handle);
    /// @brief 音声を一時停止する。
    void PauseAudio(const VoiceHandle& handle);
    /// @brief 音声を再開する。
    void ResumeAudio(const VoiceHandle& handle);
    /// @brief 音量を設定する。
    void SetVolume(const VoiceHandle& handle, float volume);
    /// @brief 全体を停止する。
    void StopAll();

    /// @brief Master音量を設定する。
    void SetMasterVolume(float volume);

private:
    /// @brief 読み込み済みのPCMデータとフォーマットの所有権を受け取り、音源へ登録する。
    bool InstallAudioData(const std::wstring& soundName, std::vector<BYTE> samples, WAVEFORMATEX* ownedFormat, size_t maxConcurrency);
    /// @brief ハンドルの音源・番号・世代を検証し、有効な再生ボイスを返す。
    /// @return 無効または再利用済みならnullptr。
    IXAudio2SourceVoice* ResolveVoice(const VoiceHandle& handle) const;
    /// @brief 再生ボイスを開始する。
    VoiceHandle StartVoice(const std::wstring& soundName, AudioData& data, size_t index, bool loop, float volume, float pitch);

    Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
    IXAudio2MasteringVoice* masterVoice_ = nullptr;
    std::unordered_map<std::wstring, AudioData> audioMap;
    uint64_t nextGeneration_ = 0;
    bool mediaFoundationStarted_ = false;
};

} // namespace cg2
