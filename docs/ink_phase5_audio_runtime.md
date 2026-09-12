# 第5段階：音声ランタイムの接続と検証

2026-09-12。[Audio.h](../project/DirectX/engine/audio/Audio.h) / [Audio.cpp](../project/DirectX/engine/audio/Audio.cpp) に、生成したPCM16 WAVを既存XAudio2へ登録する経路と、成功を確認できる再生APIを追加した。既存の `LoadAudio` によるMedia FoundationのMP3デコード、`PlayAudio` / `PlayAudioSE` は維持している。音素材の生成・試射場のイベント割り当て・最終的な聞こえ方は、このランタイム検証とは別に扱う。

## APIと初期化順序

```cpp
bool Audio::IsReady() const;
bool Audio::TryLoadPcmWave(const std::wstring& soundName,
    const std::wstring& soundPath, size_t maxConcurrency = 1);
Audio::VoiceHandle Audio::TryPlayAudio(const std::wstring& soundName,
    bool loop = false, float volume = 1.0f, float pitch = 1.0f);
bool Audio::IsVoicePlaying(const VoiceHandle& handle) const;
bool Audio::SetPitch(const VoiceHandle& handle, float pitch);
```

`Game::Initialize` は起動シーンの初期化後に `Audio::Initialize` を呼ぶ。この順番を変えず、Inkの音バンクは最初のシーンUpdateまで読み込みを遅延する。シーンからXAudio2を再初期化しない。デバイス初期化が失敗した場合は `IsReady()==false` となり、WAV読込・再生APIも失敗を返してアプリを継続する。

`TryLoadPcmWave` は成功時だけtrue。`TryPlayAudio` の戻り値の `IsValid()` は、返された時点で開始に成功したハンドルかを確認する。ハンドルが自然終了・停止・再利用された後まで、`IsValid()` だけで発音中と判定しない。現在の世代とキュー、Pause状態を確認するには `IsVoicePlaying()` を使う。

```cpp
auto* audio = Audio::GetInstance();
const bool loaded = audio->TryLoadPcmWave(
    L"ink_charge", L"resources/audio/ink/charge.wav", 1);
Audio::VoiceHandle charge;
if (loaded) charge = audio->TryPlayAudio(L"ink_charge", true, 0.12f, 1.0f);
if (charge.IsValid()) {
    audio->SetVolume(charge, 0.10f);
    audio->SetPitch(charge, 1.15f);
}
audio->StopAudio(charge);
charge = {};
// シーン終了時。Ink専用キーだけを解放する。
audio->UnloadAudio(L"ink_charge");
```

これはAPIの使用例で、パスと音量は最終音素材の指定ではない。チャージ等のループは専用キー・同時数1とし、開始の状態変化で1回だけ再生する。毎フレーム `TryPlayAudio` を呼ぶと先頭へ戻る。中断、武器変更、リセット、設定への移行、非アクティブ化、シーン終了では所有するループを止める。`StopAll` やマスター音量を使って他シーンの音まで変更しない。

## 入力、再生、寿命

| 対象 | 完成した保護と挙動 |
|---|---|
| WAV形式 | RIFF/WAVE、PCM16、1または2ch、8～192kHz。fmtは16バイトまたは拡張サイズ0の18バイトを受け付ける |
| ファイルサイズ | 1音16MiB以内。ヘッダ・RIFF長・各チャンク長・偶数境界のパディング・blockAlign・byteRateを検証し、範囲外を読む前に失敗する |
| 不正WAV | RIFF以外、float音声、圧縮、重複fmt/data、空data、欠損、切り詰め、不整合な形式を拒否。JUNK等の未知チャンクは境界とパディングを確認して読み飛ばす |
| 登録 | PCMの読込と全ボイスの作成が成功してから旧バンクを置換する。失敗時は同名の旧バッファと再生を保持する |
| 音量 | 新APIとSetVolumeは0～1にclamp、NaN・無限は0。旧再生APIの既定値−1は1として扱う |
| ピッチ | 0.25～2.0にclamp、NaN・無限は1。ピッチ比は再生速度・音の長さも変える。声質だけを変えて長さを維持する処理ではない |
| 同時発音 | 1音1～16ボイス、全登録合計128。指定した数をロード時に作成。飽和時は同じ音のプールを順番に再利用して前の発音を止める |
| ハンドル | 音名・ボイス番号・再生世代で照合。再利用、停止、Unload、同名置換後の古いハンドルは新しい音を操作できない |
| Pause/Resume | Pauseは位置を保って停止し、IsVoicePlayingはfalse。Resumeで再開できる。Stopはキューを消して世代を無効にするため再開しない |
| 解放 | ボイスをDestroyVoiceしてからPCMバッファを解放する。Unloadは音名単位で、存在しないキーも安全に無視する |

データはXAudio2へ渡した後もエンジン側で保持する必要がある。XAudio2はバッファ記述の参照を受け取り、PCM実体を利用し続けるため、バンク置換で先にベクターを解放してはいけない。[Microsoft SubmitSourceBuffer](https://learn.microsoft.com/en-us/windows/win32/api/xaudio2/nf-xaudio2-ixaudio2sourcevoice-submitsourcebuffer)

XAudio2の音量は振幅倍率で、負数は既定値ではなく位相反転になる。今回の音量制限はCG2側で採用した範囲である。ピッチ上限2.0は作成するSourceVoiceにも設定し、APIへ渡す比率をその範囲に収める。[Microsoft SetVolume](https://learn.microsoft.com/en-us/windows/win32/api/xaudio2/nf-xaudio2-ixaudio2voice-setvolume)、[SetFrequencyRatio](https://learn.microsoft.com/en-us/windows/win32/api/xaudio2/nf-xaudio2-ixaudio2sourcevoice-setfrequencyratio)

`IsVoicePlaying` は `XAUDIO2_VOICE_NOSAMPLESPLAYED` を付けてバッファ状態だけを照会し、サンプル数は取得しない。音声のコールバックからゲーム状態を変更する仕組みは追加していない。ロード・再生・停止は既存と同じゲーム側のスレッドから呼ぶ。[Microsoft GetState](https://learn.microsoft.com/en-us/windows/win32/api/xaudio2/nf-xaudio2-ixaudio2sourcevoice-getstate)

## 負荷の目安とイベントの制限

PCM16・48kHz・monoは1秒96,000バイト。0.18秒なら17,280バイトで、複数ボイスは同じPCMベクターを参照する。ボイス数を4にしたとき音データ自体が4倍になるわけではない。ステレオはその2倍。16MiBは異常入力を防ぐ上限で、実際の短い効果音はそれより小さい。

発射のたびのボイス作成、ファイルI/O、WAV合成・デコードは行わない。再生時は既存プールへバッファを送り、音量と比率を設定する。ハンドルの音名コピー等はあるため、再生呼び出し全体が完全に無確保だという主張はしない。

必要同時数の目安は「音の長さ÷最低ピッチ比×発生頻度」。10発/秒、0.18秒、最低比0.9なら約2音が重なり、4ボイス程度で余裕がある。これは選定の目安で、実測CPU時間ではない。チャージと遊泳のループは専用1ボイス、着弾・爆発は描画粒子1個ごとに鳴らさず意味のあるイベント単位にまとめる。120Hzの追いつき更新で大量イベントが出る場合も、シーン側で1フレームの再生回数を制限する。

ロード時の合計128は待機中のボイスも含む。安全な置換では新プールが成功するまで旧プールを残すため、置換中だけ最大16のボイスが追加で存在し得る。実際の音源同時負荷、クリップの音量バランス、合成音の繰り返し感はネイティブ再生で別途確認する。この文書では未測定のCPU使用率や音質を保証しない。

## 再実行

[test_ink_audio_runtime.ps1](../project/tools/test_ink_audio_runtime.ps1) は既存のSimulationテスト群から独立している。既存のVisual Studio C++環境を検出し、C++20、`/utf-8 /W4 /WX /O2` で [ink_audio_runtime_tests.cpp](../project/tools/ink_audio_runtime_tests.cpp) をビルドして実行する。

リポジトリのルートで実行する。

```powershell
& .\project\tools\test_ink_audio_runtime.ps1
```

Visual Studioの自動検出ができない場合だけ、既にインストールした場所を渡す。

```powershell
& .\project\tools\test_ink_audio_runtime.ps1 -VisualStudioPath 'C:\Program Files\Microsoft Visual Studio\18\Community'
```

生成物は `generated/ink_audio_tests/` のexe・obj・実行用cmd・小さな無音WAVと破損WAV。音素材の配布フォルダーへテストWAVを置かない。テストはCPU用の内部パーサーも確認するため実装.cppを直接includeしており、同じ実行ファイルへAudio.cppを別途リンクしない。

| 検証 | 2026-09-12の結果 |
|---|---|
| CPU | PCM16の形式と境界、RIFF長、巨大長・サイズ、重複チャンク、奇数JUNK、拡張fmt、blockAlign、失敗時の出力保持、音量・ピッチの非有限値、未初期化APIを確認してPASS |
| MP3互換 | 既存 `resources/bulletShoot.mp3` をMedia Foundationでデコードし、PlayAudioSEが有効ハンドルを返すことを音量0で確認してPASS |
| XAudio2 | 実デバイスを利用し、無音PCMでループ、ピッチ、Pause/Resume/Stop、古いハンドル、プール切替、失敗／成功置換、Unloadを確認してPASS |
| 上限 | 16×8の128ボイスを登録。追加登録は失敗して既存ループを保持し、同名置換は成功することを確認してPASS |

音声出力デバイスを初期化できない環境ではCPU検証を実行した後、ネイティブ部をSKIPと明示して終了する。これは実際に聞こえたことの検証ではない。今回もWAVは無音、MP3はgain=0であり、シーンへ組み込んだSEの聞こえ方や録音との比較は別の確認になる。
