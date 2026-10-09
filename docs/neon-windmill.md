# ネオン風車：動画観察と独自実装

2026-10-08。MinaWoMinasaiのC++ / DirectX 12自作エンジン上で動く、独立した表現デモ。

追補：[描画品質改善と実機比較](neon-windmill-quality.md)。Legacyは本書の旧描画を保持し、MでLine Art／Hybrid Goldへ切り替えられる。DevelopmentのF4で発光源の6寄与と時刻・背景を調整できる。以下の旧検証結果と、新しい48枚・連番・性能測定は追補で区別する。

## 観察した内容

参照は[青森の人の投稿](https://x.com/Aomorinohito_/status/2107822096130969986)と、提供された`キチガイ風車.mp4`。X本文は取得できなかったため、動きの分析は提供ファイルを優先した。ファイルは634×620、30fps、コンテナ上の長さ15.1333秒。1秒ごとの全体画像と、序盤・終盤の細かいフレームを`generated/windmill-reference/`へ抽出した。

- 中央の笑顔と、上下左右の四つの指で構成される。指は中央のまわりを回り、絵文字の向きも変わる。
- 前半は回転、後半はカメラとの相対距離が縮まり、終盤に回転が止まる。
- 終盤で、四つの指が閲覧者を指す手に変わり、顔も目が大きく開いた顔に切り替わる。その後、急な画面の動きと暗転がある。
- 輪郭と内部の陰影が一緒に変形し、奥行き方向の立体形状ははっきり見えない。**絵文字テクスチャの板ポリゴンという解釈が妥当**。動画だけでは元作品のメッシュ、シェーダー、ビルボード有無を確定できない。
- 接近が物体の移動か撮影カメラの移動かも確定できない。本デモでは相対距離の変化をキャラクター側のZ移動で表現する。

ユーザー指定に合わせ、iPhoneのApple版絵文字を使う。文字の同定はUnicodeの名称・コードポイント、外観は[Apple iOS 26.4の比較表示](https://emojipedia.org/apple/ios-26.4/beaming-face-with-smiling-eyes)と提供動画を照合した。認識後の顔は[Distorted Face](https://emojipedia.org/apple/ios-26.4/distorted-face)。[Unicode 17.0の文字表](https://unicode.org/charts/PDF/U1FA70.pdf)にもU+1FAEAとして記載されている。

| 用途 | 絵文字 | コードポイント |
| --- | --- | --- |
| 中央の笑顔 | 😁 | U+1F601 |
| 上 | 👆 | U+1F446 |
| 下 | 👇 | U+1F447 |
| 左 | 👈 | U+1F448 |
| 右 | 👉 | U+1F449 |
| 認識後の四つの手 | 🫵 | U+1FAF5 |
| 認識後の顔 | 🫪 | U+1FAEA |

## 本デモの設計

WindowsのSegoe UI Emojiへ置き換えず、Apple iOS 26.4の7画像をアトラスへ格納した。元の絵文字デザインはAppleの外部図版であり、作者の自作物とは区別する。フォントファイルのインストール・同梱は行わない。画像とアトラスはGit管理対象外の`generated/neon_windmill/`、出典情報は`project/resources/neon_windmill/iphone_sources.json`に置く。実装コードが独自であることは、図版の権利を変更しない。[Unicodeの図版についての説明](https://www.unicode.org/emoji/images.html)も参照。

5枚の板はワールドXY平面に配置する。頂点生成にはカメラのRight/Upを使わず、視点を横へ回すと板が薄く見える。これがカメラを向き続けるビルボードとの違い。通常時は四方向の別々の画像を使い、四つの位置を90度ずつ離して公転させ、各板も同じZ回転を受ける。中心の顔は回さない。認識時にはメッシュを閲覧者へ向けず、テクスチャを🫵と🫪へ切り替える。

手の中心は`center + (R cos θ, R sin θ, 0)`、R=1.65。四つのθに0、π/2、π、3π/2を加える。Zの揺れを入れない平面の円軌道へ修正した。約1.27秒で一周し、減速区間の角速度を積分する。停止角が10周ぴったりになるよう設定し、上下左右に整列する。これは元の動画を計測し尽くした値ではなく、観察をもとに調整したデモの値。

| デモ内の時刻 | 動作 |
| --- | --- |
| 0〜7秒 | 浮遊・回転。開始0.35秒は発光を立ち上げる |
| 7〜12秒 | 回転を続けながら近づく |
| 12〜13.4秒 | 滑らかに減速し、上下左右で止まる |
| 13.4〜14.2秒 | 指差しと顔の画像を切り替える |
| 14.2〜14.9秒 | 急接近 |
| 14.9〜16秒 | 消灯・リセット後、繰り返す |

## ネオン・ブルーム

独自の要素は板の配置、軌道、切り替え、Alpha輪郭の抽出、発光輪郭、残光、暗い展示空間、床照明、比較操作。エンジンに既にあるHDR・BloomPyramid・ACES合成を使う。

1. 絵文字のAlpha境界から凹凸を保った閉曲線を抽出し、頂点数を80点以下に簡略化する。四つの指にはシアン・ピンク・橙・紫、顔には暖色のHDR輪郭を重ねる。
2. 絵文字本体はSRGBテクスチャから線形RGBへ変換された色を使い、元の色と陰影を保つ。透明部分はPixel Shaderで破棄し、四角形の透明領域で背景を隠さない。
3. 部屋の面、絵文字板、発光線の順に描く。不透明な床は深度を書き、板もAlpha cutout後に深度を書く。既存のLDR HUD用PSOを流用せず、HDR Sceneの形式に対応したパイプラインを追加した。
4. 既存BloomPyramidのQualityモードを使用。threshold 0.8、soft knee 0.5、scatter 0.65、radius 1.0、gain初期値0.65、exposure 0.85、色相を保つACES（エンジンのmode 2）。絵文字本体の倍率は0.95。**ブルームを切っても絵文字と光源の芯は残り、光の広がりだけが変わる**。
5. 床への照り返しはブルームとは別に、顔・手の5つの点光源を使うLambert項と距離減衰で近似する。GI、鏡面反射、実際の物理的な発光量を計算しているわけではない。

HDR画像を表示範囲へ落とす考え方は[Microsoft DirectXTK12のHDR描画資料](https://github.com/microsoft/DirectXTK12/wiki/Using-HDR-rendering)、複数解像度のブルームは[Jorge JimenezのSIGGRAPH資料紹介](https://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare/)を参照した。今回は既存エンジンの実装を使い、これらの外部実装コードを追加コピーしていない。移動する細い線の履歴残像を避けるため、デモ内ではTAAとジッターを抑制する。

## 起動と操作

ルートの`ネオン風車.cmd`をダブルクリックする。通常の戦車ゲームと同じCG2.exeを、専用プロジェクト指定で起動する。Releaseにもデモが含まれる。

```powershell
Push-Location project
try {
    & ../generated/outputs/Release/CG2.exe --project resources/projects/neon_windmill.project.json
} finally { Pop-Location }
```

| 操作 | 動作 |
| --- | --- |
| Space | 停止・再開 |
| R | 先頭から再生 |
| B | 同じ姿勢・視点でブルームON/OFF |
| M | Legacy / Line Art / Hybrid Goldを同じ時刻・視点で切り替え |
| F4 | Development専用の発光設定・診断・連続シーク |
| 左右矢印 | 視点を回す。板の向きは固定 |
| 上下矢印 | 視点距離 |
| 1 / 2 / 3 | 回転 / 停止 / 認識の状態で静止 |
| + / - | ブルームの光量 |
| H | 説明表示の切り替え |
| P | DevelopmentのみPNGと実描画パラメーターを保存 |
| Esc | 終了 |

アトラスと新しい線形・乗算済みAlphaの作業textureの生成は、Pillow / NumPyが使えるPythonで`project/tools/prepare_windmill_emoji.py`を実行する。入力となる7画像は`generated/neon_windmill/iphone/`に保持する。初回の別環境では出典情報をもとに使用可能な画像を用意する必要がある。

## 検証

`project/tools/test_neon_windmill.ps1`でCPUの軌道テストとDevelopmentの実描画キャプチャを確認する。`-CpuOnly`で数学的な確認だけを実行できる。実描画では同一時刻のブルームOFF/ON、側面、停止、認識、説明付きの6枚をGPUバックバッファから取得する。画像は編集せずに保存し、JSONには実際に合成で使ったBloom・露出・時刻・カメラ・板数・頂点数を記録する。検証結果と画像は`generated/neon_windmill/`に出力する。

共通の`test_repository_validation.ps1`にはUnitとして登録し、共有スイートでは`-CpuOnly`を渡す。ローカルのApple図版がない環境でも軌道を確認できる。GPU検証はアトラスを用意したローカル環境で上記ラッパーを直接実行する。

2026-10-08の実行結果：Release / Developmentともビルド成功、コンパイラー警告なし。専用CPUテストと6枚のGPUキャプチャに成功（1280×720、5枚の板、3回の3D描画、約7.7万頂点）。ブルームON/OFFは同じ時刻・カメラ・露出で取得し、RGB差が3/255を超える画素を106,685個確認した。追加したC++の説明コメント58件にも不足はなかった。共通検証のinventory確認は、既存の`test_neon_contour_geometry.ps1`が未分類のため停止した。共通スイート全体を通過したとは扱っていない。

紹介文の例：「自作のC++ / DirectX 12エンジンで、iPhone絵文字の風車を3D空間に再構成しました。板ポリゴンの軌道、ネオン輪郭、床への照り返しを実装し、HDRブルームのON/OFFを同じ姿勢で比較できます。」絵文字図版はAppleのデザイン、動きと描画の実装は作者によるもの。
