# Repository engineering overhaul

対象は「たたかうせんしゃ」の現在のゲーム挙動を保つ全体整備。開始branchは`refactor/engine`、HEADは`5b531d6eb3f2437ad8ab552f47f512ee8e1febd2`、開始statusはcleanだった。作業は未commitの差分として保持し、commit／push／merge／checkout／reset／cleanを行っていない。

## 変更の意図と境界

GameSceneから死亡優先順位・結果timerを`CombatFlowController`へ、Bossの値からVisual actionへのmapping・短い表示履歴を`BossVisualBridge`へ分離した。SceneはUI／音／通貨／部屋遷移を実行し、componentは値と通知を返す。既存2D Gameplayがauthoritativeで、Presentationとrendererから戦闘判断へ戻る依存は追加していない。

Playerから派生statsとmovementの計算を小さい入力／出力contractへ分離した。既存Catalog、SpecialAbilities、Evolution、ClassEditor、attack helpersを尊重し、StageへX→Yで衝突解決する元loopはPlayerに残す。元の数値と3,000組のstats／19,200 trajectory stepで一致を確認した。内部stateのpublic getterを大量に増やす分離ではない。

実production ownerを使った再現でPreviewのinstance descriptor返却漏れとDroneの致死後行動を確認し、修正と回帰テストを追加した。共有texture cacheは維持する。元のDroneはHP0でも1発射撃・移動したが、修正後は即時死亡・射撃0・移動0。古いBloom source testが要求していた文字列を現在のFreeze contractへ合わせ、開始時FAILと修正後PASSを分けて保存した。

追加設定の実機probeでは、未知の部屋を拒否する失敗reportが出てもprocessが0を返す既存の終了コード不具合を再現した。MainLoop／RunからWM_QUITのコードを返し、WinMainはFinalize／trace後にその値を返す。message pumpはWM_QUITを読んだ時点で止め、後のmessageによる上書きを防ぐ。Gameplay／描画の判断にSessionを入口から参照させる依存は追加しない。

Developer専用Scenario Runnerを既存のactor／collision／room／map／restartへ接続している。process-owned Sessionはscene epochと完了frameのsnapshotを記録し、死後行動・count上限・finite・Dissolve終端資源を検査する。通常playの乱数／balanceを変えず、有効なvalidation sessionだけ固定seed／dt／inputを適用する。

Scenario一覧はShooter、Drone、Melee、ProjectileStress、EnemyStress、RivalBoss、PrototypeBoss、NeonBoss、BossDeath＋Dissolve、PlayerDeath／Restart、StageTransition、ExpeditionTransition、PreviewLifecycleの13ケース。実機wrapperは各2回の全frame observable比較と指定frameのcamera付きcaptureを検査する。追加probeで正値HP、1/120秒、authored room／wave、実Repair upgrade、明示input、無効設定、短すぎる死亡duration、誤ったTitle起動、restart境界の撮影を確認する。

## 記録

| 文書 | 内容 |
| --- | --- |
| [architecture-baseline.md](architecture-baseline.md) | file／build／workflow baseline、実際のownershipと依存、全対象領域の調査、抽出理由 |
| [refactor-log.md](refactor-log.md) | 小単位の抽出と数値・build・gameplay gate |
| [correctness-audit.md](correctness-audit.md) | 再現根拠、修正、lifetime／death regression |
| [potential-issues.md](potential-issues.md) | 確証がない問題、変更しなかった理由 |
| [deterministic-scenarios.md](deterministic-scenarios.md) | 13ケース、manifest、snapshot／invariant、再現性の範囲 |
| [performance-before-after.md](performance-before-after.md) | 複数frameのCPU／GPU／pacing／resource測定と最適化判断 |
| [visual-validation.md](visual-validation.md) | 実機captureと実際の目視、GPU完全一致を保証しない範囲 |
| [final-validation.md](final-validation.md) | 全test分類、build／runtime／Release／package／diff review |

全生成物は既存ignore対象の`generated/repository-engineering-overhaul/`と`generated/repository-validation/`へ保存する。baselineのEXE／DLL／PDB／assets／packageはhash付きで固定し、現在binaryの証拠とは分ける。外部dependency、モデル、ライセンス本文は追加・変更していない。

## 最終検証

開始HEADのDevelopment／Release x64、既存42 PowerShell scripts（variant等を含む50 case）はPASS。standalone Pythonは5 suite中4 PASS、1件は開始時に存在した古いFreeze文字列checkの失敗。修正後13 testを2回、関連source contract 33 checkもPASS。

抽出Iteration 1／2のDevelopment／Release gateがPASS。Iteration 2の現行Neon13capture、Release combat10capture、pristine package作成／起動もPASS。Release実traceでDeveloper Tools／ImGui／Profilerは0。これらは後続sourceの最終認証とは区別する。

GameScene.cpp／Player.cppの開始物理行数は7,126／5,029。抽出2回後、Runner接続前は7,096／4,990。最終source freezeでGameScene.cppは7,115、Player.cppは4,990、Enemy.cppは開始と同じ1,608。RunnerのScene adapterは別のDeveloper translation unitに置く。行数は観測値であり、改善の根拠は独立した責務・contractと回帰検証に置く。

Runnerの13ケースは初回gateに加えて各2回、26 process／15,600完了frameをPASSし、全frameのobservable比較も一致した。これは終了コード修正前のbinary SHA256を持つ証拠で、後続binaryと混同しない。全件driverは58 interface／64 command caseを実行し、63 PASS、JSONキー順を文字列比較したwrapperだけFAIL。その記録を保持してsemantic比較へ修正し、25 negative fixtureと保存済み26 report設定の再検査をPASSした。続くprobeで既存の終了コード不具合を再現・修正し、影響する13 command caseを最終binaryで再検証して全てPASSした。元のAllを64／64 PASSへ書き換えず、修正後の影響範囲を統合した`final-validation-aggregate.json`がPASSした結果として報告する。

| 最終の確認 | 結果 |
| --- | --- |
| Development／Release x64 | 両方PASS、warning／error 0。command時間18.51／15.14秒 |
| final Development | SHA256 `C3A5FF76C7CAC1AE19CA252AED5C435115816262E6EC59D4E28AAD8F6BCE30AF`、Developer Tools ON |
| final Release | SHA256 `14393F15CBA2FAAE4E4B922DD7828F1F69539A9EF21872658ECA0FADE6C80A87`、Developer／ImGui／Profilerの実counter 0 |
| 最終Neon／BossDeath | 各2回、2,880 frame、terminal invariantとcapture PASS。修正前の4軌跡とも同じ基準で全frame一致 |
| 設定／起動probe | 正常設定360 frame／restart600 frameはexit0、無効room／upgrade／objective／未完death／TITLE起動はexit9。不正・不存在manifestのconstructorも各exit9／0 frame |
| Release除外 | 不正・不存在Scenario manifestを無視して通常Title→Expeditionを描画、検証outputなし |
| 最終package | 253 files／62,911,552 bytes。asset／DLL／profile／pristine audit PASS。別copy・無引数・別cwdでTitle→tutorial→class／workshop／repair→Boss結果→Title→新run PASS |
| sourceの区別 | original All以後の変更は終了処理3fileとwrapperのみ。最終検証中のsource drift 0。旧26ケース全部を最終binaryで再実行したとは扱わない |

全command、全test分類、実行日時、build／report／package hashと限界は[final-validation.md](final-validation.md)。local v145の結果であり、静的auditした既存4 GitHub Actionsのremote v143実行を成功したとは報告しない。

入口修正前の同一sourceに対する512 trail stressでDevelopmentだけTrailManager.cppを`/Od`から`/O2`へ変え、2回ずつ計測した。CPU scope平均は11.7537／11.8340msから8.2017／8.1787msへ30.2／30.9%低下し、median／p95も両方改善した。全1,200計測frameで512trail、71,680vertex、2,617,336byte、draw1回、truncation0が同じ。256assetのSHA256も一致。計測binaryのhashを保存し、終了コード修正後の最終default binaryを再測定したとは扱わない。Release、geometry algorithm、shader、浮動小数点modeは変更しない。GPU／frame値も記録するが温度・clock未制御のためGPU改善の因果は断定しない。

## 変更ファイル

最終入口修正を含む既存変更24ファイル、新規34ファイル、削除0。下記は全変更一覧で、生成物は含まない。最終status／diff原文と統計は`generated/repository-engineering-overhaul/`に保存した。

|既存変更|役割|
|---|---|
|`project/CG2_testPro.vcxproj`、`.vcxproj.filters`|新component／Developer Session登録、Release除外、Development限定Trail最適化|
|`project/DirectX/engine/input/Input.cpp`、`Input.h`|Developerだけの1frame入力上書き|
|`project/game/debug/NeonSkinnedPreview.cpp`、`.h`|instance teardown／部分ロード失敗の返却、実D3D反復検証|
|`project/game/player/actor/Player.cpp`、`.h`|派生stats／移動計算を専用componentへ接続|
|`project/game/player/actor/PlayerDrone.cpp`、`.h`|致死後の移動／射撃／contact停止|
|`project/game/scene/Game.cpp`、`Game.h`、`project/main.cpp`|Developer Sessionの入力、背景実行、非default dtの色効果、誤起動のbounded失敗、WM_QUITの終了コードをcleanup後に返す|
|`project/game/scene/GameScene.cpp`、`.h`|Flow／Bridge ownership、Sessionの初期化／更新／snapshot／capture呼出し|
|`project/game/scene/GameScene.NeonBoss.cpp`|値入力をBridgeに渡しVisualへ反映|
|`project/game/scene/GameScene.ExpeditionMap.cpp`、`GameScene.TankRun.cpp`|Session限定seed、Flow接続|
|`project/game/scene/GameScene.TankExpedition.cpp`、`GameScene.TitleDemo.cpp`、`GameScene.ExperienceValidation.cpp`|既存呼出しをFlow ownershipへ合わせる|
|`project/tools/tank_projectile_tests.cpp`|実production adapterでplain statsと新helperを検証|
|`project/tools/test_neon_bloom_comparison.py`|古い文字列assertionを実際のFreeze contractへ合わせる|
|`docs/neon-boss-gameplay-integration.md`|過去checkpointと今回のNeon再検証を区別|

|新規|役割|
|---|---|
|`project/game/flow/CombatFlowController.h`|同時死亡の優先順位と結果clockのpure component|
|`project/game/enemy/visual/BossVisualBridge.h`|Gameplayの値からPresentationへの一方向mapping|
|`project/game/player/PlayerDerivedStats.h`、`PlayerMovement.h`|小さい値contractのstats／移動計算|
|`project/game/debug/GameplayScenario.h`、`GameplayScenarioSession.h`、`GameplayScenarioSession.cpp`|有界schema／script／snapshot／invariantとprocess-owned Session|
|`project/game/scene/GameScene.GameplayScenario.cpp`|実actor／room／map／restartと既存captureへのDeveloper adapter|
|`project/tools/combat_presentation_tests.cpp`、`test_combat_presentation.ps1`|Flow／Boss Bridge回帰|
|`project/tools/player_derived_stats_tests.cpp`、`test_player_derived_stats.ps1`|stats／HP／stamina回帰|
|`project/tools/player_movement_tests.cpp`、`test_player_movement.ps1`|移動数値とstep境界|
|`project/tools/neon_preview_lifecycle_tests.cpp`、`test_neon_preview_lifecycle.ps1`|実owner解放／partial failure／retry|
|`project/tools/player_drone_lifecycle_tests.cpp`、`test_player_drone_lifecycle.ps1`|実Drone致死後停止とliving挙動|
|`project/tools/gameplay_scenario_tests.cpp`、`test_gameplay_scenario_session.ps1`|schema／invariant／strict Release guardとJSON実出力|
|`project/tools/test_gameplay_scenarios.ps1`、`test_gameplay_scenario_release.ps1`|実機反復／設定probe／ReleaseがSessionを読まないこと|
|`project/tools/profile_gameplay_scenarios.ps1`、`summarize_runtime_profiles.py`|隔離資源で複数frame測定、GPU有効値／death phaseの集計|
|`project/tools/test_repository_validation.ps1`|全test分類、build／実機／packageの直列driver|
|`docs/repository-engineering-overhaul/README.md`、`architecture-baseline.md`、`refactor-log.md`、`correctness-audit.md`、`potential-issues.md`、`deterministic-scenarios.md`、`performance-before-after.md`、`visual-validation.md`、`final-validation.md`|調査、境界、再現、計測、検証の9文書|

## 再現と画像・配布物

repository rootのPowerShellで、既存actorを使うNeonと死亡Dissolveを短時間で再現する。その他のIDとmanifest contractは[deterministic-scenarios.md](deterministic-scenarios.md)。Releaseへこの検証UI／入力経路を公開しない。

```powershell
& .\project\tools\test_gameplay_scenarios.ps1 -Scenario @('neon_boss','boss_death') -Repeats 2
```

| 保存先（`generated/repository-engineering-overhaul/`以下） | 内容 |
| --- | --- |
| `final-entry-validation/scenarios/run_20261004_224906_970_e80d8442/` | 最終binaryの17 PNG／camera JSON、4 captured runと7設定probe。代表画像と初期blur／境界fadeを実際に目視 |
| `final-validation/artifacts/test_gameplay_scenarios/run_20261004_221102_241_e67a04a2/` | 入口修正前の13×2、54 PNG／camera JSON、15,600 snapshot |
| `final-validation/artifacts/test_neon_boss_runtime/` | 同binaryのIdle／Telegraph／実Attack／Dash／low HPのON/OFF5組とDissolve3枚 |
| `final-entry-validation/pristine-package/` | 最終Releaseの未実行配布物。検証で変更したcopyは別の`package-runtime-copy/` |
| `final-validation-aggregate.json`、`entry-runtime-preservation/results.json` | 失敗を保持した統合結果、旧／新binaryの2,880 frame比較 |

rootと別agentで以前の代表30枚に加え、新しい12枚を目視し、明らかなmodel欠落、軸誤り、浮いたOutline、Dissolve終端の人型残像を見つけていない。通常difficultyの攻略性と外観の好みはこの自動fixtureの評価範囲に含めない。

## 最終差分と自己レビュー

GameSceneの結果clock／優先順位とBoss bridge、Playerの数値合成／移動／X→Y解決順、Preview owner／共有cacheの寿命、Drone死亡、strict Developer入口、bounded manifest／snapshot、readback前のScene破棄、終了codeとcleanup順、Trailの限定compiler設定を全差分から再確認した。rootと別agentの独立reviewを行い、新しい確認済み問題を修正して関連gateを通した。

最終branchは`refactor/engine`、HEADは開始と同じ`5b531d6eb3f2437ad8ab552f47f512ee8e1febd2`。staged 0、既存変更24、新規34、削除0。通常の`git diff --stat`は追跡済み24ファイルについて509行追加／265行削除で、新規34ファイルを含まない。全58ファイルの一覧・hash・新規ファイルの物理行数は`generated/repository-engineering-overhaul/final-git-summary.json`に分けて記録した。原文は`final-git-status.txt`、`final-git-diff-stat.txt`、`final-git-diff-numstat.txt`、`final-git-diff-check.log`、要求別completion auditは`final-phase-completion-audit.md`に保存した。`git diff --check`はPASS。全生成物は既存ignore対象で、Git追跡対象へ混入していない。

## 残す境界と手動確認

GameSceneのUI／描画orchestrationとPlayerのHUDは依然大きい。singleton、engine post effectからSceneManagerへのinclude、共有乱数streamも既存のまま。調査結果と制約を明記し、一括移動や別architectureへの置換は行わない。

通常難易度で3 classの操作感、遠征の選択・upgrade／evolution・repair・restartを確認する。Neonの既存人型素材と円形colliderの大きさ・向き、ピンクの強さは好みが関係するため、数値testで決めない。専用歩行／Dash／被弾／死亡clipがないfallback、通常cameraで遠いbossがviewport外になる現在の挙動も維持する。
