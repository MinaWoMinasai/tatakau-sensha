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
		WAVEFORMATEX* waveFormat;
		IXAudio2SourceVoice* pSourceVoice;
		UINT32 bufferSize;

		// この音専用の再生用ボイスリスト
		std::vector<IXAudio2SourceVoice*> voicePool;
		size_t nextVoiceIndex = 0; // 次に使うボイスの番号
	};


	// デストラクタ
	~Audio();

	void Initialize();

	/// <summary>
	/// 音声の読み込み
	/// </summary>
	void LoadAudio(const std::wstring soundName, const std::wstring soundPath, size_t maxConcurrency = 1);

	/// <summary>
	/// 音声再生
	/// </summary>
	void PlayAudio(const std::wstring soundName, bool loop, float volume);

	/// <summary>
	/// SE再生
	/// </summary>
	/// <param name="soundName"></param>
	/// <param name="volume"></param>
	void PlayAudioSE(const std::wstring soundName, float volume);

private:

	Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
	IXAudio2MasteringVoice* masterVoice_ = nullptr;
	std::unordered_map<std::wstring, AudioData> audioMap;
};