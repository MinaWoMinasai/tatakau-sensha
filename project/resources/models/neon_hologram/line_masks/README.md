# AvatarSample_B — 顔・前髪のネオン線マスク

Developer Preview 専用の、顔と前髪の線のマスクです。Preview を初めて開いたときは自動の線のままで、UI から読み込んで当てると、くらべられます。元のモデル・BaseColor の画像・UV・Mesh・Skeleton・Animation は変えません。coverage のマスクで、SDF や画面上で一定の線幅にするものではありません。

## 出典とライセンス

- 元のモデル: `../AvatarSample_B.glb`（VRoid Project の AvatarSample_B）
- 元のモデルの SHA-256: `7FCA4A77FDC60AB2C78A9907430744562626180125FA386EB74FB2EA15C2E518`
- 出典・利用条件: [モデルの README](../README.md) / [VRoid 公式 AvatarSample A〜Z](https://vroid.pixiv.help/hc/ja/articles/4402394424089-AvatarSample-A-Z)
- マスクは元のモデルのテクスチャ・UV から作った派生データです。CC0 や独自の素材としては扱わず、元の利用条件に従います。

## 作り方

元の画像と UV から、目・閉じた唇・前髪の線を Bezier 曲線で描き、小さな多角形で自動の線を置き換える範囲を決めたマスクです。画像全体のエッジ検出の結果ではありません。目の面を白く塗りつぶさず、鼻・頬の飾り・服の模様は入れていません。

- `authoring.json`：顔の 12 本の曲線、前髪の 3 本の曲線、6 つの置き換えの多角形。
  - `points`：正規化した UV の 4 つの制御点
  - `width_texels`：1024² のマスク上の全幅
  - `coverage`：線の濃さ
  - `replace_regions`：自動の線を置き換える範囲
- 曲線を 4 倍の解像度で描いて Lanczos で縮小し、2 つの PNG を作ります。作るのは制作のときだけで、実行時は PNG を読みます。

## データの形式

2 つの PNG は 1024×1024 の 8bit RGBA です。元の 4096×4096 の atlas の 4 分の 1 の解像度です。

| Channel | 意味 |
| --- | --- |
| R | 専用の線の coverage。0 は線なし、255 は線あり。境界と弱い補助線は中間の値 |
| G | 自動の内部線から専用の線へ置き換える範囲。0 は前のまま、255 は全部置き換え |
| B | 使わない。全画素 0 |
| A | モデルの透明度には使わない。全画素 255。R/G に掛けない |

`G=255/R=0` は、不要な自動の線だけを消す画素です。本体・Depth・Stencil は消しません。元の BaseColor の Alpha Cutout はそのまま働きます。LinearData として読み、sRGB の変換はしません。PNG にも sRGB/gamma の情報は付けません。

`TextureManager` と同じ `WIC_FLAGS_NONE` で読むと、Linear な `B8G8R8A8_UNORM`（DXGI format 87）になります。メモリ上の BGRA の順は SRV の format に従って読まれ、読み込んだ後の R/G は PNG の R/G と同じです。SRV の format を RGBA に変えたり、チャンネルを入れ替えたりはしません。

**A=255 にしてください。** 低い Alpha では、MIP を作るときに RGB が Alpha で重み付けされることがあります。

## Material・UV との対応

すべて `TEXCOORD_0` で、画像の左上を原点にした glTF の UV をそのまま使います。Assimp の glTF importer は V を反転して返し、エンジンの `SkinningModelLoader` が `1-v` で戻すので、PNG の側では反転しません。

| PNG | 対応する Material | 元の BaseColor | 置き換える所 |
| --- | --- | --- | --- |
| `face_candidate.png` | `N00_000_00_Face_00_SKIN (Instance)` | GLB image0、4096² | 両まぶたのまわりの小さな領域、閉じた唇 |
| 同上 | `N00_000_00_EyeIris_00_EYE (Instance)` | 同上 | 瞳の細かい色の境界を整理し、輪郭と短い瞳孔の線を残す |
| 同上 | `N00_000_00_FaceMouth_00_FACE (Instance)` | 同上 | G と重ならないので前のまま。口の中・白目は塗らない |
| 同上 | `N00_000_00_EyeHighlight_00_EYE (Instance)` | 同上 | G と重ならないので前のまま。まつげ・ハイライトは残す |
| `bangs_candidate.png` | `N00_000_Hair_00_HAIR_01 (Instance)` | GLB image3、4096² | `u=.542〜.685 / v=.160〜.365` の前髪。広い自動のハイライトの境界を置き換え、3 本の流れの線を描く |

`bindings.json` は、Material の名前・PNG のファイル名・UV set 0 の対応です。Body / Shoes などにはマスクを決めていません。顔と髪の atlas が他の Material と共有されていても、そちらには当たりません。

## 表示の制約

Face は左右で別の atlas の領域に曲線を描いています。Hair は同じ UV を別の 3D の位置で使う所があり、UV のマスクだけで1本の髪を選ぶことはできません。この前髪の多角形の中には、その重なりはありません（前側の頂点 514、後頭部の頂点 0）。境界で接する髪や、他の領域へ広げるときは、重なりを調べ直してください。

線幅は UV 上の coverage なので、遠くでは MIP と縮小で薄くなり、細い線が消えたりにじんだりします。BaseColor の Alpha で消える画素には、専用の線も出ません。髪の輪郭全体・口の中・鼻・頬は対象外です。PNG を変えたときは、Preview を起動し直してください。

このディレクトリの PNG と設定は Developer Preview 専用で、提出パッケージには入れません。
