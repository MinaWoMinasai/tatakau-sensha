# Gameplay Scenario Runner

この基盤はDeveloper専用の設定・入力・観測adapter。通常のPlayer、Enemy、BulletManager、Stage、衝突、遠征の部屋／サービス／結果遷移を呼び、別の戦闘simulationを実装しない。ReleaseではSessionとScene adapterをbuildから除外し、全呼出箇所も`CG2_DEVELOPER_TOOLS && !defined(NDEBUG)`で除外する。

## 境界

- `game/debug/GameplayScenario.h`: graphics-freeな設定、frameごとの入力選択、observable Snapshotとinvariant。
- `GameplayScenarioSession`: processが所有する設定と記録。Sceneを破棄してもglobal simulation frameは継続し、Scene epochだけ増える。actorやrendererのpointerを保存しない。
- `GameScene.GameplayScenario.cpp`:既存の部屋／map entryとactor APIへ設定を適用し、更新後に値を読む。captureは既存NeonShowcaseCaptureと既存frame fenceを使う。
- `Game` / `Input`:有効なSessionだけ固定mouseと無入力のhardware frameを供給する。Playerの既存demo input経路へscriptを渡す。通常playの入力pollingとfocus停止は維持する。

乱数seedはScene初期化前、RunDirector／Map／blueprintにも適用する。既存global mt19937のstreamはGameplay、particles、camera shakeが共有するため、同じ更新・描画・capture経路で比較する。通常playの乱数を別streamへ変更しない。

基準dtを固定し、既存のhit hold、slow motion、menu、death presentationによるcombat時間倍率は保持する。frameは完了したsimulation updateの数。capture待ちのDrawは時間0で、simulation frameと攻撃時計を進めない。

## 指定可能な条件

`CG2_GAMEPLAY_SCENARIO`はUTF-16環境変数から読むJSONファイルのパス。manifestは最大256 KiB、出力はworkspaceの`generated/`または`project/generated/`配下に限定する。日本語pathを実際のSession process fixtureで確認した。

| 条件 | contract |
| --- | --- |
| scenario | 13個の既知ID |
| seed / fixedDeltaTime / frames | uint32 seed、有限な1/240～1/15秒、120～3600 update |
| playerStyle / HP | style -1はscenario既定、0/1/2はShooter/Drone/Melee。HP -1は既定、それ以外は正値かつ実際のmax以下。初期HP0は拒否し、死亡はplayer_restartの実damageで検証 |
| upgrades | authored CatalogのID。重複や不適格なmoduleは失敗報告 |
| room / enemyWave | authored room IDまたはarena fixture。waveは既存Catalog/EnemyManagerで解決するtype・位置・HP |
| initialProjectiles | 最大480個の実Bullet。snapshotの実数を記録する |
| input | ordered、非重複の半開frame区間。movement、world aim、shoot、dash |
| captureFrames | 最大32個、昇順・重複なしの完了frame。未指定時はduration内の既定値 |

未知field／scenario、非有限値、範囲外の数値、重複upgrade、重なるinput区間を拒否する。JSON parserの成功だけでゲーム上のCatalog参照が正しいとは扱わず、adapterでも確認する。

## 代表ケース

| ID | 実行する処理・観測 |
| --- | --- |
| shooter / drone / melee | 3 classの移動・通常攻撃。実弾、Drone companion、Melee attackと実敵へのdamage |
| projectile_stress / enemy_stress | 大量の実Bullet／64体の実Enemy。count、壁とactor衝突、増殖上限 |
| rival_boss / prototype_boss | 各combat policyの実際の射撃・phase更新 |
| neon_boss | authoritative bossから3D Visualへ値入力。model生成は1回 |
| boss_death | 致死イベント、AI／発射／移動停止、Directional Dissolve、解放1回・終端CB0 |
| player_restart | 致死イベント、通常GameOverと結果選択、実Scene破棄／再初期化、epoch増加とHP回復 |
| stage_transition | 最初の部屋の敵を検証用に撃破し、通常のclear／presentation／次room entry |
| expedition_transition | combat clear→upgrade service購入→次combat。通常map／serviceのcommit経路 |
| preview_lifecycle | 実モデル／rendererを3回load・破棄し、共有texture cacheを暖めた後のdescriptor返却を確認 |

`arena`は通常ケースでは既存outskirts、bossでは既存final_duelを使うDeveloper alias。壁はauthored geometryを保持し、波だけを検証用に置換する。明示roomは目的の一致も検査する。致死・room clear・HP低下はfixtureの明示イベントであり、自然攻略やbalanceの評価ではない。通常プレイのHP閾値・射撃間隔・movement・カメラ設定は変更しない。boss fixtureの中点追従は既存の見下ろしcameraを使うDeveloper captureだけに限定する。

## 記録と再現性

各processは`settings.json`、`report.json`、`snapshots.json`と指定frameのPNG／JSONを新しいdirectoryへ保存する。Snapshotには位置、HP、class、攻撃／実弾／Drone／敵数、最小enemy HP、boss phase／射撃／Dash／遭遇世代、room／node／訪問数、flow、descriptor、Visual生成／解放／CB／Dissolveを含む。

Snapshotの`bossActive`はSceneでそのboss encounterを扱っていることを表し、死亡後の結果演出中にもtrueになり得る。行動可能という意味ではない。`bossDead`／HPと実Enemyの死亡guard、死後の位置／shot／Dash不変でGameplay停止を検査する。

invariantは有限値、HPと死亡flag、frame／epoch／遭遇世代の合法な遷移、死後のPlayer／Boss移動・攻撃、count上限、Dissolve終端の資源解放を検査する。新しい遭遇世代とScene epochを明示し、合法なrestartを死後復活の違反と混同しない。死亡／再起動／遷移のケースは終了時に実際の完了状態も検査し、短すぎるdurationを成功にしない。攻撃・phaseの代表動作はactual-runtime wrapperが別途assertする。

`test_gameplay_scenarios.ps1`は各ケースを既定2回、新しいprocessで実行する。integer／boolean／identifierは一致、位置・simulation time・Dissolve等は絶対差1e-5以内で比較する。GPU／wall timeは比較から除外し、scene全体のdescriptorは共有UI cacheの影響があるため再現性の必須一致にしない。pixel-perfectや異なるGPU／driver間のbit一致は保証しない。

```powershell
pwsh -NoProfile -File project/tools/test_gameplay_scenarios.ps1
# 新しいDevelopment実行ファイルを指定したfocused実行
pwsh -NoProfile -File project/tools/test_gameplay_scenarios.ps1 -ExecutablePath '<CG2.exe>' -Scenario melee -Repeats 2
```

`arena`以外の非boss roomは現在eliminate目的に限定する。control roomは既存のresource objectiveを必要とするため、通常wave置換との組合せを拒否する。初期HP0、未知Catalog参照、目的が合わないroomは明確なerror。有効なmanifestでTitleから起動した場合も無入力のまま待たず、TANK_EXPEDITIONの起動引数を示してexit9にする。

撮影frame0はprocessの最初の初期化でだけ保存する。再起動で同じファイルを上書きしない。fadeの最終frameに撮影がある場合はold sceneのreadbackを既存fenceで解決してからSceneManagerの交換を許可する。

## 実機の最初のgate

13ケースすべて1回ずつ、初期実装の実機gateがPASSした。Shooterは`scenarios-first-pass/run_20261004_214414_779_583db386/`、他12ケースは`run_20261004_214441_244_365c5600/`。各ケースは指定数の完了frameとfresh PNG／JSONを保存し、exit0。Meleeの敵HP低下、Droneの実射撃、150以上の実弾、64体のEnemy、Boss発射／phase、死亡Dissolve終端、restart epoch2、A→購入→Bをwrapperで確認した。

Previewは317 descriptorsから共有cacheを暖めて321、load中323、teardown後321を3回確認した。共有cacheの増加4をinstance leakと呼ばない。Session単体の実processテストとは別のactual D3D evidence。

その後、自己レビューで短durationの終了判定、wrong startup、frame0とscene交換の撮影境界を整理し、両configurationを再buildした。

## 終了コード修正前binaryの反復gate

このgateのdefault Development EXEのSHA256は`CE7D899F92D73B554BB250050970223FD65FEC4C1966AB9940775DC6E425E13F`。`final-validation/artifacts/test_gameplay_scenarios/run_20261004_221102_241_e67a04a2/`で13ケースを各2回、合計26 process実行した。全reportがcompleted、全invariantがPASS、二回目は全完了frameのobservableを比較して一致した。固定frameのPNG／camera JSONは54組。Rival／Prototype／Neonは各960 frame、通常／stressは480、restart／遷移は600、Previewは240 frameを検証した。

BossDeathは両repeatともframe121／165／201でDissolve 0／0.511111／1。HP0以後の位置・発射・Dashは停止し、終端は生成1／解放1／resource OFF／CB0。Restartは実GameOverとSceneManagerの交換を通してepoch2・HP120へ戻り、Stageは次のcrossfire部屋、Expeditionは実service購入後の次combat到達を終了assertionで確認した。Previewは両repeatで実D3Dのload／teardownを3回実行し、warm cache後321→323→321 descriptorsを維持した。

別途、RivalとNeonの同じseed／dtによる960 frameのGameplay observable 30 fieldが一致した（`final-rival-neon-gameplay-comparison.json`）。Visual固有のresource／descriptor／CBとGPU時間はこの比較へ含めない。

この26件の後、最初の追加設定probeでwrapperがJSON objectのキー順を文字列として比較し、同じwave値を不一致とした。runtimeは指定HP12、最大HP138のRepair、敵HP70000、1/120秒、明示inputを適用して360 frame・errors0で終了した。元のsuite.jsonのcompleted=falseと失敗logを保持し、既定26件の成功と追加probeの失敗を区別する。

semantic比較ではkey set／stringをordinalに、配列を同じ形・順序で比較する。位置／movement／aimはC++のfloat32保存値へ正規化し、HP／frameは厳密な有限数値一致にする。座標9000.001や0.3の正常な丸めを許し、changed coordinate、型coercion、欠落／余分なkey、配列順、整数の小数変化など25 negative fixtureを拒否した。修正後のhelperで保存済み26 reportの設定も全て再検査してPASS。記録は`wrapper-comparison-proposal/applied-selfcheck.log`と`applied-revalidation.log`。

最初の再実行ではconfigured_inputがPASSしたが、unknown_roomが実際にはcompleted=false／0 snapshot／部屋の初期化errorを正しく記録しながらprocess exit0になった。Session／actorではなく、既存MainLoop／RunがWM_QUITのコードを捨て、WinMainが常に0を返す問題だった。`post-repair/probes/run_20261004_224057_706_e536dff2/`にその失敗を保持した。MainLoopはWM_QUITでmessage pumpを止めてコードを返し、WinMainは通常Finalize／trace後に返す形へ修正した。新たなSession依存やGameplay更新の変更は加えない。修正後の設定／起動拒否・撮影境界とNeon／Dissolveの再検証結果は以下に記録した。

## 終了コード修正後の実機gate

最終Development SHA256は`C3A5FF76C7CAC1AE19CA252AED5C435115816262E6EC59D4E28AAD8F6BCE30AF`。fresh suiteは`final-entry-validation/scenarios/run_20261004_224906_970_e80d8442/`でcompleted=true。NeonBoss／BossDeathを各2回、計2,880 frame実行し、repeat間の全observable、終端invariant、10 PNG／camera JSONをPASSした。旧binaryの同じ4軌跡との比較も、整数／bool／ID一致・float絶対差1e-5の元の基準で全frame一致した。比較結果は`entry-runtime-preservation/results.json`。旧26ケースを最終binaryで全て再実行したとは扱わない。

| 追加probe | 実process exit | reportのframe数・実確認 |
| --- | ---: | --- |
| configured_input | 0 | 360。HP12／最大HP138、Repair、outskirts、Charger HP70000、1/120秒、指定した無射撃→4回攻撃→無射撃、移動。frame0／120／240／360のcapture |
| unknown_room | 9 | 0。不存在roomを拒否 |
| unknown_upgrade | 9 | 0。不存在Catalog IDを拒否 |
| incompatible_room | 9 | 0。control objectiveのgatekeeperを通常wave fixtureへ変換する要求を拒否 |
| incomplete_boss_death | 9 | 120。未完の死亡／Dissolveを成功にしない |
| wrong_startup | 9 | 0。有効manifestを無引数TITLEから起動すると、必要なproject引数をfailureへ記録 |
| restart_capture_boundary | 0 | 600。frame0はepoch1で一度のみ、226は旧SceneのFadeOut終端、600はepoch2・生存HP120。readback後にSceneを交換 |
| malformed／nonexistent manifest | 各9 | 各0。constructorの失敗と通常Finalize／終了traceを実EXEで確認。個別証拠は`final-entry-validation/constructor-malformed/`／`constructor-nonexistent/` |

不正probeはruntime reportのcompleted=false／bounded errorを期待する。suiteのprobe completed=trueはその拒否検証が成功した意味で、壊れた設定のGameplayを完了した意味ではない。追加の正常2件・拒否7件が実process終了値まで一致した。PNGのcameraはmatrix shape／finiteとmetadataを検査・目視し、wrapperがrepeat cameraの数値まで自動比較したとは主張しない。

original Allで成功した変更なしのcase、修正したwrapperの25 negative fixture／保存済み26設定、最終入口の13 affected command case、source deltaと実Release packageを統合した`final-validation-aggregate.json`はPASS。元のAll 63／64と途中のexit0失敗は保持している。最終source／toolの実行中変更は0、original Allからの変更は終了処理3fileとwrapperのみだった。完全な一覧は[final-validation.md](final-validation.md)に記録する。
