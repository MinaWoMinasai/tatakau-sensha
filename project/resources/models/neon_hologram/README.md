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
| Animation | 0。既存SkinnedModelの空の`BindPose`クリップを利用 |

## Preview操作

PowerShellや起動引数は不要です。Visual Studioで`project/CG2.sln`を開き、**Development / x64**を選んでF5（またはCtrl+F5）で普通に起動します。タイトル画面の「遠征をはじめる」からTANK_EXPEDITIONへ進み、**F3「制作ツール」→「Neon Skinned Previewを開く」**を押すと、F12デバッグコンソールの**Neon Preview**タブが開きます。F12でコンソールを表示してNeon Previewタブを直接選んでも確認できます。従来の「ツール」タブ内のNeon Skinned Previewセクションも利用できます。

Previewは開いただけでは有効になりません。下記の`Preview Enable`をオンにしてください。

`Preview Enable`で初めてモデル・GPU資源を読み込みます。`Normal`は既存`Object3d::DrawSkinned()`、`Neon`は既存`NeonSkinnedRenderer::Draw()`を使い、同じモデル・Skeleton / Palette / Object3dのTransformを共有します。位置・回転（radian）・ScaleとNeonパラメータを操作できます。初期回転は正面（Y=0）です。`Place in front of camera`で現在のCameraの35単位前へ移動します。無効化してもGPU資源は保持し、フレーム途中で破棄しません。

初期状態はPreview無効です。タイトルのデモにはPreviewを作りません。ReleaseではPreviewクラスをビルド対象から外し、GameSceneの呼び出しもコンパイル時に無効にします。Downloadsはコピー時にのみ参照し、実行時はRepository内の`resources/models/neon_hologram/AvatarSample_B.glb`だけを使用します。

描画は`Bloom::PreDraw()`直後、ObjectPostEffectのcaptureより前のScene HDR / Normal / Material（3 MRT）+ D24S8内です。`BeginFrame()`は前フレームのFence完了後のUpdateで1回だけ呼び、Draw後は通常Object3dのRoot Signature / PSOを再Bindします。

## 暗い本体 + ネオン外周線

`Neon`の既定表示は暗い紫の本体と細いピンクの外周線です。以前の本体全体の固定発光を外し、同じSkinning Paletteで膨張したHullを追加描画します。既存のScene BloomへHDR発光を渡すので、専用Bloomや起動方法の変更は不要です。

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

Root Signatureは既存のTransform `b0`（VS）、Neon/Camera/Viewport `b1`（ALL）、Palette `t3`（VS）に、BaseColor `t0`（PS）、Submesh用2 DWORDのRoot Constants `b2`（PS）、linear clamp sampler `s0`を追加します。Neon定数は96 bytes、CBV領域は256 bytes、Submesh定数は8 bytesです。Material SRVは同じSrvManagerで初期化済みのSkinnedModelから共有し、DrawごとにCB内容とRoot Constantsを記録します。`SetSubmeshParams()`は空またはモデルのSubmesh数に一致する配列を渡してください。Renderer単体の既定値は内部線オフ・Cutoutなしのため、既存呼び出しは外周のみの表示を維持します。

参考イラストの描き込みをTexture境界だけで完全再現するものではありません。塗りの境界やハイライトも線になり、モデルに描かれていない髪の線・口の形を自動生成しません。遠距離の小さな顔、UV seam、細い髪のalpha境界では途切れやちらつきが残ります。次段階で線を厳密に指定する場合は専用Feature Maskを検討してください。Barycentricで全三角形を描く方式とは分けて設計します。

`project/tools/test_neon_skinned_pipeline.ps1`は実DXC・WARPで5種類のPSOを生成し、合成Skinned Geometryを実際に描画して、暗い本体、外周のHDR発光、線幅、Depth遮蔽、MRT / Stencil保持、Palette追従、同一フレームのDrawごとの定数、Texture内部線・無効化・Submesh強度・透明部分のDepth非書き込みを検証します。Development実機でもAvatarSample_Bを表示し、Normal / Neon切替、内部線オン／オフ、幅・閾値・Cutoutの操作とScene Bloomを確認しました（Animationは0のためBindPose）。

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

Repositoryには本モデルを収録しますが、現在の`TankSubmissionPackage.ps1`では**このGLBのパスだけ**を除外します。他のmodelsや本READMEは除外しません。正式なBossとしてReleaseで使用する段階で、`models/neon_hologram/AvatarSample_B.glb`の除外を解除してください。
