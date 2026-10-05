# Validation evidence

2026-10-05。開始時の証拠、最初の全件検証、そこで発見した不具合の修正後の確認を区別する。最初のAllは64 command case中63 PASS / 1 wrapper FAILで終了し、その実行中のsource変更は0だった。修正後の関連検証13 command caseは全てPASSし、実行中のsource変更も0。`generated/repository-engineering-overhaul/final-validation-aggregate.json`が両runのhash・結果・適用範囲を照合し、未解決のtest failureがないことを記録する。元のAll resultはfalseのまま保持している。

## 開始時baseline

branch `refactor/engine`、HEAD `5b531d6eb3f2437ad8ab552f47f512ee8e1febd2`、staged / unstaged / untrackedはなし。開始時の全source inventory、test hash、toolchain、command、exit code、elapsed timeを`generated/repository-engineering-overhaul/baseline/`に保存した。

|対象|結果|証拠・範囲|
|---|---|---|
|Development x64 / Release x64|両方PASS、warning/error 0|`results.json`と各build log。local v145 / MSVC 14.51 / SDK 10.0.26100.0|
|既存PowerShell検証42本|全て実行してPASS|追加のhardware、ASan、Shooter/Drone route、package作成を含むcommand caseは50/50 PASS|
|standalone Python 5 suite|4 PASS、1既存FAIL|`test_neon_bloom_comparison.py`の12 assertion中1件が古いsource文字列を要求。実際のfreeze branchの故障ではない。元のFAIL logを保持|
|GPU pipeline|WARP / 実機PASS|Bloom、Neon skinning、palette/null-SRV解放と再解放、asset/schema/animationを検証|
|Neon実機gameplay|PASS|2D/3Dの同一状態pair 5組、death dissolve 0/途中/1、resource create/release各1、terminal CB 0。PNG13枚とON/OFF各120frame CSV|
|既存combat / special / map / tutorial / Title|全てPASS|6 enemy AI、Rival phase 2、Special 15 probe、Title4 stage、map transition、tutorial completion。runtime assertionを変えず実行|
|経験route / class|全てPASS|Melee / Shooter / DroneのUpper / Lower。後半forced clearを含むflow fixtureであり、難易度評価とは区別|
|Release pristine package|audit / 実起動PASS|253 files、62,910,016 bytes。pristineを保持し別のplayed copyで無引数・別cwdからTitle→tutorial→Expedition→Title→新runを検証|
|Release Developer UI|OFFを実traceで確認|`release-startup-trace-runtime.json`: developer tools、ImGui、runtime profilerはいずれも0|
|不要file audit|0件|現行CIのname / directory patternsをtracked fileへ適用。ignored生成物は提出物に含まない|

最初のsandbox内buildはMSBuild FileTrackerの`TypeInitializationException / E_ACCESSDENIED`でsource compile前に停止した。同一commandを自動承認された通常のhost実行で再実行し、両configurationが通った。環境失敗logは保管し、code failureの数へ混ぜない。WindowsAppsのPython aliasは使用できなかったため、既存Codex bundled Pythonを明示指定した。依存のdownloadや追加installはしていない。

baselineのbinary / DLL / PDB / build profileとruntime resourceはhash manifest付きで固定保存した。後続buildがこれらを上書きしない。package walkthrough中のrepair boundaryや後半forced clearはそのまま記録し、自然playの結果と見なさない。

## 抽出時のfocused validation

古いBloom Python assertionは同じ意味のguard＋early-return判定へ修正され、9種類のnegative fixtureを追加。13/13 PASSを二回確認、companion source contract 33 checksもPASS。開始時FAILを後から消していない。

Combat presentation / boss bridge、Player derived stats / movement、Preview owner lifecycle、Drone即時死亡のfocused testを追加した。Previewは実際のLoad failure catch・destructor・private release bodyをadapterで実行し、partial failure/retry、重複release、100 ownerを検証する。Droneは実際のAttack / Update / OnCollision / Damage / Die bodyで致死damage後のshot・collision・movementが全て0になることを検証する。pure componentのtestと、production bodyを抜き出すadapter testの根拠は同じではない。

Scenario Sessionのisolated testはUnicode manifest / output、120 snapshotと二scene epoch、restart後のboss generation、成功 / 失敗report、12種類の不正manifestを検証してPASS。public Session / Input override APIはDeveloper flagと`NDEBUG`の四組合せでcompileし、ReleaseではDeveloper flagを明示ONにしても除外されることを確認した。更新後の最初の実機Scenario 13 caseは一回ずつPASSしたが、最終の二回反復＋configuration probeとは区別する。

Trailの既存`/O2` geometry golden test（14,016 vertex）もPASS。同一source・同一inputのcompiler設定だけの比較ではTrail CPU scopeの平均が約30%減り、before / afterの全1,200 sampled frameでvertex / upload / draw / truncation数が一致した。AllではこのDevelopmentのper-file設定を含め、他のgameplayとpipeline testもまとめて実行した。

iteration-2のDevelopment / Release build、および更新後Neon・Release combat・package smokeの証拠は`generated/repository-engineering-overhaul/current-neon/`に分離した。その後もsource変更が続いたため、この段階の証拠だけで最終tree全体を認証しなかった。

## 再現用test inventoryとrunner

`project/tools/test_repository_validation.ps1`が全`test*.ps1` / `test*.py` interfaceを分類し、source hash・parameter・command・exit code・所要時間・logをJSONへ保存する。未知のtestは分類不足として停止し、黙って省かない。buildとruntimeを直列実行し、build failure後は古いEXEでruntimeを続行しない。実機GPUとWARP、ASan、三classのrouteを含み、packageはpristine作成・audit・別copyのwalkthroughを順番に実行する。

inventory-onlyと、runner経由の実際のPython summary fixture・Player derived stats compile/runを確認してPASS。現時点では58 interface（PowerShell 50、Python 8）。新しい`test_gameplay_scenario_release.ps1`はdefault Releaseで不存在 / 不正Scenario manifestを環境変数へ指定し、既存startup autotestをそのまま通す。普通のTitle→Expedition first-frame trace、Developer / ImGui / profiler全て0、Scenario report directoryが作られないことを実EXEで検証する。最終一括検証の新Releaseでこの二caseはPASSした。resource copyは下位directoryも含めてgenerated cacheを除外し、実input hashを保存する。

実行groupはUnit 39、Source 2、Runtime 9、Rendering 2、Packaging 3、Python Unit wrapper 1、Fixture validator 2。ユーザー指定の六分類との対応は以下のとおり。同じinterfaceがGameplayとIntegrationの両方を証明する場合があるため、この表の行数をtest件数として足し算しない。

|証拠の分類|runner group / tag|何を証明するか・限界|
|---|---|---|
|Unit|Unit / Pure component policy|小さいcontract、計算、状態遷移policy。production body adapterは実bodyをstub環境で実行する別の根拠として表示|
|Gameplay|Gameplay unit、Runtime / Actual application runtime|前者はisolated simulation、後者は実actor・collision・scene transition・capture。pure policyだけで実play成功とはしない|
|Integration|OS integration、Real asset integration、Runtime|Audio / cacheのOS接続、実asset読込、実EXEとscene lifecycleを区別。source adapterはengine全体をlinkした証拠にはならない|
|Packaging|Packaging|fixtureでのfilter検証、実Release packageのasset / DLL / hash監査、pristineを保持した別copyの無引数walkthrough|
|Rendering pipeline|Rendering|実D3D12のWARPとhardware、shader / skinning / palette解放。画面品質はPNGを別に目視確認する|
|Source / static validation|Source / Source contract、Synthetic / source fixture|build profile / Release guard / source文字列の意味を検証。fixture validator二本は対応fixture suiteから実行し、実gameplay runtimeの代用とはしない|

Python quality maskは既存PowerShell wrapper経由で実行する。fixture入力に依存するPython CLI二本も専用fixture suiteが呼び出すため、58 interfaceを全て同じ単体command形式で走らせる構成ではない。実行したcommand caseは`results.json`に個別に残す。

```powershell
# 軽量inventory（build / runtimeなし）
pwsh -NoProfile -File project/tools/test_repository_validation.ps1 -InventoryOnly

# 最終tree全体。既存の使用可能なPythonを指定する。
pwsh -NoProfile -File project/tools/test_repository_validation.ps1 -PythonPath '<existing-python.exe>'

# 小さな変更のfocused check。All実行済みと同じ意味にはしない。
pwsh -NoProfile -File project/tools/test_repository_validation.ps1 -Phase Unit -TestName test_player_drone_lifecycle.ps1
```

defaultは新しい`generated/repository-validation/<timestamp>_<id>/`を使う。明示outputもrepositoryの`generated/`配下の新しいdirectoryに限定する。`-NoBuild`、`-SkipHardware`、`-SkipAddressSanitizer`は結果JSONへ残るため、欠けたcheckをfull validationと呼ばない。CIのv143を使う場合は`-PlatformToolset v143`を指定する。fixture validatorのPython CLIは対応fixture suiteに分類し、互換性のないgameplay captureを渡した検証結果を捏造しない。quality-mask Pythonは既存PowerShell wrapperから実行する。

実行前のsource input hashを保存し、終了時に変更があれば失敗にする。共有runtime directoryの成果物は開始時刻以降に書かれたfileだけを各caseへcopyし、過去のcaptureを新runの証拠にしない。

## 全件検証と、その後の修正確認

outputは`generated/repository-engineering-overhaul/final-validation/`、上位logは`final-validation-driver.log`。default outputへ新しくbuildしたDevelopment x64（33.41秒）とRelease x64（10.48秒）は共にwarning / error 0。Release EXE hashは`52AC736ED93301B7E9E1CABB2E1354C7EC638FA4631ED72331EA250E5309182E`で、Release除外smoke二caseがこの実EXEを使用した。WARP / hardware Bloomとskinning、build profile、pure Session、Player class ASanもPASSした。

captured default Scenario 13種×2は全てPASS、13組のrepeat比較と15,600 simulation frameを確認した。一方、最初の追加configuration probeではruntime自体が360 frame / errors0、指定HP12・enemy HP70000・Repair最大HP138・実attack4回を反映して終了したものの、wrapperがJSON objectのmember順を文字列比較して失敗した。requestの`type / position / hp`とreportの`hp / position / type`が同じ値でも異なる文字列になるためである。元のfailed case / report / logを保持し、元のAllを後からPASSへ書き換えない。

review済みwrapperはobject keyの順序を無視し、array順序、string / bool、整数instructionを厳密比較する。position / movement / aimは実schemaと同じfinite float32へ正規化するため、許可された高座標`9000.001`とreportの`9000.0009765625`も正しく一致する。25種類の変更・型・shape・順序negative fixtureと高座標positive fixtureがPASSし、元のdefault26 reportのsettingsも修正比較で再確認した。証拠は`wrapper-comparison-proposal/applied-selfcheck.log`と`applied-revalidation.log`。

その修正後の`post-repair/`では両incremental buildがPASSし、当時のEXE hashも変わらなかった。正常configuration probeはPASSしたが、unknown roomはbounded failure report / 0 frameを出しながらprocess exit 0となり、期待した9に一致しなかった。`WinMain`が常に0を返し、message pumpも`WM_QUIT`後のmessageで終了状態を上書きし得ることが原因だった。`Game::MainLoop / Run`からquit statusを返し、`WinMain`は資源解放とtraceを終えてその値を返すよう修正した。Releaseでも通常の正常終了は0、validation失敗は要求されたstatusになる。

元のAllから修正後freezeまでのsource input差分は`Game.cpp`、`Game.h`、`main.cpp`、Scenario wrapperの四fileだけ。`final-entry-validation/delta-from-original-all.json`に各before / after hashを保存した。simulation、Trail、Update / Draw pipelineはこの修正で変更していない。元のdefault26実機結果はDevelopment SHA `CE7D899F92D73B554BB250050970223FD65FEC4C1966AB9940775DC6E425E13F`の証拠として保持する。最終EXEと同一だったと扱わない。

関連検証は`final-entry-validation/`へ分離し、Development / Releaseのbuild、Game.cppを読むBloom Python 13 / source contract 33、startup trace unit、captured Neon / BossDeath各二回、configuration七probe、constructor不正 / 不存在manifest二probe、Release除外 / runtime UI profile、新pristine package / audit / 別copy walkthroughを直列で実行し、13 command case全てPASSした。開始UTCは`2026-10-04T22:48:25.6013251Z`、終了UTCは`2026-10-04T22:53:58.6824130Z`。source input manifestも前後で照合し、変更0を確認した。

再実行したNeon二回（各960 frame）とBossDeath二回（各480 frame）を元のAllの対応snapshotと照合した。既存の`Compare-ScenarioSnapshots`を変更せず、既存absolute float tolerance `1e-5`で全2,880 frameのgameplay observable stateが一致した。追加のapp起動は0。`entry-runtime-preservation/results.json`と`comparison.log`に両EXEのSHAと比較元を記録した。descriptor / text cacheなど比較対象外の非gameplay stateまで一致したとは言わない。

|修正後の最終確認|結果・証拠|
|---|---|
|Development x64 / Release x64|PASS、warning / error 0。MSBuild時間17.69 / 14.62秒、command時間18.51 / 15.14秒|
|変更したGame.cppのsource contract|Bloom Python 13 tests / companion 33 checks PASS。startup trace unitもPASS|
|最終captured Neon / BossDeath|各二回、計4 run / 2,880 frame / 2 repeat比較 PASS|
|configuration七probe|正常設定360 frameとrestart600 frameはexit0。unknown room / upgrade / incompatible room / short BossDeath / TITLE startupはbounded reportとexit9を確認|
|constructor不正 / 不存在manifest|各exit9 / gameplay 0 frame / bounded report。traceの`process.finalized`も確認|
|default ReleaseのScenario除外|不正 / 不存在manifestを無視してexit0。通常Title→Expedition first frame、Developer / ImGui / profiler全て0、Scenario出力なし|
|profile / 実binary|五profile組合せ、実EXEとbuild profileのSHA、上記の最終runtime UI counter全てPASS|
|新pristine Release package|253 files、62,911,552 bytes。asset / DLL / default Title / SHA / user progressなしのaudit PASS|
|別copyの無引数walkthrough|PASS、別cwdから起動。Title二回 / 新run二回 / 三新enemy、repair boundary、boss/result、Title帰還、tutorial完了後skipを確認|

最終Development SHAは`C3A5FF76C7CAC1AE19CA252AED5C435115816262E6EC59D4E28AAD8F6BCE30AF`、Release SHAは`14393F15CBA2FAAE4E4B922DD7828F1F69539A9EF21872658ECA0FADE6C80A87`。fresh packageもこのReleaseと同じSHAである。pristineは`final-entry-validation/pristine-package/`、played copyは`final-entry-validation/package-runtime-copy/`。後半forced clear / repair boundary fixtureはflow確認であり、難易度playtestとは区別する。

元の全件検証の63成功commandとdefault26 run、修正wrapperのnegative fixture / offline report比較、最後に変更した四fileに関係する上記再検証を組み合わせた結果である。最終binaryで64 command全部やdefault13種全部を再実行したと記載しない。CIは既存四workflowを静的にauditし、local toolchainで関連checkを通した。remote GitHub Actionsを起動した結果ではない。代表captureの実画像確認は[visual-validation.md](visual-validation.md)、全体の差分review / final git statusは[README.md](README.md)に記録する。

Scenario determinismはpure policyだけでなく実際のactor / collision / transitionを通した証拠で評価する。性能の判断と測定binaryの適用範囲は[performance-before-after.md](performance-before-after.md)に記録する。新しい全件runを再現する場合は上記runnerを使い、過去のfailed runを上書きしない。
