# 共通音声ランタイムの検証

`Audio.h` / `Audio.cpp` は、戦車の遠征音声と共通起動音が使うエンジン機能です。WAV向けの `TryLoadPcmWave` / `TryPlayAudio`、音声ハンドル、停止・再開・音量・ピッチ・解放、従来のMedia Foundation経由のMP3再生を維持しています。

リポジトリのルートから、既存のVisual Studio C++環境で実行します。

```powershell
.\project\tools\test_audio_runtime.ps1
```

[実行スクリプト](../project/tools/test_audio_runtime.ps1)はVisual Studioを検出します。必要なら `-VisualStudioPath <Visual Studioのインストール先>` を指定できます。成果物はGit管理外の `generated/audio_runtime_tests/` に作成します。

[テスト本体](../project/tools/audio_runtime_tests.cpp)は共通 `Audio.cpp` を直接取り込み、C++20・`/utf-8 /W4 /WX /O2` でビルドします。通常のゲームとは独立したテストなので、別途 `Audio.cpp` をリンクしないでください。

- CPU検査：PCM16 RIFF/WAVEの形式、サイズ・チャンク境界・アラインメント、不正データの拒否、失敗時に既存出力を保持すること、音量・ピッチの制限、未初期化API。
- ネイティブ検査：無音PCMのループ、停止・Pause/Resume、古い世代のハンドル、プール再利用、ロード失敗時の既存再生保持、音名単位の解放、1バンク16／全体128ボイスの制限。
- 既存MP3経路：現在の共通起動音 `resources/bulletShoot.mp3` を音量0で読み込み・再生。素材自体の再配布条件の確認とは別です。

音声デバイスを初期化できない環境では、CPU検査後にネイティブ検査をSKIPして終了します。成功終了だけで全項目が実行されたとは扱わず、ログのPASS／SKIPを確認してください。聴感・音量バランスを評価するテストではありません。

`VoiceHandle::IsValid()` は取得時の成功を示し、現在も再生中かは `IsVoicePlaying()` で確認します。ロードに失敗した場合は同名の既存バンクを保持します。各シーンは所有するキーの音声を停止・解放し、共通Audioをシーンごとに再初期化しません。
