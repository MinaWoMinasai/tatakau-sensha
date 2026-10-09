# ゲームシナリオ検証

通常のPlayer・Enemy・弾・衝突・遠征進行を使い、入力と時間を固定して再現するDevelopment用の検証基盤です。設定、観測値、再現性の範囲は[シナリオの仕様](deterministic-scenarios.md)を参照してください。現在の担当クラスは[GameSceneとPlayerの責務分離](../gameplay-responsibilities.md)にまとめています。

## 実行方法

リポジトリのルートから実行します。実ゲームの検証にはDevelopmentビルドが必要です。

```powershell
# 登録済みテストの一覧を確認する。ビルドやゲーム起動は行わない。
.\project\tools\test_repository_validation.ps1 -InventoryOnly

# 実ゲームのシナリオを繰り返す。
.\project\tools\test_gameplay_scenarios.ps1 -Scenario @('neon_boss', 'boss_death') -Repeats 2

# Releaseで検証用の入力経路が有効にならないことを確認する。
.\project\tools\test_gameplay_scenario_release.ps1
```

`test_repository_validation.ps1`にはUnit・Source・Rendering・Runtime・Packagingの分類があります。用途に合わせて`-Phase`と対象を選び、実行結果は新しい`generated/repository-validation/`フォルダーへ保存します。

入力設定と観測は[GameplayScenarioSession](../../project/game/debug/GameplayScenarioSession.cpp)、アクターと遠征への接続は[GameplayScenarioRunner](../../project/game/debug/session/GameplayScenarioRunner.cpp)が担当します。更新順は[CombatFramePipeline](../../project/game/session/CombatFramePipeline.cpp)を参照してください。

過去の改修手順、途中の失敗、特定ビルドの計測・検証報告はローカルに保持しています。ここにはクローン後に再現できる仕様と入口を残します。
