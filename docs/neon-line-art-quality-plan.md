# Neon line art: diagnosis, comparison, and selection

調査日: 2026-10-03。対象はDeveloper限定のAvatarSample_B Preview。開始基準は`feature/neon-line-art-quality`、HEAD `68d084841eaf03099c573c1b77eb4b36a84570a9`。この文書の「基準コード」は開始HEADの実装、「比較実装」は今回の作業ツリーを指す。元の曲線JSON・RGBAマスク・推奨プリセット・GLBを比較元として保持する。技術の採否は同条件の実モデル画像・動画・負荷で判断し、データテストやPSO生成だけで画質向上を断定しない。

## 1. 診断: 確認済みコードと、画像で検証する仮説

| 段階 | 基準コードで確認した事実 | 画質についての仮説 / 検証 |
| --- | --- | --- |
| 原稿 | 既存マスクはBezier由来。Rは強弱を含むcoverage、Gは自動Texture線の置換率。`G=1/R=0`は不要線を抑制する。BaseColorの自動線抽出は画面微分方向の近傍色差を使い、髪の広い陰影も信号になる | SDFがなくても、線の位置・本数・width・opacity・置換域の選別で改善する可能性。旧原稿、新原稿v1、新原稿v2を同じ表示条件で比較する |
| Texture | `TextureManager`のLinearDataは`WIC_FLAGS_NONE`、通常の`TEX_FILTER_DEFAULT`で全MIPを生成。SRVも全MIPを公開。Neon samplerは`MIN_MAG_MIP_LINEAR`、CLAMP、anisotropy 1、`SampleGrad` | 1024 atlasの細い線が縮小coverageになるのは正常。強い斜め視点ではこのfilterだけでは不足する可能性。近距離・全身・側面で比較し、消えた細部をSDFの保証として扱わない |
| Scene | Scene HDRとNormalは1280×720の`R16G16B16A16_FLOAT`、Materialは別MRT、DepthはD24S8。NeonはHDRだけへ発光を加算し、Normal/Materialを別に出力する | 発光ShaderのRGB、Bloomに渡るRGB、最終カラーを分けて見る。輝度不足とTone Mappingによる白飛びを混同しない |
| Bloom抽出 | 640×360へSceneを1回bilinear sampleしてから、線形RGB輝度`dot(rgb, (.2126,.7152,.0722))`にthresholdを適用する。既定threshold 1。Soft Kneeなし | 細い線や低coverageは縮小でthreshold未満になり得る。Scene HDRと**抽出直後**の同じ線の位置・強度を比較する |
| Bloomぼかし/合成 | 抽出後は同じ半解像度で9-sample prefilter、H/Vの5-fetch Gaussian、最終合成。通常BloomではH、V、Compositeがそれぞれ`intensity`を掛ける。既定.35の係数は合計`.35³ = .042875`。全画面blurモードではH/Vの係数は1 | 単に最終強度.35のBloomと考えると、発光の減衰を見誤る。これはコードの確定事実。現在の線が弱い主因かはHDR/抽出/ぼかし/最終を見て判断する。通常ゲームの係数を今回一括変更しない |
| Tone Mapping | 既定exposure 1、ACES FilmをRGB各成分へ適用しsaturate。Compositeにはモノクロ・色補正・画面演出もある | Coreの強度増加だけでは色が白へ近づく。狭いCore、低い色付きHalo、画面Bloomを別に調整する |
| Pause/画面演出 | `Game::Update`は最終delta timeが通常より小さいと`SetGrayscaleEnabled`を呼ぶ。Previewの停止とゲームの停止は別。基準デバッグのSceneColor/BloomOnlyもTone Mapping済みで、raw FP16値そのものではない | Pauseでカラー評価が変わり得る。Showcaseで画面演出を局所的に抑止し、ゲームから設定が渡された**後のPostDraw**へ反映する。A/B両方に同じ抑止を適用する |
| 時間方向 | 基準Temporal AccumulationはON、history blend .88、jitter ON。MotionVectorResolveはdepthで現在world位置を復元し、前カメラVPへ投影する。前Skinned Palette / 前object変形は使わない | アニメーションする顔・髪の履歴再投影は正しくない。静止比較はShowcaseでtemporal/jitter OFF、切替時history reset。新TAAを細線問題の解決として追加しない |
| 描画順 | Previewの3D描画はScene HDR pass内。後からゲームの3D、post、after-post、sprite/HUD、最後にImGuiが続く | 黒画像をモデルの上へ重ねるのではなく、Showcase時にゲーム背景/HUD等の描画経路を制御する。比較画像は同じエンジン出力をImGui描画前に保存する |

関連コード: [TextureManager](../project/DirectX/engine/2d/TextureManager.cpp)、[Neon surface](../project/resources/shaders/NeonSkinnedSurface.hlsli)、[Bloom](../project/DirectX/engine/postEffect/Bloom.cpp)、[Bloom extract](../project/resources/shaders/BloomExtract.PS.hlsl)、[Blur H](../project/resources/shaders/BloomBlurH.PS.hlsl)、[Blur V](../project/resources/shaders/BloomBlurV.PS.hlsl)、[Composite](../project/resources/shaders/Composite.PS.hlsl)、[Motion vectors](../project/resources/shaders/MotionVectorResolve.PS.hlsl)、[Temporal resolve](../project/resources/shaders/TemporalResolve.PS.hlsl)、[Game](../project/game/scene/Game.cpp)。参照は現在の作業ツリーへ開くため、基準挙動の確認時は開始HEADと区別する。

### Bloomの数値例: 実測ではなく原因候補の計算

既定外周pink `(1,.035,.6)`、intensity 12の線形輝度は約3.3714。2×2縮小footprint中に1つだけ同じ明るいpixelがあり、等しい4分の1の重みで混ざる例では約.8429となりthreshold 1を下回る。同じ縦線が2pixel分を占める例は約1.6857となる。実際のbilinear重み、coverage、線方向、画像位相は変わるため、この例を実モデルの測定値として扱わない。線がShaderには存在するのに抽出で切れる可能性を示す計算である。

## 2. 六つの候補と今回の判断

| 候補 | 解決する問題 / 部位・段階 | 入力とエンジン変更 | 品質の利点・弱点 | GPU / メモリ / 制作コスト | 判断・原文参照 |
| --- | --- | --- | --- | --- | --- |
| A. 高品質coverage＋filter | 目・口・前髪の原稿、極細線の画素coverage、遠距離の密集 | 元Bezier、stroke幅とopacity、4倍supersampling、別Core/Halo channels。既存LinearData/MIP/SampleGradを再利用 | 面積coverageは縮小で平均信号を残しやすい。原稿整理と発光の公平な対照群になる。拡大して失われた輪郭を復元できず、UV伸縮で元texel幅が変わる | 近接テストを除けば従来の1 mask fetch。追加coverage RGBA8は1枚あたり全MIP約5.33 MiB。原稿編集・再生成が中心 | **比較採用**。既存R/Gを保存し、新候補R/G/Bを別に作る。Valve §3の拡大時の問題、§4.1のfilter/AA条件を参考にする [R1] |
| B. 線領域SDF＋coverage fallback | 近距離の滑らかな目・口・前髪の芯、狭い表面Halo | 同じBezierのround stroke領域から距離を生成。距離、opacity、Gを別channel。LOD0距離と画面微分AA、縮小時は別coverageへblend。Neon surfaceだけ拡張 | 輪郭と発光の強弱を分離し、幅/狭い光を再構成できる。曲線の位置誤り、遮蔽、元UVの共有を直さない。8bit量子化、交差内部距離、縮小LOD移行は要確認 | SDFモードはcoverage fetchに距離LOD0 fetchと微分計算を追加。1枚あたり全MIP約5.33 MiB。生成はオフライン。新RT/passなし | **比較採用**。Aと同じ原稿・同じcamera/poseで比較し、画質差が小さければAを優先。Valve §3、§4.1、§4.2.2 [R1] |
| C. MSDF / MTSDF | 低解像度で鋭角が丸まる閉じた図形、尖った書体・意匠 | 閉輪郭、複数channelへedge coloring。RGB median、距離rangeと画面変換。MTSDFはさらにscalar SDF alpha | 鋭角を保ちやすい。今回の目・口・前髪は主に細いround strokeで、角の改善必要性が未確認。multi-channelをopacity/Gと共有できず、交差・薄い形状・atlas条件の制作負担が増える | 距離fetchは1枚でも3channel再構成・edge処理が必要。現在のRGBA仕様と別packingが必要。ライブラリ導入は未実施 | **保留**。鋭角の具体的な破綻が見えてから採用する。msdfgen README「Using a multi-channel distance field」「Library API」、`screenPxRange`とLinear条件 [R2] |
| D. 解析的Bezier / Loop–Blinn | 拡大しても解像度依存しない曲線境界 | 二次/三次Bezierと制御三角形、内側三角形、overlap/triangulation。UV上の曲線を3D表面へ対応させ、変形・深度・UV seamを扱う追加構造が必要 | 曲線境界をpixel単位の距離でAAできる。単なる既存fragment texture差し替えではない。stroke幅、鋭角、制御hull外のAA、edge-on退化の処理が必要 | textureメモリを減らせてもcurve/control triangleの前処理・draw・surface/skin管理が増える。今の15前後の曲線に対し実装負担が大きい | **保留**。原稿やBloomが主因なら費用に見合わない。Loop–Blinn §3/§4、§5 AA、§5.1 exterior footprint、§6退化 [R3] |
| E. Core / Halo / Bloomの役割分離 | 芯が読めず色光だけになる、顔の白潰れ、外側への光不足 | coverage/SDFのCoreと表面Halo、別色/別強度、外周Core。既存BloomをShowcase限定で条件固定し、各寄与を単独診断 | 芯の鮮明さと柔らかい光を別調整できる。表面Haloはシルエット外に出ず、Bloomの縮小thresholdやTone Mappingの限界は残る。二重加算を避ける必要 | 表面計算のみなら新pass不要。既存Bloomは4つの半解像度pass＋最終合成を再利用。Showcaseの既存パラメータ変更の負荷増は小さいが実GPUで測る | **比較採用**。通常ゲームBloom既定値は保持。Unity公式のThreshold/Intensity/Scatter/Clamp/Downscaleという役割分離を参考にし、数値や実装を流用しない [R5] |
| F. 選択的NPR線＋時間方向の安定化 | 視点依存の必要輪郭の選別、線の分裂・密集、動画のちらつき | 可視線samples、motion、stroke tracking/parameterization、LOD。履歴を使うなら前Skinned姿勢と遮蔽変化/履歴破棄が必要 | 動いてもstrokeを維持する研究がある。ただし目・口の意味を自動理解しない。出現/消失で数frameの崩れやghostingがあり、誤ったcamera-only motionで足せない | Active Strokes原実装はCPU処理・GPU readback・疎行列solveも含む。履歴RT/前姿勢/線管理は今回のmask試作より大幅に高コスト | **保留**。まず空間filter、原稿、LOD、Bloomで解決する。Active Strokes §4、§7.2/§7.3/§7.4 [R4]、TAA survey公式abstractのsample accumulation/history validation [R6] |

## 3. 一次資料の確認範囲と適用条件

原文からは方式の条件を取り出し、画像の質や今回の速度を保証する根拠にはしない。資料の処理系や用途が異なる箇所はそのまま移植しない。

- **[R1] Valve / Chris Green, 2007**: [指定PDF](https://cdn.akamai.steamstatic.com/apps/valve/2007/SIGGRAPH2007_AlphaTestedMagnification.pdf)は取得timeout、[同じValve CDNのPDF](https://steamcdn-a.akamaihd.net/apps/valve/2007/SIGGRAPH2007_AlphaTestedMagnification.pdf)で本文確認。§3は高解像度の図形から距離を作り、境界を.5に格納する条件。§4.1はscreen derivativeによるAA、§4.2.2は距離区間のglow、§4.3はsingle channelの鋭角の弱点。今回の弱いopacityは形状とは別に保持し、既存の弱いPNG値の二値化をしない。
- **[R2] msdfgen / Viktor Chlumský**: [作者README](https://github.com/Chlumsky/msdfgen)本文確認。「Using a multi-channel distance field」はRGB median、screen range、**Linearとして読む**条件。「Library API」は輪郭/edge normalizationとedge coloring。鋭角保持という入力上の必要性がない限り、MSDFを上位互換として追加しない。今回依存ライブラリは導入しない。
- **[R3] Microsoft Research / Loop–Blinn, 2005**: [論文ページ](https://www.microsoft.com/en-us/research/publication/resolution-independent-curve-rendering-using-programmable-graphics-hardware/)と[本文PDF](https://www.microsoft.com/en-us/research/wp-content/uploads/2005/01/p1000-loop.pdf)確認。§5のimplicit値/gradientによる画面距離は参考になるが、曲線境界を含むtriangulationとAAの外側pixelを描くhullが必要。§6の退化もあり、任意視点の既存skinned triangleへ自動適用できる方式ではない。
- **[R4] Princeton / Active Strokes, 2012**: [作者ページ](https://pixl.cs.princeton.edu/pubs/Benard_2012_ASC/index.php)と[本文PDF](https://gfx.cs.princeton.edu/pubs/Benard_2012_ASC/Benard_2012_ASC.pdf)確認。§4のadvect/relax/split/merge、§7.3のline-density LODを参考にする。§7.2は主にCPU処理とreadback、§7.4はocclusion/disocclusion時の短い不安定性を報告する。自動の意味的目・口抽出の根拠にはならない。
- **[R5] Unity 6 URP**: [公式Bloom説明](https://docs.unity3d.com/6000.0/Documentation/Manual/urp/post-processing-bloom.html)のProperties確認。Threshold/Intensity/Scatter/Tint/Clamp、Downscale/MaxIterationsの役割を比較軸にする。UnityのThresholdはgamma-spaceの説明であり、このエンジンのlinear輝度threshold 1と同じ数値を移さない。文書は本エンジンのSoft Kneeやblur実装の証拠ではない。
- **[R6] NVIDIA / Yang, Liu, Salvi, 2020**: [公式論文ページ](https://research.nvidia.com/labs/rtr/publication/yang2020survey/)の原文abstract確認。sample accumulationとhistory validationを分けるという条件を参照。作者[PDF](https://behindthepixels.io/assets/files/TemporalAA.pdf)は繰り返しtimeout、[Eurographics版](https://diglib.eg.org/bitstream/handle/10.1111/cgf14018/v39i2pp607-621.pdf)は403、Wiley full-textはabstractへredirectされたため、**全文を読んだとは扱わない**。前Skinned姿勢がないという今回の保留理由はローカルMotionVectorResolveの実装から判断したもので、未取得の節を引用しない。
- **[R7] Microsoft**: [D3D12 Timing](https://learn.microsoft.com/en-us/windows/win32/direct3d12/timing)本文確認。timestampはEndQueryで記録し、ResolveQueryDataでUINT64へ解決、queueのGetTimestampFrequencyでmsへ換算する。queue/GPUとCPUのclockは直接同一ではない。今回の計測は既存ProfilerのGPU timestampとFenceを再利用する。

## 4. 比較実装のデータ仕様と安全な範囲

新しいデータは[quality README](../project/resources/models/neon_hologram/line_masks/quality/README.md)、[quality authoring](../project/resources/models/neon_hologram/line_masks/quality/authoring.json)、[bindings](../project/resources/models/neon_hologram/line_masks/quality/bindings.json)へ分け、旧ファイルを保存する。元GLBはVRoid ProjectのAvatarSample_Bであり、派生線データをCC0や完全独自素材と表記しない。[元モデルの権利情報](../project/resources/models/neon_hologram/README.md)を継承する。

| データ | R | G | B | A |
| --- | --- | --- | --- | --- |
| 新coverage RGBA8, 1024² | 4倍supersampling→Lanczos縮小のCore coverage × opacity | 自動線の置換率 | Core外側4 texelsの狭いHalo coverage × opacity | 1 |
| 線**領域**SDF RGBA8, 1024² | `.5 + signedDistance/32`。内側正、境界0、外側負。texel単位、range ±16 | 同じ置換率 | 最近strokeの独立opacity | 1 |

SDFは細い帯の**内部を描く**用途であり、帯の両境界をもう一度線にして二重線にしない。Bezierを512区間のround capsulesへ近似し、異なる幅も半幅から境界を作る。unionはsigned値のmaxで合成するため、交差内部の深さは厳密なunion内部距離と異なる場合がある。8bitの距離量子化誤差は最大約.06275 texels。弱いopacityはshapeを消さず、発光の係数として別に残す。

置換GはHalo外側まで余裕を持たせ、G=1/R=0の不要線消去も維持する。描画側ではHaloもGの置換率に掛かるため、原稿Gが狭いままなら長方形に切れる問題が残る。候補データのG supportとBaseColor Alpha、Depthによる正常な遮蔽を別々に検査する。

比較Shaderは既存GPU Skinning VS/Palette/Geometry、Body 3MRT、Outline Hull、Alpha Cutoutを共有する。SDF textureをt2へ追加し、欠落・無効な距離textureはcoverageへ戻す。旧mode 0、split emission OFFで基準のR/G経路を保持する。距離LOD0をsampleし、`length(float2(ddx(distanceTexels), ddy(distanceTexels)))`を下限付きで画面pixel距離へ変換する。UV derivativesとdistance derivativesはAlpha clipより前、分岐条件はDraw単位のuniform値とする。

LODは`log2(max(length(uvDx*atlasSize), length(uvDy*atlasSize)))`から求め、既定LOD1〜2で通常filtered coverageへsmoothstep blendする。距離の平均MIPは生成されても、再構成に使わない。全遠距離細線をminimum pixel幅へ強制拡大する方法は採らない。斜め視点/LOD境界/ゼロ勾配の妥当性は画像・数値テストで確認する。

Core・Halo・外周Coreは別色/別強度。surface Haloは同じUV表面の範囲内だけに出る。シルエット外へ出る光は既存画面Bloomである。Body補助発光やメッシュ構造線を品質改善の代わりに強めない。Geometry Linesの既定OFF、Animationクリップ・再生制御・更新1回・Palette共有・BeginFrame/Fence・Normal/Material/Depth・Stencil予約bit・複数Draw定数の分離を維持する。

### メモリとデータ検証の扱い

8枚のPNGは2原稿version × 顔/前髪 × coverage/SDF。GPU RGBA8のbaseは32 MiB、全通常MIP込み約42.7 MiB。1 versionだけをロードする場合は4枚、base 16 MiB、全MIP約21.3 MiB。これはformat・resolutionからの配列サイズ計算であり、GPU allocatorやtexture cacheの実測と区別する。PNG圧縮サイズをGPU使用量と呼ばない。Material間の同じPNGはTextureManager cacheで共有する。

制作担当が実行した再生成/数値テストは、caps/異幅交差、内外符号、弱線opacityの維持、GのHalo包含、元データ保存を検査する。実WIC経路ではRGBA全画素をPillowとの照合、Linear formatと11 MIPを検査する。SDF平均MIPの形状が正しいという検証ではなく、coverage fallbackを使う条件の確認である。実行ログは最終検証報告に記録し、ここでは実モデル画質のPASSへ読み替えない。

## 5. 同条件比較と調整サイクル

ShowcaseはDeveloper Previewからのみ入る。専用camera/状態を保存し、退出時に通常camera/Transform/pose/色へ戻る。黒背景・HUDなし・ゲームscreen effect抑止・temporal/jitter OFF・同じexposureを**基準にも候補にも**適用する。画像はエンジンのfinal outputをそのまま保存し、後加工で線や光を足さない。raw HDRやBloom抽出の診断画像をTone Mapping済みfinalと明記して区別する。

| サイクル | 同条件で固定するもの | 一つずつ変えるもの | 見る部位 / 判断 |
| --- | --- | --- | --- |
| 1: 基準と信号 | 顔拡大・正面・Idle Pause/Seek・1280×720・camera・exposure | 旧mask、texture由来線、Core単独、Halo単独、Scene HDR/抽出/final | 目・口・前髪がどこで弱くなるか。原稿の欠落、正常遮蔽、threshold消失、白飛びを分ける |
| 2: 同じ原稿でA/B | 新v1原稿、camera/pose/色/強度/解像度を固定 | coverage＋Core/Halo、SDF＋Core/Halo、Halo OFF、Bloom条件 | 近距離の滑らかさ、芯/色光の区別、UV伸縮、LOD移行。SDFの効果が小さければ方式追加を価値としない |
| 3: 原稿と実用サイズ | 採用候補の表示方式と発光を固定 | v1/v2原稿、正面/斜め/側面、顔/上半身/全身、Idle/Attack/camera回転 | 線位置と本数、顔の読みやすさ、髪の流れ、遮蔽、全身サイズの信号量・ちらつき。比較元は残す |

保存JSONにはモデルhash、build/GPU、mode/version、scene resolution/render scale、camera pose/FOV、Transform、clip/time/paused、temporal/jitter、exposure/Bloom、全Neonパラメータ・Submesh設定を記録する。顔が画面で何pixelかは元画像とともに測る。capture解像度を増やした場合はリアルタイム既定との違いを明記する。

## 6. GPU計測と回帰の条件

[RuntimeProfiler](../project/DirectX/engine/commom/RuntimeProfiler.cpp)は既にD3D12 timestamp heap、queue frequency、ResolveQueryData、Fence完了後のreadbackを持つ。GPU有効frameの条件を保持し、CPUの関数所要時間と区別する。既存`Scene3D includes effects`、`Global Bloom / Post`に加えNeon Drawのscopeを分ける。Showcase開始・asset upload完了後にcaptureを開始し、warmup 60frame、測定300frame等で同じview/pose条件の基準、A、B、採用案を比較する。scope数64という既存上限を超えない。

Profilerの`CG2_PERF_CAPTURE_FRAMES`、`CG2_PERF_CAPTURE_WARMUP`、`CG2_PERF_CAPTURE_PATH`、または今回のDeveloper capture UIを利用する。CSVの`gpu_valid=1`のframeだけを使い、msの平均とP95を示す。queue単位のfrequency、GPU/driver、Development/Release、1280×720、render scale、sampling数を残す。scene全体とNeon単体の両方を比較し、負荷が小さすぎて安定差を測れなければそのまま報告する。現在この文書に未実測のGPU時間を記入しない。timestamp APIの条件は[R7]を参照する。

必要な回帰はDevelopment/Release、既存Neon Pipeline/Model/Animation、C++/HLSL定数配置、qualityデータの再現性/LinearData、旧mode復帰、Showcase入退出状態復帰、Alpha/Depth/MRT/Stencil、複数Submesh/Draw、無効texture fallback、GLB hash、Release Developer無効化、提出packageの狭いPreview素材除外。テストのPASSと、実機での画質・動画・性能の実測は別欄に記録する。

元GLB保護の開始hash: `7FCA4A77FDC60AB2C78A9907430744562626180125FA386EB74FB2EA15C2E518`。終了hashと出力ログの照合は親作業の最終検証へ記録する。branch/HEADの変更やcommit/push/mergeは行わない。

## 7. 実機比較で確認したことと選定

実機はNVIDIA GeForce RTX 4060 Laptop GPU、Development x64、1280×720、render scale 1。参考イラストを実際に確認し、白寄りの細い芯と狭いピンクの光を評価軸にした。元VRoidの閉じた口・固定表情・造形はイラストと異なり、Rendererが同じ表情を再現したとは扱わない。

最初の実機画像は `project/generated/neon_line_art_quality/showcase_1791025931781/`、修正後の同条件比較は `project/generated/neon_line_art_quality/showcase_1791026920670/`。後者の六つのcomparisonディレクトリには、それぞれAuto / Original authored / V1 coverage / V1 coverage+CoreHalo / V1 SDF+CoreHalo / V2 coverage+CoreHalo / V2 SDF+CoreHaloの7枚とJSONを保存した。いずれもUI描画前の未加工エンジン出力であり、線や光を後加工していない。

| 比較セット | ディレクトリ末尾 | 顔の露出部の幅（元1280×720画像から目視概算） |
| --- | --- | --- |
| 正面・顔 | comparison_1791027130530 | 約180 pixel |
| 正面・顔詳細 | comparison_1791027202137 | 約320 pixel |
| 正面・全身 | comparison_1791027248598 | 約45 pixel |
| 正面・ゲーム想定サイズ | comparison_1791027285479 | 約25 pixel |
| 顔・45度 | comparison_1791027341480 | 髪の遮蔽で露出範囲が変わる |
| 顔・90度 | comparison_1791027387198 | 目の大半が正常に隠れる |

全セットはIdle 1.4950000048秒で停止し、各セット内のcamera / Transform / Body / Bloom / exposure / geometry OFFを固定した。42枚のcase表・Material対応・Manifest期待SHAと実入力ファイル・PNG解像度を比較validatorで検査した。最初のfaceセット前後は全appearance JSONとPNG bytesが一致し、比較操作後の復帰を確認した。新鮮なプロセスへ検証済みPNGを読み込んで採取したが、GPU texture内容そのものをSHA計算した結果ではない。

三段階の調整は、(1)旧原稿と同条件の基準化、(2)公平な幅のV1 coverage/SDFと発光分離、(3)V2原稿修正と実機で発見したAlpha/髪強度の修正である。最初のV2と修正前入力は `generated/neon_line_art_quality/v2_before_iris_adjustment/` に保存した。最終原稿revisionは `v2-iris-hair-r2`。虹彩下側の制御点vを.964から.948へ移し、前髪3曲線の幅を2.2から1.8 texels、opacityを1から.65へ下げた。モデルやBaseColor、UVを動かして問題を隠していない。

| 単独比較 | 実機で確認した効果 | 判断 |
| --- | --- | --- |
| Auto→旧authored | 細切れの髪のTexture由来線が連続した流れへ整理される。旧の目・口は弱く、一部虹彩下線が欠ける | 専用原稿が有効。既存候補を保存 |
| V1 coverage、CoreHalo OFF→ON | 淡いピンクの芯と色光を区別しやすい。顔全体は白く潰れていない | 発光分離を採用。ただし色・強度も変える比較であり、総HDRエネルギーを正規化した実験ではない |
| V1→V2 r2、同じcoverage/CoreHalo | 上まぶたの弧、虹彩の下輪郭、単一の口線が読みやすい。髪の強弱も抑えられる | 原稿の改訂が主な改善。技術方式だけの効果として数えない |
| 同じV2、coverage→SDF | 詳細拡大では髪の芯・輪郭端に局所的なAA差。通常顔サイズで目の形や強弱に大きな差はない。ゲームサイズでは全画面1pixelの1/255差のみ | **通常の候補はV2 coverage＋Core/Halo**。SDFは拡大比較の選択肢として残し、一律の上位互換とはしない |

独立レビューは `generated/neon_line_art_quality/engine_quality_review.md/.json`、入力条件manifestは同ディレクトリの `comparison_*_manifest.json`、画像差分は `raw_reconstruction_pixel_deltas.json` に記録した。差分量やsRGB領域平均は画質の点数でもHDR強度の実測でもない。Auto/Original authored/V1 coverageの旧採取画像と修正後画像は対応する正面条件で全画素一致した。

Alphaの確認は、元画像上のBezier中心を513点でサンプリングした限定的な検査である。虹彩下側の中心Alpha≥.03は左38.2%/右39.6%から約81.1%/80.3%へ改善したが、stroke面積・bilinear/MIP・実機遮蔽込みの可視率ではない。残る端部約20%は消える可能性があり、SDFだけでは直らない。根拠は `alpha_curve_review.json`。

Core単独、表面Halo単独、Body単独、coverage R/G、Scene HDRとBloom抽出のTone Mapping表示、Bloom blurを追加採取した。PNGはすべてRGBA8の診断表示であり、raw FP16のHDR数値readbackではない。抽出表示では目の細線が途切れやすく、最終画像にはScene側の芯が残る。半解像度縮小とhard thresholdで信号が落ちるというコード上の診断と整合する。Showcase限定のthreshold .65 / intensity .75 / exposure .75を全候補へ共通適用し、通常ゲームBloomの既定値は変更していない。

左頬側の髪はR診断にも太く投影され、まだ目より強い白帯に見える。UV上の共有と曲線の投影幅、Core強度の残課題である。服の自動抽出には細かなjaggyが残り、顔約25pixelでは表情の細線が弱い。側面では正常な髪遮蔽で目・口が隠れる。GLB・UV・ウェイト・骨格・元Animationクリップは保持し、裏面透過で回避していない。

今回MSDF、解析曲線、Active Strokes、前Skinned姿勢TAA、Dissolve/粒子へ進まない。

## 8. 動画とGPU実測

最終Developmentビルドの出力は `project/generated/neon_line_art_quality/showcase_1791028647958/`。同条件7枚の追加セット `comparison_1791028703179/` は解像度・validation flags・queue frequencyを含む最新metadataで検査済み。先の42枚と合わせ49枚の比較validatorがPASSした。

`sequence_1791029162230/` に未加工の240枚の1280×720 PNGとフレームごとのJSONを保存した。0～74はIdle、75～182はAttack、183～239はIdle復帰。再生速度1、Alpha Cutout、V2 r2 coverage＋Core/Halo、Body/外周/Bloom設定は固定。orbit yawは約.005833～1.400003 radへ連続的に変化した。モデルの既存Animation経路で更新し、2Dの線画像を画面に貼った動画ではない。

同フォルダの `idle_attack_orbit.mp4` は、既存ffmpegで元PNGを60fps / H.264 CRF16 / yuv420pへ符号化した4秒の動画。追加の線、発光、補間フレーム、補正フィルタはない。PNGは原本のまま残す。MP4は圧縮と色空間変換を含むため、pixel比較の根拠には原PNGを使う。実際のエンジン画面とWindowsメディアプレーヤーの再生を確認し、独立レビューは8枚の代表raw frameを確認した。伸ばした腕の外周・袖口線と髪の線が表面へ追従し、明確に浮いた線や大きな全体欠損は見えなかった。服の細線・髪端のjaggyは残る。全240枚を連続した高速度の目視として評価したわけではなく、微細なちらつきが完全に消えたとは断定しない。motionのデータ検査と目視範囲は `generated/neon_line_art_quality/motion_review.md/.json` に分けて記録した。SDFの時間方向の優位性は未確認。

GPU計測は実RTX 4060 Laptop、Development x64、1280×720・scale 1、D3D12 Debug Layer ON・GPU-based validation OFF。各条件、asset読込後60frame warmup＋300frame。正面face、Idle 1.495秒停止、同じTransform/Body/外周/Bloom/exposureを固定した。queue timestamp frequencyは1,000,000,000 Hz、CSVのgpu_valid=1が各300件。D3D12 EndQuery→Resolve→Fence完了後のtick差を換算した実測であり、CPU時間や60FPS待ち時間ではない。

| 条件 | Neon Character 平均 / P95 ms | GPU frame 平均 / P95 ms |
| --- | ---: | ---: |
| 改善前 authored | 1.206245 / 1.268736 | 3.092214 / 3.147776 |
| V1 coverage＋Core/Halo | 1.205494 / 1.270784 | 3.089159 / 3.148800 |
| V1 SDF＋Core/Halo | 1.267152 / 1.326080 | 3.139990 / 3.196928 |
| 採用候補 V2 r2 coverage＋Core/Halo | 1.207433 / 1.270784 | 3.088643 / 3.158016 |

P95はnearest rank、300件の昇順285番目。V1同原稿のSDF−coverageは平均約+.062msだったが、単発の直列runであり、温度・クロック・電源等を統制した有意差ではない。Debug Layerを含むのでRelease性能保証へ外挿しない。Scene3DはNeon Characterを含む親scopeであり、時間を合算しない。全scopeと条件照合は `generated/neon_line_art_quality/gpu_comparison_review.md`、`gpu_comparison_summary.json`、`gpu_comparison_verification.json`。4本の元CSV/JSONは最終Showcaseフォルダへ保持した。

新規qualityデータは8PNG合計193,845 bytes。GPU画像の計算上の量は全候補のbase level 32 MiB、全MIP約42.7 MiB、1versionでは16 / 約21.3 MiB。これはRGBA8の寸法からの計算であり、実allocator・working setの測定ではない。実機画像のためにscene解像度を上げていない。

## 9. 最終回帰・元状態の保護

| 検証 | 結果と範囲 |
| --- | --- |
| Development / Release x64 | PASS。VS18 Community MSBuild、v145。最終Developmentは追加metadataを含む |
| Neon pipeline / reflection / C++・HLSL定数配置 | PASS。WARPとRTX 4060 Laptop両方。Alpha/Depth/MRT/Stencil他bit、DoubleSided、Geometry共有処理、複数Draw、t2分離、無効SDF fallback、強度0、Core/Halo/Body分離、NaN・ゼロ勾配を検査 |
| Model / Animation | PASS。実GLBのAssimpロード、元Animation 0と生成clip 2を区別。Palette変形、Idle/Attack/復帰/再実行、Pause/Resume/Restart/Seek、paused blend snapshot復帰と不正snapshotのatomic拒否 |
| データ再生成 / numeric | PASS。deterministic再生成、coverage/opacity/G、符号range、Quaternion等の既存検査を保持 |
| LinearData実WIC | PASS。8PNGの全RGBA byte照合、UNORM・11MIP、中間値・LOD0距離精度。Windows PowerShell 5.1 / PowerShell 7.6.6で実施 |
| Profiler集計 / 比較validator | PASS。fixtureと実4CSV、最新metadataを含む49枚の実model比較 |
| Package / Release runtime / Developer profile | PASS。PowerShell 7.6.6で実施。AvatarSample_Bとline_masksだけを狭く除外し、他モデルを保持。新規状態のpristine packageとプレイしたQA copyを区別 |
| Showcase退出 | PASS。実画面で通常HUD・グリッド・チュートリアルと元ゲームカメラへ戻り、Preview Enable=false、元auto線・mask未ロード状態へ復帰。pose/blendの値復帰は別の数値テストでも確認 |
| 実画面 | 実施済み。正面/45度/90度、顔詳細/顔/全身/ゲーム想定サイズ、単独寄与/抽出結果、Idle/Attack/orbit。全視点・全距離の長時間ちらつき耐性は未実施 |

Release runtime testはtitle→tutorial→style→workshop→repair→敵→result→title→new run等を確認する。後半戦はfixtureによるforced clearを含み、ゲーム難易度の実プレイ検証とは区別する。Development traceは最終metadataのみの再ビルド前の同じ既定profileから採取され、最終exeの実画面・最新JSONとは別証拠である。Package/runtime/profile全体についてPowerShell5.1までPASSとは記載しない。確認した範囲に最終FAIL/SKIPはなく、未実施範囲を上記とGPU条件に明示する。

詳細な実行コマンド、build SHA、ログ、package 247files / 34,456,279bytesの監査は `generated/neon_line_art_quality/verification_results.json`。pipelineは `generated/neon_quality_pipeline_final_warp.log` / `neon_quality_pipeline_final_hardware.log`、最終安全性監査は `generated/neon_line_art_quality/final_safety_audit.md`。

元GLBの開始・終了SHA-256はともに `7FCA4A77FDC60AB2C78A9907430744562626180125FA386EB74FB2EA15C2E518`。旧mask PNGはHEADのraw bytesと一致。旧曲線JSONとAnimation生成入力はGit正規化内容が一致し、改行コードの差をraw SHA一致と誤記しない。BaseColor/UV/weights/Skeleton/Animationクリップ本体は変更しない。

開始・終了とも branch `feature/neon-line-art-quality` / HEAD `68d084841eaf03099c573c1b77eb4b36a84570a9`。開始clean、終了は今回の未ステージ変更のみ、staged/deleted 0。branch切替・pull/reset/clean/stash/rebase/commit/push/mergeは行わない。正確な全ファイル一覧は終了時 `generated/neon_line_art_quality/work_end.json` に記録する。

変更はRenderer/共有Neon Body・Surface・Outline shader、Preview/Showcase capture、SceneのDeveloper接続、既存BloomのShowcase local設定/診断、Profiler起動API、SkinClusterの再生状態snapshot、VS登録、quality用8PNG・JSON・README・生成script、pipeline/animation/data/比較/package test、この調査文書。元のSkinnedModel通常DrawとShadow、GPU Skinning/VS/Palette更新経路は維持する。

追加した公開APIは、RendererのSubmesh distance mask指定（t2の安全な選択とDrawごとの明示bind）、SkinnedModelのAnimationPlaybackState Capture/Restore（Showcase退出で停止途中blendを含め値を復帰）、ProfilerのStartCapture（既存timestampによるDeveloper操作）、Scene経由のDeveloperShowcaseState/capture hook、BloomのShowcase local描画設定。モデル固有のMaterial名・原稿パスはPreview設定へ置く。const GetSkeleton/GetAnimationPlayerをmutableへ戻さない。

## 10. 操作と最終評価

Visual StudioでDevelopment x64を選びF5。ゲーム内F3の制作ツールから「Neon Skinned Previewを開く」→「Enter Neon Character Showcase」。Appearanceで「V2: edited coverage」を選び「Separate Core / Halo emission」をONにすると採用候補。Geometry LinesはOFF。Original autoまたはOriginal authored、Core/Halo OFFで既存候補へ戻せる。初回の既定表示とRecommended Line Artは勝手に差し替えない。

View/Captureで顔・全身・ゲーム想定サイズと正面/45/90度を選ぶ。AnimationのPause/Seekで姿勢を固定した後「Capture 7 candidates」で同条件画像を保存する。「Record 4s: Idle / Attack / orbit」でPNG連番＋各frame JSON、「Measure GPU」で既存ProfilerのCSVを保存する。画像はImGui前のエンジン出力。退出は「Return to normal Preview」。PowerShell操作は表示確認の必須条件ではない。

- 機能：比較・生成データ・従来表示fallback・発光分離・状態復帰は動作し、回帰がPASSした。
- 画質：同条件で目の弧と虹彩下線、口、前髪の連続性が改善した。原稿＋Core/Haloが主な要因。SDF単独の大幅改善とは評価しない。
- 採用価値：V2 r2 coverage＋Core/Haloを次の通常候補として推奨するが、既定presetの変更は行わず比較可能に残す。髪の白帯と小サイズの細線は追加の原稿調整が必要。
- 技術説明：原本Alphaに基づく曲線修正、opacityと線形状の分離、縮小時のSDF→coverage移行、独立したCore/Halo/Bloom診断を、実比較・CPU/GPU検証・再現可能な入力で説明できる。新しい研究手法や独自アルゴリズムを発明したとは主張しない。

次の有力な課題は、共有Hair UVで頬へ太く投影される部分の局所的な線幅/opacity整理と、服の不要auto成分の選別、現行Bloomの細線信号喪失を通常ゲームへ影響せず抑える方法の比較。元表情や髪の造形をRendererだけでイラストへ変更できたとは言わない。ここで止め、SDFの複雑化やDissolve/粒子へ先回りしない。
