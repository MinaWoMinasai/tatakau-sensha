# 虹色の炎 / VFX Lab

Development 構成を起動し、タイトルで **F7** を押すと開きます。
水中ラボは **F6** です。VFX Lab を直接起動する場合は、作業ディレクトリを
`project` にして次を実行します。

```powershell
& '../generated/outputs/Development/CG2.exe' --project resources/projects/vfx_lab.project.json
```

## 操作

- Space: 一時停止 / 再開（パネルの Pause でも操作可能）
- R: 現在の調整値を維持し、形・色・粒を同じ初期状態に戻す
- H: VFX Lab の操作パネルの表示切替
- Esc: タイトルに戻る
- A / D、W / S: カメラの旋回 / 仰角、マウスホイール: 拡大縮小

## 描画の構成

テクスチャ不要のカメラ向きビルボードです。根元2点、上昇する炎片6点、
離脱する小片4点の密度場を、上向きに流れるノイズで変形しています。
虹色の輪郭、上部の色付きの炎片、白とシアンの芯を別々に合成し、
最大8個の星と28個の小さなリング・光点を重ねます。
既存の HDR / Bloom / トーンマッピングを使用しています。

輪郭幅は密度の勾配で補正し、合流点で太くなりすぎるのを防ぎます。
ノイズの格子点には整数ハッシュを使用しています。以前の浮動小数点
ハッシュは最適化時の丸めの差で格子の継ぎ目が現れ、偽の輪郭を生むためです。

## 主な調整項目

- Flow Speed: 形・色・星・微粒子を一緒に速度変更（0で停止）
- Domain Warp / Distortion、Noise Scale: 炎のうねりと細部
- Satellite Separation: 離脱する小片の広がり
- Rainbow Band Width / Intensity: 輪郭の細さと明るさ
- Core Threshold / Breakup: 白い芯の範囲と崩れ方
- Star Spark Size / Intensity: 星の大きさと発光

Enable Star Sparks は星と微粒子の両方を切り替えます。
表示モードで密度場、塗り、輪郭、芯のマスクを個別に確認できます。
Restart Flame は調整値を維持します。

炎は実測時間で進行し、長い停止からの復帰は1フレーム0.1秒に制限します。
`GetFinalDeltaTime()` はゲーム共通のスロー演出判定にも使われるため、
ラボ内の実測時間とは分離しています。

## 確認

Development / x64 ビルド成功。ピクセルシェーダーは DXC の `ps_6_0` で
`-O3` と `-Od` の両方、頂点シェーダーは `vs_6_0` でコンパイル確認済みです。
実機で複数時刻の描画、格子状の継ぎ目の解消、Pause と Restart を確認しました。
参照動画のフレーム抽出と確認画像・ビルドログは Git 管理外の
`generated/flame-reference/` に保存しています。
