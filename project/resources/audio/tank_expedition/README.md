# 遠征モード用・オリジナル音声試作

このプロジェクト向けに波形から合成した SE 7 種と、128 BPM・8 小節・15 秒の電子音 BGM です。既存ゲームの録音、外部の楽曲・サンプル素材は使用していません。SE のうち発射・命中・撃破は、先に作成した `generated/expedition_review/audio_demo` の合成式を引き継いでいます。

**聴感は未評価です。** ファイル・ループ境界・実 Audio API でのロードと再生は検証していますが、完成楽曲・最終ミックスではありません。実際にプレイして、連射時の疲れや敵の予告音の聞き取りやすさを調整するための初期素材です。

| ファイル | 長さ | 用途 |
|---|---:|---|
| `shot.wav` | 0.120 秒 | 短い下降音の発射 |
| `hit.wav` | 0.175 秒 | 細い金属・結晶系の命中 |
| `kill.wav` | 0.530 秒 | 低音と細かな破片による撃破 |
| `warning.wav` | 0.290 秒 | 敵の攻撃予告を知らせる二音 |
| `dash.wav` | 0.235 秒 | ダッシュの短い空気音 |
| `upgrade.wav` | 0.680 秒 | 改造・進化・整備の確定 |
| `armor_break.wav` | 0.380 秒 | 現在はプレイヤー被弾時の衝撃音。将来の装甲破壊にも転用できる素材 |
| `music_base.wav` | 15 秒 | キック、低音、控えめな和音 |
| `music_intensity.wav` | 15 秒 | ハット、スネア、短いアルペジオの追加レイヤー |
| `preview.wav` | 15 秒 | 確認用の音声。ゲームからのロードは不要 |

`preview.wav` は 1 秒から発射・命中・撃破・警告・ダッシュ・強化・被弾用衝撃音を 1 秒おきに鳴らし、10 秒から短い連射を重ねています。BGM の追加レイヤーは 4〜8 秒で徐々に増えます。各 SE はモノラル、BGM とプレビューはステレオ。全て 48 kHz / PCM 16 bit です。ゲームでは `upgrade.wav` をジャスト回避の成立音にも使います。敵の装甲破壊・コア露出の仕組みは今回の実装には含みません。

## 再生成と検証

Python と NumPy を用意し、このファイルと同じフォルダーの `generate_audio.py` を実行します。

```powershell
python project/resources/audio/tank_expedition/generate_audio.py
```

同じ乱数 seed と合成式から WAV を再生成し、`measurements.json` へ測定値を保存します。検証で不一致があれば assert で終了します。

- SE のピークは -7〜-5 dBFS、全ファイルのクリップ数は 0。
- 全 SE の先頭・末尾のサンプルは 0。プレビューにも入出のフェードを適用。
- BGM は 32 拍を正確に 720,000 サンプルにし、末尾の残響を先頭へ回す循環加算で作成。
- ループ境界の隣接サンプル差は base で 1/32768、intensity で 0。通常のサンプル変化範囲内に収まることを検証。
- BGM 2 本を等倍で足してもピークは -8 dBFS。実ゲームではさらにゲインを下げる。

`generated/expedition_audio_checks/build.cmd` の検証では、実エンジンの `Audio.cpp` を MSVC `/W4 /WX` でビルドし、9 素材のロード、実再生開始、同種 SE の連続通知抑制、2 インスタンスの所有権、繰り返し終了と再初期化を確認しました。極小音量で API の状態を調べる検証であり、試聴を代替するものではありません。

## ゲーム側の利用

ヘッダーのみの `project/game/run/TankExpeditionAudio.h` をシーンのメンバーとして所有します。シーンの終了時には `Shutdown()` を呼べます。デストラクタからも呼びますが、必ず Audio シングルトンが生存している間に破棄してください。

```cpp
TankExpeditionAudio expeditionAudio_;

// Audio の初期化は既存 Game に任せる。デバイス準備前の呼び出しは無害。
expeditionAudio_.Initialize();
expeditionAudio_.SetCombat(inCombat);
expeditionAudio_.SetBoss(isBossRoom);
expeditionAudio_.SetDucked(isChoiceOrPauseScreen);
expeditionAudio_.Update(realDeltaSeconds); // 選択画面でも更新し、フェードを続ける

// 対応するゲーム内イベントから呼ぶ。連射は弾ごとより一斉射撃ごとが望ましい。
expeditionAudio_.Shot();
expeditionAudio_.Hit();
expeditionAudio_.Kill(chainCount);
expeditionAudio_.EnemyWarning();
expeditionAudio_.Dash();
expeditionAudio_.Upgrade();
expeditionAudio_.ArmorBreak(); // 現在の scene ではプレイヤーの被弾時に利用
```

`SetMusicVolume(0〜1)` と `SetEffectsVolume(0〜1)` で個別ゲインを変更できます。`LoadedClipCount()`、`ClipCount()`、`IsMusicPlaying()`、`PlayCount()` は診断用です。

SE の同時発音は全体で最大 17 voice、BGM は 2 voice。さらに種類ごとの短いクールダウンで散弾・連鎖による音の重なりを抑えます。音程変化は独立した固定パターンを使い、ゲームの乱数を消費しません。遠征中の従来の `bulletShoot` は呼び出し元で抑制し、二重発音を避けてください。

BGM の 2 レイヤーは起動時に続けて再生を開始し、通常戦・ボス・選択中でゲインだけを変えます。既存 API にサンプル単位の同時開始はないため、厳密なサンプル同期ではありません。同じデバイス・同じ長さ・同じ再生レートで継続し、部屋ごとの頭出しはしません。

各インスタンスは `__tank_expedition_<id>_...` の専用キーを持ち、終了時は自分の音声だけを停止・解放します。`StopAll()`、マスター音量の変更、他シーンの BGM 停止は行いません。
