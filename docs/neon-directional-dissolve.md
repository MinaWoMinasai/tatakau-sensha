# Neon directional dissolve preview

Developer build only. Start Development x64 in Visual Studio (Ctrl+F5), enter a
GameScene, and open Developer Tools with F3 → **Neon Skinned Previewを開く** →
**Enter Neon Character Showcase**. The existing F12 console is another entry;
the live verification used F3. No PowerShell is required for preview controls.
Keep Mesh Geometry Lines OFF. Select V2 edited coverage
and Separate Core/Halo explicitly; the original comparison candidates remain available.

## Frozen pose and scan plane

Trigger saves AnimationPlaybackState including the current blend, then pauses both
playback and blend. It scans existing vertices, four influences and the updated
Palette once to bound the **whole current skinned model**. Rendering continues to use
the existing GPU Skinning VS. No GLB, animation clip, skeleton or weight is edited.

Positions use row vectors (`pWorld = pModel * World`). The trigger camera's right-minus-up
axis defines the upper-left to lower-right scan. A plane coefficient is a covector:
`nModel = World_linear * nWorld`, normalized before measuring projection bounds.
World translation cancels in the range. This remains correct for nonuniform scale;
it is tested with scale, shear and reflection. The plane, pose and seed stay fixed
when the camera subsequently moves. [HLSL mul conventions](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-mul).

`skinnedModelPosition : TEXCOORD1` is assigned before World and before Outline's
screen-space expansion. Body, Geometry and Outline use `NeonDissolve.hlsli` for one
stable trilinear value-noise implementation. No screen-position or time-dependent
random field is used.

## Field, endpoints and edge

The field is `dot(p,n) + (noise-.5)*strength`. Noise strength is its full model-space
distance range, scale is lattice cells per model-space unit, and the seed is fixed.
The scan threshold spans `[scanMin-strength/2, scanMax+strength/2]`; zero noise scale
also disables its strength. Increasing progress cannot resurrect a surface point.

Disabled and progress 0 preserve the original result without edge glow. Progress 1
discards Body and Hull completely. Partial progress clips negative signed distance,
then adds a narrow white-pink core and colored surface halo on the surviving side.
Width is measured along the normalized model-space plane, not a constant pixel width.
Screen derivatives soften the emission boundary. Width/intensity 0 disables edge
emission safely. No cutting polygons, particles or moving fragments are generated.
The final defaults are width 0.008 model units and HDR intensity 4. Initial live
captures with width 0.018/intensity 8 showed a wider saturated band and are retained
as an initial trial, not the final appearance.

All required UV/SDF/Barycentric/edge derivatives precede Alpha and dissolve clips.
Original Alpha Cutout remains independent. Discarded fragments do not write Depth
or Stencil. The pre/post model clear still removes reserved Stencil bit `0x80` and
preserves the other bits; Normal/Material MRT encoding and the Hull PSO are unchanged.
[HLSL clip](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-clip).

## Constant storage

NeonDissolveParams is 80 bytes appended at offset 208. NeonSkinnedParams is 288 bytes;
GpuConstants is 320 bytes (camera at 288, viewport at 304). Each Draw owns a 512-byte
resource, computed by rounding sizeof(GpuConstants) to the 256-byte CB alignment.
The existing b1 binding and seven-root-parameter signature remain unchanged.
Reflection tests verify the actual shader offsets; allocation and multiple-Draw tests
verify the actual resource size and immutable snapshots, not just static assertions.
[DXC buffer packing](https://github.com/microsoft/DirectXShaderCompiler/wiki/Buffer-Packing).

## State and comparisons

NeonDissolvePreviewController owns CPU preview state, with no owning pointer into the
model. Restart reuses the initial pose, plane and seed. Reset restores original time,
blend and playing/paused state; its first frame does not advance animation. Preview
disable, Showcase exit and destruction reset the controller. While active, animation,
seek, transform and Normal mode controls are locked. Dissolve has its own Play/Pause,
speed, progress, wait and duration; game pause policy remains intact.

Quality comparison, recording, dissolve autoplay and GPU timing are mutually restricted.
A paused dissolve can be timed or captured. Eight-condition capture holds pose, camera
and exposure fixed: disabled, 0, .25, .5, .75, 1, .5 without noise and .5 without edge.
The eight-second engine frame sequence includes Idle Trigger, camera orbit, Reset,
Attack Trigger and Reset. Settings JSON records the frozen playback/pose, start World
and camera, direction/range, noise, progress, edge settings and quality asset revision.

## Local hair candidate

V3 local-hair-r1 preserves V1/V2 and the V2 face images. It only tapers the final part
of one front-hair flow curve, including the UV-shared portion beside the cheek. It is
a separate candidate; no global exposure or face core adjustment is used to hide it.
See the quality mask README and generated UV lookup for the exact authoring conditions.

## Validation record

Build, numerical, GPU pipeline, model/animation, LinearData, package and real-render
results are recorded after verification in generated/neon_dissolve and the final report.
PSO creation alone is not visual validation. GPU timings compare the same frozen pose,
camera and emission. Progress 1 still submits the model passes and clips in the shader;
it is not reported as a draw-skipping optimization.

## 実装・検証記録（2026-10-04）

開始時のrootは `C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2`、branchは
`feature/neon-directional-dissolve`、HEADは
`138fe3ef5b4aee16ec9c0d76cb754b6d8a2ee8f7`。tracked / untrackedともcleanでした。
リポジトリと祖先ディレクトリにAGENTS.mdはありませんでした。現在のローカルコードを
基準に作業し、branch / HEADは維持、commit / push / merge等のGit変更操作はしていません。
変更は作業ツリーに残します。環境はWindows / Visual Studio 18 Community / x64 /
PowerShell 7.6.6です。画面操作はComputer Use、画面保存は既存エンジンの未加工captureです。
終了時も同じbranch / HEADで、tracked変更22個、untracked追加13個（合計35個）です。

変更・追加ファイルは次の35個です。生成画像・CSV・動画・ログはignoredのgenerated配下です。

| 責務 | 変更・追加ファイル（repo rootから） |
| --- | --- |
| 描画 / 共通判定 | `project/DirectX/engine/3d/neon/NeonDissolve.h` (追加), `NeonSkinnedRenderer.h`, `NeonSkinnedRenderer.cpp` |
| Preview / 状態 | `project/game/debug/NeonDissolvePreviewController.h`, `.cpp` (追加), `NeonSkinnedPreview.h`, `.cpp` |
| Shader | `project/resources/shaders/NeonDissolve.hlsli` (追加), `NeonSkinned.hlsli`, `NeonSkinnedSkinning.hlsli`, `NeonSkinnedBody.hlsli`, `NeonSkinnedOutline.PS.hlsl` |
| V3候補 | `project/resources/models/neon_hologram/line_masks/quality/README.md`, `authoring.json`, `bindings.json`; `bangs_v3_coverage.png`, `bangs_v3_sdf.png`, `face_v3_coverage.png`, `face_v3_sdf.png` (追加) |
| 生成 / データ回帰 | `project/tools/generate_neon_quality_masks.py`, `test_neon_quality_masks.py`, `neon_quality_mask_data_tests.cpp` |
| CPU / 実モデル / GPU | `project/tools/neon_dissolve_tests.cpp`, `test_neon_dissolve.ps1` (追加), `neon_preview_animation_tests.cpp`, `test_neon_preview_animations.ps1`, `neon_skinned_pipeline_tests.cpp` |
| 比較 / package | `project/tools/test_neon_dissolve_comparison.py`, `test_neon_dissolve_comparison_fixtures.py` (追加), `test_neon_showcase_comparison.py`, `test_neon_showcase_comparison_fixtures.py`, `test_tank_submission_packaging.ps1` |
| 登録 / 記録 | `project/CG2_testPro.vcxproj`, `.filters`, `docs/neon-directional-dissolve.md` (追加) |

元GLBの前後SHA-256は同じ
`7FCA4A77FDC60AB2C78A9907430744562626180125FA386EB74FB2EA15C2E518`。
既存V1/V2の全8 PNGとversion原稿を保持し、V3の顔2 PNGもV2とbyte同一です。
Skeleton / Animation / SkinCluster / Object3dの既存通常・Shadow処理は編集していません。
ReleaseはPreview controllerをビルド対象から除外し、既存のPreviewモデル・line_masksだけを
除外するpackage規則を維持しています。

髪調整は1候補で区切りました。左flowの末尾t=.72〜1だけ幅1.8→1.2 atlas texels、
opacity .65→.25へ滑らかに減衰させます。Hair_01 / UV0の実三角形とcurveの対応を調べ、
頬側への対応と別の前側髪束へのUV共有を記録しました。顔・上半身・45度の同条件raw比較では
下端が弱まり、顔の読みと外周は保持されています。上部の明るい線は残るため、
白帯の完全除去・完成原稿品質とは評価しません。局所改善のために全体Coreや露出を下げていません。

追加の実機単独表示では、自動Texture内部線、Authored Core、外周Core、表面Halo、
画面Bloomをそれぞれ保存しました。Authored Core単独ですでに頬側の太い縦線があり、
表面Haloは同じ線を太く広げ、Bloomは周囲へにじみを加えます。外周Core単独は外形の
細い輪郭、自動Texture線単独は元画像の装飾・髪の境界を含む別の線として見えます。
この結果は白帯全体をBloomや外周だけの原因として扱わない根拠です。
同じ停止Idle .1166151464秒・顔正面・露出.75を維持していますが、単独化には診断mode・
必要な強度・出力経路を変更しています。手動sliderの自動線強度5.016、外周Core強度7.975
は元の5 / 8と微差があるため、この5枚を完全な同一設定での加算分解とは扱いません。
V2→V3の品質比較は別の固定設定ペアで評価しています。
5成分の状態・画像SHA・素材SHAと評価は
`generated/neon_dissolve/line_contributions_manifest.json` / `line_contributions_review.md`、
V3の局所評価は `generated/neon_dissolve/local_hair_final_review.md` に記録しました。

ディゾルブは凍結した現在のGPU Skinning姿勢をモデル空間で評価します。開始カメラの
right-minus-upをWorldの線形部分で平面係数へ変換し、全モデルの現在頂点から一度だけ
走査範囲を求めます。固定seedのvalue noiseを小さく加え、範囲にはノイズの最大幅も含めます。
Body / SDF比較 / Geometry / Hullが同じ判定を共有します。無効とprogress 0は元表示と一致し、
progress 1は発光・Depthの残骸なく完全消滅します。Hullの画面膨張前の座標を使い、
必要な微分をclip前に計算します。切断面や実際の破片は作っていません。

PreviewではTrigger、独立したDissolve Play/Pause、Progress Seek、同姿勢Restart、
Reset、待ち時間・duration・速度・方向・noise・境界色/幅/強度を操作できます。
Resetは元のanimation時刻・playing/paused・進行中blendと全Dissolve設定を復帰し、
最初の復帰frameをdt=0で更新します。演出中はAnimation Seek・Transform・Normal切替を
ロックし、比較batch / 動画 / GPU計測の危険な同時操作を無効化します。

### 実機の保存先

| 内容 | 保存先 |
| --- | --- |
| 髪V2 / V3の顔・上半身・45度raw PNG + JSON | `project/generated/neon_directional_dissolve/showcase_1791057370425/000_baseline`〜`006_baseline` |
| 髪の5成分単独表示raw PNG + JSON（labelは過去のcapture名のまま、状態はJSONが正） | `project/generated/neon_directional_dissolve/showcase_1791058850211/004_dissolve_full` Authored Core、`005` 表面Halo、`006` 自動Texture線、`007` 外周Core、`008` Bloom |
| 初期顔8条件（境界幅.018 / 強度8） | 上記sessionの `dissolve_comparison_1791057508013/` |
| 細い境界の上半身45度 / 全身45度8条件（幅.008 / 実UI強度3.937） | 上記sessionの `dissolve_comparison_1791057767365/` / `dissolve_comparison_1791057796893/` |
| 最終設定の全身正面8条件（幅.008 / 強度4） | `project/generated/neon_directional_dissolve/showcase_1791058850211/dissolve_comparison_1791059145807/` |
| Idle / Attack / Orbit / Reset 480 raw PNG + frame JSON | 上記sessionの `dissolve_sequence_1791058897029/` |
| 8秒 / 60fps動画（raw PNGからCPU H.264 encoding、画像加工なし） | `generated/neon_dissolve/final_dissolve_idle_attack.mp4` |
| 同条件検証 / 数値 / 元原稿 / 視覚review | `generated/neon_dissolve/`、`generated/neon_directional_dissolve_numeric/tests_summary.md` |

動画ではIdle 1.0166664秒とAttack .5166668秒の異なる途中姿勢からTriggerし、
進行中のOrbitでも保存した姿勢・平面・seedが不変、Reset frameで元時刻へ復帰することを
480設定JSONと実rawで確認しました。両演出のprogress 1画像のRGBは全画素黒です。
最終画像の発光pixel数は境界光・奥面・Hullの変化も含むため、単調性の基準にしていません。

### ビルド・回帰・GPU計測

| 検証 | 結果 / 証拠 |
| --- | --- |
| Development / Release x64 build | PASS。`generated/neon_directional_dissolve/*_build_final.log` |
| Animation / Skeleton / Palette / controller（実GLB） | PASS。`generated/neon_directional_dissolve_numeric/animation_results.log` |
| CPU field・端点・単調性・行列・不正入力 | PASS。2,205点 × 101進行率。`..._numeric/results.log` |
| 実WARP / RTX 4060 Laptop pipeline + DXC / PSO / offscreen readback | PASS。両Device SM6.1 / Barycentrics対応、強制未対応fallbackもPASS。`generated/neon_directional_dissolve_pipeline_{warp,hardware}/results.log` |
| C++ / HLSL reflection、実Resource容量・2Draw snapshot | PASS。GPU定数320 bytes、実確保512 bytes、b1 / 7 Root Params / 16 DWORD維持 |
| Alpha / 遮蔽 / MRT / reserved stencil以外のbit | PASS。全attachment byteを検証。`0xa5`→`0x25`、完全消滅も予約bit clearを維持 |
| Assimpモデルロード | PASS。7 submesh / 85 unique bones / 21,961 vertices / GLB Animation 0（生成clip 2は別） |
| Quality生成 / WIC LinearData / 11 MIP | PASS。12数値テスト、全12 PNG再生成byte一致、中間値fixture保持 |
| 比較回帰 | PASS。旧7組49画像、旧10 fixture、新Dissolve6 fixture、実機8条件batch |
| Developer profile / Package fixture / pristine package | PASS。5 profile構成。package 248 files / 34,460,713 bytes、Preview GLB・line_masksなし |
| Release package実起動 | PASS。別QAコピーをHidden・引数なし・無関係cwdから起動して正常終了。`generated/neon_directional_dissolve/release_runtime_tests.log` / `verification_results.json` |

RTX 4060 Laptop / Development / 1280×720 / scale1 / V2 Coverage+Separate Core/Halo /
全身正面 / 同じ停止Idle .1166151464秒 / Geometry OFF / TAA OFFで、各60 warmup＋300有効GPU
timestamp frameを保存しました。部分消滅はスライダー実値progress .4920000136です。
親scopeと子scopeを合算せず、BodyとHullの個別値も未測定として扱います。

| 状態 | Neon Character mean ms | GPU frame mean ms |
| --- | ---: | ---: |
| 無効 | 0.869700 | 2.744484 |
| 部分（.492） | 1.037500 | 2.898842 |
| 完全（1） | 0.980541 | 2.851977 |

設定・全scopeのmean / P95は `generated/neon_dissolve/actual_gpu_timing_summary.json`。
この順序の1 runでありRelease性能の保証ではありません。完全消滅でもモデルpassesを提出し、
Shaderでclipします。Draw省略による高速化とは説明しません。

機能面は実モデルで方向性消滅・姿勢固定・Orbit・完全消滅・Resetを確認済みです。
品質面は、広い初期境界を最終.008 / 4へ狭め、方向と残る服・袖・脚・靴が読める一方、
近距離の境界はなお白く飽和し、髪の上部の強さ・元Alphaの細かい段差・UV共有は残ります。
任意の距離で一定pixel幅を保証せず、微小欠片を時間方向に安定化する新TAAもありません。
通常ゲームのTAA / history経路に演出を本格統合した確認は未実施です。
GPU粒子・破片移動・Boss死亡処理には進んでいません。
実機でDissolveを開始してからShowcaseのReturn to normal Previewを押し、通常ゲーム画面と
元の全画面Bloom設定（露出1、強度.35）へ戻ることも確認しました。その後ゲームを終了し、
Releaseの隔離QAを実行するために実機GPU計測との同時実行を避けました。
Release QAは既存の内部テスト経路でタイトル2回、新run2回、新敵3種、修理境界、Boss結果、
チュートリアル完了保存と次回skipを確認しました（PowerShell 7.6.6、wrapper実測112.278秒）。
後半戦闘の強制clearを含むUX回帰であり、
全戦闘を人間の手操作でクリアした検証とは扱いません。ReleaseでDeveloper Toolsが無効の
fresh startup traceと最終実行ファイルSHAもprofileテストでPASSを確認しました。
`developer_tools` / `imgui_initialized` / `runtime_profiler_allowed` はReleaseで全て0、
Developmentで全て1です。ログは
`generated/neon_directional_dissolve/developer_profile_fresh_runtime_tests.log`。
