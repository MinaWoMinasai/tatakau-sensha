# Neon Bloom presentation polish

開始基準: `feature/neon-bloom-presentation-polish`、HEAD
`9d0a55a3a0581bc358d60bb46c3d60b017e55530`。開始時はtracked / untrackedともclean。
現在のローカルコードを基準に作業し、branch / HEADを維持する。
commit / push / mergeや既存変更の破棄は行わず、成果を作業ツリーに残す。
この文書はコードで確定した経路と、実機で検証する画質を区別する。

## 変更ファイル

以下は今回の変更33件。`generated/`の画像、CSV、ログ、manifestは検証成果物として別に保持する。

| 範囲 | 変更 / 追加したファイル |
| --- | --- |
| Engine / 定数 | `project/DirectX/engine/postEffect/Bloom.cpp`, `Bloom.h`, `BloomConstantBuffer.cpp`, `BloomConstantBuffer.h`, `ObjectPostEffect.cpp`, `ObjectPostEffect.h`; 同directoryに新規`BloomPyramid.cpp`, `BloomPyramid.h`; `project/DirectX/engine/struct/Struct.h` |
| Shader | `project/resources/shaders/Composite.PS.hlsl`, `ObjectPostBloomAdd.PS.hlsl`, `ObjectPostOutlineAdd.PS.hlsl`, `PostEffectCommon.hlsli`; 同directoryに新規`BloomPyramidCommon.hlsli`, `BloomPyramidExtract.PS.hlsl`, `BloomPyramidDownsample.PS.hlsl`, `BloomPyramidUpsample.PS.hlsl` |
| Developer比較 / ゲーム調整 | `project/game/debug/NeonSkinnedPreview.h`, `NeonSkinnedPreview.cpp`; `project/game/scene/IScene.h`, `Game.cpp`, `GameScene.h`, `GameScene.cpp`, `SceneManager.h`, `SceneManager.cpp`; `project/game/ui/NeonTextEffect.cpp` |
| VS登録 | `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| 新規回帰テスト | `project/tools/bloom_pipeline_tests.cpp`, `test_bloom_pipeline.ps1`, `test_neon_bloom_comparison.py`, `test_neon_bloom_source_contract.py` |
| 記録 | `docs/neon-bloom-presentation.md` |

Skinning / Neon本体Shader、モデル・mask、Animation / Skeleton / SkinCluster、
`NeonPreviewAnimations.h/.cpp`、`TankSubmissionPackage.ps1`は変更していない。
GLB SHA-256は`7fca4a77fdc60ab2c78a9907430744562626180125fa386eb74fb2ea15c2e518`。
Previewモデルexact fileと`line_masks/`だけを除外する既存package規則、Releaseでの
Preview / animation generator / capture controllerのビルド除外を維持する。

## 既存ビルド / Package / Profileの再確認方法

repository rootで実行する。現在のVS18 / v145に合わせた通常buildは以下。
`Debug` solutionは`Development`へ割り当てられており、独立した結果として数えない。

```powershell
MSBuild.exe project/CG2.sln /m /p:Configuration=Development /p:Platform=x64 /p:CG2DeveloperTools=true
MSBuild.exe project/CG2.sln /m /p:Configuration=Release /p:Platform=x64 /p:CG2DeveloperTools=false
./project/tools/test_tank_submission_packaging.ps1
./project/tools/test_developer_tools_profile.ps1
./project/tools/package_tank_submission.ps1 -OutputDirectory generated/bloom_presentation_tests/pristine_submission_<unique>
./project/tools/test_tank_submission.ps1 -PackageDirectory generated/bloom_presentation_tests/pristine_submission_<unique>
```

Package出力は存在しない新規directoryを指定する。fixture検査とprofileの引数なし検査は
ゲームやGPUを起動しない。profileは5構成のMSBuild property evaluationと最終exe / build.jsonの
SHA-256を検査するため、最終Development / Release build後に再実行する。
新規shaderとincludeは既存resource収録でpackageへ入る。

`test_tank_submission_runtime.ps1 -PackageDirectory <別途作成したQA用新規package>`は
HiddenでCG2を起動するGPU/runtime検査なので、CU / timestamp計測と同時実行しない。
pristine packageは実行せず保管し、QA copyの結果を未使用packageの証拠と取り違えない。
titleからtutorial、style、repair境界、敵、result、title、新runへの回帰であり、一部forced clearを
含むので通常操作による難易度検証とは区別する。
`test_title_demo.ps1 -Configuration Release`もHiddenの実CG2 / GPU起動を行う。
実traceが用意できた場合は`test_developer_tools_profile.ps1 -ReleaseTrace <trace.json>
-DevelopmentTrace <trace.json>`でUI / profilerの実行時counterも照合する。

## 調査: 光が太くなる経路

| 対象 | 開始時の経路・設定 | 読みづらさにつながる原因 |
| --- | --- | --- |
| Skinned line art / outline / dissolve edge | 3 MRTのScene HDRへ直接発光 → Global Bloom → exposure / tone mapping | Surface CoreとHaloに画面Bloomが加わる。高いHDR値へRGB各成分のACESをかけると色が白に寄る |
| 自機・敵・グリッド線 | NeonGridRendererの細いCoreとsoft strip → ObjectPostEffect → Scene HDR → Global Bloom | 発光済み画像をさらにぼかす二段経路。local thresholdは概ね0で、暗い面も光源に含まれる |
| 弾 | `Bullet::Draw()`は空。TrailManagerのリボンが表示源 → local Trail Post → Global Bloom | local capture scale .5、その半分でblurするため画面の1/4解像度。legacy intensity 2は三乗して8倍になる |
| 普通のモデルの共有発光 | .5 capture → shared Object Bloom → Scene HDR → Global Bloom | local blurは画面の1/4解像度。元の細部・輪郭とぼかしが重なりやすい |
| 文字・UI | sharp textとは別に、scale 1と.4の2つのObject Bloom → tone mapping後のbackbufferへ加算 | Global Bloomとは異なる経路。開始時の加算Shaderにはtone mappingがなく、HDRのままLDRへ足して白くclipし得る |

LegacyのGlobal Bloomはthreshold 1、intensity .35、exposure 1が初期値。
Showcaseはthreshold .65、intensity .75、exposure .75。
半解像度でまずbilinear sampleしてから線形輝度thresholdを適用し、Soft Kneeはない。
同じ半解像度で9-sample prefilterとH/V Gaussianを行う。Gaussianはradius 4、sigma 1.75相当を
隣接tapのbilinear共有で各5 fetchへまとめている。段階的なdownsample / upsampleはない。
元の1pixelの発光が先に平均されるとthreshold未満になり、小さい線や弾のBloomが欠け得る。
Gaussianの画面幅は処理解像度に依存し、低いcapture scaleほど粗く・太く見える。

LegacyはH blur、V blur、最終合成で各`intensity`を掛けるため、通常時の係数は
**intensity³**。全画面Gaussian / Box Blur演出の別用途はこの説明と区別する。
新しいlinear gainとの値をそのまま同一視しない。

## 採用した改善

推奨候補は**Quality Bloom**。比較用のLegacyを残し、軽量なLightも同じ式で段数だけ変える。
元のSceneやsharp emitterをぼかして置き換えず、独立したglow textureを加える。
弾については新方式/OFFのsource captureをfull resolutionへ改善し、Legacyだけ旧.5 captureを保持する。
開始時には弾のsharp source自体が低解像度capture内にしかなかったため、filterだけの変更では
細い芯を取り戻せない。両サイズを初期化時に確保し、mode切替時にGPU使用中のRTをresizeしない。
したがってゲームの弾のBefore / Afterには、filter改善に加えてこの意図的なsource解像度改善も含む。
Skinnedモデル比較は元のScene sourceを共用する。

1. 元のHDR 2×2各pixelへthreshold / soft kneeを先に適用し、その後平均して半解像度へ抽出。
   明るさはRGB最大値を使い、低い輝度係数のmagenta / cyanも発光源として扱う。
   kneeは`threshold × softKnee`。0のkneeでもゼロ除算しない。
2. Qualityは5段、Lightは3段。各downsampleは正規化した13-tap、upsampleは9-tapのtent。
   upsampleはfineとcoarseの`lerp(scatter)`であり、段数分を単純加算しない。
3. radiusは各段のtexel単位のsample footprint、scatterは広い段の寄与。
   強度は最終合成で`bloomGain`を**1回だけ**掛ける。Gainで半径が変わらない。
4. 任意のtone mode 2は、RGBの最大値へACESのshoulderを適用し、1つの係数でRGBを縮小する。
   この段階でネオンのRGB比を保つ。Legacy ACESとReinhardは比較のために残す。
   既定はtone mode 1を維持した。mode 2の色比保持はGPU readbackで確認したが、
   今回の同条件実画像比較にはtone1を使用し、mode2の実モデル画質は未評価。
5. tone mapping後の文字glowはHDR出力と分け、Quality / Light時のみ最大channelに基づく
   scalar圧縮で1 layer当たり.6以下へ抑える。Sharp textは従来の別drawを維持する。
   最終LDRに複数glowを加えるため、全状況でclipしない保証とは説明しない。

`BloomPyramid`は専用filter PSO / root constantsを持ち、既存のScene MRT / Depth / Stencilを
変更しない。filterはDepthなしのHDR単体RTへ描く。共有SrvManagerを利用し、filter PSOは同一Device内で共有する。
Legacy用RTと新しいpyramid RTは比較のため共存するので、実行pass削減とは別に常駐メモリが増える。
コンスタントバッファは同一フレームの複数filter / compositeが後から上書きされないsnapshotを使用する。

## OFF、分類ごとの調整、限界

OFFは**Bloomだけを無効**にする。AdditiveOnly captureで表示しているグリッド・弾のsharp sourceは残す。
BloomOnly captureは光を足さない。source色、モデル姿勢、輪郭幅やAlpha Cutoutを同時に変更しない。
Legacyは旧抽出 / 旧filter / intensity³を保持する。

初期Quality Gainはgrid .16、trail .4、particle .25、player / boss / exp / shared model .18、stage .15。
文字はinner .35、outer .28を基準に既存の各style intensityとの比でlinear調整する。
Showcaseだけは実画像比較後にgain .35、scatter .45を初期値とした。
`Recommended Bloom presentation`はQuality / threshold .65 / knee .5 / scatter .45 /
radius .75 / gain .35だけを設定し、exposure、tone mapping、線、姿勢、cameraは変更しない。
Globalおよびゲーム各分類の初期gain / scatterを、このShowcase用調整では変更しない。
各カテゴリのmode / knee / scatter / radius / gainは既存Post UIから調整・保存できる。
旧JSONに新しいkeyがない場合は初期値を維持する。

自機・敵・弾・文字には既存のlocal capture分類があるため、glowの寄与を別々に調整できる。
Skinned Body、内部線、Hull、dissolve edgeは**同じScene HDRへ出す**。その後のBloomで各成分を
別分類しているわけではない。各Shaderの既存Core / Halo / edge強度とGlobal Bloomを組み合わせる。
選択的なemission bufferやモデルの重ね描きは追加しない。
小さな線の元coverage、低解像度capture、UV共有、時間方向のちらつきはBloomだけでは復元できない。
新TAAやモデル・mask・animationの変更は行わない。

## 同条件のBefore / After

Development x64を通常起動し、GameSceneの**F3 → Neon Skinned Previewを開く → Enter Neon Character Showcase**。
View / CaptureでBloom OFF / Legacy / Quality / Lightを選べる。AnimationをPauseし、Camera orbitをOFFにする。
`Capture 3 Bloom modes (same pose)`はOFF / Legacy / Qualityのclean PNGとJSONを保存する。
固定されるものは姿勢、camera、exposure、線の設定、停止中のdissolve progress。
終了時には元のBloom modeとoutput diagnosticを戻す。比較のために線・Body・カメラを同時調整しない。
普段のMask / Animation / Dissolve / Normal比較は残す。

ゲーム内のF12 Debug Console → Post tabにはtemporaryなDeveloper Bloom comparisonがある。
Engine settingへ戻すと通常設定へ戻り、Title demoやReleaseへ比較操作を自動有効化しない。
PreviewモデルとmaskのSubmission Package除外は既存の狭い規則を維持する。

Post tabの`Freeze game for Bloom comparison`で通常ゲームの更新を止め、Drawだけを続けられる。
モデル、弾・trail、particle、follow-cameraを保持し、比較をslow motionのgray effectとして扱わない。
`Save game clean PNG + metadata`で各modeを手動保存する。
保存先はproject working directoryの`generated/neon_bloom_presentation/game_<timestamp>/`。
JSONには最終Compositeへ渡した実際のglobal params、local分類、camera、弾/敵位置、各source件数、
text style、validation状態を記録する。未停止の画像はlive gameplayと明記し、同じ姿勢のA/Bと説明しない。
停止比較でもGPU CSVは別に保存し、画像からGPU時間を推測しない。

既存`Measure GPU: 60 warmup + 300 frames`はD3D12 timestampとqueue frequency、既存Fenceを使う。
GPU Frame、Global Bloom / Post、Neon Character等の既存scopeを比較する。
同じcamera / pose / 出力解像度 / emission / exposureで、各modeを変えた後にwarmupを行う。
新しい細分scopeがないBody/Hull個別時間を推測しない。debug layer、GPU-based validation、GPU名もJSONへ残る。
GPU計測とPNG保存は競合させない。

## 検証記録

コード上の接続、CPU fixture、PSO生成、
実モデルの画質は別の根拠で評価し、synthetic PNGを実画面と説明しない。

| 項目 | 状態 / 保存先 |
| --- | --- |
| Development / Release build | PASS: 最終変更を含むx64両構成、MSBuild exit 0。`generated/bloom_development_build_final.log`, `generated/bloom_release_build_final.log` |
| Bloom / Neon GPU pipeline、CB配置、複数Draw | PASS: production Bloomと既存NeonをWARP / RTX 4060 Laptop GPUの両方で実行。DXC / reflection / readback / Fence / HDR-LDR / Depth・MRT・Stencil・Alpha・Mask・Geometry・Dissolve回帰。`generated/neon_bloom/tests/test_results.md/.json`。WARPもBarycentrics対応で該当SKIPなし |
| 既存mask / comparison / package / profile CPU回帰 | PASS: 9 suite。mask/WIC全12 quality画像、元の2mask、synthetic比較fixture、GPU CSV集計fixture、package fixture、5 profile組合せ。最終build後のprofileと実package起動は下の別項目。logs: `generated/bloom_presentation_tests/cpu_regression_results.json` |
| 3モード比較validatorと不正metadata/source guard fixture | PASS: 11 tests。`generated/bloom_presentation_tests/bloom_comparison_source_fixtures_final.log`。実画面の検証ではない |
| Animation / Model / CPU接続回帰 | PASS: Assimpモデル7submesh / 85 unique bones / 21,961 weighted vertices / 元Animation 0。生成2clip、Paletteによる18,955頂点変形、Idle loop / Attack / Pause・Seek・Restart・checkpoint / Dissolve凍結とReset。最終CPU接続25項目PASS。`generated/neon_bloom/tests/model_animation/summary.json`, `source_contract.json` |
| Skinned line art実機 | PASS: RTX 4060 Laptop GPU、1280×720、正面 / 側面90deg / 停止Dissolveの各OFF・Legacy・Qualityでmode以外metadata一致。Idle→Attack→Idleの240frameも実描画、選択frameを目視。詳細と未検証範囲は以下 |
| 弾 / 自機 / 敵 / 文字の実機比較 | PASS: 同じ停止world / camera、4自弾 / 敵1体のraw3PNG比較。`generated/bloom_presentation_tests/real_game_bloom_comparison.json`。敵弾・particleは0件のため画質未検証 |
| 同条件GPU time | PASS: 実GPU timestamp、60warmup + 300frame、4mode。以下の表とraw CSV参照。Release・ゲーム分類別の計測は未実施 |
| モデル・mask・animation保存 | PASS: GLB / mask PNG計15binaryはHEAD byte一致。Animation / Skeleton / SkinCluster / Preview生成8ソース差分なし。SHA-256は以下 |
| 最終buildのDeveloper profile / 実Release package / 起動 | PASS: 最終exe hash / 5profile組合せ / 実Dev・ReleaseのUI counters。pristine252file / 34,481,005byteの作成・再監査、別QAでRelease walkthrough、Title Demo 4stageと新規開始。全10段階exit0。`generated/bloom_presentation_tests/run_20261004_081636_fffdc19c/manifest.json` |

実画像はエンジンがImGuiより前のbackbufferを保存した原本であり、見栄えを変える後加工はしていない。
PNG比較は停止した条件の証拠で、動き中の品質保証やGPU時間の根拠とは区別する。

最終Release QAは10 PASS / 0 FAIL / 0 SKIP、全exit0。
Release提出runtimeとRelease title demoの実counterはdeveloper_tools / ImGui / runtime profilerが全て0、
Development起動は全て1。証拠のtrace pathとexe / profile / source-script hashは
`generated/bloom_presentation_tests/run_20261004_081636_fffdc19c/final_integrity.json/.md`。
pristine packageは未起動のまま最終再監査を通過し、
`resources/models/neon_hologram/AvatarSample_B.glb`と`resources/models/neon_hologram/line_masks/`は存在しない。
別QAコピーで提出フロー2周、title demo4stage / 466発射sample / 37実kill / 34dash / 新規遠征復帰を検証した。
forced clearを含む提出フローはUX回帰であり、通常難易度のplaytestとは区別する。
全GPU / CU検証を終えてCG2は終了済み。ユーザーの進行データや原project/configsはQAで変更していない。

正面の実画像: `project/generated/neon_directional_dissolve/showcase_1791067820668/bloom_comparison_1791067931793/`。
PNGそのものを後加工せず確認した。OFFでは細い芯と暗い面が残り、Qualityでは芯を保持したまま
外周・口・髪飾り・目の周囲へ小さなmagenta haloが加わる。
Legacyでも外周のglowは存在するため、Bloomの有無だけを改善の根拠とはしない。
Qualityの前髪や服に残る途切れ・細かなtexture線は、元のcoverage / 自動抽出に由来する制約で、
この比較だけで解消したとは評価しない。ここでは固定した正面姿勢の見え方を評価し、動き中のちらつきは未評価。
これらの実画像は最後のUI preset追加・JSON gain上限一致・通常BloomAddのUV offsetを0にする修正より前の
Development exeで取得した。撮影条件ではoffset両値0・gain4以下であり、後の修正による出力差はない。
撮影時のgain .352 / scatter .455に近い値を最終Showcase既定値 .35 / .45へ採用した。

側面90degの実画像: `project/generated/neon_directional_dissolve/showcase_1791067820668/bloom_comparison_1791068378316/`。
正面と同じ停止Idle時刻・Bloom・exposureで、側面の3mode間もmode以外の全metadataが一致した。
manifest: `generated/bloom_presentation_tests/showcase_side_bloom_comparison_actual.json`。
OFFで芯・暗面が保たれ、Qualityで額・鼻先・顎・髪束・肩縁へ小haloが付く。
Legacyにもglowがあり、Qualityの服内部の縁や髪飾りが読みやすいという評価はこの同条件の主観比較。
前髪の段差・線の途切れは残り、side画像だけで正面の目・口や動きのちらつきを評価しない。

停止Dissolveの実画像: `project/generated/neon_directional_dissolve/showcase_1791067820668/bloom_comparison_1791068504420/`。
Full body / Side、progress .394停止。3modeのmode以外全metadataは凍結joint poseや
scan boundsを含めて完全一致した。
manifest: `generated/bloom_presentation_tests/showcase_dissolve_bloom_comparison_actual.json`。
消えた上半身の輪郭は残って見えず、OFFでも切断境界の明るい芯が存在する。
Qualityで境界・髪先・脚・靴の周囲へglowを足し、残る面を大きな光の塗りつぶしへ変えない。
Legacyも境界を発光させ、この条件の切断線では差が小さい。境界の白寄りの箇所はOFFにも存在するため、
Quality filterだけでHDR / tone1の白寄りを解消したとは評価しない。
静止progress1点の証拠として記録し、連続Dissolveのちらつき、reset、前景遮蔽、Depth / Stencilを
この画像だけからPASSと扱わない。

Idle / Attack / orbit実モデルsequence: `project/generated/neon_directional_dissolve/showcase_1791067820668/sequence_1791068567589/`。
240枚の連続raw PNG / JSON（1280×720、60fps）を検査し、Qualityは全frameで持続、
Animation / camera / sequenceFrame以外の全metadataは一致した。
Idle frame0〜74 → Attack frame75〜182 → Idle frame183〜239へ戻る。
原本を維持し、CPU ffmpeg / libx264でfilterなしの4秒動画
`generated/neon_bloom/real_model_idle_attack.mp4`を作成した。codecと出力色空間変換は非可逆なので、
raw PNGを画質確認の原本として扱う。
frame0 / 90 / 120 / 239を直接確認し、腕を押し出すAttackに袖の外周・服の内部線が付いている。
最終frameは約80degの側面で、大きな肩の破綻や極端な腕のめり込みは選択frameに見えない。
全身表示の顔の細線・途切れは残る。orbit付きsequenceは固定cameraの3mode A/Bではなく、
全動画目視・ちらつき定量評価やNormal比較をこの証拠だけでPASSと扱わない。
検証: `generated/bloom_presentation_tests/model_idle_attack_sequence_verified.json`。

通常ゲームの停止比較: `project/generated/neon_bloom_presentation/game_1791068938890/{000,001,002}_game_bloom.*`。
OFF / Legacy / Quality、1280×720、exposure1 / tone1、TAA / jitter OFF、AO / SSR ON。
manual freezeでcamera、actor / 弾位置、local分類、text style、mode以外のpost条件が一致した。
manifest: `generated/bloom_presentation_tests/real_game_bloom_comparison.json`。
raw PNGで、OFFは芯とUIを維持し、Legacyの大きく丸い緑の弾glowに対してQualityは細い芯と
小haloで方向が読める。自機のシアン角型輪郭と敵の淡緑円も周辺glowが狭く、黒背景を保つ。
上部の黄色の目標textはQualityで輪郭が残り、Legacyより周囲glowが小さい。
下部チュートリアル、HP、装備説明も判読できるが、全text styleへ同じNeonTextEffectが
適用されるとは説明しない。弾についてはfull-resolution source改善とlinear gainも含む比較で、
filterだけの効果とは扱わない。敵1・自弾4・敵弾0・particle0の停止1条件であり、
敵弾 / particleや移動中のちらつきはこのcaseでは未検証。画像の後加工・GPU時間推定は行っていない。

## 同条件のGPU実測

RTX 4060 Laptop GPU、Development、1280×720、render scale 1、Debug Layer ON、GPU-based validation OFF。
正面 / Preview_Idle 1.749986秒停止 / orbit OFF、gain .352 / scatter .455 / radius .75 /
exposure .75 / tone1で4modeを直列計測した。各60frame warmup後300frame、
全条件300件の`gpu_valid=1`、queue timestamp frequency 1,000,000,000 Hz。
4条件のJSON全体から`bloom.mode`だけを除いた値が完全一致した。
CPUでraw CSVから再集計し、既存summaryとの全scope統計・settingsの一致も確認した。

| Mode | GPU frame 平均 / P95 ms | Global Bloom / Post 平均 / P95 ms | Neon Character 平均 / P95 ms |
| --- | --- | --- | --- |
| OFF | 2.764 / 2.817 | .585 / .586 | 1.219 / 1.302 |
| Legacy | 3.113 / 3.173 | .933 / .950 | 1.217 / 1.299 |
| Quality | 3.155 / 3.213 | .969 / .983 | 1.212 / 1.286 |
| Light | 3.091 / 3.143 | .915 / .929 | 1.215 / 1.291 |

D3D12 timestampを既存Fence完了後に読み、P95はnearest rank。GPU frameは親scopeであり、
各scopeと加算しない。OFFのGlobal Bloom / Post時間はBloom以外の後処理を含む。
この1回の直列runではQualityのGlobal Bloom / Post平均はLegacyより約+.036msであり、
性能改善とは説明しない。Lightは約-.018msだが、温度・クロックを統制した有意差ではなく、
Release性能保証へ外挿しない。Body / Hull個別やゲーム分類別のGPU時間は測定していない。

元CSV/JSON: `generated/neon_bloom/real_gpu/{off,legacy,quality,light}.{csv,json}`。
既存集計: `generated/neon_bloom/real_gpu/summary.json`。
再計算・non-mode条件一致検証: `generated/bloom_presentation_tests/real_gpu_comparison_verified.json`。

CPU比較validator実行例: `python project/tools/test_neon_bloom_comparison.py <比較folder> --output <manifest.json>`。
復帰の検証には`--before-json`と`--after-json`を両方指定し、JSON全体とraw PNGのbyte一致を確認する。
引数なしはsource / synthetic fixture回帰のみであり、実モデルを描画した証拠にはならない。
手動の停止ゲーム比較には`--game-json <OFF.json> <Legacy.json> <Quality.json>`を使う。
mode以外の記録条件、source位置、実際のTAA/jitter無効、PNG解像度を検査する。

## 終了状態と残課題

開始 / 終了ともbranchは`feature/neon-bloom-presentation-polish`、HEADは
`9d0a55a3a0581bc358d60bb46c3d60b017e55530`。
開始cleanから、変更tracked22件 / 新規untracked11件の計33件を作業ツリーに残した。
branch作成・切替、pull / reset / clean / stash / rebase、commit / push / mergeは行っていない。
状態とGLB hashの機械可読記録は`generated/bloom_presentation_tests/final_repository_state.json`。
祖先directoryを含め適用されるAGENTS.mdはなかった。

GLBの開始 / 終了SHA-256はともに
`7FCA4A77FDC60AB2C78A9907430744562626180125FA386EB74FB2EA15C2E518`。
mask画像もHEAD byte一致。モデル・UV・Animation・Skinningの描画方式は変更していない。
既存のSDF / coverage / Core-Halo / Geometry / Dissolveは回帰対象として保持し、今回それらを新設していない。

実装が動作したことと画質を改善できた範囲は区別する。今回の推奨はQualityであり、
弾の丸いにじみ、輪郭を覆う過剰なglow、暗部の霞みを減らし、実モデルの細い芯へ小さな色haloを足せた。
Legacy / OFFへ戻せる比較導線とLightも残した。
次の点は未解決、または今回の画面確認範囲外。

- 顔・前髪の元coverageの途切れや段差、全身表示で消える細線は残る。Bloomで線そのものを再建していない。
- Dissolve境界の芯の白寄りはOFFでも存在する。tone1のRGB別圧縮と元emissiveの寄与が残り、
  新Bloomだけで全白飛びを解消したとは評価しない。任意tone2はGPU検証済み、実モデルの画質比較は未実施。
- 移動中の弾、敵弾・particle、全text style、連続Dissolveの全過程、全動画の目視、ちらつきの定量評価は未実施。
- Body / Hull / 内部線 / Dissolve edgeのBloom寄与を個別に分ける追加bufferは作っていない。
- QualityとLegacyのRTを同時保持するため常駐メモリが増える。Release GPU時間・カテゴリ個別の時間は未計測。
- 比較画像のQualityとLegacyには意図的なgain契約差がある。弾にはsource解像度差もある。
  同じgain数値や全ての明るさを揃えたfilterだけの優劣として説明しない。

## 2026-10-06: ゲーム内のほかの発光を弾の見え方へ揃える

上記の初回改善後に、弾以外のにじみが弱く見えるというフィードバックへの追加調整。
開始HEADは`38eef3e`。今回の対象は通常ゲームのカテゴリ設定と共有モデルcaptureであり、
弾の描画geometry、弾のBloom設定、Global Bloom、exposure、tone mappingは変更していない。

`GameScene.cpp`の`ApplyGameplayNeonBloomPreset`で、player / boss / exp enemy / stage /
grid / particle / shared modelを同じQuality filterの形へ揃える。
threshold 0、soft knee .5、scatter .55、radius 1を使用する。
grid（自機・敵・ブロック線・アウトライン三角粒子を含む）と共有モデルのgainは1.2。
小さくフェードするモデル粒子は、HDR projectile headより元のエネルギーが低いためgain 2。
旧gainはgrid .48、particle .65、shared model .3だった。
個別player / boss / exp enemy / stage設定もgain 1.2とするが、通常のモデル発光は既存の
共有capture、ネオン線は既存のgrid captureを通る。個別設定がすべて別passで実行されるとは説明しない。
既存のカテゴリ有効/無効と、JSONに明示してある設定を読み込む仕組みは維持する。

共有モデルも弾と同じく`Initialize(..., .5f, 1.0f)`とし、Quality / Light / OFFのsourceを
full resolutionへ変更する。Legacyは元の半解像度を使う。sourceとglowは別のままで、
通常のsharpモデルの描画や粒子の寿命・フェードは変更しない。
追加RTは初期化時に確保する。常駐メモリとfilterの処理画素数は増えるが、今回は時間を計測していない。

検証用scenario captureにも既存のlocalCategories / visualAppearance / sourceResolution /
sourceCountsを収録する。`particles`はCPUリスト件数なので、GPU更新の有無とdraw readinessを
別に記録する。GPU更新中のCPU件数0を「画面に粒子なし」と判断してはならない。
draw readinessは描画後の状態であり、GPU粒子の`ExecuteIndirect`後にはfalseへ戻る。
この値も描画済み粒子の有無やalive件数として扱わない。

- Development / Release x64の最終ビルドPASS。ログは
  `generated/bloom_gameplay_restore/{Development,Release}_final_verified.log`。
  制限内の初回buildはPDB manager C1902で失敗し、制限外の通常buildで両構成を確認した。
- 既存production Bloom pipeline検査はWARP / RTX 4060 Laptop GPUの両方でPASS。
  `generated/bloom_gameplay_restore/pipeline/{warp,hardware}/`にHDR readback / halo測定を保持。
  immutable source、細い発光の抽出、色、Gain一回適用、OFF / Legacy / Quality / Lightを検査した。
- CPU source contract 33項目と5通りのDeveloper build profile検査PASS。
- 射撃 / 近接、seed 20261006、各480 updateで旧ローカル強度と新既定値を実描画。
  60 / 120 / 210 / 360 frameの各4枚を保存し、自機・敵・ブロック・三角のモデル粒子を目視。
  カメラ、ゲーム状態（descriptor cache件数を除く）、emitter設定、Global Post、弾の設定を照合。
  生画像とmetadataは`generated/bloom_gameplay_restore/runtime-project/generated/`、
  照合結果は`generated/bloom_gameplay_restore/comparison.json`。

この比較は同じ最終exeで旧gain / scatter / radiusを再現した設定比較であり、両方とも共有モデルは
新しいfull resolution captureを使う。初回変更前exeとの完全なBefore / Afterではない。
独立起動間のGPU粒子の乱数位置までは固定していない。prototype初期化がrender mode 1を設定するため、
試した`*_model`出力もmode 1であり、通常モデルmode 0の画質検証からは除外した。
通常モデルの新captureはコード接続と既存GPU filter検査までで、mode 0の実画面は未確認。
検証はコピーしたresourcesとconfigで行い、原本の設定・ユーザー進行データは変更していない。


## 2026-10-06: gameplay contour and defeat-particle source polish

弾の頭はpale HDR core、彩度のある肩、広い低alpha haloを別の太さで描く。
以前の機体・敵・ブロック輪郭は単一の色付きstripで、角も独立した線の重なりだった。
ブルームだけを増やすと芯と色の帯を作れないため、今回は発光源の形・断面を改善した。
[Retrograde Arenaの公式Steam画面](https://store.steampowered.com/app/1055210/Retrograde_Arena/)で見える
明るい芯、色付きの周辺、暗い床との対比を参考にした。これは画面からの観察であり、
同ゲームの内部実装を確認したものではない。

- NeonGridRendererのopt-in contourは、細いpale芯・colored shoulder・低alpha haloの連続断面を作る。
  尖った角は外側round join/内側shared miter、滑らかな円弧は共有断面とし、独立線の丸い端は
  本体と重ならない。Styleは呼出し引数なので既存の床格子・弾・攻撃予告へ漏れない。
  実機の初回比較では白い帯が広がりすぎたため、芯2.8/肩1.8と早めの色遷移へ調整した。
- 自機・boss・droneの本体と砲身、基本敵の2D/3D枠線、遠征combat rolesの本体/砲身/模様、
  可視ブロック面の分割線へ適用。閉じた本体は一つのpolygonとして予約する。
  stageのdepth bias/可視面判定を保持し、分割線はcaplessの42頂点で描く。
- Outline triangleはmain輪郭を一つのcontourへ置換し、従来の三重描画を外した。
  外光幅/芯幅の既存調整値を継続し、薄いtrail copiesは従来の軽量stripを使う。
- 既定のLegacyModel particleはhard-rasterized OBJ stripから専用quad/三角距離フィールドへ変更。
  距離の画面微分を使って縁を滑らかにする。
  [HLSL fwidth公式仕様](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-fwidth)
  のddx/ddy合計をpixel幅の積分目安として用いている。
  neonRadiance opt-inだけがHDR芯4.5/肩2.8/haloを描き、HDR flashのenergy上限は1.0。
  通常材質のModelParticle shader分岐を保持し、一般emitterのRGB/alphaを増幅しない。
- Neon粒子のGPU alphaはEaseOutでendAlphaへ補間した後にraw alphaも掛けていた。
  半寿命で0.125となる旧方式を、CPU/GPU共通の単一smooth fade（半寿命約0.583）へ変更。
  縮小もraw lifetimeに合わせる。ネオンだけに適用し、寿命自体は変更しない。
- GPU batchが既定scalar scale=1を小さいauthored vector scaleより優先する問題をneonだけ修正。
  小破片の指定サイズを保つ。三角本体半径を維持し、halo用のquad余白は見た目だけに使う。
- Material byte28、ParticleGPU128byte、EmitterRequest128byte、RenderData208byteのlayoutを保持。
  既存reserved slotとnormalに使われないhomogeneous elementでstyleを伝える。

頂点容量は786,432（27MiB/renderer、旧196,608から20.25MiB増）とした。
random spawnの45体制限はauthored roomには当てはまらず、128体を考慮する必要があった。
CPU stressでは128 support＋各spokes6本/warning70線、115 blocks/2300分割線、床と18 local grids、
自機、60 main triangle bursts＋各trail3個を同時投入し、765,192頂点を完全生成した。
任意の無制限burstは保証せず、容量を超えた場合は完全primitive単位で省略してoverflowを防ぐ。

実機確認にはgenerated/neon_emitter_polish/capture_death_burst.ps1を使用する。
固定入力の通常射撃・衝突でHP1のShooterを3体倒し、高HPの遠方1体で部屋clearを防ぐ。
実際のcollision frame73/121/169、翌updateでのactor removal74/122/170をsnapshotから確認する。
41..220の180連続frameはcomparison freezeなしで保存し、PNG名ではなくsimulation frameで比較する。
変更前exeとシェーダーをbaseline-bin/baseline-resourcesに保存し、最終実装と比較する。
元resources/configs・進行データを変更せず、撮影はresourcesのコピーで行う。

prototypeの通常設定は -PrototypeDefaults を使用する。authored=trueを付けるとJSON全内容が
prototype presetへ再適用されるため、通常設定の比較と混同しない。通常モデルmode0の検証だけは
コピーしたvisual/post JSONへexpeditionAuthored:trueを付け、actual mode0をmetadataでassertする。
これにより前項で未確認だったmode0を、今回のfixtureでは実際に起動できる。

検査スクリプト: project/tools/test_neon_contour_geometry.ps1（実production CPU geometryの角/端coverage、
finite/容量/room stress）、test_neon_particle_shader_contract.ps1（7 DXC shaders -WX/23 ABI reflection checks）、
test_neon_projectile_geometry.ps1（既存弾の芯・肩・haloとbatch/collider契約）、
test_neon_bloom_source_contract.py（既存33 source契約）。
独立レビューで発見した128体の容量回帰は上記増設で解消した。

最終検証結果: Development/Release x64ビルドPASS（Development_final.log / Release_final.log）。
RTX 4060 Laptop GPUの最終通常neon runは240 updates/180連続PNG/3撃破を完了。
comparison_before_after_legacy_neon_final.jsonで240 snapshotsと180 camera/bloom/appearance/clock記録に
意図しない差分なしを確認した。独立起動間のGPU乱数配置は完全同一とは主張しない。
9 combat rolesはD3D12 debug layer/GPU-based validation付きで120 updates/5実画像を完了し目視確認。
通常model mode0は32実画像で確認し、Outline mode0の粒子も180連続画像/peak34 trianglesで確認。
各modeはmetadataの実値をassertした。原本config/進行データのhash保護も通過。
結果一覧はgenerated/neon_emitter_polish/verification-summary.json、raw PNGとmetadataは各run内に保存。
表示品質をraw画像で確認したが、今回の変更によるframe time/FPSの差は計測していない。

## RetroGradeの動画を参考にした演出構成（2026-10-06）

提供された2本の動画を比較し、撃破時の発光の順番、破片の形と減速、通常時と撃破時の明暗差を変更した。
自機と弾の追いやすさ、ダッシュの移動・残像の勢い、攻撃予告と敵の種類を表す記号を保つ方針とした。
参照作品の内部実装を推定して再現するものではなく、動画で観察できた演出構成をこのエンジンで組み直している。

- 撃破直後の単一の丸い発光点から、0.045秒後に丸い閃光・拡大する輪・破片を発生させる。
  従来の巨大な三角形のCharge/Flashを複数重ねる演出を置換した。Flashは0.14秒、輪は0.42秒。
  HP減算・撃破通知・報酬・敵の除去タイミングは演出待ちにしない。
- 非対称の四角い破片、細長い三角片、進行方向に沿う火花を混ぜる。破片は角速度とサイズを変え、
  drag 2.8で減速しながら0.56〜1.12秒残す。alphaを初期28%の期間保持してからsmooth fadeする。
  撃破対象の色を主色に使い、少数の副色を混ぜる。火花の寿命は0.12〜0.23秒と短くする。
- NeonParticleShapeをCPU/OutlineとGPUへ渡す。GPUは既存padding2/byte124を使い、
  128-byte Particle/Emitter、208-byte RenderDataのABIを維持する。
  新しい輪のSDF距離はworld scaleを考慮し、輪の拡大で線が太いドーナツにならないようにした。
  通常の三角形のHDR profileは従来の芯4.5/肩2.8を保持し、新しい破片は色を残す芯3.4/肩2.2を使う。
- 本体輪郭はcore ratio 0.14、white mix 0.28とし、通常時の白い面積とhaloを抑える。
  壁の外縁と内部の分割線を区別し、分割線は細く色を残す。側面の線は正面より暗くする。
- 自機と2D表示の敵には暗い面と光の向きで明暗が変わるbevelを描く。
  戦闘役の面は既存の本体polygonから作り、形・当たり判定を合わせる。
  描画順を床→面→輪郭/内部記号/攻撃演出とし、面が射撃記号を隠す問題を解消した。
  同じupload内の範囲を分けて描き、未実行drawが参照するvertex bufferをBeginFrameで上書きしない。
- ダッシュ残像の本体・砲身にも残像のalphaを渡す。保存されたoutlineColorがalphaを上書きする問題を修正し、
  古い残像はlife ratioの二乗で薄くする。ダッシュ速度・持続時間・弾の形状は変更していない。
- 粒子の乱数をParticleManagerが所有するgeneratorへ分離した。
  破片数や形状の調整がカメラの揺れ・敵の弾の散らばりの乱数消費を変えないようにする。
  旧実装と完全に同じ乱数列にはならないため、変更前後の厳密な全状態一致は主張しない。

CPU geometry suiteでは方向付きbevelのcoverage・容量ガード、破片silhouetteの余白・finite geometry、
fadeの単調性・保持期間を追加検査した。128 supportの面を含むstressは772,104/786,432 verticesで完全生成。
DXCの7 shaderを-WXでコンパイルし、shape用のreserved slotを含む27 ABI reflection checksを通過した。
既存の弾のgeometry testと33 source-contract checksも通過。Development/Release x64をビルドした。

最終実機記録・検証一覧はgenerated/retrograde_presentation/verification-summary.jsonに保存する。
通常射撃による3回の撃破、LegacyModel/Outline、通常model表示、自機の移動とダッシュ、9種類の戦闘役を確認する。
連続撮影は41..220の180枚、通常modelは32枚。原本config・進行データのhashを撮影後も照合する。
GPU-based validationを有効にした9種類の敵のgalleryでは、初期配置と通常AI更新後の5枚を確認する。
旧版との厳密比較の失敗もcomparison_final_strict.jsonに残し、乱数分離に伴う差を個別に検査する。
before/after動画はraw PNGから生成し、見やすい1/2速度にする。色や発光を後から加工しない。
この実機確認は固定入力のfixtureであり、FPSの前後差や全ゲーム進行の完走を証明するものではない。

最終検証はPASS。通常/Outline各240更新・180画像、model240更新・32画像、dash240更新・180画像を保存した。
3種類の撃破runはいずれも除去frame74/122/170で一致し、dash入力85/170後の移動ピークを両方確認した。
9役galleryはGPU-based validation有効で120更新・5画像を保存した。全runで原本config hashは一致した。
厳密な旧版比較は乱数分離により全一致ではなく、記録されたcamera位置差の最大は0.041874 world units、
敵弾の存在数が異なるsnapshotは179/226/227の3frameだった。これら以外のsnapshot差はない。
比較動画はgenerated/retrograde_presentation/before_after_half_speed.mp4（左BEFORE、右AFTER、6秒・1/2速度）。
Native recorderは既存の連続画像を上書きしないため、最終撮影は新しいrun directoryで行った。
