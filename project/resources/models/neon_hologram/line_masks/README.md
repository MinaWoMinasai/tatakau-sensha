# AvatarSample_B — 顔・前髪のネオン線マスク候補

Developer Preview専用の試作です。初回表示は従来の自動抽出のままで、UIから明示的に読み込み・適用して比較します。元モデル・BaseColor画像・UV・Mesh・Skeleton・Animationは変更しません。これはcoverageマスクであり、SDFや一定の画面線幅を保証するものではありません。

## 入力と出典

- 入力: `../AvatarSample_B.glb`（VRoid ProjectのAvatarSample_B）
- 入力SHA-256: `7FCA4A77FDC60AB2C78A9907430744562626180125FA386EB74FB2EA15C2E518`
- 出典・利用条件: [モデルREADME](../README.md) / [VRoid公式AvatarSample A〜Z](https://vroid.pixiv.help/hc/ja/articles/4402394424089-AvatarSample-A-Z)
- マスクは元モデルのテクスチャ・UVに基づく派生データです。CC0・完全な独自素材として扱いません。元の利用条件を保持します。

## データ仕様

両PNGは1024×1024の8bit RGBAです。元の4096×4096 atlasに対して4分の1の解像度で、限定した細い曲線を4倍supersamplingして縮小します。Previewの全身表示から顔拡大までの試作に必要な規模を選び、元画像の複製はresourcesへ追加しません。

| Channel | 意味 |
| --- | --- |
| R | 専用線のcoverage。0は線なし、255は線あり。境界と弱い補助線には中間値 |
| G | 自動内部線から専用線へ置き換える範囲。0は従来維持、255は完全置換 |
| B | 未使用、全画素0 |
| A | モデル透明度に使用しない。全画素255。R/Gを乗算しない |

`G=255/R=0`は不要な自動線だけを消す有効な画素です。本体・Depth・Stencilを消しません。原本BaseColorのAlpha Cutoutは独立して維持します。LinearDataとして読み込み、sRGB変換をしません。PNGにもsRGB/gamma変換情報を付加しません。

実際の`TextureManager`と同じ`WIC_FLAGS_NONE`の読み込みでは、両候補はLinearな`B8G8R8A8_UNORM`（DXGI format 87）になりました。メモリ上のBGRA順序はSRVのformatに従って解釈され、読み込み後の論理R/GはPNGのR/Gと一致します。PNG保存時のRGBA順序へ合わせるためにSRVを強制的にRGBA formatへ変更したり、チャンネルを手動で入れ替えたりしません。

本候補では**A=255を必須**にします。A=64の検証画像もWIC読み込み直後の論理RGBAは元値と一致しますが、既存のMIP生成経路では低AlphaによるRGBの重み付け・量子化が起こり得ます。任意AlphaのMIP不変性は保証しません。A=255の候補では、既存`TEX_FILTER_DEFAULT`によるLinear formatの保持・全MIPのB=0/A=255・元MIP全画素・最終MIPのR/G平均値を検証しています。A=255の定数中間値は全MIPで元値と一致します。

## Material・UV対応

すべて`TEXCOORD_0`、画像の左上を原点にしたglTFのUVをそのまま使用します。AssimpのglTF importerはVを反転して返し、エンジンの`SkinningModelLoader`が`1-v`で戻します。PNG側でさらに反転しません。7 Assimp Submeshの230頂点を元glTFのPOSITIONで照合し、エンジンUVとglTF UVの最大差は`4.999e-10`でした。

| PNG | 対応Material | 元BaseColor | 置き換える部分 |
| --- | --- | --- | --- |
| `face_candidate.png` | `N00_000_00_Face_00_SKIN (Instance)` | GLB image0、4096² | 両まぶた周辺の小領域、閉じた唇 |
| 同上 | `N00_000_00_EyeIris_00_EYE (Instance)` | 同上 | 両瞳の細かな色境界を整理し、輪郭と短い瞳孔線を残す |
| 同上 | `N00_000_00_FaceMouth_00_FACE (Instance)` | 同上 | 候補のGと重ならないため従来維持。口内部・白目は新しく塗らない |
| 同上 | `N00_000_00_EyeHighlight_00_EYE (Instance)` | 同上 | 候補のGと重ならないため従来維持。既存のまつげ・ハイライトは残す |
| `bangs_candidate.png` | `N00_000_Hair_00_HAIR_01 (Instance)` | GLB image3、4096² | `u=.542〜.685 / v=.160〜.365`の前髪。幅広い自動ハイライト境界を置換し、3本の連続した流れの線を指定 |

`bindings.json`はexact Material名・PNG basename・UV set 0の対応です。Body / Shoes等は未指定で、顔atlasと髪atlasが他Materialにも共有されていても、そちらにはマスクを設定しません。

## 選別・修正と再生成

元画像全体のエッジ検出を保存した候補ではありません。生成担当エージェントが元画像とprimitiveごとのUV三角形overlayを確認し、目・閉じた唇・前髪だけをBezier曲線と小さな置換polygonで選別しました。目の面を白く塗りつぶさず、鼻・頬の装飾・服の模様は追加していません。作者が手描き・承認した完成素材という意味ではありません。

- **自動化した部分**: GLB画像のbyte-for-byte展開、UV overlay、座標/共有領域集計、JSONの曲線を4倍で描画してLanczos縮小、検証。
- **選別した部分**: `authoring.json`の12本の顔曲線、3本の前髪曲線、6つの置換polygon。全画像の自動抽出候補は今回使用していません。
- **調整するデータ**: `points`（正規化UVの4制御点）、`width_texels`（1024²マスク上の全幅）、`coverage`、`replace_regions`。再生成は制作時だけ行い、RuntimeではPNGを読みます。

Repository rootで、Pillow・NumPyが使える既存Pythonから実行します。入力GLBには書き込みません。

```text
python project/tools/generate_neon_feature_masks.py --inspect
python project/tools/generate_neon_feature_masks.py
python project/tools/generate_neon_feature_masks.py --verify
```

`--inspect`は`generated/neon_feature_mask_inspection/`へ原画像・UV overlay・material/alpha/UV reportを出力します。通常実行は本ディレクトリの2 PNGだけを再生成し、R/G単体・選別overlayはgeneratedへ置きます。`--verify`はgenerated側へ再生成してbyte一致、R/G中間値、B=0/A=255、G=1/R=0、限定領域からの漏れ、色変換情報の不在を確認します。PNG変更はPreviewを再起動して反映してください。

Development / x64を先にビルドしたうえで、制作データと実際のWIC / LinearData / MIP経路は次のテストで再検証できます。`-PythonPath`にはPillow・NumPyが使える既存Pythonの実行ファイルを指定します。

```text
powershell -NoProfile -File project/tools/test_neon_feature_masks.ps1 -PythonPath "C:/Users/k024g/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe"
```

このテストは独立したPillowのRGBA値とWICの全画素を比較し、向き・論理チャンネル・中間値・読み込み時のAlpha非乗算を確認します。さらに実候補・A=255の中間値画像・checker画像のMIPを検証します。A=64画像については読み込み直後の比較だけを行い、低AlphaのMIP不変性を検証済みとは扱いません。

追加のUV数値比較は、`generated/neon_feature_mask_inspection/assimp_uv_probe.cpp`を既存Assimpライブラリで実行したCSVに対して行いました。

```text
python project/tools/generate_neon_feature_masks.py --compare-assimp generated/neon_feature_mask_inspection/assimp_uv_samples.csv
```

## UV・表示の制約

Faceは左右別のatlas領域に曲線を指定しています。Hair全体には同じUVを別の3D位置で使う1259組があり、UVマスクだけで任意の1本の髪を識別することはできません。ただし今回の前髪polygon内にはその重複がなく、前側の頂点514、後頭部の頂点0です（前側`Z<-.035/Y>1.35`、後ろ側`Z>.035`で元object-space位置を分類）。UV三角形・位置・範囲を調べてこの狭い領域に限定しました。境界で接する髪、側面の見え方、他の将来領域には同じ保証を広げません。UV・メッシュの作り直しはしていません。

線幅はUV上のcoverageであり、遠距離ではMIPと縮小で薄くなり、細い線が消えたりにじむ場合があります。BaseColor Alphaで消える画素には専用線も出ません。髪の輪郭全体・口内部・鼻・頬は今回の制作対象外です。実モデルでのOFF/ON比較と品質評価は、単体マスクやPSO生成の成功とは別に行います。

このディレクトリのPNGと設定はDeveloper Preview専用で、提出パッケージへ収録しません。親ディレクトリのGLB除外、他のモデル・文書の収録は維持します。

## 実モデルでの比較結果（2026-10-03）

Development / RTX 4060 Laptop GPUで5 Materialの読み込み成功を確認しました。既存Recommended Line Art、Geometry Lines OFFを基準に、Idleの停止姿勢で顔拡大・全身のOFF/ON、R/G診断、90度の側面を確認しました。Idle再生と、Attackの0.870秒へSeekした姿勢も確認し、その停止姿勢でNormal / Neonを切り替えても時刻と姿勢は維持されました。外周とAlpha Cutoutを維持したまま、選択した内部線だけを置換しています。

未加工のエンジンスクリーンショットは`generated/neon_feature_mask_preview/`へ保存しました。

| 比較 | OFF | ON | 条件 |
| --- | --- | --- | --- |
| 顔拡大・固定比較 | `fixed_close_auto_idle_1_917.jpg` | `fixed_close_mask_idle_1_917.jpg` | Idle 1.917秒、位置(34,20,-45)、Scale 8。既存F3制作ツールのPauseでゲームとカメラを停止。両画像とも既存メニューのモノクロ効果が掛かるため、線の配置比較に使用 |
| 全身・固定比較 | `fixed_full_auto_idle_1_917.jpg` | `fixed_full_mask_idle_1_917.jpg` | Idle 1.917秒、位置(34,23.6,-20)、Scale 8。同じ制作ツールPauseとメニュー効果 |
| 通常カラー・顔拡大 | `idle_front_close_auto_1_750.jpg` | `idle_front_close_mask_1_750.jpg` | Idle 1.750秒、同一Preview Transformと発光設定。ゲームの追従カメラに僅かな位置変化があり、画素一致比較には使用しない |

R/G診断は`idle_front_close_R_1_750.jpg` / `idle_front_close_G_1_750.jpg`、側面は`idle_side_close_mask_1_750.jpg`、Attack比較は`attack_front_normal_0_870.jpg` / `attack_front_neon_0_870.jpg`です。画像を後加工して線や発光を変更していません。

**機能評価:** 置換領域内の自動線を消し、指定したcoverageへ置き換える制御が実モデル上で動作しました。前髪の幅広いハイライト境界を減らし、少数の縦方向の流れを出せました。範囲外の髪飾りなどは従来のままです。今回確認した側面では後頭部へ同じ新しい流れの線が出る現象は見られませんでしたが、全視点でのUV共有の影響を保証するものではありません。

**品質評価:** 顔はまだ未達です。目元の細かな線は減りますが、新しい輪郭が細く弱く、全身表示では読み取りにくくなります。顔・瞳の別表面、前髪の被さり、縮小MIPにより、選んだ曲線のすべてが意図した見え方にはなっていません。唇の線は拡大時に確認できます。前髪も表面境界や遮蔽で線が途切れる箇所があり、完成したイラスト風の線画とは扱いません。曲線位置・幅・coverageを同条件で再調整する必要があり、推奨プリセットは置き換えません。

Idleの短時間再生では線がモデルに追従することを確認しました。Attackは途中の停止姿勢と終了後のIdle復帰を確認しましたが、全区間を連続して観察した視覚検証や、長時間・遠距離でのちらつき評価は未実施です。SDFや一定の画面線幅は実装していません。
