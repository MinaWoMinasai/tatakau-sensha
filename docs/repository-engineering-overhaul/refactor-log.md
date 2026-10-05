# 責務分離と反復検証

開始点は `refactor/engine` / `5b531d6eb3f2437ad8ab552f47f512ee8e1febd2`、開始statusはclean。既存のPlayerClassCatalog、Player.SpecialAbilities、Player.EvolutionUi、Player.ClassEditor、GameSceneの遠征／制作／実機検証translation unitを調査したうえで、残っている独立した計算・状態責務を抽出した。既存ファイルを別の場所へ移すだけの分割は行っていない。

## 抽出した境界

| 責務 | Before | After / 小さいcontract | 維持した挙動 |
| --- | --- | --- | --- |
| 死亡判定・結果表示の時計 | GameSceneが同時死亡の優先順位、撃破timer、impact通知、結果readyを直接管理 | `CombatFlowController`と`SelectCombatDeathOutcome`が値入力から通知・状態を返す。Sceneは音、画面効果、通貨回収、UIと遷移要求を実行 | 遠征の同時死亡はPlayer優先、旧通常モードはBoss優先。timerが0になるframeもpresentation待ちを返す。通貨回収待ちと一度だけのimpactを維持 |
| GameplayからBoss Visualへのmapping | GameScene.NeonBoss.cppが前回shot、位置、遭遇世代、0.70秒表示holdを所有 | `BossVisualBridge`がraw snapshotからaction／phaseTwo／resetEncounterを返し、短い表示履歴を所有 | Rival／Prototypeのcombat時計は変えない。単発射撃の短いAttack表示はTelegraph／Dashを上書きしない。RendererからGameplayへの逆依存なし |
| Player派生stats | Playerが基礎値、class profile、7強化、run tuning、整備、clampを一体で計算 | `PlayerDerivedStats`が固定長の入力値からstatsを返す。小さいHP finalizerとPlayer adapterがHP／collision damageを反映 | 乗算順序、Meleeの3.8換算、reload下限、spent stamina、旧満タンHP成長を維持。balance編集・refitのno-heal復元は元callerに残す |
| Player移動計算 | Player::Updateが入力正規化、inertia、dt換算、substep数を直接計算 | `PlayerMovement`がdirection／velocity／speed／response／dt／4行動flagから次velocityとstep計画を返す | runの指数応答と旧linear応答、dash1.5、rail0.78、spin0.55、60FPS基準単位、0.35軸stepを維持。StageへX→Yで解決する元loopとdash前入力更新はPlayerに残す |

新しいcomponentにはPlayer／Enemy／Scene pointer、Input／Audio singleton、Renderer callback、UI resourceを渡していない。`Player::PlayerStats`はplain値型へのaliasで既存callerとのsource互換を保った。内部stateを大量のpublic getterへ変えて抽出したことにはしていない。

## Iteration 1: Flowと派生stats

先にpure componentを孤立検証し、その後production adapterを接続した。Player側では変更前の実際の`RecalculateStatsFromBase`出力を7条件で保存し、arena基本／全強化／満タンHP成長、run legacy／Shooter／Drone／Meleeの数値を固定した。

- `player-stats-stage1-tests.log`: 7数値goldenのfloat bit一致、HP成長／保持／上限低下／zero、stamina保持／clamp、同じ入力の再計算で積み重ならないことを確認。
- `player-prepared/player-preservation-prepared.log`: 保存した元production本文と3,000条件のstats構成がbit一致。既存run tuningとmaintenance自体を使い、設定選択／run有効無効／core／強化の組合せを比較。
- `player-stats-stage1-projectiles.log`: 実際のBullet／BulletManager／AttackController／CollisionManagerとPlayer能力・新しいrecalculation adapterの回帰がPASS。640射撃条件、class／equipment／wallet／HP／staminaを補充しない切替、evolution合成を含む。
- Flowのisolated suiteと既存TankRun／Prototype／modifier回帰がPASS。`iteration-1-development-build.log`、`iteration-1-release-build.log`のDevelopment / Release x64 gateが通過した。

元Player methodをコンパイルする既存test adapterは、新しいplain stats型を使いながら元のtest初期値を明示して保った。test adapterの`input` memberと新しいlocalが衝突するC4458を見つけ、localを`derivedStatsInput`へ改名した。新componentの警告を抑制して通したものではない。

## Iteration 2: 移動とBoss Visual Bridge

移動componentを接続する前に、元Player::Updateの正規化／inertia／substep本文で8条件の出力を保存した。legacy／runの斜め移動とcoast、dash、rail＋spin、zero dt、0.8秒の有効な大きいdtで22stepになる条件を含む。

- `player-movement-stage2-tests.log`: 8数値goldenと境界条件がPASS。
- `player-movement-stage2-equivalence.log`: 現在のproduction helperを読み、元本文との19,200連続trajectory stepと3,000stats構成がbit一致。保存した元algorithmはignored evidenceに限り、常設テストへ同じ実装を複製していない。
- `player-movement-stage2-projectiles.log`、`player-movement-stage2-run.log`: 既存実production回帰がPASS。
- `iteration-2-combat-presentation.log`、`iteration-2-neon-boss-visual.log`、`iteration-2-rival-combat.log`、`iteration-2-tank-run.log`: Bridge／Visual／Rival／Prototypeのfocused回帰がPASS。
- `iteration-2-development-build.log`、`iteration-2-release-build.log`: Development / Release x64 gateが通過。
- `current-neon/test_neon_boss_runtime_current.log`、`test_tank_combat_runtime_current.log`、`test_tank_submission_runtime_current.log`: 現在のNeon実機、Release実戦闘、package起動の回帰がPASS。各wrapperの対象範囲を超えて全ゲーム状態を保証するものではない。

BridgeはSceneの4つの履歴fieldを所有し、SceneはEnemyの最終HP／位置／照準／radius／combat phaseを読み、decisionをVisualへ渡す。3D modelとanimationは既存NeonBossVisualが所有し、Directional Dissolveと明示resource解放の寿命は変えていない。

## Iteration 3: 確認されたDrone死亡edge

これは責務抽出とは別のcorrectness修正。元の実production method再現では、致死衝突後も`hp=0 diedImmediately=0 shotsAfterLethal=1 collisionCallsAfterLethal=2 movementAfterLethal=0.235362`だった。

`OnCollision`と`Damage`でHP0を即時死亡にし、`Die`はHP0・velocity0にする。terminal actorはAttack／Update／contactを進めない。living one-point damage、invincibilityを検査しない衝突方式、資源無視、5.5秒bomb rebuildは維持した。現在の実production再現は`hp=0 diedImmediately=1 shotsAfterLethal=0 collisionCallsAfterLethal=0 movementAfterLethal=0`。

`player-drone-stage3-tests.log`、`player-drone-stage3-projectiles.log`、`player-drone-stage3-collisions.log`、`player-drone-stage3-abilities.log`がPASS。new lifecycle suiteは元と現在のmethod本文を実行し、通常／同位置の致死衝突、反復damage、死後移動／射撃、living knockback／resource／rebuild completionを確認する。unchanged Updateのlocal `dir` shadowに限ってC4458を抑制し、他のW4警告はerrorとしている。

Iteration 3の最終構造checkを含む`iteration-3-final-development-build.log`／`iteration-3-final-release-build.log`はDevelopment / Release x64で0 warning・0 error、30.81／34.37秒。最終構造check前の同一Runnerに対する実機初回gateは13 caseすべてPASSで、各frameのactual state、固定frame capture、死亡／復活、room／shop transition、Previewの実D3D teardownを確認した。初回suiteは`scenarios-first-pass/run_20261004_214414_779_583db386/suite.json`と`run_20261004_214441_244_365c5600/suite.json`。

Developer Scenario Runnerはstrict Developer guardでmain Sceneの既存入力、Player attack、Enemy AI、authored room geometry、map／shop service、fade／scene replacementを実行する。pure `GameplayScenario`はschema／固定時刻／script選択を所有し、`GameplayScenarioSession`はprocessをまたがないglobal frame、複数scene epoch、bounded reportを所有する。GPU capture中はsimulationを進めず、元の0-dt描画branchでrenderer frame arenaを再開する。old Sceneの最終frameをcaptureした場合もreadback完了まで遷移を遅らせる。

常設`test_gameplay_scenarios.ps1`はfresh processで実機observableを検査し、反復した全frameのHP、position、class、attack／enemy数、boss phase、flow／map stateを比較する。最終反復ではsuite専用resources copyから起動し、各processの前にconfig snapshotを復元する。BossDeath captureはframe121／165／201でdeath start／midpoint／terminalを取得する。custom manifestのpositive HP、実room／wave／upgrade、明示input、非default dt、無効設定のexit9とcapture跨ぎも同wrapperのoptional probeで検査する。

測定したDevelopmentの512 trail描画では既存`TrailManager.cpp`のCPU geometry構築が大きな負荷だった。同じsource・resource入力でそのtranslation unitだけを`/Od`から`/O2`へ変え、二回ずつ比較した。Stress trails CPU meanは30.22%／30.89%減り、median／p95も両回改善。全1,200 sampleで512 trail、71,680 vertex、upload 2,617,336 byte、draw 1、truncation 0が一致し、既存`/O2` geometry goldenもPASSだったため採用した。algorithmとRelease設定は変えず、当該fileのsource stepping／local変数観察には最適化の制約がある。GPU mean低下だけをこのCPU設定の因果効果として扱わない。四統計と比較条件は[performance-before-after.md](performance-before-after.md)に記録する。

source freeze後の最初のAll gateは64件中63 PASS・1 FAILだった。実機の標準13 case×2回は全26 case、15,600 simulation frameを完了し、全frameのobservable反復比較、固定frame captureとactual camera metadataの検査がPASS。Developer binaryはSHA256 `CE7D899F…`。残ったFAILはcustom manifestのJSON object member順序を文字列で比較したwrapperの誤判定で、元suiteと失敗logを保存した。objectのordinal key集合・型・値を再帰比較し、array順序は維持するよう修正した。宣言された座標／movement／aimはfloat32へ正規化し、HP／frame整数は正確に比較する。shuffled keyの正例と25件の値・型・shape・順序の負例がPASSし、保存済み26設定も新helperでoffline再検証した。

続く無効room probeではbounded error reportとzero simulation frameを確認できたが、実processのexitは0だった。`Game::MainLoop`が`WM_QUIT`のcodeを捨て、`WinMain`が常に0を返していたため、message pumpを`WM_QUIT`で止め、`MainLoop`→`Run`→`WinMain`でcodeを返すよう修正した。Finalizeと終了traceは従来どおり実行する。Gameplay計算やbalanceは変えていない。

最終entry修正後のDevelopment／Release buildは0 warning・0 errorで、driver計測18.51／15.14秒。最終Developer SHA256 `C3A5FF76…`でNeon／BossDeath各2回の4 captured caseと7 configuration probeがすべてPASSした。positive HP／実room・wave・Repair upgrade／script／1/120秒、未知room・upgrade、非対応room、短すぎるBossDeath、誤startup、restart跨ぎcapture0／226／600を確認した。5つの失敗probeは実exit9とbounded reportを返した。さらに不正／不存在manifestのconstructor failureもzero gameplay・exit9がPASSし、ReleaseのScenario除外、Developer profile、pristine package作成・audit・実機walkthroughがPASS。実行中source driftは0だった。

最終binaryと保存済みAll binaryの対応する4 trajectoryを、変更していない`Compare-ScenarioSnapshots`でoffline比較した。計2,880 frameのcategorical observableとposition／時刻／dissolveは既存の絶対誤差1e-5以内で一致した。新たなCG2起動は0。証拠は`entry-runtime-preservation/results.json`、最終entry gateは`final-entry-validation/results.json`、元のAll失敗は`final-validation/results.json`に残した。完了したperformance反復とbinary別の検証範囲は[performance-before-after.md](performance-before-after.md)、[final-validation.md](final-validation.md)に記録する。

## 行数と残る責務

開始の空行込み物理行数はGameScene.cpp 7,126、Player.cpp 5,029。抽出2回後、Runner adapter追加前の記録では7,096／4,990だった。これらはサイズの観測であり改善の判定には使わない。独立した責務、明示contract、数値・実機回帰が判断根拠で、最終数はREADMEに記録する。

GameSceneの描画／UI／遠征orchestrationとPlayerのarena HUD／codexは依然大きい。初期化・GPU resource・入力の広いownership移動を同時に行う理由がないため、今回確認できた小さい境界から分離した。既存attack／ability helperやCatalogを別体系へ置換していない。

詳細なPlayer調査、元sourceと反復ログは`generated/repository-engineering-overhaul/audit-player.md`と`player-stage1.md`～`player-stage3.md`。correctnessと未確証問題は[correctness-audit.md](correctness-audit.md)、[potential-issues.md](potential-issues.md)へ分けている。
