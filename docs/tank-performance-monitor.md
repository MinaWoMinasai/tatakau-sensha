# 性能モニターと60 FPS制御

## 開き方

開発機能が有効な構成で使えます。Developmentでは標準で有効、Releaseでは標準で無効です。Releaseで使用する場合は `CG2DeveloperTools=true` を指定してビルドします。[開発機能の切り替え](developer-tools-switch.md)を参照してください。モニターは通常起動時には非表示です。

| 操作 | 動作 |
| --- | --- |
| F1 | OFF → 簡易表示 → 詳細表示 → OFF |
| Shift＋F1 | 詳細表示のCPU更新 → GPU全体 → 描画パス → 描画量を循環 |

表示は約250 msごとの平均値。グラフは直近120フレームの実時間を示す。描画量ページでは弾・敵・パーティクル数、軌跡の頂点数・描画呼び出し・転送量・上限による省略数を確認できる。

環境変数`CG2_PERF_DISABLED=1`を設定して起動すると、モニター・ショートカット・GPU計測・CSV記録を無効化する。Releaseでは診断用ImGuiも初期化しない。通常のF1で非表示にした場合も、CSV記録を指定していなければ詳細計測を停止する。

## 数字の読み方

| 項目 | 意味 |
| --- | --- |
| FPS／ms/frame | FPS制御の終了点同士で測った実経過時間。60 FPSは約16.67 ms |
| CPU処理（推定） | 全体時間からPresent・GPUフェンス・FPS制限の待ちを除いた時間。CPU使用率ではない |
| GPU描画 | GPU timestampによる実測。CPUが描画命令を作った時間とは別 |
| Present | 画面提示APIにかかった時間 |
| GPU待ち | GPU完了をCPUが待った時間。GPU描画時間そのものではない |
| 60FPS待ち | 余った時間を60 FPS上限に合わせて待った時間。大きくても処理の重さを意味しない |

CPU・GPUには親項目とその内訳があるため、全行を合計しない。例えば「3D全体」は「弾・軌跡と個別発光」などを含む。描画パスページでは同じ処理のCPU命令作成とGPU実行を並べて比べられる。

GPU値は既存のフレーム終了フェンスを利用して回収し、計測だけのためにGPU待機を追加しない。リソース読み込みがフレーム途中にコマンドを送信した場合は、フェンス値の変化から検知してそのフレームのGPU値を除外する。CPUの読み込み待ちをGPU負荷として数えないためで、有効なGPU値がない表示区間は`N/A`になる。CSVの`gpu_valid`が0のフレームも同様。

## 60 FPS制御

既定で有効。従来の約15.4～16.7 msのフレームが待機を抜ける条件を修正し、Windows高精度タイマーで残り時間を待つ。OSの待機精度やCPU／GPUの処理時間によって60 FPSを下回ることはあり、性能不足を補う機能ではない。VSyncとは別の上限制御。

`CG2_FRAME_LIMIT=0`で解除できるが、**現在のゲームには固定1/60秒を前提にした処理があるため、解除するとゲーム速度が変わる。通常の試遊では有効のまま使う。**

## 見た目を保つ最適化

- 軌跡を同じ順序・形状・色・太さでまとめて描画する。寿命や補間の密度は下げず、軌跡間は面積ゼロの三角形で接続する。変更のない頂点は再計算・再転送を省く。
- 頂点領域を8,192頂点から必要量に合わせて拡張する。極端な設定への上限は262,144頂点で、省略した分は「上限超過で省略した頂点」に表示する。通常の試遊ではこの値が0であることを確認する。
- ガウスぼかしは半径・重み・発光強度を保ち、隣接サンプルの線形補間を利用して片方向9回の取得を5回へ削減する。GPUの補間丸めによる微小差はありうる。
- 弾・表示可能な軌跡・近接リボンがすべてない場合は、その専用発光キャプチャを省略する。表示する内容がある時のブルームは維持する。

## CSVで比較する

Releaseをビルドしたうえで、リポジトリのルートからPowerShellで実行する。各コマンドは対象アプリの終了まで待つため、順番に実行する。

```powershell
# 60 FPS制御を含む通常設定
./project/tools/measure_tank_performance.ps1 -Configuration Release -Trails 512 -Frames 300

# 同じ頂点を個別描画する比較経路と、まとめ描画を上限なしで比較
./project/tools/measure_tank_performance.ps1 -Configuration Release -Trails 512 -Frames 300 -Uncapped -LegacyTrails
./project/tools/measure_tank_performance.ps1 -Configuration Release -Trails 512 -Frames 300 -Uncapped
```

開始機体選択画面の既存背景に、毎フレーム同じ規則で更新する512本の測定用軌跡を重ねる。**敵AIや実弾の戦闘を再現する試験ではない。** 120フレームの準備後に300フレームを記録し、自動終了する。`-LegacyTrails`は同じ頂点を1本ずつ描く比較設定で、過去の実装全体へ戻すものではない。スクリプトは変更した環境変数を終了時に戻す。

出力先は`project/generated/performance/`。CSVは`frame,category,name,value`形式で、`frame`／`cpu`／`gpu`はms（`gpu_valid`を除く）、`count`は各項目の個数・bytes。任意のプレイを記録したい場合は`CG2_PERF_CAPTURE_FRAMES`、`CG2_PERF_CAPTURE_WARMUP`、`CG2_PERF_CAPTURE_PATH`を指定して通常起動する。記録だけならF1表示を開く必要はない。

### このPCでの測定結果

Release、上記512本、各300フレームの平均。両経路とも実形状71,680頂点、省略0。

| 設定 | ms/frame | FPS | 軌跡CPU ms | 軌跡GPU ms | 軌跡の描画呼び出し |
| --- | ---: | ---: | ---: | ---: | ---: |
| 個別描画・上限なし | 7.171 | 139.44 | 1.613 | 0.092 | 512 |
| まとめ描画・上限なし | 5.891 | 169.75 | 0.866 | 0.096 | 1 |
| まとめ描画・60 FPS制御 | 16.703 | 59.87 | 1.081 | 0.373 | 1 |

この比較では、軌跡のCPU時間が約46%減少した。軌跡GPU時間の改善は確認できず、主な効果はCPU側の描画呼び出し削減だった。60 FPS制御時は平均約5.50 msを制限のために待っている。制限ON／OFFのGPU値は動作状態の影響も含むため、その差を最適化効果として扱わない。実戦の敵AI・衝突判定・弾数による負荷は、ゲーム中のモニターで別途確認する。

## 動作確認

2026-09-19にRelease／Developmentの両構成をビルドし、Releaseの実画面でF1の簡易表示・詳細表示・非表示と、Shift＋F1の4ページ切り替えを確認した。`CG2_PERF_DISABLED=1`で診断用ImGuiを初期化しないReleaseでも、Variant 4の遠征自動テスト（5部屋、バンクショット進化、4回の整備、音声読み込み）が完走した。これは動作確認であり、難易度の評価ではない。

フレーム待機のWindows実機テスト、軌跡の頂点属性・三角形の一致と容量・キャッシュのテスト、ぼかしシェーダーのコンパイルと数値比較も通過した。

## 参照仕様

- Windowsの高精度タイマー: [CreateWaitableTimerExW](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerexw)、[SetWaitableTimerEx](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-setwaitabletimerex)
- GPU timestampの取得と周波数による換算: [Direct3D 12 Timing](https://learn.microsoft.com/en-us/windows/win32/direct3d12/timing)、[ResolveQueryData](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-resolvequerydata)
