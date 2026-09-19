# 60 FPS 上限制御の更新と検証

60 FPS 制限は起動時から有効。`DirectXCommon::SetFrameLimitEnabled(bool)` で診断用に解除できる。現在のゲーム処理は 1/60 秒を前提にしている部分があるため、解除は描画性能を調べる目的に限定する。

従来の処理には「1/65 秒未満の時だけ待つ」という条件があり、約 15.385～16.667 ms のフレームは待機せずに次へ進んでいた。新しい `FramePacer` はこの抜けをなくし、16.666667 ms に満たない間は残り時間を待つ。遅いフレームの後も新たな 1/60 秒の区間を始め、遅れを取り戻すための短い連続フレームは発生させない。

待機は Windows の高精度 waitable timer を使用し、最終 0.5 ms 以内だけ CPU の `YieldProcessor()` で時刻を確認する。高精度タイマーを作れない環境は通常タイマー、それも失敗した場合は標準ライブラリの待機へフォールバックする。システム全体のタイマー分解能は変更しない。ハンドルは再初期化と破棄時に解放する。

OS のスケジューリングや GPU の処理が遅れた場合まで、60 FPS を保証するものではない。VSync/表示モニターとの同期とは別の上限制御で、既存の `Present(0, 0)` と GPU フェンス待機順序は維持している。

## 計測値の意味

`GetFramePacingStats()` は直近の以下の値を返す。

| 値 | 意味 |
| --- | --- |
| `frameMs` | 前回の FPS 制御終了から今回終了までの実経過時間 |
| `preLimitMs` | 制御開始までの実経過時間。GPU フェンス・Present の待機も含む |
| `waitMs` | FPS 制限による待機時間。末尾の時刻確認も含む。解除時は 0 |
| `overshootMs` | 16.666667 ms を超えた時間。解除時は 0 |

`preLimitMs - fenceWaitMs - presentMs` は制限と GPU 待機を除くメインスレッド処理時間の目安になる。ただし OS によるスレッド休止なども含み、CPU 使用率や GPU 実行時間とは異なる。GPU コストの評価には GPU timestamp 計測を使う。

## Windows 実行確認

`project/tools/test_frame_pacer.ps1` で実際のタイマーを使い、MSVC `/W4 /WX /O2` でビルド・実行した。

最終設定（末尾 0.5 ms、120 フレーム）の一回の観測値:

| 項目 | 結果 |
| --- | --- |
| 高精度タイマー | 有効 |
| 平均フレーム時間 | 16.7207 ms |
| 平均 FPS | 59.8062 |
| 最短／最長 | 16.6667 ms ／ 17.0433 ms |
| 待機だけのスレッド CPU 時間 | 約 46.875 ms / 実時間約 2.007 秒 |
| 1 コアに対する CPU 使用割合 | 約 2.34% |

CPU 時間は `GetThreadTimes` の値のため量子化誤差を含む。この計測はゲームや GPU の性能テストではなく、待機の精度と CPU 消費の確認である。

追加確認もすべて成功:

- 実仕事に見立てた約 16 ms の後でも残り時間を待つ（8/8）。
- 約 24 ms の重いフレームは待機を足さず、次の軽いフレームは再び 1/60 秒を待つ。
- 制限 OFF 時は制限待機 0、ON へ戻すと待機を再開。
- 再初期化後も正常に待機し、統計をリセット。

参照した Microsoft の仕様: [CreateWaitableTimerExW](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerexw)、[SetWaitableTimerEx](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-setwaitabletimerex)。高精度タイマーは Windows 10 version 1803 以降でサポートされる。
