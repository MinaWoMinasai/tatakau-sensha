# Neon Boss Depth Encounter

本編の最深部で使う3Dネオンボスです。床のcoreが戦闘上の位置と当たり判定を持ち、その上に浮遊する本体と立体エフェクトを描きます。射撃・ドローン・近接は同じcoreと単一のHPを使用します。

## 設計と設定

| 資料 | 内容 |
| --- | --- |
| [アーキテクチャ](architecture.md) | 戦闘から描画への値の受け渡し、カメラ、死亡と資源解放 |
| [攻撃設計](attack-design.md) | Volley・Dive・Beamの予告、危険範囲、反撃時間 |
| [性能と計測条件](performance.md) | CPU/GPU計測、資源と計測上の制約 |
| [素材の出典](asset-source-notes.md) | 既存モデル、生成モーション、派生素材と利用条件 |
| [本編への接続](../neon-boss-gameplay-integration.md) | 担当クラスと検証スクリプト |

設定は[neonBossDepth.json](../../project/resources/configs/neonBossDepth.json)です。`Enemy`が攻撃とHPを管理し、`NeonBossVisual`は戦闘状態を値として受け取ります。カメラの設定と復元は`DepthEncounter`、描画への接続は`BossPresentation`が担当します。

## 確認方法

通常のRelease版はタイトルから遠征を開始して最深部へ進みます。WASDで移動し、マウスで床のcoreを狙って左ボタンで攻撃、右ボタンで特殊行動を使います。床の予告から退避し、Recoveryでcoreを攻撃します。

Development版で短時間に確認する場合は、タイトル→Return→部屋に入る前の作戦マップ→F3→「Neon Boss / 本編ボス確認」→「本編ボス戦へ移動 / 再生成」と進みます。この確認用入口は自機を無敵にするため、通常攻略の確認とは条件が異なります。

リポジトリのルートから既存の検査を実行できます。実ゲームの検査にはDevelopmentビルドが必要です。

```powershell
.\project\tools\test_neon_depth_combat.ps1
.\project\tools\test_neon_depth_config.ps1
.\project\tools\test_neon_depth_presentation.ps1
.\project\tools\test_neon_depth_runtime.ps1
```

過去の作業計画、試行別のログ集計、公開素材の引継ぎメモはローカル記録です。実行時の画像・JSON・計測ログは`generated/`または`project/generated/`へ保存し、クローンには含めません。共有する資料の方針は[資料案内](../README.md)を参照してください。
