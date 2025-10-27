#include "Audio.h"

#pragma comment(lib,"xaudio2.lib")
#pragma comment(lib, "Mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "Mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

Audio::~Audio()
{
    for (auto& [name, data] : audioMap) {
        if (data.waveFormat) {
            CoTaskMemFree(data.waveFormat);
        }
        if (data.pSourceVoice) {
            data.pSourceVoice->DestroyVoice();
        }
    }
}

void Audio::Initialize()
{
	HRESULT hr = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
	if (FAILED(hr)) {
		// エラー処理
		return;
	}

	hr = xAudio2_->CreateMasteringVoice(&masterVoice_);
	if (FAILED(hr)) {
		// エラー処理
		return;
	}
}
void Audio::LoadAudio(const std::wstring soundName, const std::wstring filePath)
{
    // ソースリーダーの作成
    IMFSourceReader* pMFSourceReader{ nullptr };
    LPCWSTR soundFilePath = filePath.c_str();
    HRESULT hr = MFCreateSourceReaderFromURL(soundFilePath, NULL, &pMFSourceReader);
    if (FAILED(hr)) return;

    // メディアタイプの取得
    IMFMediaType* pMFMediaType{ nullptr };
    MFCreateMediaType(&pMFMediaType);
    pMFMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    pMFMediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    pMFSourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pMFMediaType);

    pMFMediaType->Release();
    pMFMediaType = nullptr;
    pMFSourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pMFMediaType);

    // オーディオデータ形式の作成
    WAVEFORMATEX* waveFormat{ nullptr };
    MFCreateWaveFormatExFromMFMediaType(pMFMediaType, &waveFormat, nullptr);

    // データの読み込み
    std::vector<BYTE> mediaData;
    while (true)
    {
        IMFSample* pMFSample{ nullptr };
        DWORD dwStreamFlags{ 0 };
        pMFSourceReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &dwStreamFlags, nullptr, &pMFSample);

        if (dwStreamFlags & MF_SOURCE_READERF_ENDOFSTREAM) {
            break;
        }

        IMFMediaBuffer* pMFMediaBuffer{ nullptr };
        pMFSample->ConvertToContiguousBuffer(&pMFMediaBuffer);

        BYTE* pBuffer{ nullptr };
        DWORD cbCurrentLength{ 0 };
        pMFMediaBuffer->Lock(&pBuffer, nullptr, &cbCurrentLength);

        size_t currentSize = mediaData.size();
        mediaData.resize(currentSize + cbCurrentLength);
        memcpy(mediaData.data() + currentSize, pBuffer, cbCurrentLength);

        pMFMediaBuffer->Unlock();
        pMFMediaBuffer->Release();
        pMFSample->Release();
    }

    // 後始末
    pMFMediaType->Release();
    pMFSourceReader->Release();

    // XAudio2 初期化確認
    if (!xAudio2_) {
        OutputDebugStringA("XAudio2 has not been initialized!\n");
        return;
    }

    // audioMapに登録用のデータ準備
    AudioData audioData;
    audioData.bufferData = std::move(mediaData); // mediaData の寿命を確保
    audioData.waveFormat = waveFormat;

    // SourceVoice の作成と登録
    xAudio2_->CreateSourceVoice(&audioData.pSourceVoice, waveFormat);

    // バッファ設定（audioData.bufferData を参照！）
    XAUDIO2_BUFFER buffer{ 0 };
    buffer.AudioBytes = static_cast<UINT32>(audioData.bufferData.size());
    buffer.pAudioData = audioData.bufferData.data();
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    audioData.pSourceVoice->SubmitSourceBuffer(&buffer);

    // 最後にマップに登録
    audioMap[soundName] = std::move(audioData);
}

void Audio::playAudio(const std::wstring soundName)
{
	auto it = audioMap.find(soundName);
	if (it != audioMap.end()) {
		it->second.pSourceVoice->Start();
        // 再生出来ていたらログを出す
        OutputDebugStringA("Succees LoadAudio");
	}
}