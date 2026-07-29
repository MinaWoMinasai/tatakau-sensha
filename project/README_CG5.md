# CG5 評価課題1「たたかうせんしゃ」提出用 README

## ゲーム概要

「たたかうせんしゃ」は、ネオングリッド上で敵を倒して経験値を集め、機体を強化・進化させながらボス撃破を目指す2Dシューティングゲームです。ポストエフェクトは常時画面を派手にするためではなく、移動速度、被弾、戦況変化、ボス撃破を視覚的に伝えるために使用しています。

## 起動方法

1. `CG2.exe`を起動します。
2. タイトル画面で左クリックすると通常ゲームを開始します。

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
| Resultで`W/S`または上下キー | Retry／Return to Title選択 |
| ResultでEnter／Space／左クリック | 決定 |

## クリアとゲームオーバー

- ボスのHPを0にすると、ボス撃破演出、`STAGE CLEAR`、Resultの順に進みます。
- プレイヤーのHPが0になると、死亡演出、`GAME OVER`、Resultの順に進みます。
- Resultから同じゲームをRetryするか、タイトルへ戻れます。

## ポストエフェクトの確認方法

### Grayscale

1. 発動条件: ジャスト回避、進化ツリー表示中、ゲームオーバー時に発動します。
2. 見る場所: ジャスト回避時はゲーム世界全体、進化中はUIの背後を確認します。
3. 見た目: 背景の彩度が下がります。進化UIや結果文字は全画面ポストエフェクト後に描画されるため鮮明なままです。
4. 目的: 時間停止感を伝え、操作対象のUIや重要な状態を背景から分離します。

### Vignetting

1. 発動条件: 通常被弾、低HP、ボス登場、ゲームオーバー時に発動します。
2. 見る場所: 画面中央ではなく画面四隅と外周を確認します。
3. 見た目: 画面周辺が暗くなり、危険度に応じて中央へ視線が集まります。
4. 目的: 敵弾やUIを隠さず、ダメージと危険状態を伝えます。

### Box Filter

1. 発動条件: ボスHPが一定値まで減少してフェーズが変化すると発動し、画面に`PHASE 2`、`HAZARD DEPLOYED`、`FINAL PHASE`のいずれかが表示されます。
2. 見る場所: ボスと背景グリッドの境界を確認します。
3. 見た目: 色収差とカメラ揺れの直後、Box Filter、Random／Noise、Glitch、Scanlineが時間差で適用され、ゲーム世界が一時的に粗く乱れます。UIとフェーズ名は後段描画のためぼけません。
4. 目的: EMPによる映像信号の乱れと、ボスの攻撃段階が変わったことを知らせます。

### Gaussian Filter

1. 発動条件: 進化ツリーを開いている間に発動します。
2. 見る場所: 進化ノードではなく、その背後にあるゲーム世界を確認します。
3. 見た目: 背景だけが滑らかにぼけ、進化ノードと説明文は鮮明に残ります。
4. 目的: 戦闘画面の情報量を抑え、進化先の選択を読みやすくします。

### Depth Based Outline

1. 発動条件: 通常ゲーム中は常時有効です。
2. 見る場所: プレイヤー、敵、ボス、壁が前後に重なる境界を確認します。
3. 見た目: ネオンBloomとは別に、深度バッファのView空間Z差分から水色の輪郭を抽出します。平坦な地面や遠景の小さな差はしきい値で除外します。
4. 目的: 発光が重なった場面でも、主要オブジェクトと遮蔽物の前後関係を読みやすくします。

Developmentビルドでは、ImGuiの「ゲームデバッグコンソール」→「概要」→「ゲーム用 Depth Based Outline」でON/OFF比較できます。この比較UIはReleaseには含まれません。

### Radial Blur

1. 発動条件: 右クリックの通常ダッシュ、ジャスト回避、ボス撃破時に発動します。
2. 見る場所: 発動時のプレイヤー位置を中心に、その周囲のグリッドとオブジェクトを確認します。
3. 見た目: プレイヤー位置から外側へ画面が短時間流れます。通常ダッシュは控えめで、ジャスト回避とボス撃破はより強く表示されます。
4. 目的: 通常移動との差を付け、ダッシュの速度感と重要イベントの衝撃を伝えます。

### Dissolve

1. 発動条件: ボスのHPを0にすると、ボス撃破シーケンス中に発動します。
2. 見る場所: 全画面ではなく、ボス本体と橙赤色の消失境界を確認します。
3. 見た目: ボス専用RenderTexture上で本体が徐々に欠けます。形状と赤橙色の境界を見せるため、Dissolve中だけボスBloomを抑え、開始から0.30秒後に大型Shockwaveと発光を重ねています。その後`STAGE CLEAR`へ進みます。
4. 目的: 白い爆発だけで消すのではなく、撃破対象そのものが崩壊したことを示します。

### Random / Noise

1. 発動条件: 通常被弾、ボスフェーズ変更／EMP、ゲームオーバー時に発動します。
2. 見る場所: ボス周辺だけでなくゲーム世界全体の明るさの細かな乱れを確認します。
3. 見た目: ボスフェーズ変更時は、色収差の後にランダムノイズとGlitchが入り、終盤は走査線とともに弱まりながら通常画面へ戻ります。
4. 目的: 被弾やEMPによる一時的な映像障害を表現し、ボスの状態変化を通知します。

## 補助的に使用している効果

- Shockwave: 敵撃破、ジャスト回避、進化決定、ボス撃破の発生位置をリング状の歪みで示します。
- Chromatic Aberration: 被弾、回避、フェーズ変更、ボス撃破の瞬間的な衝撃を示します。
- Scanline／Glitch: ボスフェーズ変更／EMPとゲームオーバー時の映像異常を示します。
- Bloom: ネオンの弾、軌跡、キャラクター、パーティクルを発光させます。通常Bloomの基準値は今回変更していません。

## 描画接続と描画順

`game/effects/ScreenEffectDirector.cpp`がイベント時間、Envelope、優先順位、NaN防止、Clampを管理し、`GameScene::GetScreenEffectState()`から`Bloom::SetScreenEffectState()`へ毎フレーム値を渡します。全画面効果は`resources/shaders/Composite.PS.hlsl`の最終合成へ接続されています。

Dissolveだけは全画面処理ではありません。`GameScene::DrawAfterPostEffect3D()`でボスを`ObjectPostEffect`の専用RenderTextureへ再描画し、`resources/shaders/ObjectPostComposite.PS.hlsl`でボスのピクセルだけを消失させます。

描画順は次のとおりです。

1. 3Dワールド、ステージ、敵、弾
2. オブジェクトBloom、ネオングリッド、弾道トレイル、パーティクル
3. 全画面BloomとScreenEffectDirectorの合成結果
4. ジャスト回避時のプレイヤー再描画、ボス撃破時のボスDissolve
5. HP、操作ガイド、進化UI、イベント表示、Stage Clear／Game Over／Result
6. Fade

この順序により、Gaussian Filter、Box Filter、Radial Blur、Random、Glitchはゲーム世界へ適用されますが、HPや進化UI、Result文字はぼけたり歪んだりしません。

## 設定ファイル

`resources/configs/screenEffects.json`から、各イベントの時間、Depth Outline、通常ダッシュのRadial Blur、ボスフェーズ変更のBox Filter／Noise／Random／Scanline／Glitch、ボス撃破Dissolve速度と主要衝撃の遅延を調整できます。欠損値・非数・範囲外の値にはC++側の安全な既定値とClampを使用します。

オブジェクト単位のBloom設定は`resources/configs/gamePostEffects.json`にあります。

## 主な実装ファイル

- `game/effects/ScreenEffectDirector.h/.cpp`
- `game/scene/GameScene.h/.cpp`
- `game/enemy/actor/Enemy.h/.cpp`
- `game/scene/IScene.h`
- `game/scene/SceneManager.h/.cpp`
- `game/scene/Game.cpp`
- `DirectX/engine/postEffect/Bloom.h/.cpp`
- `DirectX/engine/postEffect/ObjectPostEffect.h/.cpp`
- `resources/shaders/Composite.PS.hlsl`
- `resources/shaders/Random.PS.hlsl`
- `resources/shaders/ObjectPostComposite.PS.hlsl`

## DevelopmentとRelease

- DevelopmentではImGuiの既存調整UI、Depth Outline比較、Post Profileを使用できます。
- Releaseでは`USE_IMGUI`を定義せず、ImGui、FPS、Post Profile、Collision Debug、F2～F12の開発操作を表示・処理しません。
- DevelopmentとReleaseで、通常ゲームのポストエフェクト設定とボス撃破シーケンスは共通です。

## ビルド対象

- `Development|x64`
- `Release|x64`

提出時はRelease実行ファイル、`dxcompiler.dll`、`dxil.dll`、`resources`、ビルド可能な`project`一式、本READMEを同じ構成で含めます。
