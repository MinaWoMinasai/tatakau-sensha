# GameSceneとPlayerの責務分離

2026-10-09の整理では、GameSceneをシーン管理との接続に限定し、プレイの処理を用途ごとのクラスへ移しました。既存の公開API、更新順、戦闘用と演出用の時間、設定JSONは維持しています。

## 読み始める場所

| 入口 | 役割 |
| --- | --- |
| [GameScene](../project/game/scene/GameScene.cpp) | ISceneの呼び出しをGameSessionへ渡す |
| [GameSession](../project/game/session/GameSession.cpp) | プレイの寿命を持ち、適切な担当へ接続する |
| [SessionBootstrap](../project/game/session/SessionBootstrap.cpp) | 描画資源・自機・敵・レベル・UIの初期化 |
| [CombatFramePipeline](../project/game/session/CombatFramePipeline.cpp) | 入力、時間、戦闘、衝突、演出の更新順を管理する |
| [GameplayRenderer](../project/game/render/session/GameplayRenderer.cpp) | 描画パスの順序とポストエフェクトへの接続 |
| [Player](../project/game/player/actor/Player.cpp) | 移動・当たり判定・生死と担当クラスへの公開窓口 |

GameScene.hは100行、GameScene.cppは142行です。元の1,540行／7,261行から縮小し、GameScene以外の実装はGameScene.hに依存しません。

## ゲーム側の担当

| 配置 | 主な担当 |
| --- | --- |
| `game/flow/` | CombatFlow、CombatTutorial：戦闘結果と基本チュートリアル |
| `game/run/session/` | 遠征の部屋、地図・サービス、ビルド選択、通貨とガイド、共有アリーナ進行 |
| `game/render/session/` | 描画パス、地形、自機・ボス・資源敵の外観 |
| `game/effects/session/` | レーザー・地雷・斬撃・命中・死亡パルスの処理と表示 |
| `game/ui/session/` | 追従HPバー、ゲーム中の文字とHUD |
| `game/editor/session/` | レベル編集、遠征制作、性能・外観設定、機体設定の再読込 |
| `game/debug/session/` | 性能計測、戦闘・特殊攻撃・経験値・Depthの検証、シナリオとリプレイ |
| `game/demo/` | タイトル背景の自動プレイ |
| `game/encounter/` | Depth戦の設定、カメラ、導入演出 |
| `game/level/` | レベルの読込、反映、配置物の寿命 |

17個のGameScene.機能名.cppは廃止しました。例えばGameScene.ExpeditionMap.cppはExpeditionMapController.cpp、GameScene.TitleDemo.cppはTitleDemoController.cppになっています。同じGameSceneの実装を別ファイルに置く方式から、担当クラスが処理を持つ方式へ変更しました。

## 共有状態と寿命

[GameWorld](../project/game/session/GameWorld.h)は処理を持つ巨大なシーンの代わりではなく、共有資源と担当クラスの所有者です。状態は`game/session/state/`に分けています。

- TitleDemoState：タイトルデモの進行。
- RunSessionState：遠征・部屋・報酬・制作データ。
- WorldResources：カメラ、アクター、地形、弾、描画資源。
- CombatUiState：戦闘の進行、入力ガイド、結果表示、時間。
- NeonPresentationState：外観設定、演出、描画用の残存状態と計測。
- ValidationState：経験値・操作検証の観測状態。

各担当はGameWorldを参照で借用します。別の担当への呼び出しと共有状態への参照はコード上で明示しています。更新順の判断はCombatFramePipelineに置き、弾の走査中の生成、衝突、撃破通知、後続の演出更新という既存の順序を保っています。

終了時はカメラを復元し、遠征音声を停止し、敵の通知コールバックを解除してから担当と資源を解放します。タイトルから遠征へ切り替わる際も同じ寿命管理を通ります。

## Player側の担当

| クラス | 配置と役割 |
| --- | --- |
| PlayerWeapons | `game/player/combat/`：射撃、レール砲、特殊攻撃、ドローンへの指示 |
| PlayerProgression | `game/player/progression/`：機体設定、装備、遠征成長、整備、性能計算 |
| PlayerHud | `game/player/ui/`：強化HUD、ゲージ、文字、表示設定 |
| PlayerEvolution | `game/player/ui/`：進化経路、候補、図鑑、外観編集 |
| PlayerClassEditor | `game/player/editor/`：制作時の機体編集 |
| PlayerUiState | `game/player/ui/`：画面の資源・配置・表示キャッシュ |

Player.cppは5,036行から1,774行になりました。Playerは引き続きColliderであり、衝突ID、自機のアドレス、HP・装備・成長の所有者を変えていません。表示担当は必要な自機状態を借用し、公開APIはPlayerの委譲関数から呼べます。

Player.hには既存APIと戦闘状態が残ります。担当間で同じ状態を使用する箇所も明示した共有境界に残しています。新しい機能は担当のクラスへ追加し、GameSceneやPlayerの窓口へ実装を戻さない方針です。

## 検証

構造の回帰検査は次のコマンドで実行できます。GameSceneの250行上限、用途別GameSceneファイルの禁止、表示状態の分離、Visual Studioへの登録漏れを検出します。

```powershell
python project/tools/test_gameplay_boundaries.py
```

既存の機体設定・射撃・衝突・ブルーム契約のテストは新しい実装を読み込むよう変更しました。CPU用アダプターは担当が借用する自機状態へ接続します。戦闘の数値を変更するための修正は含めていません。古いブルーム初期値の期待と敵の表示色が欠けたテスト用状態も、現在の実装に合わせて更新しています。

移動前後の490メソッドについて、参照先と担当名を正規化した上で処理本体の字句を照合し、一致を確認しています。これはコード移動の確認であり、実行時の寿命とリンクの確認はビルド・実ゲームの既存テストで補います。

実行済みの検査は次のとおりです。

| 検査 | 結果 |
| --- | --- |
| Visual Studioビルド（Development / Release / Debug、x64） | 3構成すべてコンパイル・リンク成功 |
| `test_gameplay_boundaries.py` | 4検査成功 |
| `test_player_class_config.ps1` | 20グループ成功 |
| `test_tank_projectiles.ps1` | 射撃・特殊攻撃・640通りの装備組合せなど成功 |
| `test_tank_collisions.ps1` | 弾・壁・近接・ダッシュの衝突検査成功 |
| `test_neon_bloom_source_contract.py` | 33検査成功 |
| `test_neon_bloom_comparison.py` | 13検査成功 |
| `test_title_demo.ps1 -Configuration Development` | 4ステージ、射撃438回・撃破37回・ダッシュ25回、フェード、新しい遠征への遷移を確認 |
| `test_tank_expedition_map_runtime.ps1`（Development / Release） | 両構成で10部屋、購入4回、修理1回、最終部屋までの進行を確認 |

地図の実行検査では戦闘を自動クリアし、進行と状態の受け渡しを検証しています。通常の戦闘はタイトルデモと射撃・衝突の検査で確認しています。
