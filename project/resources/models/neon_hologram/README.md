# AvatarSample_B — Neon Skinned Preview

| 項目 | 記録 |
| --- | --- |
| Model | AvatarSample_B |
| Creator / Project | VRoid Project（VRM内のtitle / authorも一致） |
| Original format | VRM（GLB 2.0コンテナ） |
| Repository format | GLB: `AvatarSample_B.glb` |
| 用途 | Neon Hologram character / boss renderingのDeveloper Preview。現在のRelease runtimeでは使用しない |
| 取得日 | 2026-10-01（作者提供のローカル入力をRepositoryへ取り込んだ日） |
| 入力ファイル名 | `6493143135142452442.vrm` |
| 取得元 | 作者のWindows Downloads内の指定ファイル。ダウンロード元URL・経路は未確認 |
| 公式利用条件 | [VRoid Project: AvatarSample A〜Z](https://vroid.pixiv.help/hc/ja/articles/4402394424089-AvatarSample-A-Z) |

このモデルは**CC0ではありません**。上記の公式条件では無料の営利・非営利利用、改変、キャラクターとしての利用が認められ、クレジット表記は不要とされています。改変したキャラクターモデルの公開・販売例も示されています。CC0への変更、キャラクター作成サービスへの利用、正当な理由のない有料再配布などの禁止事項を含む公式条件を確認してください。本書やRepositoryのコードライセンスはモデルをCC0として扱う根拠になりません。

今回の取り込みはVRMを**byte-for-byteでコピーし、出力ファイル名だけをGLBへ変更**したものです。Blender等での再エクスポート、バイナリ加工、再圧縮は行っていません。VRM拡張情報も元のまま残ります。元VRMはRepositoryに重複収録していません。

- ファイルサイズ: 28,333,772 bytes
- コピー元 / コピー先ともにSHA-256: `7FCA4A77FDC60AB2C78A9907430744562626180125FA386EB74FB2EA15C2E518`

## 読み込み確認

エンジンと同じAssimpフラグ（Triangulate / JoinIdenticalVertices / GenSmoothNormals / CalcTangentSpace / LimitBoneWeights / ImproveCacheLocality）で確認しました。再検証: `project/tools/test_neon_skinned_model.ps1`。レポートは `generated/neon_skinned_model_tests/report.json` に出力します。

| 項目 | 結果 |
| --- | --- |
| glTF Mesh / skin | 3 / 3（各skinのJointリストは113） |
| Assimp Mesh / 描画Submesh | 7 / 7 |
| Skeleton Node / Joint | 118（glTFの117 node + Assimpのroot） |
| AssimpのユニークBone名 | 85、正のWeightを持つBoneは84 |
| 頂点 / Skin Weight | 21,961頂点すべてに正規化済みWeightあり。正のWeight 40,145件、最大4 influence/vertex |
| Material | glTF 7 / Assimp 8（追加の未使用defaultを含む） |
| Embedded Texture | 6 |
| GLB由来のAnimation | 0。元姿勢比較には既存SkinnedModelの空の`BindPose`クリップを利用 |
| Preview生成クリップ | 2（`Preview_Idle` / `Preview_Attack`）。C++で生成し、GLBには保存しない |

## Preview操作

PowerShellや起動引数は不要です。Visual Studioで`project/CG2.sln`を開き、**Development / x64**を選んでF5（またはCtrl+F5）で普通に起動します。タイトル画面の「遠征をはじめる」からTANK_EXPEDITIONへ進み、**F3「制作ツール」→「Neon Skinned Previewを開く」**を押すと、F12デバッグコンソールの**Neon Preview**タブが開きます。F12でコンソールを表示してNeon Previewタブを直接選んでも確認できます。従来の「ツール」タブ内のNeon Skinned Previewセクションも利用できます。

Previewは開いただけでは有効になりません。下記の`Preview Enable`をオンにしてください。

`Preview Enable`で初めてモデル・GPU資源を読み込みます。`Normal`は既存`Object3d::DrawSkinned()`、`Neon`は既存`NeonSkinnedRenderer::Draw()`を使い、同じモデル・Skeleton / Palette / Object3dのTransformを共有します。位置・回転（radian）・ScaleとNeonパラメータを操作できます。初期回転は正面（Y=0）です。`Place in front of camera`で現在のCameraの35単位前へ移動します。無効化してもGPU資源は保持し、フレーム途中で破棄しません。

初期状態はPreview無効です。タイトルのデモにはPreviewを作りません。ReleaseではPreviewクラスをビルド対象から外し、GameSceneの呼び出しもコンパイル時に無効にします。Downloadsはコピー時にのみ参照し、実行時はRepository内の`resources/models/neon_hologram/AvatarSample_B.glb`だけを使用します。

描画は`Bloom::PreDraw()`直後、ObjectPostEffectのcaptureより前のScene HDR / Normal / Material（3 MRT）+ D24S8内です。`BeginFrame()`は前フレームのFence完了後のUpdateで1回だけ呼び、Draw後は通常Object3dのRoot Signature / PSOを再Bindします。

## 検証用アニメーション

初回有効化では`Preview_Idle`を再生します。`Animation`で元のTポーズの`BindPose`、3秒ループの`Preview_Idle`、1.8秒非ループの`Preview_Attack`を選択できます。Idleは腕を下げ、肘を軽く曲げ、胸・首・腕を小さく動かします。Attackは右腕を引いて溜め、前へ押し出して戻り、終了後に0.2秒の既存ブレンドでIdleへ戻ります。`Play Attack`は再度押すと先頭から再実行します。

`Play` / `Pause`、`Restart`、`Playback Speed`、現在のクリップ名・ループ状態・時刻 / durationを表示します。Pauseは進行中のポーズブレンドも停止します。Pause中の再生位置スライダーは遷移を終了し、その時刻のサンプル姿勢を表示します。Pause中のIdle / Attack選択だけでは現在姿勢を保持し、Playでブレンドを再開するかSeekで直接姿勢を指定できます。RestartはPause状態を維持し、指定クリップの時刻0へ戻します。BindPoseはPause中も直ちに元姿勢へ戻し、duration=0でスライダーを無効にします。Normal / Neonは再ロードや時刻リセットを行いません。

`game/debug/NeonPreviewAnimations.cpp`がロード時に一度だけ回転キーフレームを生成し、`cg2::SkinnedModel::RegisterAnimations()`が検証後に所有します。既存`AnimationPlayer` → `SkeletonSystem` → `SkinCluster` / PaletteをUpdateで一回だけ更新し、BodyとOutlineで共有します。登録は全クリップを検証してからvectorを確定し、Playerの参照を再接続して時刻・再生・遷移状態を保ちます。失敗時は既存状態を維持し、Previewに不足ボーン名等を表示してBindPoseを使用します。

対象は名前解決した`J_Bip_C_Chest`、`J_Bip_C_Neck`、`J_Bip_L_UpperArm`、`J_Bip_R_UpperArm`、`J_Bip_L_LowerArm`、`J_Bip_R_LowerArm`です。Assimp / エンジン変換後の腕のbind回転は単位Quaternion、左の子ボーンは+X、右は-Xに伸びます。行ベクトルの回転行列に対し`R(qA*qB)=R(qB)*R(qA)`となる合成順を数値テストで確認し、ローカル差分`Rx*Ry*Rz`の後にbind回転を適用します。左右の腕はZの符号を反転して下げ、肘はYで曲げます。Translation / Scale曲線は追加せずbind値を維持し、root・脚・足は動かしません。glTFからの座標変換は追加適用しません。

再検証は`project/tools/test_neon_preview_animations.ps1`。実GLBと既存Animation / Skeleton / SkinnedModel / SkinClusterをWARP upload bufferで数値検証します。Texture / descriptorサービスを隔離したテストのため、描画確認とは別です。登録の妥当性と失敗時の状態保持、vector再配置時の寿命、Quaternion正規化・合成順、Idle境界、途中姿勢・実WeightによるPalette変形、Pause / Resume / Restart / Seek / 速度、Attack再実行とIdle復帰、const getter維持を確認します。

2026-10-03のDevelopment実機では正面・側面のIdle / Attack、肩・肘・首、押し出し姿勢、同じ停止時刻でのNormal / Neon比較と外周・内部線の追従を確認しました。比較画像は`generated/neon_animation_preview/`に保存します。髪や細部の線の密集・一部の途切れ、Normal側の顔の白さは既存表現の制約です。全角度の連続動画によるちらつき評価や衝突の保証は行っていません。Spring Bone、布物理、表情、戦闘処理は追加していません。モデルのSHA-256は作業前後で上記と一致しました。

## 暗い本体 + ネオン外周線

Previewの初回表示は下記の`Recommended Line Art`を使用します。Renderer単体の既定表示は暗い紫の本体と細いピンクの外周線です。同じSkinning Paletteで膨張したHullを追加描画し、既存のScene BloomへHDR発光を渡します。

- `Reset: dark body + neon lines`: 色・強度・線幅を既定値へ戻し、内部線を有効にする。
- `Outline Enable`: 外周線のオン／オフ。
- `Outline width (pixels)`: 0〜8px、既定1.75px。0は線を描かない。
- `Neon line color` / `Neon line intensity (HDR)`: 線の色と発光強度。既定はピンク／12.0。細い線も既存Bloomの半解像度・輝度しきい値へ届きやすい値を使い、Scene全体のBloom設定は変更しない。
- `bodyColor`: 本体色。
- `Optional body rim`: 面の補助発光。既定`rimStrength=0`。外周線の幅とは独立。

本体を全Submesh描いた後、モデル全体のStencilマスクの外側だけに線を描きます。片面SubmeshはFront CullのHull、`doubleSided`の薄い面はCull Noneを使います。線はDepth Testを行い、DepthやNormal / Material MRTへ書き込みません。Stencil bit **0x80**はこのパス専用に予約し、Drawの前後でこのbitだけを消去します。他の7bitは保持します。同じDSVの0x80を別のパスが保持中の状態で呼び出さないでください。

`Draw(model, transformCbv, cameraPosition)`は既存の1280×720 Sceneサイズを使います。別サイズのRender Targetへ描く場合は`Draw(model, transformCbv, cameraPosition, viewportSize)`で実際のViewport寸法を渡してください。線幅は法線を画面方向へ投影して作るため、ハードエッジ・分割法線・正面を向いた薄い面などでは途切れや太さの差が出る場合があります。

## Texture由来の内部特徴線

更新済みPaletteと同じVertex UVを使い、既存BaseColor Textureの色・alphaの境界をNeonのBody PSで検出します。画面微分から上下左右のTextureを`SampleGrad`で読み、暗い本体にHDR発光を追加します。三角形の分割線は描きません。モデル・Textureのバイナリや通常Skinning Shaderは変更しません。

- `Internal Line Enable`: 内部線だけのオン／オフ。外周線とは独立。
- `Internal width (pixels)`: 境界検出のサンプル間隔、既定1.0px（0〜4px）。線の正確な幾何幅を保証する値ではない。
- `Internal intensity (HDR)`: 内部線の発光強度、既定8.0。色は外周と同じ`Neon line color`。
- `Internal edge threshold`: 色・alpha差の閾値、既定0.12。陰影や細かい模様を拾いすぎる場合は0.2程度へ上げ、太く見える場合は幅を0.65程度へ下げて比較する。
- `Texture alpha cutout`: Neonだけに簡易Cutoutを適用。Previewは既定オン。MASKは元の`alphaCutoff`、BLENDは0.03を初期値とする。
- `Submesh / Material diagnostics`: Submeshごとの`Line strength`と`Alpha cutoff`を調整。不要な模様のSubmeshは強度0で内部線だけを止められる。

Root SignatureはTransform `b0`（VS）、Neon/Camera/Viewport `b1`（ALL）、Palette `t3`（VS）、BaseColor `t0`（PS）、Submesh用4 DWORDのRoot Constants `b2`（PS）、専用線マスク `t1`（PS）、linear clamp sampler `s0`です（合計11 DWORD）。Neonパラメータは128 bytes、Camera/Viewportを含むGPU定数は160 bytes、CBV領域は256 bytes、Submesh定数は16 bytesです。Material SRVは同じSrvManagerで初期化済みのSkinnedModelから共有し、DrawごとにCB内容・Root Constants・マスクSRVを記録します。`SetSubmeshParams()`と`SetSubmeshFeatureMasks()`は空またはモデルのSubmesh数に一致する配列を渡してください。マスクの`nullopt`はR/G=0を返す有効なnull Texture2D SRVへ戻ります。実マスクは同じHeapのLinearData Texture2Dとし、GPU完了までTextureManagerのキャッシュ等で保持してください。Renderer単体の既定値は内部線・Geometry Lines・Body emission・マスク適用がオフ、Cutoutなしのため、既存呼び出しの表示を維持します。

参考イラストの描き込みをTexture境界だけで完全再現するものではありません。塗りの境界やハイライトも線になり、モデルに描かれていない髪の線・口の形を自動生成しません。遠距離の小さな顔、UV seam、細い髪のalpha境界では途切れやちらつきが残ります。次段階で線を厳密に指定する場合は専用Feature Maskを検討してください。Barycentricで全三角形を描く方式とは分けて設計します。

`project/tools/test_neon_skinned_pipeline.ps1`は実DXC・WARPでBody / Outline / Stencil用の5種類のPSOと、対応時にはGeometry用2種類のPSOを生成します。`-Hardware`で実GPUも選べます。合成Skinned Geometryのreadbackで、定数配置、暗い本体と補助発光、外周のHDR発光、線幅、Depth遮蔽、MRT / Stencil保持、Palette追従、Drawごとの定数、Texture内部線・Submesh強度 / 閾値・Alpha CutoutとGeometry Linesの独立性を検証します。実モデルの見た目確認とは別の検証です。

## 推奨ネオン線画プリセット

`Recommended Line Art`は外観設定だけを変更します。初回ロードにも適用します。`Legacy Neon comparison`（または既存Resetボタン）で調整前の設定と比較できます。両操作ともAnimation / Pause / Seek位置、Normal / Neon、Transform、手動のAlpha Cutout設定を保持し、モデルを再ロードしません。

- 外周：1.15px、HDR強度6、主線はピンク。内部線：サンプル間隔0.7px、HDR強度5、共通閾値0.16。
- 顔・目・口を優先し、髪の陰影帯と服の細かい模様を抑えます。`Submesh / Material diagnostics`の`Line strength`と`Internal threshold scale`で個別調整できます。後者は共通閾値への倍率です。
- `Body tint`と`Body emission`は線から独立しています。弱いピンク紫の面を視線と法線で補助し、通常ライティングは追加しません。Body emission=0で従来の固定Body色へ戻ります。
- `Mesh Geometry Lines (diagnostic)`は初期OFF・折りたたみです。OFF / Subtle / Full Mesh Diagnostic、色・幅・強度と各SubmeshのGeometry強度を引き続き比較できます。線画プリセットを適用するとGeometry LinesはOFFになります。

| Materialの役割 | 内部線強度 | 閾値倍率 |
| --- | ---: | ---: |
| FaceMouth | 1.4 | 0.8 |
| EyeIris | 1.15 | 1.0 |
| EyeHighlight | 0.45 | 1.0 |
| Face Skin | 1.25 | 0.35 |
| Hair | 0.75 | 1.15 |
| Body Skin | 0.12 | 1.8 |
| Shoes / Cloth | 0.45 | 1.4 |

線は既存テクスチャの描き込みに依存します。鼻先や頬の薄い線が全距離で読める保証はなく、髪のハイライト境界と服の模様を意味的な線だけへ完全分離するものではありません。

## 専用線マスクの比較試作

初回のPreview表示では専用マスクを適用せず、従来の推奨線画を保持します。`Authored Feature Mask (candidate)`の`Load / apply mask candidate`で、`line_masks/bindings.json`に指定した顔・髪の候補PNGを一度だけ読み込みます。`Auto lines only`と`Apply candidate`で同じ姿勢を比較でき、`Mask application`で適用率、`Authored line color` / `Authored line intensity (HDR)`で専用線だけの色・強度を調整できます。これらの操作は外周・Body・Geometry設定、モデルTransform、再生時刻を変更しません。

`Mask display`のR / G診断はモデル表面のUVに従うグレースケール表示です。Rは専用線のcoverage、Gは自動内部線を置き換える領域で、G=1 / R=0は本体を残したまま内部線だけを消します。適用率0なら診断を選んだままでも従来表示へ戻ります。診断ではマスク未指定のSubmeshは黒く表示します。BaseColor alphaによるCutoutはこのマスクから独立しています。

各対象Materialのパス・LinearData RGBA8 / UV0・解像度・読込状態を表示します。無効な設定や画像の読み込み失敗では該当Materialの自動内部線を保持し、UIへエラーを表示します。TextureManagerのキャッシュとGPU資源の寿命を保持するため、候補の修正はアプリケーション再起動後に反映してください。今回選んだ前髪の領域では前後の重複はありませんが、髪全体にはUV共有があり、対象領域を広げると他の束にも線が現れる可能性があります。制作方法・元データのハッシュ・対象Materialと調査結果は[line_masks/README.md](line_masks/README.md)を参照してください。

## Materialの制約

現在のNeonパスは簡易Alpha Cutout付きの不透明描画です。透明部分は本体のDepth / StencilとHullの両方から除外します。BLENDもCutoutで近似するため、半透明合成・ソートやMToon、Morph Target、Spring Bone、Expressionを再現しません。Normal側は既存の汎用Skinning Materialをそのまま使用します。

実機では`Face (merged).baked-2` / `N00_000_00_EyeHighlight_00_EYE (Instance)`（BLEND、doubleSided=true）の透明な板がCutoutなしでは瞳を覆い、顔の細部が失われます。NeonのCutoutで軽減しますが、小さな瞳や髪の端では線が密になり、Hairの描かれたハイライトも特徴線に含まれます。通常描画の顔が白く見える点も既存Material経路の制約です。Skinning不具合と区別し、大規模なVRM Material対応は行っていません。

| Submesh（Assimp名） | Material | alpha mode | doubleSided |
| --- | --- | --- | --- |
| `Face (merged).baked-0` | `N00_000_00_FaceMouth_00_FACE (Instance)` | MASK | false |
| `Face (merged).baked-1` | `N00_000_00_EyeIris_00_EYE (Instance)` | BLEND | false |
| `Face (merged).baked-2` | `N00_000_00_EyeHighlight_00_EYE (Instance)` | BLEND | true |
| `Face (merged).baked-3` | `N00_000_00_Face_00_SKIN (Instance)` | MASK | true |
| `Body (merged).baked-0` | `N00_000_00_Body_00_SKIN (Instance)` | MASK | false |
| `Body (merged).baked-1` | `N00_005_01_Shoes_01_CLOTH (Instance)` | MASK | true |
| `Hair001 (merged).baked` | `N00_000_Hair_00_HAIR_01 (Instance)` | MASK | true |

## Submission Package

Repositoryには本モデルを収録しますが、現在の`TankSubmissionPackage.ps1`では**このGLBのパスとPreview専用の`models/neon_hologram/line_masks/`内だけ**を除外します。他のmodels、隣接ディレクトリ、本READMEは除外しません。正式なBossとしてReleaseで使用する段階で、モデルと必要なマスクの除外を見直してください。
