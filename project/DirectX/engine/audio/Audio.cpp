#include "Audio.h"

#pragma comment(lib,"xaudio2.lib")
#pragma comment(lib, "Mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "Mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

Audio* Audio::GetInstance()
{
    static Audio instance;
    return &instance;
}

Audio::~Audio()
{
    for (auto& [name, data] : audioMap) {
        if (data.waveFormat) {
            CoTaskMemFree(data.waveFormat);
        }
        // プール内の全てのボイスを破棄
        for (auto* pVoice : data.voicePool) {
            if (pVoice) pVoice->DestroyVoice();
        }
    }
}

void Audio::Initialize()
{
    MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);

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
void Audio::LoadAudio(const std::wstring soundName, const std::wstring filePath, size_t maxConcurrency)
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
    // 1. 先にマップに登録して、その参照を取得する
    AudioData& audioData = audioMap[soundName];

    // 2. その参照に対してデータを格納していく
    audioData.bufferData = std::move(mediaData);
    audioData.waveFormat = waveFormat;
    audioData.bufferSize = static_cast<UINT32>(audioData.bufferData.size());

    // 指定された数だけ SourceVoice をあらかじめ作っておく
    audioData.voicePool.resize(maxConcurrency);
    for (size_t i = 0; i < maxConcurrency; ++i) {
        xAudio2_->CreateSourceVoice(&audioData.voicePool[i], waveFormat);
    }
}

void Audio::PlayAudio(const std::wstring soundName, bool loop, float volume)
{
    auto it = audioMap.find(soundName);
    if (it == audioMap.end()) return;

    auto& data = it->second;

    // voicePoolが空でないかチェック
    if (data.voicePool.empty()) return;

    // pSourceVoice の代わりに voicePool[0] を使う
    IXAudio2SourceVoice* pVoice = data.voicePool[0];

    // 1. 再生中なら一度止めてバッファをクリア
    pVoice->Stop();
    pVoice->FlushSourceBuffers();

    // 2. 音量の設定
    pVoice->SetVolume(volume);

    // 3. バッファの設定
    XAUDIO2_BUFFER buffer{ 0 };
    buffer.AudioBytes = data.bufferSize;
    buffer.pAudioData = data.bufferData.data();
    buffer.Flags = XAUDIO2_END_OF_STREAM;

    if (loop) {
        buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
    }

    // 4. バッファの送信と再生開始
    pVoice->SubmitSourceBuffer(&buffer);
    pVoice->Start();
}

void Audio::PlayAudioSE(const std::wstring soundName, float volume)
{
    auto it = audioMap.find(soundName);
    if (it == audioMap.end()) return;

    auto& data = it->second;
    if (data.voicePool.empty()) return;

    // 次に使うボイスを選択
    IXAudio2SourceVoice* pVoice = data.voicePool[data.nextVoiceIndex];

    // インデックスを更新 (最後までいったら0に戻る)
    data.nextVoiceIndex = (data.nextVoiceIndex + 1) % data.voicePool.size();

    // 再生設定
    pVoice->Stop();            // 念のため停止
    pVoice->FlushSourceBuffers(); // キューを空にする
    pVoice->SetVolume(volume);

    XAUDIO2_BUFFER buffer{ 0 };
    buffer.AudioBytes = data.bufferSize;
    buffer.pAudioData = data.bufferData.data();
    buffer.Flags = XAUDIO2_END_OF_STREAM;

    pVoice->SubmitSourceBuffer(&buffer);
    pVoice->Start();
}
