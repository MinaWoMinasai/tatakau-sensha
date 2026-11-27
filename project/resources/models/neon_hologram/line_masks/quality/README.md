# AvatarSample_B line quality — comparison masks

Developer Preview 専用の、線の見え方をくらべるためのマスクです。親ディレクトリの `authoring.json`、`bindings.json`、`face_candidate.png`、`bangs_candidate.png` は比較元として残します。元の GLB・画像・UV・Mesh・Skin Weight・Skeleton・Animation には書き込みません。実行時は作った PNG だけを読みます。

## Source / license

- Source: VRoid Project / AvatarSample_B、`../../AvatarSample_B.glb`
- SHA-256: `7FCA4A77FDC60AB2C78A9907430744562626180125FA386EB74FB2EA15C2E518`
- 元のモデルから作った派生データです。CC0 や独自の素材としては扱わず、[元のモデルの出典・利用条件](../../README.md) に従います。
- 元の 4K の画像、Primitive の UV、Alpha から、線の曲線を選んで描いたものです。画像全体のエッジ検出の結果ではありません。

## Comparison data

全 PNG は 1024×1024 RGBA8、LinearData、A=255 です。sRGB / gamma / ICC プロファイルは付けません。`TEXCOORD_0` と画像の左上の原点を使い、Assimp の importer の反転をエンジンが戻した後に、もう一度 V を反転しません。

| Version | 中身 | くらべる目的 |
| --- | --- | --- |
| `v1` | 顔 12 本・前髪 3 本。親ディレクトリの曲線の位置のまま、上まぶた 2.6 texels、瞳の輪郭 2.5 texels、前髪 2.0 texels などに全幅を変える。弱い下まぶた・瞳孔の opacity は別の値 | coverage にも同じ幅・強さ・狭い Halo を与え、距離場だけの効果と分ける |
| `v2` | 顔 13 本・前髪 3 本。まぶたを元の Alpha の不透明な側へ数 texels 動かし、二重の唇を1本にし、別のまつげの atlas に上側の弧を2本足す。r2 では虹彩の下半分の2つの制御点の v を `.964 → .948` にし、前髪の全幅を 1.8 texels / opacity 0.65 にする | 線の位置と量の調整を、描き方の変更と分ける |
| `v3` | v2 r2 の顔の原稿・顔の PNG と同じ。左の髪の流れだけ、末尾 t=.72〜1 で全幅 1.8→1.2 texels / opacity .65→.25 へなめらかに弱める。他の 2 本と置き換えの多角形はそのまま | 頬の横の白い帯を、その所だけ弱める候補。v2 を上書きせず、選んでくらべる |

`bindings.json` に、5 つの Material の名前・UV0・役割・coverage/SDF の組を書きます。FaceSkin、EyeIris、FaceMouth、EyeHighlight は同じ顔の組を共有し、Hair_01 は前髪の組を使います。Body と Shoes には決めていません。

各 binding の `coverageSha256` / `sdfSha256` は PNG のバイト列の SHA-256 です。各 version の `authoringSha256` は authoring.json 全体のバイト列、`authoringVersionSha256` はその version と size / AA / range / Halo の canonical JSON、`authoringRevision` は原稿の revision です。作るときに書き、`--verify` で同じ値かを見ます。version ごとの hash で、v2 を調整しても v1 の原稿が変わっていないことが分かります。

### v3 の髪の末尾

`localTailProfile` は t=.72 まで元の幅・opacity を保ち、その後 smoothstep で末尾の値へ弱めます。SDF は 512 区間ごとに一番近い t の半幅と opacity を使う、細くなる capsule の近似です。この曲線だけは、4 倍の解像度の内外判定から coverage を縮小します。v1/v2 と他の一定幅の曲線は、前と同じ raster の作り方です。顔の 2 PNG は v2 とバイト列が同じで、UV・点の位置・髪の他の 2 本・Alpha は変えていません。髪は UV を共有するので、別の髪の束の末尾も弱まることがあります。

### Coverage texture

| Channel | 意味 |
| --- | --- |
| R | Core の AA coverage × 曲線の opacity。4 倍で描いて Lanczos で縮小 |
| G | 自動の線を置き換える・抑える割合。G=1 / R=0 で不要な線を消す |
| B | 狭い表面の Halo の coverage × 曲線の opacity。Core の外側 4 texels まで二乗で弱まる |
| A | 常に 255。モデルの透明度や線の強さには使わない |

### Signed line-region distance texture

| Channel | 意味 |
| --- | --- |
| R | 線の**領域**の signed distance を `.5 + distance / 32` で UNORM に入れる。内側が正、境界が 0、外側が負。単位は 1024² の atlas の texel、範囲は [-16,+16]、端では clamp |
| G | coverage texture と同じ置き換えの割合 |
| B | 一番近い stroke の opacity。形とは別で、弱い線も距離場の内側に残る |
| A | 常に 255 |

戻し方は `(R - .5) * 32` です。8bit では 0 の境界が 127/128 の間にあり、量子化の誤差は最大で約 0.06275 texels です。中心線からの距離から各曲線の半幅を引いた round-cap の stroke の領域を作り、内側を1つの帯として表示します。

Bezier を 512 区間の capsule に近づけ、外側の距離は一番近い stroke までの距離、端とつなぎ目は round、交わる所は signed の値の max で合わせます。交わる所の内側の深さは各 capsule の最大値なので、全体の正確な内部の距離とは違うことがあります。0 の境界と内外は、合わせた形に合います。opacity は一番近い stroke の値で、交わる所で弱い stroke が強い stroke の形を削りません。

4 texels の Halo より 2 texels 広い置き換えの領域を各 stroke のまわりに足し、なめらかに変わるようにして、G の長方形の境界が光を切らないようにしています。広い消す領域の G=1/R=0 は残します。Halo は表面の上の狭い光で、シルエットの外へ広がる画面の Bloom とは別です。BaseColor の Alpha Cutout・通常の遮蔽は Renderer の側で働きます。

SDF の平均 MIP は距離場として使いません。近くでは LOD0 の距離と画面の微分の AA で作り、縮小したときは coverage texture の filtered R/B へ切り替えます。coverage の B は 4 texels の Halo を作るときに焼き込んでいるので、SDF 側の Halo の幅を 0 より大きく 4 未満にしても、遠くでは 4 texels に戻ります。幅 0 は、全 LOD で Halo を無効にします。比較のプリセットは、両方を 4 texels にそろえています。

### 大きさ

v1・v2 の 8 PNG の合計は 193,845 bytes（約 189.3 KiB）です。GPU の RGBA8 では、1 version（4 texture）で base が 16 MiB、全 MIP で約 21.3 MiB、3 version すべてで base が 48 MiB、全 MIP で約 64 MiB です。同じ PNG は Material の間で共有します。

## Regeneration

Pillow・NumPy を使う Python のスクリプトで作ります。実行時には作りません。

```text
python project/tools/generate_neon_quality_masks.py
python project/tools/generate_neon_quality_masks.py --verify
```

通常の実行は、このディレクトリの 12 PNG を作ります。`--verify` は、作り直したバイト列が同じか、元の GLB のハッシュ、0 の境界の量子化の誤差、弱い opacity でも形が残るか、G で消す領域、Halo が G の中にあるか、A=255、色変換の情報が無いかを見ます。スクリプトは作者の手元にあり、リポジトリには入っていません（`project/tools/` は git の対象外）。

## Known constraints

元の UV の髪の atlas には、別の表面との共有があります。この狭い前髪の曲線の領域の外へ広げるときは、共有を調べ直してください。別の表面の目・まつげ、髪の通常の遮蔽、元の Alpha の複雑な切れ目は、原稿だけでは直せません。目の線を見せるために裏側を透かしたり、元の Alpha を書き換えたりはしません。鼻・頬の飾り、服の模様、全身のマスクは対象外です。

このサブディレクトリも `models/neon_hologram/line_masks/` の Preview 素材として、提出パッケージには入れません。
