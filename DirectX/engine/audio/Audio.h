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

	struct AudioData {
		std::vector<BYTE> bufferData;
		WAVEFORMATEX* waveFormat;
		IXAudio2SourceVoice* pSourceVoice;
	};


	// デストラクタ
	~Audio();

	void Initialize();

	/// <summary>
	/// 音声の読み込み
	/// </summary>
	void LoadAudio(const std::wstring soundName, const std::wstring soundPath);

	/// <summary>
	/// 音声再生
	/// </summary>
	void playAudio(const std::wstring soundName);

private:

	Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
	IXAudio2MasteringVoice* masterVoice_ = nullptr;
	std::unordered_map<std::wstring, AudioData> audioMap;
};