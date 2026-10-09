# 3Dネオンボスの本編への接続

戦闘の位置・HP・攻撃・死亡は`Enemy`が持ち、3D表現は値のスナップショットを受け取って描画します。依存方向は`Enemy / Combat → BossVisualBridge → NeonBossVisual → NeonSkinnedRenderer`です。描画側から敵の攻撃やHPを変更しません。

## 担当クラス

| 実装 | 担当 |
| --- | --- |
| [CombatFramePipeline](../project/game/session/CombatFramePipeline.cpp) | 自機・敵・弾・衝突・演出の更新順 |
| [BossPresentation](../project/game/render/session/BossPresentation.cpp) | ボス状態の読み取り、Visualの更新、表示選択とDeveloper確認 |
| [BossVisualBridge](../project/game/enemy/visual/BossVisualBridge.h) | 戦闘状態から描画用の値への変換 |
| [DepthEncounter](../project/game/encounter/DepthEncounter.cpp) | Depth設定、遭遇中のカメラと終了時の復元 |
| [NeonBossVisual](../project/game/enemy/visual/NeonBossVisual.cpp) | モデル・モーション・発光・死亡演出と資源の寿命 |
| [GameplayRenderer](../project/game/render/session/GameplayRenderer.cpp) | 本編の描画パスへの接続 |

シーンの入口と共有状態の所有関係は[責務分離の案内](gameplay-responsibilities.md)、Depthの攻撃と表示は[Depthボスの案内](neon-boss-depth/README.md)を参照してください。

## 更新と寿命

Gameplayが確定した状態をVisualへ渡し、Drawは攻撃時計やモーションを進めません。同じ状態を毎フレーム渡してもモーションを開始し直さず、通常の戦闘用時間と停止状態を使います。死亡後の消去演出は別の進行時間で更新します。

HPが0になった時点でAI・発射・接触の処理を停止します。Visualは死亡状態を保持し、Directional Dissolveを終えてから所有するモデルと描画資源を一度だけ返却します。結果画面への進行、再試行、タイトルへの帰還はGameplay側が管理します。カメラは遭遇終了時に以前の設定へ復元します。

共有テクスチャやPSOのキャッシュと、遭遇ごとに所有する資源は別の寿命です。資源の解放は既存のフレームフェンスに従います。

## 素材と描画

本編は既存のGPU Skinning、NeonSkinnedRenderer、Directional Dissolveを使います。元GLBに埋め込みアニメーションはなく、骨格から生成したモーションを使用します。素材の説明は[モデルのREADME](../project/resources/models/neon_hologram/README.md)、利用条件と派生素材は[出典メモ](neon-boss-depth/asset-source-notes.md)を参照してください。

## 回帰検査

リポジトリのルートから実行します。実ゲームの検査にはDevelopmentビルドが必要です。

```powershell
.\project\tools\test_boss_gameplay_lifecycle.ps1
.\project\tools\test_neon_boss_visual.ps1
.\project\tools\test_neon_boss_runtime.ps1
.\project\tools\test_neon_depth_runtime.ps1
```

最初の2本は戦闘の停止・Visualの状態と寿命を検査し、後の2本は実ゲームを起動します。詳細な試行記録、特定ビルドのハッシュ、撮影結果はローカルの`generated/`へ保存します。自動検査の成功と、人が遊んだ際の操作感・難易度の評価は別に扱います。
