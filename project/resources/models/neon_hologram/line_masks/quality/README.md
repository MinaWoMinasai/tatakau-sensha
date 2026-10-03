# AvatarSample_B line quality — separate comparison candidates

Developer Preview-only制作データです。親ディレクトリの旧`authoring.json`、`bindings.json`、`face_candidate.png`、`bangs_candidate.png`は比較元として保持します。元のGLB・画像・UV・Mesh・Skin Weight・Skeleton・Animationには書き込みません。Runtimeでは制作済みPNGだけを読みます。

## Source / license

- Source: VRoid Project / AvatarSample_B、`../../AvatarSample_B.glb`
- SHA-256: `7FCA4A77FDC60AB2C78A9907430744562626180125FA386EB74FB2EA15C2E518`
- 元モデル由来の派生データです。CC0や完全な独自素材として扱いません。[元モデルの出典・利用条件](../../README.md)を保持します。
- 元の4K画像、Primitive UV、Alpha、実機で取得された前段階の画像を調査してエージェントが曲線を選別しました。作者が手描き・承認した完成原稿ではありません。全画像のエッジ検出結果を保存したものではありません。

## Comparison data

全PNGは1024×1024 RGBA8、LinearData、A=255です。sRGB / gamma / ICCプロファイルを付けません。`TEXCOORD_0`と画像左上原点を使用し、Assimp importer反転をエンジンが戻した後にもう一度Vを反転しません。

| Version | 原稿 | 比較する目的 |
| --- | --- | --- |
| `v1` | 顔12本・前髪3本。旧曲線の位置を維持し、上まぶた2.6 texels、瞳輪郭2.5 texels、前髪2.0 texels等に全幅を調整。弱い下まぶた・瞳孔のopacityは別値 | Coverageにも公平な幅・強度・狭いHaloを与え、距離場だけの効果と分ける |
| `v2` | 顔13本・前髪3本。まぶたを元Alphaの不透明側へ数texels移動、二重の唇を1本へ整理、別のまつげatlasに上側arcを2本選別。r2は虹彩下半を内側へ寄せ、前髪の全幅1.8 texels / opacity 0.65 | 必要線の位置と情報量の調整を、描画技術の変更と分ける |

`bindings.json`に5つのexact Material名・UV0・役割・coverage/SDFのペアを記録します。FaceSkin、EyeIris、FaceMouth、EyeHighlightが同じ顔ペアを共有し、Hair_01が前髪ペアを使用します。BodyとShoesには設定しません。両候補の原稿JSONは変更前の15曲線と独立しています。

各bindingの`coverageSha256` / `sdfSha256`は実PNG byteのSHA-256です。各versionの`authoringSha256`は全authoring.json byte、`authoringVersionSha256`はそのversionとsize / AA / range / Haloのcanonical JSON、`authoringRevision`は原稿revisionです。生成時に更新し、`--verify`で同値を確認します。version単独hashにより、V2を調整してもV1の原稿が同じことを識別できます。旧baselineのbindingsには追記しません。

### V2 r2 — 実機観察を受けた最小調整

初回の実機Coverage＋Core/Halo比較で、目の上側arcは読みやすくなる一方、虹彩下側が半円や点々に見え、頬脇の髪が顔より強い白い帯として見える箇所がありました。調整前V2のJSON・4 PNG・SHA manifestは`generated/neon_line_art_quality/v2_before_iris_adjustment/`へbyte-for-byteで保存しました。V1の4 PNGと旧15曲線は保持します。

`v2-iris-hair-r2`では虹彩下半の2制御点のvだけを`.964 → .948`へ変更し、端点・横座標・Face Core原稿強度は変えません。GLB image0の中心線513等間隔tサンプルでは、LOD0 bilinear参考値のAlpha≥0.03割合が左右37.6% / 27.5% → 80.5% / 80.5%、Alpha≥0.5が2.7% / 3.5% → 73.3% / 76.6%となりました。旧診断のnearest参照ではAlpha≥0.03が38.2% / 39.6% → 81.1% / 80.3%です。これは原本Alphaと原稿の位置対応の数値であり、実画面で見える割合ではありません。端付近のAlpha損失・通常遮蔽・縮小による欠けはまだ残り得ます。

同じ3本の髪曲線は全幅`2.2 → 1.8 texels`、独立opacity`1 → 0.65`へ弱め、顔の強度を巻き添えにしないようにしました。前髪として選別した曲線が頬側の長いstrandに見える部分もあり、UV対応・面の投影の制約を尊重して透明越しの線は追加しません。髪のどの表面へ効いたかは実機の同条件画像で再確認します。r2のデータ・WIC検証と実画面の品質評価は区別し、実機再起動後の比較前に改善完了とは扱いません。

再起動後の正面face比較を`project/generated/neon_line_art_quality/showcase_1791026920670/comparison_1791027130530/`へ保存しました。全7条件の1280×720 raw PNGとJSONについて、同じ停止姿勢・カメラ・Transform・Body・外周・Bloom、5 MaterialのPNG/原稿manifest SHAを検証しました。batch前後のJSON・raw PNG byteも一致しました（`generated/neon_line_art_quality/comparison_face_r2_manifest.json`）。SHAはロード時manifest期待値と現在のファイルの照合であり、GPU内のtexture内容をハッシュしたものではありません。

この正面距離では、虹彩の下半がより連続した輪郭として読み取れ、口と前髪の不要な自動線が整理されました。Coverage＋Core/HaloとSDF＋Core/Haloの見た目は近く、この画像をSDFだけによる大改善の証拠とは扱いません。頬脇・正中の髪が顔より強い箇所、残る元Alphaによる端付近の欠け、服の自動線の細かな段差は残っています。側面・拡大・縮小・動作中の結果はそれぞれの追加実機比較で評価します。

### Coverage texture

| Channel | 意味 |
| --- | --- |
| R | CoreのAA coverage × 曲線の独立opacity。4倍supersamplingしてLanczos縮小 |
| G | 自動線の置換・抑制率。G=1 / R=0による不要線除去も維持 |
| B | 狭い表面Haloのcoverage × 曲線opacity。Coreの外側4 texelsまで二乗減衰 |
| A | 常に255。モデル透明度や線強度には使用しない |

### Signed line-region distance texture

| Channel | 意味 |
| --- | --- |
| R | 線**領域**のsigned distanceを`.5 + distance / 32`でUNORM格納。内側が正、境界0、外側が負。単位は1024² atlasのtexel、範囲[-16,+16]、端ではclamp |
| G | coverage textureと同じ置換率 |
| B | 最寄りstrokeのopacity。形状の有無とは独立し、弱い線も距離場の内側として残す |
| A | 常に255 |

デコードは`(R - .5) * 32`です。8bitでは0境界が127/128の間にあり、量子化の最大誤差は約0.06275 texelsです。距離は中心線距離をそのまま線として表示する用途ではありません。中心線距離から各曲線の半幅を引いたround-cap stroke領域を再構成し、内側1つの帯として表示します。両境界を再度線として抽出しません。

Bezierを512区間のcapsuleへ近似し、外側距離は最近strokeまでの距離、caps / joinsはround、交差はsigned値のmaxでunionにします。交差の内側深さは各capsuleの最大値であり、union全体の厳密な内部Euclidean距離とは一致しないことがあります。0境界と内外はunion形状に対応します。幅が違うcurveも同じ規則です。opacityは最も近いstrokeの値を使い、交差で弱いstrokeが強いstrokeの形状を削除しません。

4 texelsのHaloより2 texels余裕を持たせた置換域を各stroke周辺へ加え、滑らかなtransitionでGの長方形境界が光を切らないようにしました。広い消去域のG=1/R=0は残します。Haloは表面上の狭い光であり、シルエットの外へ広がる画面Bloomとは別です。BaseColor Alpha Cutout・通常の遮蔽はRenderer側で維持します。

平均SDF MIPを正しい距離場として扱いません。近距離はLOD0距離と画面微分のAAで再構成し、縮小時は別coverage textureのfiltered R/Bへ切り替える比較構成です。UV伸縮、強い斜め視点、遠距離で消える線を距離場が自動修復する保証はありません。すべての遠距離細線を強制的に太くする設計でもありません。

coverageのBは4 texels幅のHaloを制作時に焼き込んでいます。SDF側のHalo幅を0より大きく4未満へ調整しても、遠距離のcoverage fallbackでは焼き込み4 texelsへ戻るため、全LODで任意幅を厳密に保つ保証はありません。幅0は特別に全LODでHaloを無効にします。比較プリセットでは両方式を4 texelsに揃え、任意の可変幅を検証済みと説明しません。

## Regeneration / verification

既存Python環境のPillow・NumPyだけを使用します。今回ライブラリのインストールは行いません。Runtimeで生成しません。

```text
python project/tools/generate_neon_quality_masks.py
python project/tools/generate_neon_quality_masks.py --verify
python project/tools/test_neon_quality_masks.py
```

Development / x64の既存DirectXTex libraryをビルド済みなら、実際のWIC / MIP経路も次で検証できます。

```text
powershell -NoProfile -File project/tools/test_neon_quality_data.ps1 -PythonPath "C:/Users/k024g/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe"
```

独立したPillowの全RGBA pixel byteと、`WIC_FLAGS_NONE`読み込み結果を照合します。実測では全8 PNGはLinear `B8G8R8A8_UNORM`（DXGI 87）となり、論理RGBAは全画素一致しました。各11 MIPでA=255・Linear format・RGB平均値を保持します。LOD0 signed距離の最大誤差は約0.062745 texels、定数(128,64,192,255) fixtureは全MIP不変です。SDFの平均MIPを距離場の正しい再構成として検証した意味ではありません。最終1×1距離は外側符号となり細線の形状を失うため、表面ShaderがLOD0とcoverage fallbackを使う理由も確認します。

通常生成はこのディレクトリの8つのPNGだけへ書き込みます。調査overlay、R/G/Halo単体、再生成コピー、数値reportは`generated/neon_quality_mask_inspection/`へ保存します。`--verify`は再生成byte一致、元GLBハッシュ、0境界量子化誤差、弱いopacityでも形状が残ること、G消去領域、HaloのG内包含、A=255、色変換chunk不在を確認します。追加数値テストはcaps / joins / 異幅交差、内外符号、距離単位、元の比較データのハッシュ、縮小coverageの信号保持、不正入力の拒否も確認します。テストPASSは見た目の完成を意味しません。

8 PNGの圧縮合計はr2で193,845 bytes（約189.3 KiB）です。GPU RGBA8のbase levelは計32 MiB、全通常MIPを生成する場合は計約42.7 MiBになります。同じPNGをMaterial間で共有します。PNG圧縮byte数をGPUメモリと扱いません。1 versionだけなら4 texture、base計16 MiB / 全MIP約21.3 MiBです。Runtimeキャッシュの実測は実機結果と別途照合します。

実GPU timestamp比較は`project/generated/neon_line_art_quality/showcase_1791028647958/`の4 CSVとJSONへ保存しました。RTX 4060 Laptop / Development / 1280×720 / scale1、同じ正面停止姿勢・Bloom等、各60 warmup＋300有効フレームです。Debug Layer有効、GPU-based validation無効で、元描画／V1 Coverage+Core/Halo／V1 SDF+Core/Halo／V2 r2 Coverage+Core/HaloのNeon scope meanは約1.206 / 1.205 / 1.267 / 1.207 msでした。SDF−Coverage約+0.062 msはこの直列1runの差であり、Release性能や一般的な速度差の保証ではありません。全scope mean/P95・入力条件・独立検証は`generated/neon_line_art_quality/gpu_comparison_review.md`とJSONに記録しています。CPU時間をGPU時間へ代用せず、親scopeと子scopeを合算していません。

## Known constraints

元UVの髪atlasには別表面での共有があります。今回の狭い前髪curve領域は前段階の調査範囲内です。任意のside/back hairへこの保証を広げません。別表面の目・まつげ、髪の正常な遮蔽、原本Alphaの複雑な切れは原稿だけで解決できません。目の線を見せるために裏側を透過表示したり、原本Alphaを書き換えたりしません。鼻・頬の装飾、服の追加模様、全身マスクは今回制作しません。

このサブディレクトリも既存の狭い`models/neon_hologram/line_masks/` Preview素材除外に含まれ、提出パッケージへ入れません。採用判断は同条件の実モデル画像・動画・負荷結果で行います。元の推奨プリセットと既存候補は保持します。
