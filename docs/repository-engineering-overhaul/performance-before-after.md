# Performance measurements

2026-10-05。開始時binaryを固定保存して測定し、更新後の同一sourceでDevelopmentのTrailManagerだけのcompiler最適化を二回比較した。頂点・upload・draw数を変えず、該当CPU scopeが二回とも約30%減ったため、この限定した設定を採用した。

## 条件と集計

開始HEADは`5b531d6eb3f2437ad8ab552f47f512ee8e1febd2`。fixed Development EXE / DLL / resourceのmanifestを`generated/repository-engineering-overhaul/baseline/`に保存した。Windows 11 Home 10.0.26200、i7-12650H（10 core / 16 logical）、実adapter RTX 4060 Laptop、NVIDIA driver 32.0.16.1692、1280×720、既存60Hz limiter、D3D debug layer ON / GPU-based validation OFF。Release defaultはprofiler OFFのため、この表はRelease保証ではない。

CPU timerと実際のD3D12 timestampを使う。GPUは`gpu_valid=1`のframeだけ採用し、矛盾したflagがあるframeも除外する。mean / median / nearest-rank p95 / maximumを保存し、親scopeと子scopeは足し算しない。`cpu_work_estimate_ms`は既存の推定値であり、個々のCPU scopeとは別扱い。GPU fence、Present、limiter waitも別項目に残す。

## 開始時Neon同一状態pair

各mode 30 warmup＋120 measured frame、120/120有効GPU frame。OFFも同じskeletal updateを続け、drawだけを切り替える。単位ms。

|scope / mode|mean|median|p95|max|
|---|---:|---:|---:|---:|
|Frame OFF|16.6800|16.6668|16.7218|17.2629|
|Frame ON|16.6899|16.6669|16.7874|16.8947|
|CPU work estimate OFF|4.7189|4.5766|5.6764|7.0910|
|CPU work estimate ON|5.1584|4.9418|6.5336|11.0803|
|GPU frame OFF|2.7491|2.4858|3.2891|3.3485|
|GPU frame ON|4.4544|4.4657|4.5158|4.5486|
|Global Bloom OFF|1.2356|1.0808|1.5473|1.5841|
|Global Bloom ON|1.6532|1.6594|1.6937|1.7060|
|Neon Boss ON|0.7715|0.7480|0.9226|1.0015|

OFF / ONは順番に一回ずつ計測し、thermal / power / clockは固定していない。ON−OFFを厳密な効果量や最適化の証明には使わない。既存2D gameplay・camera・world transform・clip timeが同一なのは別のruntime assertionで確認した。

## 既存fixtureでの追加baseline

Title、通常enemy AI、Special、Rival phase 2、512 trail描画stressを固定binaryで直列計測した。既存fixtureは新Scenario Runnerの固定seed / fixed timestepの保証とは別。`CG2_TITLE_AUTOTEST=1`を旧MainLoopのbackground pause回避に使う。Expedition projectは直接GameSceneで起動するため、このflagでTitleSceneを追加することはない。実際のTitle測定は元のTitle demo validationを使用する。

固定binaryで5 case全て300 CPU frameを取得。数値は`mean / median / p95 / maximum`、単位ms。GPUの有効frame数はcaptureごとに異なり、無効frameは除外済み。

|case|warmup|GPU有効 / 無効|Frame|CPU work estimate|GPU frame|
|---|---:|---:|---|---|---|
|Title live demo|120|295 / 5|19.5229 / 16.6669 / 16.7842 / 310.5221|9.1595 / 6.0199 / 10.0425 / 305.1767|4.5363 / 4.4155 / 4.9971 / 7.6728|
|通常Charger AI|180|299 / 1|16.7331 / 16.6668 / 16.7739 / 32.4439|5.3716 / 4.9370 / 7.5972 / 27.5467|4.2171 / 4.2732 / 4.5138 / 5.4108|
|Special mixed probes|180|296 / 4|17.4684 / 16.6669 / 17.0291 / 164.1351|7.0352 / 5.5838 / 11.2313 / 156.7016|4.1689 / 3.8717 / 5.7774 / 6.2310|
|Rival probe（phase 2を含む）|4500|297 / 3|16.8511 / 16.6668 / 16.7932 / 39.5081|5.8375 / 5.2337 / 8.7218 / 34.1721|4.6976 / 4.7483 / 4.8497 / 4.8753|
|512 trail描画stress|120|299 / 1|30.8363 / 30.5535 / 32.7733 / 41.9057|21.7296 / 21.4516 / 23.3179 / 32.9300|7.9839 / 8.0374 / 8.3036 / 8.4460|

Title / Specialは既存fixtureのPNG copy・capture freezeを含み、CPU maximumの大きなspikeを自然playの負荷と解釈しない。専用の新Scenario profileは`captureFrames=[]`にする。Rivalのphase-2 PNGは測定中に生成されたが、旧partial reportはprobe開始時に書かれるため、その`phase2HpInjection=false`を測定終点の状態と見なさない。通常Charger probeは敵1体・enemy bullet 0、Rivalはenemy bullet最大11。Titleはenemy最大9、Specialは2であり、大量enemyの性能保証には使わない。

512 trailは71,680 vertex / upload 2,617,336 byte / draw 1回、truncation 0。`Stress trails` CPU平均11.7503 ms（p95 12.3901）、`Draw Record (total)`平均17.5904 ms（p95 18.7212）が主要なCPU負荷。実projectile大量発射やEnemy stressの代用にはしない。旧moduleは`TITLE` / `TANK_EXPEDITION`だけを登録し、legacy `TANK_RUN`を起動する既存projectがないため、Prototypeやscene lifecycleは新Runnerの測定として区別する。

## 更新後と最適化の判断

新Scenario Runnerのsource freeze後、同じseed / timestep / actor数 / limiter / debug条件で反復し、同じscopeを比較した。`profile_gameplay_scenarios.ps1`はreal Shooter / ProjectileStress 192 / EnemyStress 64 / Rival / Prototype / Neon / BossDeathとlegacy 5 caseを直列実行する。新caseは30 warmup＋240 measured frame、Session 300 frame、画像captureなし。profiler完了で早期quitせず、Sessionが最後までsnapshot / invariant reportを書いて終了する。BossDeathは120 update完了後にkillし、最初のdead snapshotは121。living / dissolve / terminalが混ざるwindowとして、実`Scenario simulation frame` counterでCSVとsnapshotを結合して別統計を出す。新caseの開始HEAD測定は存在しない。

### TrailManagerのDevelopment最適化

既存TrailManagerはgeometry cache、Catmull-Rom係数の事前計算、linear curveのfast path、単一batch drawを既に備える。測定で重いDevelopmentの`TrailManager.cpp`だけを`/Od`から`/O2`（MaxSpeed）へ変更し、algorithm、Release設定、`/GL`を変更しない。既存のCalculation / Audio / DirectX等と同じper-file方針で、当該fileのlocal変数やsource steppingが最適化の影響を受ける点はdebug作業上の制約となる。

比較は現行同一source、同じ固定resource入力256 file、warmup120＋300 sample、overlay OFF、既存512 trail fixtureを使い、各binary二回を直列実行した。EXE SHA256はBefore `2DC4DF29DC0AEFE5D76D0EBBAE2C7E21BF5588B806B368A6E51EE8BD234997D6`、After `8B0552A1AD8AECDA7B2FF8C8336F062C8F6FBF62BAEFF9CD1A1B43167B1DE92A`。before / after / 開始時のresource入力hashは全て一致する。

数値は`mean / median / p95 / maximum`、単位ms。

|scope|Before 1|After 1|Before 2|After 2|
|---|---|---|---|---|
|Stress trails CPU|11.7537 / 11.7114 / 12.3871 / 13.1783|8.2017 / 8.1203 / 8.7798 / 12.4768|11.8340 / 11.7253 / 12.5928 / 21.2157|8.1787 / 8.1566 / 8.6944 / 9.2302|
|Draw Record total CPU|17.9816 / 17.8719 / 19.1735 / 20.6408|14.0593 / 13.9544 / 15.0092 / 23.1169|17.8230 / 17.6606 / 19.1246 / 28.7875|13.1885 / 13.1488 / 13.9424 / 14.5654|
|CPU work estimate|22.3235 / 22.1690 / 23.9599 / 26.2402|18.4050 / 18.0606 / 19.7005 / 41.8728|22.0181 / 21.8625 / 23.7856 / 33.8994|16.7213 / 16.6871 / 17.8799 / 21.5779|
|Frame|32.0354 / 31.7766 / 34.2482 / 37.7559|26.0948 / 26.3833 / 29.6325 / 50.0954|31.6205 / 31.4199 / 34.1414 / 43.0345|23.3725 / 22.6542 / 26.6380 / 29.8058|
|GPU frame|8.5565 / 8.5407 / 8.8453 / 9.0706|6.4961 / 7.4609 / 7.8664 / 8.0988|8.3848 / 8.3640 / 8.5637 / 8.6262|5.4266 / 4.8328 / 7.8469 / 7.9985|
|GPU Stress trails|0.0507 / 0.0502 / 0.0604 / 0.0666|0.0404 / 0.0410 / 0.0512 / 0.0727|0.0529 / 0.0532 / 0.0573 / 0.0655|0.0362 / 0.0338 / 0.0471 / 0.0594|
|GPU有効 / 無効frame|300 / 0|295 / 5|299 / 1|298 / 2|

Stress trails CPU meanは30.22% / 30.89%、median / p95も両回改善する。全1,200 sampleで512 trail、71,680 vertex、upload 2,617,336 byte、draw 1、truncation 0が一致し、既存の`/O2` geometry golden testも14,016 vertexを検証してPASS。既存のCPU負荷に対して限定したcompiler設定を採用する根拠とする。Afterにもmaximumのspikeがあり、60fps達成やRelease高速化は主張しない。thermal / clock / powerを固定していないため、GPU meanの減少をこのCPU設定だけの因果効果として解釈しない。

原CSV・exact command / environment・input manifest・四統計は`profiles-before-optimization/`、`profiles-after-optimization/`、paired resultは`controlled-trail-before-after.json`。新Scenario七種とlegacy残り四種を同じAfter binaryで二回ずつ測定して全て完了した。

```powershell
# repository rootから、別の新規output・同じ固定resourceで比較する。
pwsh -NoProfile -File project/tools/profile_gameplay_scenarios.ps1 `
  -Mode Legacy -CaseName existing-trail-stress-512 -Repeats 2 `
  -ExecutablePath '<current-Development-CG2.exe>' `
  -ResourceDirectory generated/repository-engineering-overhaul/baseline/runtime-project/resources `
  -OutputDirectory '<new-directory-below-repository-generated>'

& '<existing-python.exe>' project/tools/summarize_runtime_profiles.py '<profile-output-directory>'

# 採用後の七Scenario＋legacy四種。既に計測済みのtrailを重複実行しない。
pwsh -NoProfile -File project/tools/profile_gameplay_scenarios.ps1 `
  -Mode All -SkipLegacyTrail -Repeats 2 `
  -ExecutablePath '<optimized-Development-CG2.exe>' `
  -ResourceDirectory generated/repository-engineering-overhaul/baseline/runtime-project/resources `
  -OutputDirectory '<another-new-directory-below-repository-generated>'
```

原CSV、全scope/countの四統計と除外数は`baseline/performance-summary.json`、環境は`baseline/performance-environment.json`。binary/asset manifest、capture command、warmup / measured frame数、background flagを保存し、後から変更したbinaryを開始時baselineにすり替えない。


## 採用後の実Scenario / legacy反復

同じAfter EXEと固定resource入力を使用し、七つのreal Scenario各二回（warmup30 / measured240 / Session300）と既存fixture四種各二回（上記baselineと同じwarmup / measured300）を直列で完了した。全Session reportはcompleted / errors0 / capture0、CSV frame数も一致する。新Scenarioは開始時binaryでの同条件baselineがないため、その高速化率は計算しない。legacyは旧fixtureと同じ条件を維持するが、seed / clock / thermalを完全固定した因果比較ではない。

全scopeの四統計、actor count、snapshot、terminal descriptor / resource stateは`profiles-current-full/performance-summary.json`、実command / environment / EXE hashは同directoryの`run-manifest.json`へ保存した。以下は`mean / median / p95 / maximum`、単位ms。

|case / repeat|GPU valid / invalid|Frame|CPU work estimate|GPU frame|
|---|---:|---|---|---|
|shooter_1|236 / 4|17.0151 / 16.6668 / 16.8584 / 41.5892|5.1837 / 4.4915 / 8.0178 / 36.1381|4.3134 / 4.4918 / 4.7319 / 5.0186|
|shooter_2|239 / 1|16.6867 / 16.6668 / 16.8189 / 17.1021|4.7902 / 4.4397 / 7.8026 / 10.8860|4.1275 / 4.3162 / 4.5281 / 4.8742|
|projectile_stress_1|236 / 4|17.4047 / 16.6668 / 17.4036 / 154.2344|8.7715 / 8.5033 / 11.4953 / 148.7105|4.6396 / 4.6500 / 4.9644 / 5.1579|
|projectile_stress_2|236 / 4|17.4077 / 16.6668 / 17.3214 / 141.6268|8.7233 / 8.2045 / 11.0368 / 136.9326|4.6827 / 4.7437 / 5.0135 / 5.0391|
|enemy_stress_1|236 / 4|17.1305 / 16.6668 / 18.2057 / 35.9952|7.6571 / 6.9423 / 12.5483 / 30.7465|4.3532 / 4.4785 / 4.8476 / 5.0985|
|enemy_stress_2|237 / 3|16.9032 / 16.6668 / 17.4584 / 31.6335|7.5539 / 7.0778 / 11.8277 / 26.7701|4.3302 / 4.3756 / 5.5736 / 5.6975|
|rival_boss_1|227 / 13|17.0613 / 16.6669 / 17.9703 / 35.7126|6.6138 / 5.7017 / 11.1026 / 29.6984|4.5786 / 4.7268 / 5.1466 / 5.2296|
|rival_boss_2|230 / 10|16.7702 / 16.6668 / 16.8339 / 21.2218|5.9944 / 5.3623 / 9.6095 / 15.2172|4.2108 / 4.5215 / 4.8200 / 4.8783|
|prototype_boss_1|239 / 1|16.7423 / 16.6668 / 16.8537 / 19.9588|5.5830 / 5.2663 / 7.6792 / 14.5478|4.4615 / 4.7380 / 5.0268 / 5.1917|
|prototype_boss_2|238 / 2|16.7475 / 16.6668 / 16.8501 / 22.0579|5.5831 / 5.2037 / 7.5331 / 14.7531|4.3082 / 4.4380 / 5.0309 / 5.7201|
|neon_boss_1|229 / 11|16.8348 / 16.6668 / 16.9225 / 31.1832|6.2958 / 5.5770 / 10.6259 / 25.0695|3.6442 / 4.8589 / 5.1528 / 5.3248|
|neon_boss_2|229 / 11|16.7781 / 16.6668 / 16.9694 / 21.4254|6.2876 / 5.8283 / 9.8335 / 15.3908|4.8471 / 4.8640 / 5.1804 / 5.3709|
|boss_death_1|233 / 7|17.9797 / 16.6669 / 16.9348 / 206.5731|7.2876 / 5.6656 / 8.4939 / 200.8397|4.4780 / 4.7186 / 5.2132 / 5.4047|
|boss_death_2|233 / 7|16.9864 / 16.6668 / 16.8461 / 58.4586|6.0970 / 5.5734 / 7.7829 / 49.4720|4.5263 / 4.8189 / 5.2193 / 5.3381|
|title-live-demo_1|291 / 9|18.4984 / 16.6668 / 16.9450 / 225.8763|7.6671 / 5.6266 / 8.8841 / 220.7241|4.6707 / 4.6940 / 4.8783 / 5.6494|
|title-live-demo_2|294 / 6|17.6220 / 16.6668 / 16.8677 / 211.5284|6.7497 / 5.4798 / 9.1561 / 206.0609|4.6996 / 4.6925 / 4.9347 / 5.6064|
|normal-enemy-ai-charger_1|299 / 1|16.7481 / 16.6669 / 16.8064 / 34.8723|5.2072 / 4.9526 / 6.6893 / 29.0488|3.9416 / 4.0489 / 4.2824 / 5.1067|
|normal-enemy-ai-charger_2|299 / 1|16.8010 / 16.6669 / 16.7856 / 45.2929|6.0244 / 5.4886 / 8.2743 / 40.2635|3.9979 / 4.0929 / 4.3520 / 5.0647|
|special-attacks-mixed-probes_1|298 / 2|17.0777 / 16.6669 / 16.8131 / 92.7424|6.3138 / 5.5682 / 8.1323 / 88.0013|3.3197 / 3.6142 / 4.0673 / 5.1180|
|special-attacks-mixed-probes_2|298 / 2|17.0954 / 16.6668 / 16.8045 / 86.8034|5.9001 / 5.1907 / 7.1210 / 80.2006|3.3019 / 3.6060 / 4.0602 / 5.0708|
|rival-phase-two_1|298 / 2|16.8278 / 16.6668 / 16.8688 / 50.1533|5.5577 / 5.1278 / 7.4437 / 44.3841|4.7968 / 4.8005 / 5.0084 / 5.1118|
|rival-phase-two_2|298 / 2|16.7334 / 16.6668 / 16.8540 / 21.3411|5.4943 / 5.0774 / 7.6087 / 14.2988|4.7474 / 4.7821 / 4.9009 / 4.9951|

ProjectileStressは実player bulletを192で初期化し、計測中のplayer bullet平均169.06 / 最大196、enemy bullet平均1.19 / 最大4。EnemyStressは全240 sampleで64体、enemy bullet平均5.60 / 最大15。通常Shooterは3体、Rival / Prototype / Neonは3体のregular enemyに加えて実bossを動かす。Trail描画だけのfixtureから推測した数ではない。

画像captureなしでもProjectileStress maximumは154.23 / 141.63 ms、BossDeathは206.57 / 58.46 msだった。Scene Update scopeにもdeath時spikeが記録されているが、その内訳を示す証拠なしに原因を断定しない。medianがlimiter付近でも、全frameの60FPSやspike解消を保証しない。Neon一回目の有効GPU mean 3.6442 / median 4.8589 msと二回目4.8471 / 4.8640 msの差もそのまま残す。

### BossDeathの実frame別統計

実`Scenario simulation frame` counterでCSVとsnapshotを結合した。livingは31–120（90 frame）、dissolveは121–200（80）、terminalは201–270（70）である。画像copyは0であり、GPU scopeは各phase内でもvalid frameだけを採用する。terminal snapshotはNeon constant buffer 0 / visual resource OFF、全Session invariant PASS。ただしglobal descriptor総数はtext等のcacheも含むので、その増減をboss resource leakの数へ置き換えない。

|repeat / phase|GPU valid / measured|Frame|CPU Scene Update|GPU frame|GPU Neon Boss|
|---|---:|---|---|---|---|
|boss_death_1 / living|88 / 90|16.7225 / 16.6668 / 16.9352 / 19.2678|0.4720 / 0.2957 / 0.8218 / 7.9302|4.6589 / 4.7022 / 4.9203 / 4.9459|1.4000 / 1.3911 / 1.5350 / 1.6241|
|boss_death_1 / dissolve|77 / 80|19.2739 / 16.6669 / 16.8803 / 206.5731|3.5662 / 0.7987 / 1.4400 / 195.3611|5.0810 / 5.0586 / 5.3473 / 5.4047|1.3940 / 1.3885 / 1.5186 / 1.5565|
|boss_death_1 / terminal|68 / 70|18.1169 / 16.6669 / 17.0051 / 113.4725|2.6709 / 0.9432 / 2.2883 / 104.2064|3.5609 / 3.5845 / 3.6424 / 3.6690|scopeなし|
|boss_death_2 / living|87 / 90|16.7116 / 16.6668 / 16.7976 / 19.4524|0.5209 / 0.2960 / 0.8487 / 9.1891|4.7812 / 4.7862 / 5.0391 / 5.0852|1.4057 / 1.4111 / 1.5462 / 1.5780|
|boss_death_2 / dissolve|78 / 80|17.2126 / 16.6669 / 16.8367 / 58.4586|1.3702 / 0.7619 / 1.1869 / 43.8301|5.0742 / 5.0437 / 5.2941 / 5.3381|1.3777 / 1.3670 / 1.5196 / 1.5759|
|boss_death_2 / terminal|68 / 70|17.0813 / 16.6668 / 16.8639 / 43.2495|1.4290 / 0.8119 / 2.6513 / 34.4886|3.5717 / 3.6009 / 3.6372 / 3.6772|scopeなし|

この測定では新たなalgorithm変更を採用しない。TrailのCPU bottleneckに対する限定したDevelopment設定だけを残し、Boss death / cache / resource周辺のspikeをさらに細分化する必要がある場合は、別の測定課題として扱う。

上記Afterの固定binaryは、最終検証で発見した終了コードの修正より前のもの。後続変更は`WM_QUIT`でmessage pumpを停止し、そのstatusを`MainLoop / Run / WinMain`へ返す処理であり、Trail・Update・Drawのsourceとcompiler設定は変えていない。記載した実測値とSHAは測定したbinaryにだけ対応する。最終default outputの実行ファイルを再測定した値とは扱わない。
