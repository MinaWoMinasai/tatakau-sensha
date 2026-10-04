# ネオンのグローと弾の表示調整

2026-10-04。開始branchは`feature/neon-bloom-presentation-polish`、HEADは
`9d0a55a3a0581bc358d60bb46c3d60b017e55530`。前段のBloom作業33ファイル
（tracked 22 / untracked 11）を保護して追加調整した。開始状態の全ファイルと
SHA-256、binary diffは`generated/neon_glow_revision/starting_files/`、
`starting_state.json`、`starting.diff`へ保存。commit / push / mergeは行わない。

## 調査と採用方針

[Retrograde Arena公式Steamページ](https://store.steampowered.com/app/1055210/Retrograde_Arena/)の
トレーラーとギャラリーをブラウザで確認した。短い明るい弾の先端、所有者色の帯と残光、
暗い背景へ広がる柔らかい光を参考にした。以下は見た目から選んだ自作の実装であり、
参考作品の非公開Shaderや素材を取得・複製したものではない。

[EpicのBloom解説](https://dev.epicgames.com/documentation/en-us/unreal-engine/bloom-in-unreal-engine)は
高解像度の狭いぼかしと低解像度の広いぼかしを組み合わせる設計を説明している。
[UnityのBloom解説](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/post-processing-bloom.html)も
明るさの抽出、強度、光の広がりを別の設定として扱っている。
それらの数値を別エンジンへそのまま移植せず、既存の正規化Bloom Pyramidを調整した。

前段のGlobalはthreshold 1 / gain .15 / scatter .3。
5段の最も広い2段の係数は合計.027で、最終Gainも小さく、Haloが控えめだった。
今回のGlobalはthreshold .65 / knee .5 / scatter .55 / radius .9 / gain .55、
Showcaseはthreshold .65 / knee .5 / scatter .55 / radius 1 / gain .8。
元の線の幅、露出、tone mappingは変えない。
Generic BloomParamの既定値、正規化filter、Legacy/OFF、CB/Root配置も維持した。

ゲームのlocal gainはGrid .48、Trail 1.2、Particle .65、Player/Boss/Exp .38、
Stage/Shared .3。通常のbillboard actorはGrid Postが主経路。
文字は元の字形を別に保持し、inner gain .55 / outer .75。LDRの色を保つ上限制御を維持する。

## 弾の接続

`NeonProjectileRenderer`はゲーム側の表示専用クラス。既存NeonGridRendererを別instanceで
使用し、所有者色の短い帯、淡い小さな芯、低AlphaのHaloを1batchで描く。
寸法はworld単位で、弾のcollision radius、速度、ダメージを参照・変更しない。
通常弾は発射初frameとゼロ速度でも表示する。死/null弾は除外し、既存の別演出を持つ
Rail / SlashWaveも除外する。Bulletへ追加したAPIはconstの`GetVisualColor()`だけ。

GameSceneは既存Trail Passの直前に一度だけBeginFrameを呼び、capture内または
post無効の直接描画分岐で一度だけDrawする。以降は既存のObject3d state再Bindを行う。
GPU Fence完了前のVB/VP再利用、同frameの再準備・重複Draw、容量末尾の不完全な頭部を防ぐ。
GPU Skinning / Neon Shader / Palette / Animation / mask / モデルは変更しない。

`gameVisuals.json`の通常弾の尾はlifetime .13、halfWidth .16、
head intensity 1.6 / alpha .9、tail intensity .35 / alpha .04へ調整。
汎用Trail ShaderやMelee Trail設定は変更しない。

## GUIでの確認

通常のDevelopment起動で新しい設定が有効。PowerShellは不要。
F3制作ツール →「Neon Skinned Previewを開く」→「Enter Neon Character Showcase」で
キャラクターを確認できる。View / CaptureのRecommended Bloom presentationで今回の
Bloom設定へ戻せる。AnimationからIdle / Attackを操作する。

F12 Debug Console → Postでは、Freeze game for Bloom comparisonと
Developer Bloom comparisonのOFF / Legacy / Qualityで同じ弾を比較できる。
「グリッド / 弾軌跡 / パーティクル」内のNeon Projectile HeadsとHead/Core/Haloで
先端表示を調整できる。Head設定はその起動中だけ有効。
比較操作は外観・時刻を固定し、モデルの再ロードやAnimation再登録を行わない。

## 実機の証拠

実際のエンジン出力1280×720、UIなし・画像の後加工なし。

- ゲームの同条件停止比較：`project/generated/neon_bloom_presentation/game_1791071737634/`。
  000=Quality、001=OFF、002=Legacy。3発のPlayer弾、同じ位置・camera・post設定。
  `generated/neon_glow_revision/game_comparison.json`で条件一致を検査。
  Qualityは小さい明るい芯と緑の広いHalo、Legacyは白い大きな塊になりやすい。
- 実モデル正面：`project/generated/neon_directional_dissolve/showcase_1791071796753/bloom_comparison_1791071848806/`。
- 実モデル側面：同directoryの`bloom_comparison_1791071901685/`。
  OFF / Legacy / Qualityの各batchで姿勢・camera・露出・発光設定を固定。
- 動作：`project/generated/neon_directional_dissolve/showcase_1791071929369/sequence_1791071961539/`。
  エンジン出力240 frame、60fps、Idle 75 frame → Attack 108 frame → Idle 57 frame。
  同じ発光・Bloom設定のままcameraをorbitし、輪郭と内部線の追従を確認した。
  `generated/neon_glow_revision/idle_attack_glow.mp4`へ通常の動画圧縮だけでまとめた。
  `motion_evidence.json`で全frameのclip、固定設定、画像存在、モデルhashを確認した。

## 検証

Development / Releaseの最終buildは両PASS、warning 0 / error 0。
`generated/neon_glow_revision/{development,release}_build_final.log`に保存。
既存Neon Model / Animation / mask数値・WIC / Trail / Projectile / Package fixture、
Bloom比較・接続・buildの11項目がPASS。新しい弾頭のproduction CPU recording testもPASS。
後者は初frame、静止弾、死亡・特殊弾除外、有限値、容量、Fence、collision radiusの独立性を検査した。

Bloomと既存Neon PipelineのWARP / RTX 4060 Laptop GPU 4 suiteがPASS。
MRT / Depth / Stencil / Alpha / Palette / SDF / Barycentrics / 既存Dissolve、
黒・DC・有限値・色比・gain一回・immutable CBの回帰を維持。
各Deviceで新しいHalo fixture 40条件もPASS。
最終結果とlog一覧は`generated/neon_glow_revision/tests/final_results.json`。

1px点・3px弾・1px線・6×9字形、256² / 257×129 sourceについて、
光源を除く1..48px帯の実HDR加算後の光量は旧Qualityに対してGlobal 7.314..10.656倍、
Showcase 10.984..19.076倍。同Thresholdでgain前の遠方filter光量は6.139..7.566倍。
これはlinear HDRのR成分をsource面積で積算した値であり、表示輝度・画質スコア・GPU時間ではない。
1px点の16..48px帯は加算Shaderの既存cutoff後には両設定とも0で、比率を作っていない。
原CSVと帯別結果は`tests/glow_band_summary.json`。
初回の新fixtureは未初期化の未使用fieldでFAILし、`array{}`で修正して正式再実行した。
本体コードやassertを緩めず、初回の失敗logも保持した。

同条件のゲーム・モデル正面・側面比較metadata検査は全PASS。
実機でQualityの広い色付きHalo、弾頭の淡い芯、Idle / Attackの追従を確認。
GPU時間は今回測定していない。全weapon・全密度の見た目検証も未実施。

ReleaseのDeveloper profile検査と、新規pristine提出Package作成・manifest / 起動file検査はPASS。
`generated/neon_glow_revision/pristine_final/`は最終buildの未起動の提出確認用、
`qa_final/`はruntime検査用の隔離コピー。遊んだQAコピーは提出しない。
QAコピーのRelease runtimeはPASS：引数なし・別cwdから起動し、title → 初回tutorial →
style / workshop / repair境界 → 3種類の新enemy → boss / result → title → 新run / tutorial skipを確認。
後半のforced clearとrepair fixtureは進行回帰用であり、難易度playtestではない。
別のTitle Demo回帰もPASS：4stage、454発光弾sample、35 real kill、39 dash、3 reward、
1 route、scene fade中の停止、操作可能な新ゲームへの遷移。
最終Developer-only UI範囲修正前のReleaseで実行し、描画設定・Rendererは最終buildと同じ。
logは`package_runtime_final.log`と`title_runtime.log`。
最終buildのpackage実行結果・exe一致・Preview素材除外・branch / HEAD / treeをまとめた
`generated/neon_glow_revision/final_verification.json`もPASS。
提出確認用pristineは最後まで未起動で、ゲーム・テストprocessは終了した。

## 今回追加調整したファイル

開始スナップショットの11fileを再調整し、7fileに新たな差分を追加
（既存2fileの変更、新規5file）。前段33file中22fileはbyte一致のまま。
既存の作業分と今回の追加分を合わせた終了treeはtracked 24 / untracked 16、計40file。

- `project/DirectX/engine/postEffect/Bloom.cpp`
- `project/game/debug/NeonSkinnedPreview.h`、`.cpp`
- `project/game/ui/NeonTextEffect.cpp`
- `project/game/player/actor/Bullet.h`
- `project/game/scene/GameScene.h`、`.cpp`
- `project/resources/configs/gameVisuals.json`
- `project/CG2_testPro.vcxproj`、`.filters`
- `project/tools/bloom_pipeline_tests.cpp`
- `project/tools/test_neon_bloom_comparison.py`、`test_neon_bloom_source_contract.py`
- 新規：`project/game/render/NeonProjectileRenderer.h`、`.cpp`
- 新規：`project/tools/neon_projectile_geometry_tests.cpp`、`test_neon_projectile_geometry.ps1`
- 新規：このdocument

前段からuntrackedのtest 3fileも今回再調整した11fileに含む。
renderer/UI入力範囲はSetParamsのclampと一致させた。無関係なコード整形は行っていない。
終了branch / HEADは開始時と同じ。commit / push / merge、branch変更は行っていない。

線配置やTexture抽出そのものの品質は今回の調整対象と区別する。
既存の細かな途切れた特徴線、小さい表示でのaliasingは残り、Bloomだけで修復できない。
多数の発光弾が重なる場面ではGlowが加算されるため、全密度・全weaponの画質保証ではない。

モデルSHA-256：`7fca4a77fdc60ab2c78a9907430744562626180125fa386eb74fb2ea15c2e518`。
ReleaseのDeveloper Preview無効と、提出PackageのGLB / line_masks限定除外を維持する。
