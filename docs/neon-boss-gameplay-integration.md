# 3D Neon Boss の本編統合

実施日: 2026-10-05（Asia/Tokyo）。既存の2D戦闘を基準に、既存モデル・GPU Skinning・NeonSkinnedRenderer・Directional Dissolveを本編へ接続した作業記録。

## 開始状態と対象

- branch: `feature/neon-boss-gameplay-integration`
- HEAD: `a2d9c86b15f6fbf74bca2bb628cc7872c252c086`
- `git status --short`: 出力なし。既存変更・未追跡ファイルなし。
- 調査開始後も同じbranch/HEADを維持。commit / push / merge / checkout / reset / cleanは実行しない。
- 親ディレクトリとリポジトリ内に適用されるAGENTS.mdは見つからなかった。

## 調査した既存構造

`GameScene::Initialize`がEnemyと既存Object3dを生成する。遠征では`StartAuthoredExpeditionRoom` / `StartTankExpeditionRoom`が同じEnemyへ`ResetRunEncounter`を呼び、本編ボス部屋では`EnableExpeditionRival(true)`を適用する。戦闘更新はPlayer攻撃→Enemy AI→弾→特殊攻撃→CollisionManager→HP/死亡イベントの順序。`UpdateGameplayEventEffects`から`BeginBossDefeatSequence`へ移り、通常戦闘を停止する。既存のボスHPバー・予告・弾・結果画面はこの経路を維持する。

EnemyがXY平面の位置（Z=0）、照準（rotate.z）、HP/maxHP、半径、被弾フィードバック、死亡を保持する。RivalBossCombatはReposition / Tracking / Locked / Volley / DashWarning / Dash / ReloadとHPによる第2段階を公開する。PrototypeBossCombatはRecovery / Telegraph / Attackと発射要求を計算する。AimedSpreadは同じ更新内にRecoveryへ戻るため、発射回数もPresentationへ渡す。

CollisionManagerはHP<=0のボスをペア生成と走査中の再検査で除外する。元のEnemyには致死ダメージ通知後、次のUpdateの末尾まで死亡が確定しない経路があった。今回、ダメージ通知中に死亡を確定し、AI・射撃・接触通知・ノックバック・捕食回復を止める。

NeonSkinnedPreviewはDevelopment限定でF3/F12から利用する。既存Recommended Line Artの面・輪郭・Texture内部線を小さな共有ヘルパーへ移した。Previewの巨大なUIやShowcaseカメラをGameSceneへコピーしていない。通常Bloom設定は変更しない。

SkinCluster / SkinnedModelはAssimp GLB、骨格・GPU palette SRV、既存`RegisterAnimations` / `TransitionToAnimation` / 再生速度・停止・Seekを利用できる。NeonSkinnedRendererはScene HDR/Normal/Materialの3MRTとD24S8に描き、独立Body/Outline/Stencilパスが共通Dissolve判定を使う。progress=1の全消去は既存pipelineテストで画素・Depthも検証する。

実素材は`project/resources/models/neon_hologram/AvatarSample_B.glb`（28,333,772 bytes）。7描画submesh、118 joint、21,961頂点。埋め込みAnimationは0。既存生成クリップは`Preview_Idle`（3.0秒）と`Preview_Attack`（1.8秒）。`BindPose`は既存の空クリップ。Walk / Dash / Damage / Deathクリップは存在しない。外部から新素材は取得していない。

GLBのSHA256は開始時から変わらず`7FCA4A77FDC60AB2C78A9907430744562626180125FA386EB74FB2EA15C2E518`。確認結果は`generated/neon-boss-integration/model-integrity.json`。

## 採用した構造と変更ファイル

依存方向は`Enemy / Combat → 値のsnapshot → NeonBossVisual → NeonSkinnedRenderer`。VisualはEnemyやCombatへの参照・コールバックを持たず、ゲームプレイ状態を変更できない。

| ファイル | 役割 |
| --- | --- |
| `project/game/enemy/visual/NeonBossVisualState.h` | 純粋な行動mapping、死亡の不可逆latch、Dissolve終端・解放要求 |
| `project/game/enemy/visual/NeonBossVisual.h/.cpp` | lazyな一度のモデル/生成clipロード、更新、描画、位置・照準・HP表示、Dissolve、資源解放 |
| `project/game/scene/GameScene.NeonBoss.cpp` | 状態snapshot、本編描画の選択、Developer UI、実機capture/Profiler検証 |
| `project/game/scene/GameScene.h/.cpp` | 小さな所有memberと更新/描画/capture hook、既存ボス描画の重複抑止 |
| `project/game/scene/GameScene.Authoring.cpp` | F3から本編ボス確認タブを開く入口 |
| `project/game/enemy/actor/Enemy.h/.cpp` | 即時の安全な致死処理、行動停止、遭遇世代・行動段階・射撃数の読取API |
| `project/DirectX/engine/3d/neon/NeonCharacterStyle.h` | 既存Recommended Line Artの共通設定 |
| `project/game/debug/NeonSkinnedPreview.cpp` | 共通設定の利用（既存値を保持） |
| `project/game/debug/NeonPreviewAnimations.h` | 既存生成clipを本編でも共有する説明 |
| `project/DirectX/engine/3d/SkinCluster.h/.cpp` | fence完了後にpalette SRVとbufferを明示解放するAPI |
| `project/DirectX/engine/3d/neon/NeonSkinnedRenderer.h/.cpp` | null-mask SRV / PSO / CBの明示解放API |
| `project/CG2_testPro.vcxproj/.filters` | 新規source登録、生成clipをReleaseでも利用 |
| `project/tools/boss_gameplay_lifecycle_tests.cpp`, `test_boss_gameplay_lifecycle.ps1` | 実Enemyメソッドの致死・停止・明示reset回帰 |
| `project/tools/neon_boss_visual_tests.cpp`, `test_neon_boss_visual.ps1` | mapping / 非restart / 不可逆死亡 / Dissolve lifecycle |
| `project/tools/test_neon_boss_runtime.ps1` | 本編での代表状態撮影、ON/OFF比較、実資源解放、Profiler記録 |
| `project/tools/tank_collision_tests.cpp` | 既存adapterに遭遇世代と射撃数のfieldを合わせる |
| `project/tools/neon_preview_animation_tests.cpp`, `neon_skinned_pipeline_tests.cpp` | 明示SRV解放・二重解放防止のadapter検証 |
| `project/tools/TankSubmissionPackage.ps1`, `test_tank_submission_packaging.ps1` | Release配布へ既存GLBと必要shaderを含め、欠落を検出。Previewのline_masksは除外維持 |
| `project/resources/models/neon_hologram/README.md` | 本編/Releaseでの素材利用と共有clipを説明 |
| `docs/neon-boss-gameplay-integration.md` | この実装・検証記録 |

## Boss state → Visual / Animation

| 状態 | 実clip / フォールバック |
| --- | --- |
| Idle / Prototype Recovery | Preview_Idle、loop、速度1.0 |
| Reposition / 通常AIの移動 | Preview_Idle、loop、速度1.35 |
| Tracking / Locked / DashWarning / Prototype Telegraph | Preview_Attack、速度0.8、0.50秒の溜め姿勢を保持 |
| Volley / Prototype Attack | 同じPreview_Attackを続行、速度1.6 |
| 同一更新内にRecoveryへ戻る単発射撃 | 発射数の増加を観測し、既存Idle/移動時だけ0.70秒の攻撃表示を保持 |
| Dash | Preview_Attackの0.82秒の押し出し姿勢を保持 |
| Reload | Preview_Idle、loop、速度0.7 |
| Damage / HP低下 / Phase 2 | 専用clipはないため現在動作を維持し、局所Body/発光色へHP・被弾率・第2段階を反映 |
| Death | 専用clipはないため現在の骨格姿勢を停止し、Directional Dissolve |

同じ行動を毎フレーム渡してもclipを選び直さない。clip種別が変わるときだけ既存0.2秒blendを使う。Telegraph→Attackは同一clip上で速度と再生停止だけを切り替える。生存中は戦闘時間・メニュー/進化画面の停止に同期し、死後は基準時間で消去を進める。

実モデルboundsの中央をauthoritativeなボス位置へ置き、高さを生存時半径×2.2に合わせる。見下ろしカメラはそのまま。モデル側に0.35radの傾きと照準方向の平面回転を与える。致死フレームの最終位置/向きへ追従してから停止し、死亡によって半径が0になっても直前の生存時scaleを保持する。

## Death / Dissolve lifecycle

1. `TakeDamage` / 弾衝突中にHP=0と死亡が確定。速度・ノックバック・Dash距離をゼロにし、AI/射撃/接触/捕食を停止。
2. 衝突後にVisualが死亡snapshotを読む。現在のpalette姿勢を停止し、カメラに対して左上→右下の方向を既存APIでモデル軸へ変換。実骨格頂点から一度だけscan boundsを求める。
3. 0.12秒の姿勢保持後、1.2秒でDissolveを0→1へ進める。ゲームプレイ死亡へは戻れない。
4. progress=1で描画を停止し、次の通常fence完了後Updateでモデル・Object・Rendererを解放する。palette/null-maskの2つのinstance SRVもFreeする。見えなくなった後に旧ボスBodyへ戻るfallbackはしない。
5. 次の正式な`ResetRunEncounter`で遭遇世代が変わった場合にだけVisualの死亡latchをresetする。

既存の死亡particles・画面pulse・結果遷移は残るため、Dissolve終端画像で見える既存演出は3D残骸と区別する。

## Developerでの再現

Visual Studio Development x64で起動 → タイトル「遠征をはじめる」→ F3「Neon Boss / 本編ボス確認」（F12のNeon Bossタブでも可）。

- 「本編ボス戦へ移動 / 再生成」: 現在の制作済みBoss部屋を、本編の部屋生成経路で開始。メモリ内のルートを1ノードにし、制作JSONは保存しない。現在の遠征進行はこの確認runに置き換わる。
- `3D Neon Visual` OFFで従来戦車と比較。`Freeze for visual comparison`で同じ状態・カメラを維持。
- `HP -> 33% / Phase 2`で実HPを減らす。`Kill / Directional Dissolve`で実際に死亡させる。
- HP、Phase、Phase 2、射撃数、clip、Dissolve、資源生成/解放数を表示。
- `Capture gameplay`で既存NeonShowcaseCaptureを使用し、PNG+JSONを保存。
- `Profile 3D ON/OFF`で既存timestamp Profilerを120フレーム記録。
- 自機死亡中は結果画面から再出撃して利用する。確認runでは自機を無敵にする。
- ReleaseではこのUI、環境変数確認run、captureは`CG2_DEVELOPER_TOOLS && !NDEBUG`で除外。本編3D Visual自体はReleaseでも有効。

自動の実機確認（リポジトリルート）:

```powershell
.\project\tools\test_neon_boss_runtime.ps1
```

このスクリプトはDevelopment実行ファイルを`project`作業ディレクトリで起動する。実際のRival AIが予告・Volley・Dashを行うのを待ち、HPだけphase/death確認用に注入する。生存5状態は同じカメラ・姿勢の従来/3Dペア。Dissolveは0 / 約0.5 / 1の3状態。

自動fixtureだけは、同じ見下ろし方向・Z=-55・projectionで自機とボスの中点を追う。これで画面端に走ったDashも全身を撮れる。通常プレイの自機中心follow-cameraは変更しない。比較Freezeは既存BloomのDeveloper `comparisonFreeze`を使って一時的にTAA/jitterを止め、両ボス表現の行列を時間0で更新する。通常設定・threshold/intensity/exposureは保存も変更もしていない。Idleは生成clipの初期blendが終わる0.45秒以降を撮る。

Attackは実Volley中、clipの0.80秒以降の押し出し姿勢を撮る。fixtureの自機は移動・射撃入力を与えない無敵状態であり、物理的なノックバックまで固定してはいない（metadataの`stationary`はこの入力条件を指す）。Developer用HP注入と無敵は難易度・攻略性のテストではない。

## テスト・ビルド・実機結果

検証ログは`generated/neon-boss-integration/`。実装直後と自己レビュー後に重要な回帰を再実行した。

| 検証 | 結果・範囲 |
| --- | --- |
| `test_boss_gameplay_lifecycle.ps1` | 実装後/自己レビュー後/最終確認PASS。実Enemy宣言/メソッドを描画・経路adapter付きで使用。直接/弾の即時致死、UINT32_MAX、死後AI/射撃/衝突/ノックバック/捕食停止、保持particles、非復活、世代reset、無効遭遇を検証 |
| `test_neon_boss_visual.ps1` | 実装後/自己レビュー後/P2修正後PASS。実clipへのfallback、600フレーム同一行動の非restart、Telegraph→Attack継続、Attack→Telegraphの溜めpose再入、Dash姿勢、HP0不可逆、inactive中の死後進行、不正dt、終端1/解放要求1回/reset |
| `test_rival_boss_combat.ps1` | 初期/実装後/自己レビュー後PASS。既存の行動時計・残弾・Dash・phase |
| `test_tank_run.ps1` | 初期/実装後/自己レビュー後PASS。Run、PrototypeBossCombat、Run modifiersの3suite |
| `test_tank_collisions.ps1` | 実装後/自己レビュー後PASS。実CollisionManagerの致死中再検査を含む |
| `test_tank_enemy_combat.ps1` | PASS。既存AIの時間計算と実コードadapterによるguard/reflection/EMP/summon/navigation |
| `test_neon_skinned_model.ps1` | 初期PASS。実GLBの骨格・weight・構造 |
| `test_neon_preview_animations.ps1` | 実装後/自己レビュー後PASS。実GLBの18955 weighted vertex変形、blend/停止/Seek/復帰、Dissolve姿勢bounds、palette SRV解放1回・二重解放防止 |
| `test_neon_dissolve.ps1` | 初期/自己レビュー後PASS。方向変換・パラメータ・scan/lifecycle |
| `test_neon_skinned_pipeline.ps1` WARP | 初期/実装後/自己レビュー後PASS。DXC、MRT、Depth、Outline、Stencil、palette motion、texture内部線、alpha cutout、Dissolve終端の画素/Depth消去、Renderer SRV解放・冪等性 |
| 同pipeline `-Hardware` | NVIDIA GeForce RTX 4060 Laptop GPUでも初期/実装後/自己レビュー後PASS |
| `test_tank_submission_packaging.ps1` | 実装後/自己レビュー後PASS。必須GLB/shaderの欠落拒否、候補mask除外、既存著作権/ライセンスnotice保持 |
| `package_tank_submission.ps1` → `test_tank_submission.ps1 -PackageDirectory …` | 最終Releaseから実配布packageを生成・監査してPASS。253 files / 62,910,014 bytes |
| `test_developer_tools_profile.ps1` | 5構成の設定・build profile/SHA256検証PASS。Release通常のDeveloper OFFを確認 |
| `test_tank_combat_runtime.ps1 -Configuration Release` | 自己レビュー後/最終Releaseの2回実機PASS。6種AIの実移動/実弾/再装填/壁衝突。Rivalは移動約130.1、25発射、5Dash、5Reload、第2段階、全patternMask=7。壁交差/貫通/reload違反は0。最終回は`CG2_NEON_BOSS_AUTOTEST=1`も設定し、ReleaseがDeveloper fixtureを無視することを確認 |
| `test_neon_boss_runtime.ps1` | 修正後/最終画像QA後PASS。13枚の新規PNG/JSON、5組の同状態比較、Dissolve 3段階、資源lifecycle、120フレームずつのProfilerを検証 |
| `git diff --check`、branch/HEAD再確認 | PASS。同じbranch/HEAD、想定した統合・検証・文書のみの変更 |

実行コマンド・試行ごとの結果・ログ名は[validation-results.json](../generated/neon-boss-integration/validation-results.json)に記録。親側で再実行した新規2suiteの最終ログは[final-boss-gameplay-lifecycle.log](../generated/neon-boss-integration/final-boss-gameplay-lifecycle.log)と[final-neon-boss-visual.log](../generated/neon-boss-integration/final-neon-boss-visual.log)。テストadapterと実GPU/本編runの範囲は上表のとおり区別する。

ビルドコマンド（同じMSVC v145、Visual Studio 18 Community）:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' project/CG2.sln /m /p:Configuration=Development /p:Platform=x64
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' project/CG2.sln /m /p:Configuration=Release /p:Platform=x64 /p:CG2DeveloperTools=false
```

Development / Releaseの実装後・自己レビュー後buildは成功。最終sourceを含むincrementalも両方exit 0で成功。最終ログは[development-build-visual-qa.log](../generated/neon-boss-integration/development-build-visual-qa.log)と[release-build-visual-qa.log](../generated/neon-boss-integration/release-build-visual-qa.log)。Debugはsolution上Developmentへ割当されるため、別構成の成功として水増ししていない。

途中の失敗も区別して保存:

- 最初のsandbox内MSBuild FileTrackerはE_ACCESSDENIED（環境制約）。同じbuildのsandbox外実行は自動承認を経て実行できた。
- 初期build調査と並行中に親側の新sourceがまだprojectへ登録されていなかったためLNK2001/LNK1120。既存baseline不具合とは扱わず、登録後に解決。
- 実装初回にC2668（MakeAffineMatrix overload）。コード修正後に成功。
- 明示`SrvManager::Free`追加後、既存animation test adapterに同stubがなくLNK2019。stubと実際の解放回数検証を加えて成功。
- 最初のDeveloper実機runは0xC0000005。dump/stack/traceを保存してmap UI ownershipを修正。`first-runtime-crash.dmp` / `first-crash-stack.log` / `first-crash-startup-trace.json`は上記ログディレクトリ。
- 次のrunは本編側`completed=true/errors=[]`と13captureを保存したが、wrapperがTAAによるsubpixel camera行列差を検出してFAIL。テスト基準は緩めず既存comparisonFreezeへ接続。画像の目視でDashの端切れとIdleの初期bind-to-idle姿勢も見つけ、fixtureのmidpointカメラとIdle撮影待ちを追加。最初の画像は`generated/neon-boss-integration/jitter-capture-attempt/`へ保存して最終画像と区別。

Release packageも実際に生成し、既存GLB/必須shaderを含むことと候補maskの除外を検証した。最終生成先`generated/neon-boss-integration/release-package-visual-qa/`。最終Release executableのSHA256と一致し、`test_tank_submission.ps1`の監査もPASS。初期/中間packageは別directoryへ保持した。このローカル生成は配布・公開を行う操作ではない。Third-party noticesの本文・利用条件は変更していない。

## 実機captureと目視結果

最終Developer runの[validation.json](../project/generated/neon_boss_gameplay/validation.json)は`completed=true`、`errors=[]`、13 capture、実戦闘時間約11.83秒。最終ログは[visual-qa-runtime.log](../generated/neon-boss-integration/visual-qa-runtime.log)。最新画像/metadataは`project/generated/neon_boss_gameplay/`、固定コピーは`generated/neon-boss-integration/visual-qa-gameplay-captures/`。PNGは実backbufferをImGui前に保存した未編集画像。

| 状態 | 最新capture | 目視した内容 |
| --- | --- | --- |
| Idle / 位置取り | [idle.png](../project/generated/neon_boss_gameplay/idle.png) | 腕を下ろした人型、暗い面とピンク輪郭。初期blend後の姿勢 |
| Telegraph | [telegraph.png](../project/generated/neon_boss_gameplay/telegraph.png) | 実Locked状態の溜め。既存予告・HPバーも維持 |
| Attack | [attack.png](../project/generated/neon_boss_gameplay/attack.png) | 実Volley、clip約0.8267秒の押し出し姿勢。Body全体の白飛びは見られない |
| Dash | [dash.png](../project/generated/neon_boss_gameplay/dash.png) | 全身がviewport内。既存Dash予告・弾と並存 |
| HP低下 | [low_hp.png](../project/generated/neon_boss_gameplay/low_hp.png) | 実HP 300/900、第2段階、赤寄りの局所表現 |
| Dissolve 0% | [dissolve_000.png](../project/generated/neon_boss_gameplay/dissolve_000.png) | HP0・停止した姿勢の全身が残る |
| Dissolve 中間 | [dissolve_050.png](../project/generated/neon_boss_gameplay/dissolve_050.png) | progress約0.5111、上半身が消え脚が残る。既存撃破grain/歪みもかかる |
| Dissolve 100% | [dissolve_100.png](../project/generated/neon_boss_gameplay/dissolve_100.png) | Body・Neon輪郭の残骸なし。既存結果演出・他entityは残る |

生存5状態には対応する`*_legacy.png/.json`も保存。wrapperが各ペアのゲームプレイsnapshot、camera全行列、world、clip時刻、descriptor数の完全一致を確認した。3D ON/OFFでAI/HP/位置/弾・Dash状態を変更していない。死亡3段階のHP0・射撃数・Dash数・位置の固定も検証した。

親と別agentで最終画像を目視し、既存2D collider中心との大きなずれ、90度の軸ずれ、地面への埋まり、浮いた輪郭、Dashの欠け、終端の残像は見つからなかった。これは代表状態の画像確認であり、全操作・全部屋の網羅や美的品質の保証ではない。人型は約120pxの輪郭で顔の細部は小さく、頭を照準側へ向ける平面配置と中間の既存撃破filterには好みの確認余地がある。

Releaseの実機戦闘証拠は`project/generated/combat_validation/`と`generated/neon-boss-integration/final-release-combat-captures/`。[final-release-combat-runtime.log](../generated/neon-boss-integration/final-release-combat-runtime.log)で、最終executableの6AI検証とDeveloperフラグ除外を確認できる。

## 性能と資源

最終測定はDevelopment x64、RTX 4060 Laptop GPU、1280×720、D3D12 debug layer ON / GPU Based Validation OFF。既存timestamp Profiler（周波数1,000,000,000Hz）で同じFreeze状態を30フレームwarmup後、OFF/ONそれぞれ120有効フレーム測定。撮影copyは測定フレームに入れていない。CSVは[profile_off.csv](../project/generated/neon_boss_gameplay/profile_off.csv) / [profile_on.csv](../project/generated/neon_boss_gameplay/profile_on.csv)、集計は[final-performance-summary.json](../generated/neon-boss-integration/final-performance-summary.json)。

| 計測scope | OFF 平均 / P95 ms | ON 平均 / P95 ms |
| --- | ---: | ---: |
| GPU frame | 2.0200 / 2.9256 | 4.2172 / 4.3305 |
| Scene 3D（他effectsを含む） | 0.5945 / 0.8581 | 1.7137 / 1.7838 |
| Neon Boss描画 | — | 0.7863 / 0.9390 |
| Global Bloom / Post | 0.9370 / 1.3527 | 1.5136 / 1.5759 |
| CPU Neon Boss Update | 0.0447 / 0.0576 | 0.0430 / 0.0652 |
| Frame elapsed | 16.6995 / 16.8656 | 16.6805 / 16.7577 |

一度の順次OFF/ON計測で、温度・clock・powerは制御していない。前回の別試行とも値に差があるため、GPU frame差分を3Dだけによる制御された増分とは断定しない。親GPU scopeに子scopeが含まれ、表の値を足し合わせない。debug layer込みのDevelopment値でありReleaseの性能保証ではない。ON/OFFは描画選択だけを変え、両方で同じVisual状態/骨格更新を続けるので、CPU測定はVisual完全停止との比較ではない。

本編runでモデル/animation/rendererの生成1回、死亡終端の解放1回を確認。Freeze/Profiler中も描画CBは1個のまま、終端で0。生存ペアはdescriptor数が同じで、終端のscene全体descriptor数は353。paletteとnull-maskの2個のinstance SRVを返却する実model/renderer APIは各Free 1回・二重解放なしをテストでも確認した。他の既存particles等もdescriptorを取得するため、capture間のscene全体のdescriptor数は単純な単調列にはならず、その差分だけでVisualの解放個数を測らない。TextureManagerの既存共有texture cacheは保持する。

source確認でも毎フレームのmodelロード・clip生成・scan bounds計算・追加GPU fence待ちはなく、scanは死亡開始時の1回。解放は既存frame fence完了後のUpdate/終了順序に乗せる。無制限CB生成は自己レビューで発見して修正し、最終runで再確認した。

## 発見して修正した問題

- 致死ダメージが次のAI/射撃を先に許す経路、および死亡後に通常敵の捕食でHPを回復し得る経路を停止。
- unsignedダメージのint変換によるoverflowを避け、UINT32_MAXでもHPを安全に0へclamp。
- Prototype単発射撃を1フレームだけ表示するとclipが即Idleへ戻るため、Presentationだけで短い保持時間を追加。
- HP0で半径が0になることと、致死前のDash移動でVisual位置が前フレームに残ることを、最終位置と生存時scaleの保持で解決。
- モデルpalette SRVとRenderer null-mask SRVが従来はインスタンス破棄で返却されなかったため、今回の所有境界に明示解放APIを追加。
- モデルがRelease配布から除外されていたため、必要GLB/shaderの存在検証を追加。
- 初回コンパイルの`MakeAffineMatrix` brace引数がQuaternion overloadと曖昧になったため、Vector3型を明示。
- 最初の実機確認でDeveloperの1ノードfixtureに旧map edge UIが残り、存在しないnode参照でアクセス違反が発生した。既存dumpをDbgHelpでsymbolizeし、`RefreshExpeditionMapUi`の位置計算まで特定。fixtureのnode Visual数とedge所有を同時に更新して修正。
- 自己レビューで、Freeze時の描画にもRenderer::BeginFrameが必要と判明。時間0のVisual更新を呼び、比較/Profiler中のCB無制限生成を防止。
- Volley→DashWarningが同じAttack clipの後半姿勢を保持していたため、Telegraphへ入るエッジでだけ既存0.50秒のpull poseへ戻す。毎フレームのSeekやTelegraph→Attack restartは行わない。
- 新しい比較Freezeで`GetFinalDeltaTime`が元のslow-motion値を返すと、Game側の既存低速時グレースケールが残っていた。既存Bloom比較Freezeと同じ1/60秒の扱いへ合わせ、通常プレイを変えずに最終Idleのピンク色を確認した。
- Attack撮影が0.533秒の溜め寄りだったため、実Volleyの0.80秒以降を待って再撮影。アニメーション本体や戦闘の時間は変更していない。
- 検証中、長いtool出力に欠落表示が見られたが、rgとコンパイルで実ファイルを確認し、不必要な既存ファイルの修復は行わなかった。

## 制約と朝の目視確認項目

- AvatarSample_Bは既存の人型素材で、戦車専用の歩行・Dash・被弾・死亡clipはない。共有Idle/Attackと姿勢保持は明示的な暫定フォールバック。
- Geometry Linesは既存推奨どおりOFF。候補の顔/髪line_masksはDeveloper用に保持し、本編では既存Textureの自動内部線を使う。
- 既存カメラ・戦闘balanceは維持。人型の大きさ、傾き、照準に対する頭の向き、ピンクの強さは好みの確認対象。大規模な外観変更は行わない。
- 通常難易度での操作感・攻略性と、HPバー/当たり判定に対して人型輪郭が自然に見えるかをユーザーが目視確認する。
- 通常プレイでは自機中心の既存cameraを維持するため、遠くのボスや画面端へ走ったボスがviewport外になることはある。全身capture用の中点cameraはDeveloper自動fixtureに限定する。
- テスト画像はEngine backbufferの保存で、画像生成・編集は行わない。数値テストのPASSを画質改善の主張に使わない。

## 最終差分

最終branchは`feature/neon-boss-gameplay-integration`、HEADは開始時と同じ`a2d9c86b15f6fbf74bca2bb628cc7872c252c086`。commit / push / merge / checkout / reset / cleanは実行していない。

`git diff --stat`は追跡済み19ファイル、253 insertions / 83 deletions。未追跡は9表示項目（`visual/`内3ファイルを展開すると新規11ファイル）。変更ファイルと役割は上の一覧のとおり。新規Visual、分離したScene接続、共有preset、回帰テスト、文書に限定され、モデル本体・戦闘balance・ライセンス本文は変えていない。build/capture/package/dump類は既存ignore対象の`generated/`へ保存し、差分に含まれない。

最終`git diff --check`はexit 0。GitのLF→CRLF注意表示はあるが空白errorはない。原文は[final-git-status.txt](../generated/neon-boss-integration/final-git-status.txt)と[final-git-diff-stat.txt](../generated/neon-boss-integration/final-git-diff-stat.txt)へ保存。diffを自己レビューし、別agentでもcombat/lifetimeと最終画像を確認した。
