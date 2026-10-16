# たたかうせんしゃ：Phase 1 グラフィックス改修

2026-10-13 / DirectX 12 / ローカル作業。Phase 1の床・ブロック・描画整合性を実装した。機体の3Dモデル化には着手していない。commit / push / merge、新規ライブラリ追加は行っていない。

## 調査と設計

通常カメラは負のZ側から+Zを向き、ゲームはXY平面のZ=0で進行する。既存の ground.obj は SessionBootstrap で初期化されるが、本編の GameplayRenderer からは描画されていなかった。既存の壁はモデルを持つ一方、ネオン輪郭と透明描画が主体で、床の深度・材質バッファがなかった。

そこで、床を+Z側に置き、ブロックの裏面を床に寄せる。衝突用の worldTransform / AABB / OBB と描画専用の surfaceObject を分けた。Stageの衝突処理、プレイヤー移動、弾道、敵AI、照準計算、カメラ投影・追従、HUDレイアウトは変更していない。

既存 Object3d の lightingMode=2（Cook–Torrance PBR）、Scene HDR色・法線・材質MRT、深度、Bloom、SSAO、SSRを利用した。専用シェーダーや外部モデルの追加は不要だった。環境マップには既存 skybox.dds を明示的に割り当てた。

## 追加した処理

- 不透明なXY床：4頂点・2三角形。全マップセル、追加障害物の実AABB、既存のカメラ可視範囲と余白を覆う。背景色の変更ではなく、深度・法線・材質を持つ実メッシュ。
- ベベル付きブロック：6面、12の面取り、8の角。96頂点・44三角形の閉じた共通メッシュを ModelManager で再利用する。描画用Z厚みのみ調整し、元の衝突形状を保持する。
- 本体と発光輪郭を分離。本体はemissive=0のPBRで描き、輪郭のみネオン経路へ送る。新ブロック本体を従来の共有Bloomキャプチャから外し、二重描画を防ぐ。
- 通常／ダメージブロックの色と脈動、isActive、既存の可視範囲判定、本体表示スイッチを保持。側面ラインの分割を調整可能にした。
- 床グリッドは床より0.025手前へ配置。遠方・傾斜カメラでのサブピクセル線を広げ、濃度を補正する。カメラの角度・投影は変更しない。

描画順は次の通り。

```text
Scene HDR / 法線 / 材質 MRT + Scene Depth
  → 不透明3D床・ベベルブロック（kNone、深度書込み）
  → 既存ボス・特殊演出、本体描画
  → 床グリッド／機体ネオン、ブロックの発光輪郭
  → 弾・剣の軌跡、共有発光、透明パーティクル
  → 既存のグローバルSSAO / SSR / Bloom / Tonemap
  → 既存のゲームHUD・文字・ImGui
```

新表示のグリッド・輪郭・軌跡・パーティクルは、旧Bloom方式を含めて必要なキャプチャをフル解像度にし、Scene深度を参照する。輪郭・透明VFXは深度を書き込まず、法線／材質への書込みをマスクする。ポスト合成は色だけへ行い、全MRT・DSV・viewport・scissorを復元する。

D3D12検証で見つかったHDR/LDRのPSO不一致、深度なし描画先とDSVを要求するPSOの不一致、MRT復元の不足も修正した。共通Spriteの変換CBを実際の共有VSが要求する256バイトへ揃え、VS用materialを明示的にバインドした。強化プレビューのクリアカラーもリソース作成時と一致させた。HUDの座標・操作・表示内容を変更する処理は加えていない。

## 切替と調整

通常の「たたかうせんしゃ」遠征・試作ランでは新表示を標準候補として有効にしている。その他のシーンは従来の適用条件を維持する。

1. Developmentで **F3 → 床・立体ブロック** を開く。
2. 「新しいステージ表現を使用」、床／ブロックの個別スイッチ、色・金属度・粗さ・高さ・照明・輪郭発光を変更する。
3. 「適用」でプレビュー。「保存」で次回起動へ反映。「再読込」で保存済み値へ戻る。
4. 「従来表示へ戻す」で復帰可能。F12 → 見た目 → Phase 1には初期値復帰と法線・深度・材質・SSAO・SSRの診断もある。

保存先は [gameVisuals.json](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/resources/configs/gameVisuals.json) の stageSurface（version 2）。既存セクションを維持し、sectionのないversion 1も読み込める。保存済みの無効スイッチは、古い expeditionAuthored フラグがfalseでもラン初期化で上書きしない。診断のAO/SSR強制値は保存せず、ゲームの通常設定を復元する。

| 初期値 | 値 |
|---|---|
| 床色 RGB | 0.035 / 0.045 / 0.060 |
| 床の金属度 / 粗さ | 0.20 / 0.72 |
| 床Z / 外周余白 | +1.10 / 8.0 |
| 床グリッド最小画面幅 | 1.15px |
| ブロック色 RGB | 0.085 / 0.105 / 0.140 |
| ダメージブロック色 RGB | 0.140 / 0.035 / 0.045 |
| ブロックの金属度 / 粗さ | 0.30 / 0.56 |
| 単位セルの見た目の全高 / 枠線発光倍率 | 2.8 / 0.65 |
| 側面ライン分割 | 1（最大8） |

設定の数値は有限値・範囲を検証し、本体のalphaは1へ固定する。床Zを大きく変えると見た目とXY平面の距離も変わるため、まず初期値で比較することを勧める。

## ビルドと検証

Development x64 / Release x64の検証用ビルドは成功、コンパイラ警告0・エラー0。通常のPDB生成ではこのPCのツール環境で C1902 / LNK1318 が発生したため、ローカル検証用の ForceImportBeforeCppTargets で /Z7 を使い、ReleaseはリンクPDB生成を無効にした。リポジトリの標準コンパイル／リンク設定は変更していない。通常PDB方式でのビルド成功を確認したという意味ではない。

ビルド手順とログ：[Development実行スクリプト](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/generated/phase1_surfaces/build_development.cmd)、[Release実行スクリプト](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/generated/phase1_surfaces/build_release.cmd)、[Developmentログ](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/generated/phase1_surfaces/development_build_z7.log)、[Releaseログ](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/generated/phase1_surfaces/release_build.log)。

ネイティブ実行の自動検証は、固定入力・seed=1337・1280×720・RTX 4060 Laptop GPUで行った。[検証集計JSON](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/generated/phase1_surfaces/final_validation_summary.json) と各ケースのreport/frame JSONに実際の状態・画像・D3D12メッセージを残している。

| 検証 | 内容 |
|---|---|
| 通常戦闘／新旧表示 | 各180フレーム。実プレイヤー移動・射撃・敵AI・弾／壁処理・カメラ追従を通す |
| 壁への移動・射撃 | 新旧各180フレーム。壁へ移動し続けてX≈21.80で接触、射撃7回・残存自機弾0。位置・HP・弾数・カメラの比較差0 |
| 床のみ／ブロックのみ | 各180フレーム。個別切替を確認 |
| Bloom | OFF・Quality・Legacy、新表示の本体と輪郭を比較 |
| 法線／粗さ／SSAO／SSR | 診断画像を出力、AO/SSRのOFFも比較 |
| NeonDepth特殊カメラ | 新旧各1800フレーム、3種類のPhase 1攻撃と回復を完了（attack mask=7） |
| 既存ボス | Rival／Prototype各360フレーム |
| 負荷 | 最大敵128体・初期弾480発、360フレーム |
| 部屋遷移 | 新旧各1200フレーム。実際の強化購入・次の戦闘部屋へ到達 |
| F3実操作 | 従来表示→適用→保存→再起動、再読込、新表示への復帰・保存を確認 |
| 設定互換 | version 1・stageSurfaceなし、保存済み無効＋expeditionAuthored=falseを実起動で確認 |
| GPUベース検証 | 通常戦闘180フレーム。実GPUからのdescriptorアクセス・CB範囲を検証し、エラー0・警告0 |
| メッシュ | 閉じた多様体、法線・巻き順・体積、床方向、描画TransformのXY不変、NaN等の設定補正をC++で確認 |

ゲーム状態の比較は [gameplay_parity.json](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/generated/phase1_surfaces/gameplay_parity.json) に記録した。改修前baseline対新表示、新旧表示、ボス、部屋遷移、壁への移動の共通捕捉フレームで、描画リソースのdescriptor数を除いた全snapshotフィールドとカメラ行列を比較し、差分は0だった。GPUの実際の非同期タイミングや全内部状態まで証明する比較ではない。集計した24ケースの捕捉フレームは、通常D3D12検証でエラー0・警告0。

## 性能と既知の課題

共通メッシュを使うが、GPUインスタンシングは導入していない。可視ブロック1個につき不透明本体1draw、床は1draw。通常戦闘の計測では本体中央値70個、床を含め3082三角形。旧本体のdrawを置き換えるため、不透明本体は単純に二重に増えない。一方、描画専用Object3dの定数バッファ分のメモリは増える。

120フレーム・warmup 30のCPU/GPUタイムスタンプ計測は [性能集計](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/generated/phase1_surfaces/final_performance_summary.json)、[旧表示CSV](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/generated/phase1_surfaces/final_legacy_perf.csv)、[新表示CSV](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/generated/phase1_surfaces/final_full_perf.csv) を参照。ビルドや別ゲームを並行実行せず、通常デバッグ層ON・GPUベース検証OFFで採った最終計測値は次の通り。

| 時間（ms） | 従来表示・中央値 | 新表示・中央値 |
|---|---:|---:|
| CPU描画コマンド記録全体 | 4.219 | 4.163 |
| GPUフレーム全体 | 4.180 | 3.653 |
| GPU床・ブロック本体 | 新パスなし | 1.162 |
| GPUグローバルBloom／Post | 1.383 | 1.722 |
| GPUフレーム全体P95 | 4.556 | 5.216 |

GPU時計・発熱・実行時負荷による変動があり、中央値の低下を一律の高速化とは解釈しない。P95は増加している。このPCでの短いDevelopment計測を製品のFPS保証には使えない。床・壁の新パスと実G-bufferを利用するAO/SSRには追加コストがある。表のGPUスコープは入れ子なので加算しない。フレーム全体のDrawCall数は直接計測しておらず、追加した床・ブロック本体のdraw数を記録した。

- SSAOは接触部の控えめな陰影として寄与する。シャドウマップにStageを追加する改修はしておらず、床／壁のshadow receiveは0として既存の影用カメラとの不整合を避けた。
- 正面寄りのカメラと床roughness=0.72ではSSRの寄与は小さい。SSR診断は黒い領域が多く、鏡面の床にはしていない。画面外の情報も得られない。機体はまだ2Dネオン主体なので、機体からの立体的な反射や接触陰影には限界がある。
- 1280×720の既存レンダリング基盤を維持。高DPI・任意リサイズの全面改修、すべてのGPU・全デバッグシーン、任意の極端なdebug camera位置、全種類の破壊／ダメージブロック配置、物理マウスでの連続照準の手動試遊は網羅していない。
- ボス検証は既存ゲームのPhase 1攻撃を対象にした。既存ボスのPhase 2全経路、長時間連続プレイは未検証。今回のグラフィックスPhase 2（機体3D化）は対象外。

## 比較用画像と再実行

[新旧・法線・粗さの比較画像](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/generated/phase1_surfaces/final_comparison.png)、[新表示の元画像](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/generated/phase1_surfaces/final_full/frame_180.png)、[特殊カメラの元画像](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/generated/phase1_surfaces/final_depth_full/frame_1800.png)、[Bloom OFF](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/generated/phase1_surfaces/final_bloom_off/frame_180.png)、[SSAO診断](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/generated/phase1_surfaces/ssao_verified/frame_180.png)、[SSR診断](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/generated/phase1_surfaces/ssr_verified/frame_180.png)。

再現スクリプトは [run_scenario.ps1](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/generated/phase1_surfaces/run_scenario.ps1)。環境変数による新旧／診断指定はDevelopmentのみで、Releaseや保存設定へ持ち込まれない。生成した検証資料はgenerated以下のローカル成果物でGit管理対象外。

## 変更ファイル

**床・ブロック・共通メッシュ**

- [Stage.cpp](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/game/player/actor/Stage.cpp)
- [Stage.h](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/game/player/actor/Stage.h)
- [StageSurfaceGeometry.h](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/game/render/StageSurfaceGeometry.h)
- [StageSurfaceSettings.h](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/game/render/settings/StageSurfaceSettings.h)
- [ModelManager.cpp](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/DirectX/engine/3d/ModelManager.cpp)
- [ModelManager.h](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/DirectX/engine/3d/ModelManager.h)

**本編の描画順・グリッド**

- [GameplayRenderer.cpp](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/game/render/session/GameplayRenderer.cpp)
- [StageNeonRenderer.cpp](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/game/render/session/StageNeonRenderer.cpp)
- [NeonGridGeometry.cpp](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/DirectX/engine/common/NeonGridGeometry.cpp)
- [NeonGridGeometry.h](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/DirectX/engine/common/NeonGridGeometry.h)
- [NeonGridRenderer.cpp](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/DirectX/engine/common/NeonGridRenderer.cpp)
- [NeonGridRenderer.h](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/DirectX/engine/common/NeonGridRenderer.h)
- [TrailManager.cpp](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/DirectX/engine/common/TrailManager.cpp)

**MRT・深度・Bloomの整合性**

- [DirectXCommon.cpp](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/DirectX/engine/common/DirectXCommon.cpp)
- [DirectXCommon.h](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/DirectX/engine/common/DirectXCommon.h)
- [Object3dCommon.cpp](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/DirectX/engine/3d/Object3dCommon.cpp)
- [SpriteCommon.cpp](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/DirectX/engine/2d/SpriteCommon.cpp)
- [Sprite.cpp](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/DirectX/engine/2d/Sprite.cpp)
- [Sprite.h](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/DirectX/engine/2d/Sprite.h)
- [GameplayHud.cpp](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/game/ui/session/GameplayHud.cpp)
- [CombatUiState.h](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/game/session/state/CombatUiState.h)
- [ObjectPostEffect.cpp](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/DirectX/engine/postEffect/ObjectPostEffect.cpp)
- [ObjectPostEffect.h](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/DirectX/engine/postEffect/ObjectPostEffect.h)
- [Bloom.cpp](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/DirectX/engine/postEffect/Bloom.cpp)
- [TankRewardPreviewRenderer.cpp](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/game/ui/TankRewardPreviewRenderer.cpp)

**調整・保存・診断**

- [ExpeditionAuthoring.cpp](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/game/editor/session/ExpeditionAuthoring.cpp)
- [GameplayEditor.cpp](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/game/editor/session/GameplayEditor.cpp)
- [GameplaySettings.cpp](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/game/editor/session/GameplaySettings.cpp)
- [RunVisualSettings.cpp](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/game/render/settings/RunVisualSettings.cpp)
- [NeonPresentationState.h](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/game/session/state/NeonPresentationState.h)
- [GameplayQueries.cpp](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/game/session/GameplayQueries.cpp)
- [IScene.h](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/game/scene/IScene.h)
- [gameVisuals.json](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/resources/configs/gameVisuals.json)

**プロジェクト登録**

- [MyEngine.vcxproj](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/MyEngine.vcxproj)
- [MyEngine.vcxproj.filters](C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/MyEngine.vcxproj.filters)

