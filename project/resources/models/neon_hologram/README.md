# AvatarSample_B — Neon Skinned Preview

ボスのネオン表示（Neon Skinned Visual）と、開発用の Neon Skinned Preview で使うモデルです。

| 項目 | 内容 |
| --- | --- |
| Model | AvatarSample_B |
| Creator / Project | VRoid Project（VRM 内の title / author も同じ） |
| Original format | VRM（GLB 2.0 コンテナ） |
| Repository format | GLB: `AvatarSample_B.glb` |
| 用途 | 本編ボスの Neon Skinned Visual（Development / Release）と Developer Preview |
| 取り込んだ日 | 2026-10-01 |
| 入力ファイル名 | `6493143135142452442.vrm` |
| 公式利用条件 | [VRoid Project: AvatarSample A〜Z](https://vroid.pixiv.help/hc/ja/articles/4402394424089-AvatarSample-A-Z) |

## 出典と利用条件

このモデルは**CC0ではありません**。上記の公式条件では、無料の営利・非営利利用、改変、キャラクターとしての利用が認められ、クレジット表記は不要とされています。改変したキャラクターモデルの公開・販売例も示されています。CC0への変更、キャラクター作成サービスへの利用、正当な理由のない有料再配布などの禁止事項を含む公式条件に従ってください。本書やリポジトリのコードのライセンスは、モデルを CC0 として扱う根拠になりません。

VRM を**バイト列を変えずにコピーし、ファイル名だけを GLB にした**ものです。Blender などでの再エクスポート、バイナリの加工、再圧縮はしていません。VRM の拡張情報も元のまま残ります。元の VRM は重ねて収録していません。

- ファイルサイズ: 28,333,772 bytes
- SHA-256: `7FCA4A77FDC60AB2C78A9907430744562626180125FA386EB74FB2EA15C2E518`

## モデルの内容

エンジンと同じ Assimp のフラグ（Triangulate / JoinIdenticalVertices / GenSmoothNormals / CalcTangentSpace / LimitBoneWeights / ImproveCacheLocality）で読み込んだときの内容です。

| 項目 | 内容 |
| --- | --- |
| glTF Mesh / skin | 3 / 3（各 skin の Joint リストは 113） |
| Assimp Mesh / 描画 Submesh | 7 / 7 |
| Skeleton Node / Joint | 118（glTF の 117 node + Assimp の root） |
| Assimp のユニークな Bone 名 | 85、正の Weight を持つ Bone は 84 |
| 頂点 / Skin Weight | 21,961 頂点すべてに正規化済みの Weight。正の Weight 40,145 件、1 頂点あたり最大 4 |
| Material | glTF 7 / Assimp 8（使われない default を含む） |
| Embedded Texture | 6 |
| GLB のアニメーション | 0。元の姿勢には SkinnedModel の空の `BindPose` クリップを使う |
| Preview で作るクリップ | 2（`Preview_Idle` / `Preview_Attack`）。C++ で作り、GLB には保存しない |

## Preview の開き方

Visual Studio で `project/MyEngine.sln` を開き、**Development / x64** で起動します。タイトル画面の「遠征をはじめる」から TANK_EXPEDITION へ進み、**F3「制作ツール」→「Neon Skinned Previewを開く」**を押すと、F12 デバッグコンソールの **Neon Preview** タブが開きます。F12 でコンソールを出して Neon Preview タブを直接選ぶこともできます。「ツール」タブ内の Neon Skinned Preview の節からも使えます。

Preview は `Preview Enable` をオンにすると、モデルと GPU 資源を読み込みます。`Normal` は `Object3d::DrawSkinned()`、`Neon` は `NeonSkinnedRenderer::Draw()` で描き、同じモデル・Skeleton / Palette・Object3d の Transform を共有します。位置・回転（radian）・Scale と Neon のパラメータを操作できます。初期回転は正面（Y=0）です。`Place in front of camera` で今の Camera の 35 単位前へ移します。無効にしても GPU 資源は保持し、フレームの途中で破棄しません。

初期状態では Preview は無効です。タイトルのデモには Preview を作りません。Release では Preview のクラスをビルドから外し、GameScene からの呼び出しもコンパイル時に無効にします。本編のボスは別の Presentation component から、同じ GLB と NeonSkinnedRenderer、生成したクリップを使います。実行時に読むのは `resources/models/neon_hologram/AvatarSample_B.glb` だけです。

描画は `Bloom::PreDraw()` の直後、ObjectPostEffect の capture より前の Scene HDR / Normal / Material（3 MRT）+ D24S8 の中です。`BeginFrame()` は前フレームの Fence 完了後の Update で1回だけ呼び、Draw の後は通常の Object3d の Root Signature / PSO をもう一度 Bind します。

## アニメーション

初めて有効にしたときは `Preview_Idle` を再生します。`Animation` で、元の T ポーズの `BindPose`、3 秒ループの `Preview_Idle`、1.8 秒で終わる `Preview_Attack` を選べます。Idle は腕を下げ、肘を軽く曲げ、胸・首・腕を小さく動かします。Attack は右腕を引いて溜め、前へ押し出して戻り、終わると 0.2 秒のブレンドで Idle へ戻ります。`Play Attack` をもう一度押すと先頭から再生します。

`Play` / `Pause`、`Restart`、`Playback Speed`、今のクリップ名・ループ状態・時刻 / duration を表示します。Pause はポーズのブレンドも止めます。Pause 中に再生位置のスライダーを動かすと遷移を終え、その時刻の姿勢を表示します。Pause 中に Idle / Attack を選ぶだけでは今の姿勢を保ち、Play でブレンドを再開するか、Seek で直接姿勢を決めます。Restart は Pause の状態を保ったまま、選んだクリップの時刻 0 へ戻します。BindPose は Pause 中もすぐ元の姿勢へ戻し、duration=0 なのでスライダーは無効になります。Normal / Neon を切り替えても、再ロードや時刻のリセットはしません。

`game/debug/NeonPreviewAnimations.cpp` がロード時に一度だけ回転のキーフレームを作り、`myEngine::SkinnedModel::RegisterAnimations()` が受け取って持ちます。`AnimationPlayer` → `SkeletonSystem` → `SkinCluster` / Palette を Update で1回だけ更新し、Body と Outline で共有します。登録は全クリップを確かめてから確定し、Player の参照をつなぎ直して時刻・再生・遷移の状態を保ちます。失敗したときは前の状態を保ち、Preview に足りないボーン名などを出して BindPose を使います。

動かすボーンは `J_Bip_C_Chest`、`J_Bip_C_Neck`、`J_Bip_L_UpperArm`、`J_Bip_R_UpperArm`、`J_Bip_L_LowerArm`、`J_Bip_R_LowerArm` です。Assimp / エンジンで変換した後の腕の bind 回転は単位 Quaternion で、左の子ボーンは +X、右は -X に伸びます。行ベクトルの回転行列では `R(qA*qB)=R(qB)*R(qA)` の順に合成し、ローカルの差分 `Rx*Ry*Rz` の後に bind 回転を当てます。左右の腕は Z の符号を反対にして下げ、肘は Y で曲げます。Translation / Scale の曲線は足さず bind の値のままで、root・脚・足は動かしません。glTF からの座標変換は足していません。Spring Bone、布の物理、表情、戦闘の処理はありません。

## 暗い本体 + ネオンの外周線

Preview を初めて開いたときは、下の `Recommended Line Art` を使います。Renderer の既定の表示は、暗い紫の本体と細いピンクの外周線です。同じ Skinning Palette で膨らませた Hull を重ねて描き、Scene Bloom へ HDR の光を渡します。

- `Reset: dark body + neon lines`: 色・強さ・線幅を既定値へ戻し、内部線を有効にする。
- `Outline Enable`: 外周線のオン／オフ。
- `Outline width (pixels)`: 0〜8px、既定 1.75px。0 なら線を描かない。
- `Neon line color` / `Neon line intensity (HDR)`: 線の色と光の強さ。既定はピンク／12.0。細い線でも Bloom の半解像度・輝度のしきい値に届く値で、Scene 全体の Bloom の設定は変えない。
- `bodyColor`: 本体の色。
- `Optional body rim`: 面の補助の光。既定 `rimStrength=0`。外周線の幅とは別。

本体の Submesh を全部描いた後、モデル全体の Stencil マスクの外側だけに線を描きます。片面の Submesh は Front Cull の Hull、`doubleSided` の薄い面は Cull None を使います。線は Depth Test をし、Depth や Normal / Material の MRT へは書きません。Stencil の bit **0x80** はこのパス専用で、Draw の前後でこの bit だけを消します。他の 7bit は保持します。同じ DSV の 0x80 を別のパスが使っている間は呼ばないでください。

`Draw(model, transformCbv, cameraPosition)` は 1280×720 の Scene の大きさを使います。別の大きさの Render Target へ描くときは、`Draw(model, transformCbv, cameraPosition, viewportSize)` で実際の Viewport の寸法を渡してください。線幅は法線を画面の向きへ投影して作るので、ハードエッジ・分割した法線・正面を向いた薄い面では、途切れや太さの差が出ます。

## Texture から作る内部の線

更新した Palette と同じ頂点の UV を使い、BaseColor Texture の色・alpha の境界を Neon の Body PS で見つけます。画面の微分から上下左右の Texture を `SampleGrad` で読み、暗い本体に HDR の光を足します。三角形の分割線は描きません。モデル・Texture のバイナリや通常の Skinning Shader は変えません。

- `Internal Line Enable`: 内部線だけのオン／オフ。外周線とは別。
- `Internal width (pixels)`: 境界を探すサンプルの間隔、既定 1.0px（0〜4px）。線の幾何の幅そのものではない。
- `Internal intensity (HDR)`: 内部線の光の強さ、既定 8.0。色は外周と同じ `Neon line color`。
- `Internal edge threshold`: 色・alpha の差のしきい値、既定 0.12。陰影や細かい模様を拾いすぎるときは 0.2 ほどへ上げ、太く見えるときは幅を 0.65 ほどへ下げる。
- `Texture alpha cutout`: Neon だけに簡易の Cutout を当てる。Preview は既定でオン。MASK は元の `alphaCutoff`、BLEND は 0.03 が初期値。
- `Submesh / Material diagnostics`: Submesh ごとの `Line strength` と `Alpha cutoff`。不要な模様の Submesh は、強さ 0 で内部線だけを止められる。

Root Signature は、Transform `b0`（VS）、Neon/Camera/Viewport `b1`（ALL）、Palette `t3`（VS）、BaseColor `t0`（PS）、Submesh 用の 4 DWORD の Root Constants `b2`（PS）、専用の線マスク `t1`（PS）、linear clamp sampler `s0` です（合計 11 DWORD）。Neon のパラメータは 128 bytes、Camera/Viewport を含む GPU 定数は 160 bytes、CBV 領域は 256 bytes、Submesh の定数は 16 bytes です。Material の SRV は、同じ SrvManager で初期化した SkinnedModel と共有し、Draw ごとに CB の内容・Root Constants・マスクの SRV を記録します。`SetSubmeshParams()` と `SetSubmeshFeatureMasks()` には、空か、モデルの Submesh 数と同じ数の配列を渡してください。マスクの `nullopt` は R/G=0 を返す null Texture2D SRV になります。実際のマスクは同じ Heap の LinearData Texture2D にし、GPU が使い終わるまで TextureManager のキャッシュなどで保持してください。Renderer の既定値では、内部線・Geometry Lines・Body emission・マスクの適用はオフ、Cutout は無しなので、前からの呼び出しの表示は変わりません。

Texture の境界から作る線なので、塗りの境界やハイライトも線になり、モデルに描かれていない髪の線・口の形は作りません。遠くの小さな顔、UV seam、細い髪の alpha の境界では、線が途切れたりちらついたりします。線を細かく決めたいときは、下の専用の線マスクを使います。

## おすすめのネオン線画プリセット

`Recommended Line Art` は見た目の設定だけを変えます。初めて読み込んだときにも当たります。`Legacy Neon comparison`（または Reset ボタン）で、前の設定とくらべられます。どちらも Animation / Pause / Seek の位置、Normal / Neon、Transform、手で決めた Alpha Cutout の設定を保ち、モデルを読み直しません。

- 外周：1.15px、HDR 強さ 6、主線はピンク。内部線：サンプル間隔 0.7px、HDR 強さ 5、共通のしきい値 0.16。
- 顔・目・口を優先し、髪の陰影の帯と服の細かい模様を抑えます。`Submesh / Material diagnostics` の `Line strength` と `Internal threshold scale` で個別に変えられます。後者は共通のしきい値への倍率です。
- `Body tint` と `Body emission` は線とは別です。弱いピンク紫の面を視線と法線で補い、通常のライティングは足しません。Body emission=0 で前の固定の Body の色へ戻ります。
- `Mesh Geometry Lines (diagnostic)` は初めは OFF で、折りたたんであります。OFF / Subtle / Full Mesh Diagnostic、色・幅・強さと各 Submesh の Geometry の強さをくらべられます。線画プリセットを当てると Geometry Lines は OFF になります。

| Material の役割 | 内部線の強さ | しきい値の倍率 |
| --- | ---: | ---: |
| FaceMouth | 1.4 | 0.8 |
| EyeIris | 1.15 | 1.0 |
| EyeHighlight | 0.45 | 1.0 |
| Face Skin | 1.25 | 0.35 |
| Hair | 0.75 | 1.15 |
| Body Skin | 0.12 | 1.8 |
| Shoes / Cloth | 0.45 | 1.4 |

## 専用の線マスク（比較用）

Preview を初めて開いたときは専用マスクを使わず、おすすめの線画のままです。`Authored Feature Mask (candidate)` の `Load / apply mask candidate` で、`line_masks/bindings.json` に書いた顔・髪の PNG を一度だけ読み込みます。`Auto lines only` と `Apply candidate` で同じ姿勢をくらべられ、`Mask application` で当てる割合、`Authored line color` / `Authored line intensity (HDR)` で専用の線だけの色・強さを変えられます。これらは外周・Body・Geometry の設定、モデルの Transform、再生の時刻を変えません。

`Mask display` の R / G 表示は、モデルの表面の UV に沿ったグレースケールです。R は専用の線の coverage、G は自動の内部線を置き換える領域で、G=1 / R=0 は本体を残したまま内部線だけを消します。当てる割合が 0 なら、表示を選んだままでも前の表示に戻ります。表示では、マスクを決めていない Submesh は黒になります。BaseColor の alpha による Cutout は、このマスクとは別です。

対象の Material ごとに、パス・LinearData RGBA8 / UV0・解像度・読み込みの状態を表示します。設定や画像が読めないときは、その Material の自動の内部線を保ち、UI にエラーを出します。TextureManager のキャッシュと GPU 資源を保持するので、PNG を変えたときはアプリケーションを起動し直してください。髪には UV を共有する所があるので、対象の領域を広げると他の束にも線が出ることがあります。作り方・元データのハッシュ・対象の Material は [line_masks/README.md](line_masks/README.md) にあります。

## Material の制約

Neon のパスは、簡易の Alpha Cutout 付きの不透明な描画です。透明な部分は、本体の Depth / Stencil と Hull の両方から外します。BLEND も Cutout で近づけるので、半透明の合成・ソートや MToon、Morph Target、Spring Bone、Expression は再現しません。Normal 側は汎用の Skinning Material のままで、顔が白く見えます。

`Face (merged).baked-2` / `N00_000_00_EyeHighlight_00_EYE (Instance)`（BLEND、doubleSided=true）の透明な板は、Cutout が無いと瞳を覆い、顔の細部が消えます。Neon の Cutout で軽くしますが、小さな瞳や髪の端では線が密になり、Hair に描かれたハイライトも線になります。

| Submesh（Assimp 名） | Material | alpha mode | doubleSided |
| --- | --- | --- | --- |
| `Face (merged).baked-0` | `N00_000_00_FaceMouth_00_FACE (Instance)` | MASK | false |
| `Face (merged).baked-1` | `N00_000_00_EyeIris_00_EYE (Instance)` | BLEND | false |
| `Face (merged).baked-2` | `N00_000_00_EyeHighlight_00_EYE (Instance)` | BLEND | true |
| `Face (merged).baked-3` | `N00_000_00_Face_00_SKIN (Instance)` | MASK | true |
| `Body (merged).baked-0` | `N00_000_00_Body_00_SKIN (Instance)` | MASK | false |
| `Body (merged).baked-1` | `N00_005_01_Shoes_01_CLOTH (Instance)` | MASK | true |
| `Hair001 (merged).baked` | `N00_000_Hair_00_HAIR_01 (Instance)` | MASK | true |

## 提出パッケージ

リポジトリにはこのモデルを収録しますが、`TankSubmissionPackage.ps1` は**この GLB と、Preview 専用の `models/neon_hologram/line_masks/` の中だけ**を外します。他の models、隣のディレクトリ、この README は外しません。

## 配布する前に

- モデルのダウンロード元の URL を記録する（今の記録は、取り込んだ日と入力ファイル名だけ）。
- 正式なボスとして Release で使うときは、この GLB と必要なマスクを提出パッケージから外す設定を見直す。
