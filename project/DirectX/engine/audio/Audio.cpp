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
    MFShutdown();
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

Audio::VoiceHandle Audio::PlayAudioSE(const std::wstring soundName, float volume)
{
    auto it = audioMap.find(soundName);
    if (it == audioMap.end() || it->second.voicePool.empty()) return { L"", 0 };

    auto& data = it->second;

    // 現在のインデックスを保存
    size_t currentIndex = data.nextVoiceIndex;
    IXAudio2SourceVoice* pVoice = data.voicePool[currentIndex];

    // 再生設定
    pVoice->Stop();
    pVoice->FlushSourceBuffers();
    pVoice->SetVolume(volume);

    XAUDIO2_BUFFER buffer{ 0 };
    buffer.AudioBytes = data.bufferSize;
    buffer.pAudioData = data.bufferData.data();
    buffer.Flags = XAUDIO2_END_OF_STREAM;

    pVoice->SubmitSourceBuffer(&buffer);
    pVoice->Start();

    // 次に使うボイスのインデックスを更新 (ここだけにする)
    data.nextVoiceIndex = (data.nextVoiceIndex + 1) % data.voicePool.size();

    return { soundName, currentIndex };
}

// --- 追加機能 1: 名前指定での全停止 ---
void Audio::StopAudio(const std::wstring& soundName) {
    auto it = audioMap.find(soundName);
    if (it != audioMap.end()) {
        // その音のボイスプールにあるすべてのボイスを止める
        for (auto* pVoice : it->second.voicePool) {
            pVoice->Stop();
            pVoice->FlushSourceBuffers();
        }
    }
}

// --- 追加機能 2: 音声データの完全解放 ---
void Audio::UnloadAudio(const std::wstring& soundName) {
    auto it = audioMap.find(soundName);
    if (it != audioMap.end()) {
        // 1. 再生中のボイスを止めて破棄
        for (auto* pVoice : it->second.voicePool) {
            if (pVoice) {
                pVoice->Stop();
                pVoice->FlushSourceBuffers();
                pVoice->DestroyVoice();
            }
        }
        // 2. メモリの解放
        if (it->second.waveFormat) {
            CoTaskMemFree(it->second.waveFormat);
        }

        // 3. マップから削除 (bufferData は vector なので自動で解放される)
        audioMap.erase(it);
    }
}


void Audio::PauseAudio(const VoiceHandle& handle) {
    auto it = audioMap.find(handle.soundName);
    if (it != audioMap.end() && handle.voiceIndex < it->second.voicePool.size()) {
        it->second.voicePool[handle.voiceIndex]->Stop(0); // 0を指定すると一時停止
    }
}

void Audio::ResumeAudio(const VoiceHandle& handle) {
    auto it = audioMap.find(handle.soundName);
    if (it != audioMap.end() && handle.voiceIndex < it->second.voicePool.size()) {
        it->second.voicePool[handle.voiceIndex]->Start(0);
    }
}

void Audio::StopAll() {
    for (auto& [name, data] : audioMap) {
        for (auto* pVoice : data.voicePool) {
            pVoice->Stop();
            pVoice->FlushSourceBuffers();
        }
    }
}

void Audio::StopAudio(const VoiceHandle& handle) {
    auto it = audioMap.find(handle.soundName);
    if (it != audioMap.end() && handle.voiceIndex < it->second.voicePool.size()) {
        // 即時停止し、たまっているバッファ（キュー）をクリアする
        it->second.voicePool[handle.voiceIndex]->Stop();
        it->second.voicePool[handle.voiceIndex]->FlushSourceBuffers();
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

void Audio::SetVolume(const VoiceHandle& handle, float volume) {
    auto it = audioMap.find(handle.soundName);
    if (it != audioMap.end() && handle.voiceIndex < it->second.voicePool.size()) {
        // 指定したボイスの音量のみを変更
        it->second.voicePool[handle.voiceIndex]->SetVolume(volume);
    }
}

void Audio::SetMasterVolume(float volume) {
    if (masterVoice_) {
        masterVoice_->SetVolume(volume);
    }
}