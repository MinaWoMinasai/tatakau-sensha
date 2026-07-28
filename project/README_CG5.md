# CG5 評価課題1「たたかうせんしゃ」提出用 README

## ゲーム概要

「たたかうせんしゃ」は、ネオングリッド上で通常敵を倒して経験値を集め、機体を強化・進化させながらボス撃破を目指す2Dシューティングゲームです。戦闘中の出来事とポストエフェクトを対応させ、成功・危険・戦況変化が画面から分かるようにしています。

## 起動とモード選択

1. `CG2.exe` を起動します。
2. タイトルで `W/S` または上下キーを使い、`NORMAL MODE` / `SHOWCASE MODE` を選択します。
3. 左クリックまたは Enter で開始します。

タイトルの開始操作は1クリックです。ゲーム内にも常時操作ガイドを表示します。

### NORMAL MODE

従来の戦闘バランスで、通常戦闘、経験値獲得、進化、ボス戦をプレイします。

### SHOWCASE MODE

評価者が約60～90秒で主要演出を確認できるガイド付きモードです。通常の操作と戦闘を維持しながら、敵撃破、ジャスト回避、被弾、進化、ボス登場、フェーズ変更／EMP、ボス撃破が順番に起こりやすくなります。約61秒でボス撃破シーケンスへ進みます。

## 操作方法

| 操作 | 内容 |
|---|---|
| `W/A/S/D` | 移動 |
| 左クリック | 射撃 |
| 右クリック | ダッシュ／ジャスト回避 |
| `C` | 進化ツリーを開閉 |
| `1`～`7` | 対応する能力を強化 |
| `H` | 操作ガイド表示切替 |
| `Esc` | タイトルへ戻る |
| 結果画面で `W/S` または上下 | Retry / Return to Title 選択 |
| 結果画面で Enter／Space／左クリック | 決定 |

## クリア・ゲームオーバー

- クリア条件: ボスのHPを0にする。
- ゲームオーバー条件: プレイヤーのHPが0になる。
- ボス撃破直後はタイトルへ戻らず、ボス撃破演出 → `STAGE CLEAR` → Result へ進みます。
- プレイヤー死亡後は死亡演出 → `GAME OVER` → Result へ進みます。
- Result には Clear Time、Just Dodge 回数、Damage Taken、Defeated Enemies を表示します。
- Result から同じモードを Retry するか、タイトルへ戻れます。

## 使用ポストエフェクト

| エフェクト | 発生条件 | ゲーム上の目的 | 実装ファイル |
|---|---|---|---|
| Grayscale | ジャスト回避、進化ツリー、ゲームオーバー | 成功時の時間停止感、背景とUIの分離、敗北の明示 | `ScreenEffectDirector.cpp`, `Composite.PS.hlsl` |
| Vignette | 被弾、低HP、ボス登場、ゲームオーバー | 危険度とダメージを画面周辺から伝える | `ScreenEffectDirector.cpp`, `Composite.PS.hlsl` |
| Gaussian Filter | 進化ツリー表示中 | 背景を抑え、選択UIを読みやすくする | `ScreenEffectDirector.cpp`, `GaussianFilter.PS.hlsl` |
| Box Filter | ボスフェーズ変更／EMP | 電子妨害による一時的な映像劣化 | `ScreenEffectDirector.cpp`, `Composite.PS.hlsl` |
| Luminance Outline | ゲームシーン中の高輝度弾・ネオン対象 | 危険弾と発光物を背景から分離する | `ScreenEffectDirector.cpp`, `Composite.PS.hlsl` |
| Depth Based Outline | ゲームシーン中のプレイヤー、ボス、壁 | 奥行きの不連続を使って主要形状を読みやすくする | `ScreenEffectDirector.cpp`, `Composite.PS.hlsl` |
| Radial Blur | ダッシュ、ジャスト回避、ボス撃破 | 高速移動と衝撃の方向・強さを伝える | `ScreenEffectDirector.cpp`, `Composite.PS.hlsl` |
| Shockwave | 敵撃破、ジャスト回避、進化決定、ボス撃破 | 発生位置と重要度をリング状の歪みで示す | `ScreenEffectDirector.cpp`, `Composite.PS.hlsl` |
| Chromatic Aberration | ジャスト回避、被弾、フェーズ変更、ボス撃破 | 瞬間的な衝撃や異常を強調する | `ScreenEffectDirector.cpp`, `Composite.PS.hlsl` |
| Random / Noise | 被弾、フェーズ変更／EMP、ゲームオーバー | ダメージや電子妨害を短時間で伝える | `ScreenEffectDirector.cpp`, `Random.PS.hlsl` |
| Scanline / Glitch | ボスフェーズ変更／EMP、ゲームオーバー | ボスの攻撃変化とシステム異常を予告する | `ScreenEffectDirector.cpp`, `Composite.PS.hlsl` |
| Bloom | 射撃、軌跡、パーティクル、各成功／撃破イベント | ネオン表現の維持と重要イベントの強弱付け | `Bloom.cpp`, `ObjectPostEffect.cpp`, Bloom系HLSL |
| Dissolve | ボス撃破シーケンスのボス専用再描画 | 全画面ではなく、撃破対象の消失を表現する | `GameScene.cpp`, `ObjectPostComposite.PS.hlsl` |

## イベントと演出の対応

- ジャスト回避: 0.20秒を基準に Grayscale、Time Scale 0.20、プレイヤーのみカラー再描画、Shockwave、弱い Radial Blur／Chromatic Aberration、Bloom boost、`JUST DODGE` 表示、既存SEを小音量で再利用。
- 通常被弾: 赤系 Vignette、Chromatic Aberration、Noise／Random、小さな Camera Shake、`ARMOR HIT` 表示。
- 低HP: HP比率30%未満から弱い Vignette。最大値をClampし、弾とUIの視認性を維持。
- 進化ツリー: ゲーム時間停止、背景 Grayscale＋Gaussian、UIは後段で鮮明に描画。決定時は Bloom＋Shockwave。
- 通常敵撃破: 既存ネオンパーティクル、小Shockwave、短いBloom、軽いChromatic Aberration。
- ボス登場: フェードイン後に `WARNING: BOSS UNIT`、短いスロー、Vignette、Bloom。
- ボスフェーズ変更／EMP: Glitch、Random／Noise、Scanline、Chromatic Aberration、Box Filter、Camera Shake、PHASE表示。
- ボス撃破: Hit Stop、ボス中心の大型Shockwave、Bloom、Chromatic Aberration、Radial Blur、Camera Shake、パーティクル、ボス専用Dissolve、`BOSS DESTROYED`、`STAGE CLEAR`、Result。
- ゲームオーバー: Grayscale、Vignette、Noise／Glitch、死亡パーティクル、`GAME OVER`、Result。

## ScreenEffectDirector

`game/effects/ScreenEffectDirector` はゲームロジックとレンダラーの間に置いたイベント演出管理クラスです。

- ゲーム側は `TriggerJustDodge`、`TriggerPlayerDamage`、`TriggerEnemyDefeat`、`TriggerBossEntry`、`TriggerBossPhaseChange`、`TriggerBossDefeat` などの意味のあるイベントだけを通知します。
- Director が効果時間、補間、排他効果の優先順位、加算／最大値合成、NaN防止、上限Clampを担当します。
- 毎フレーム0から一時パラメータを合成し、`ApplyTo(BloomParam&)` でエンジンへ渡すため、イベント終了後は基準値へ戻ります。
- `Bloom` 側ではシーン開始前の基準値を保存し、GameScene終了時に復元します。GraphicsLabScene、NavalBattleScene の設定を上書きしません。
- 同一HLSLの重複コンパイルを避けるメモリ内シェーダーキャッシュを追加し、GameScene初回ロードを短縮しています。シェーダーそのものは重複実装していません。

## JSON調整

`resources/configs/screenEffects.json` で以下を調整できます。欠損時や不正値には安全な既定値とClampを使います。

- ジャスト回避、被弾、敵撃破、ボス登場、フェーズ変更、ボス撃破、ゲームオーバー、ダッシュ、進化決定の時間
- `showcaseTimeScale`（提出値は`1.0`。イベント列の短時間リグレッション確認にも利用）
- Grayscale強度
- Vignette強度、低HPしきい値
- Gaussian強度
- Box Filter強度
- Radial Blur強度
- Shockwave半径、幅、強度
- Chromatic Aberration強度
- Random強度
- Bloom boost
- Hit Stop時間
- Camera Shake時間、強度
- ボス消失速度
- Depth／Luminance OutlineのON/OFF、幅、しきい値、深度倍率

## ON/OFF比較

- Developmentビルド: ImGui「ゲームデバッグコンソール」→「概要」→「ゲーム用 Depth/Luminance Outline」で輪郭を比較できます。
- JSON: `screenEffects.json` の `outlineEnabled` を切り替えます。
- 各イベント効果: 対応する強度を0にして比較できます。
- `F8` はDevelopmentのPost Profile表示切替、`F9` は負荷比較モードです。Releaseでは無効です。

## 描画順

1. 3Dワールド、ステージ、敵、弾
2. オブジェクトBloom、ネオングリッド、弾道トレイル、パーティクル
3. 全画面Bloom／ScreenEffectDirectorの合成結果
4. ジャスト回避時のプレイヤーカラー再描画、ボス撃破時のボスDissolve
5. HPバー、操作ガイド、進化ツリー、イベント表示、Stage Clear／Game Over／Result
6. Fade

UIを全画面ポストエフェクト後に描画する理由は、Gaussian、Box Filter、Radial Blur、Shockwave、Glitchで文字や選択肢がぼけたり歪んだりするのを防ぐためです。

## 主なC++ファイル

- `game/effects/ScreenEffectDirector.h/.cpp`
- `game/scene/GameScene.h/.cpp`
- `game/scene/TitleScene.h/.cpp`
- `game/scene/IScene.h`
- `game/scene/SceneManager.h/.cpp`
- `game/scene/Game.cpp`
- `game/player/actor/Player.h/.cpp`
- `game/enemy/actor/Enemy.h/.cpp`
- `game/exp/ExpEnemy.h/.cpp`
- `DirectX/engine/postEffect/Bloom.h/.cpp`
- `DirectX/engine/commom/DirectXCommon.h/.cpp`
- `DirectX/engine/commom/WinApp.h/.cpp`

## 使用シェーダーファイル

新しいポストエフェクトHLSLは追加せず、既存実装を再利用しています。

- `resources/shaders/Composite.PS.hlsl`
- `resources/shaders/GaussianFilter.PS.hlsl`
- `resources/shaders/Random.PS.hlsl`
- `resources/shaders/ObjectPostComposite.PS.hlsl`
- `resources/shaders/ObjectPostBloomAdd.PS.hlsl`
- `resources/shaders/ObjectPostOutlineAdd.PS.hlsl`
- `resources/shaders/BloomExtract.PS.hlsl`
- `resources/shaders/BloomDownsample.PS.hlsl`
- `resources/shaders/BloomBlurH.PS.hlsl`
- `resources/shaders/BloomBlurV.PS.hlsl`

## 工夫した点・難しかった点

- 常時派手にするのではなく、「回避成功」「被弾」「フェーズ変更」「撃破」の理由が分かる短い演出にしました。
- 通常敵、ジャスト回避、ボス撃破にShockwave優先順位を付け、同時発生時は重要なイベントを採用します。
- Bloom、Chromatic Aberrationなどは加算後にClampし、GrayscaleやShockwave中心のような排他的値には優先順位を付けました。
- UIとワールドの描画順を分け、進化ツリーと結果選択を常に読める状態にしました。
- シーン固有の一時値とエンジン既定値を分離し、別ゲームモジュールへの回帰を防ぎました。
- Releaseで残っていたTextLabelとウィンドウタイトルのPost Profile更新を除外しました。

## ビルドと提出物

確認構成:

- `Development|x64`
- `Release|x64`

提出時は次を同じフォルダ構成でまとめます。

1. Release実行ファイルと `dxcompiler.dll` / `dxil.dll`
2. `resources` フォルダ
3. ビルド可能な `project` フォルダ一式
4. 本README

Releaseでは `USE_IMGUI` を定義せず、ImGui、FPS、Post Profile、Collision Debug、開発用ショートカットを表示しません。

## 検証メモ

- Developmentビルド: 成功
- Releaseビルド: 成功
- HLSL: Release起動時のコンパイル成功、タイトルとゲームシーンまで起動確認
- Releaseタイトル: ImGui／FPS／Post Profile／Collision Debugなし
- Releaseゲーム: 通常戦闘、操作ガイド、イベントコールアウトを実画面確認
- Showcase: 敵撃破→ジャスト回避→被弾→進化→ボス登場→フェーズ変更→ボス撃破→Stage Clear→Resultまで完走確認
- ボス撃破中の結果バナーは状態遷移時だけ再構築し、GPU使用中のTextLabelリソースを毎フレーム破棄しないことを確認
- 通常描画では新規フルスクリーンパスを増やしていません。追加パスはボス撃破中の短いボス専用Dissolveのみです。
- 変更前の同一環境GPUベースラインは未保存のため、厳密な変更前後比較値はありません。DevelopmentのPost Profileで継続比較します。
