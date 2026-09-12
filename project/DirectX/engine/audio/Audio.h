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

class Audio
{
public:

	// シングルトン
	static Audio* GetInstance();

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

	struct VoiceHandle {
		std::wstring soundName;
		size_t voiceIndex = 0;
		uint64_t generation = 0;
		bool IsValid() const { return !soundName.empty() && generation != 0; }
	};

	// デストラクタ
	~Audio();

	void Initialize();
	bool IsReady() const { return xAudio2_ != nullptr && masterVoice_ != nullptr; }

	static constexpr size_t kMaxPcmWaveBytes = 16 * 1024 * 1024;
	static constexpr size_t kMaxVoicesPerSound = 16;
	static constexpr size_t kMaxLoadedVoices = 128;
	// Generated RIFF/WAVE PCM16 only (mono/stereo, 8-192 kHz), no Media Foundation
	// decoder. A failed load preserves the previous bank and its active voices.
	bool TryLoadPcmWave(const std::wstring& soundName, const std::wstring& soundPath,
		size_t maxConcurrency = 1);
	// Gain is clamped to 0..1, pitch ratio to 0.25..2 (changes playback speed too).
	// Pool saturation steals its next voice. Old handles cannot control the reuse.
	VoiceHandle TryPlayAudio(const std::wstring& soundName, bool loop = false,
		float volume = 1.0f, float pitch = 1.0f);
	bool IsVoicePlaying(const VoiceHandle& handle) const;
	bool SetPitch(const VoiceHandle& handle, float pitch);

	/// <summary>
	/// 音声の読み込み
	/// </summary>
	void LoadAudio(const std::wstring soundName, const std::wstring soundPath, size_t maxConcurrency = 1);

	/// <summary>
	/// 音声再生
	/// </summary>
	void PlayAudio(const std::wstring soundName, bool loop, float volume = -1.0f);

	/// <summary>
	/// SE再生
	/// </summary>
	/// <param name="soundName"></param>
	/// <param name="volume"></param>
	Audio::VoiceHandle PlayAudioSE(const std::wstring soundName, float volume = -1.0f);

	/// <summary>
	/// 特定の音をすべて停止（BGMの切り替えやエディターでの停止用）
	/// </summary>
	void StopAudio(const std::wstring& soundName);

	/// <summary>
	/// 音声データの解放（エディターで項目を削除した時や、メモリ節約用）
	/// </summary>
	void UnloadAudio(const std::wstring& soundName);

	void StopAudio(const VoiceHandle& handle);
	void PauseAudio(const VoiceHandle& handle);
	void ResumeAudio(const VoiceHandle& handle);
	void SetVolume(const VoiceHandle& handle, float volume);
	void StopAll();

	void SetMasterVolume(float volume);
private:
	bool InstallAudioData(const std::wstring& soundName, std::vector<BYTE> samples,
		WAVEFORMATEX* ownedFormat, size_t maxConcurrency);
	IXAudio2SourceVoice* ResolveVoice(const VoiceHandle& handle) const;
	VoiceHandle StartVoice(const std::wstring& soundName, AudioData& data,
		size_t index, bool loop, float volume, float pitch);

	Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
	IXAudio2MasteringVoice* masterVoice_ = nullptr;
	std::unordered_map<std::wstring, AudioData> audioMap;
	uint64_t nextGeneration_ = 0;
	bool mediaFoundationStarted_ = false;
};
