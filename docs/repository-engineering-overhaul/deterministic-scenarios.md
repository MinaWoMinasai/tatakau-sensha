# Gameplay Scenario Runner

この基盤はDeveloper専用の設定・入力・観測adapter。通常のPlayer、Enemy、BulletManager、Stage、衝突、遠征の部屋／サービス／結果遷移を呼び、別の戦闘simulationを実装しない。Releaseでは設定読込・入力・記録の経路を`CG2_DEVELOPER_TOOLS && !defined(NDEBUG)`で無効にする。

## 境界

- `game/debug/GameplayScenario.h`: graphics-freeな設定、frameごとの入力選択、observable Snapshotとinvariant。
- `GameplayScenarioSession`: processが所有する設定と記録。Sceneを破棄してもglobal simulation frameは継続し、Scene epochだけ増える。actorやrendererのpointerを保存しない。
- `game/debug/session/GameplayScenarioRunner.cpp`:既存の部屋／map entryとactor APIへ設定を適用し、更新後に値を読む。captureは既存NeonShowcaseCaptureと既存frame fenceを使う。
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

実行結果、比較画像、特定ビルドの測定値はローカルの`generated/`に保存する。過去の実行結果を現在のソースの検証済み表示へ流用しない。
