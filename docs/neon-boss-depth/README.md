# Neon Boss Depth Encounter

大型の浮遊ネオン投影体と攻撃可能な床コアを、2D Gameplayが正のまま通常本編へ統合する作業記録。frozen `build_i`のDevelopment／Release buildと最終All 69/69がPASS、source driftは0。Depth 41定義×2＝82実行、ON／OFF／reducedの2比較、旧scenario 26実行・設定probe 7件を照合した。通常Developmentは18列ルートから自然撃破・結果／Title帰還／再出撃を実証。実OSマウスで標準Depthの床照準・移動とHP900→883を確認し、性能4run、45秒動画、8秒抜粋、最終8画像とPages引継ぎを保存した。現在21条件PASS、通常Release通し確認C18は操作対象画面を取得できずBLOCKED（通し実行はNOT RUN）。3連続Goalターンで同じblockerを確認し、Goal全体は未完了のままblockedへ移す。

仕様原本は[CODEX_NEON_BOSS_DEPTH_GOAL.md](CODEX_NEON_BOSS_DEPTH_GOAL.md)、開始状態・判断・内部gateは[plan.md](plan.md)。生成物は`generated/neon-boss-depth/`へ保存し、Pagesの別repositoryへは触れない。

## 完成条件

判定はPASS／FAIL／BLOCKED／NOT RUNを使い、PASSの実証範囲を各行に記す。過去の部分PASSや失敗は保存し、異なる版の結果を合算して最終全件PASSとはしない。公開採否や人間の面白さ評価は自動試験のPASSと区別する。

現在の証拠は[validation.md](validation.md)、画像確認は[visual-review.md](visual-review.md)に記録した。

- `build_i`：Development／Release x64とも0 warning／0 error。907入力のmanifestとEXE／DLL hashを保存。snapshot時の`finalGoalValidation=false`は原本のまま保持し、後続の最終All結果を別receiptで記録した。h以前の部分PASSをiの全件結果へ流用していない。
- `cycles_d`：2,400実更新×2回PASS、両phaseの3攻撃／Recovery maskは7/7、全行hash一致。stationary invulnerable Shooter、boss HP10,000、制御したPhase2 HP変更を使うshortcut fixture。
- `lifecycle_focused_d`：HP0 Intro／Dive着地直前、abort Beam、player death、同時death、実retry、実Title帰還、Visual OFFの8ケース×2回＝16実行PASS。選択8/41で、全件完了ではない。
- `config_e`と`presentation_e`の純粋schema／Placement／Presentation／Lifecycle／EffectGeometry契約PASS。`camera_d`の11枚の1280×720原本はrootがoriginal detailで確認した。画像はstationary invulnerable shortcut、通常HUD、無音、撮影frameだけcomparison freeze。採取時の`camera_d/results.json`に残る`visualReview=NOT RUN`とは分け、確認結果を`visual-review.md`に保存した。
- `normal_f_style0`はfrozen Developmentから実Title Returnを経て通常ルートを試したが、未成功で停止した。実death／通常retryの後、attempt2は射撃recoilとnavigationの停滞が続き、`root-stop.json`に`INTERRUPTED_WITHOUT_ROUTE_SUCCESS`／`finalBossReached=false`を保存。既存入力・menu handlerを使うDeveloper replayで、HP／無敵／位置／wallet／mapの直接変更はしない。boss／結果の成功証拠はなく、Release通常ルートの代用にもならない。
- `normal_g_style0`は経路停滞が再発せず、実戦・通常サービス・retryを通ったが、3回ともボス到達前に死亡し`retry-limit`で終了。通常入力だけによる敵弾回避の修正を採用し、実抽出C++ unit PASS。その後の通常ルート試行の結果は下記に分けて記録する。
- `styles_preflight_g`は12実行予定中2PASS／1FAIL／9NOT RUN。Shooter damageは各900更新／実core damage234で一致。回避はVolleyのみ成功後、合法な壁際位置から経路seedが得られずDive計画が停止した。回避成功や3style完了とは扱わない。
- 修正後`styles_preflight_h`は6/41定義×2＝12/12 PASS、各900実更新。Shooter／Drone／Meleeのcore damageは両回とも234／216／389、primary attack eventsは39／24／27。DamageはHP120→75・4accepted hits、DodgeはHP120を保ち3攻撃の安全なRecoveryまで3回の実dashを確認した。boss HP10,000のshortcut fixture、通常の0.45秒room保護、debug無敵／upgrade／forced attack／途中HP・Phase2注入なし。当時の残35定義×2とreduced=true／cross-parityは未実行であり、後続の最終Allは同じ部分結果の合算ではなく全41定義を再実行した。
- `normal_h_style1`は実Title Returnからtick由来20列mapを進み、通常Drone攻撃・有料upgrade／repairを使ってroute17_1・HP120に達したが、600秒の上限で`bounded-timeout`（35,528frame、boss未到達）。Developer入力補助の総上限だけを900秒／54,000更新・最大3attemptへ拡張。この補助はReleaseへ含めない。
- `normal_i_style1`はfrozen `build_i` Developmentから実Title Returnで開始し、tick由来18列／seed226381203を通常map／service handlerと既存攻撃入力で第1試行完走。46,065更新は欠落・不正行なし、全18列の合法な選択／訪問、debug無敵・直接state変更なし。実core damage900でHP0／Defeated→StageClear、自然Phase2、両phaseのActive／Recovery mask7/7、Recovery18/18/18、最終Player HP111を確認。通貨は20＋572 earned−328 spent＝264、6upgradeと2repairの各実支払を照合した。rootは実結果「最深部突破／遠征成功」→Title→再出撃時HP120／通貨20を観測し、原本は上書きされなかった。`generated/neon-boss-depth/audit-gameplay/normal-route/normal_i_style1-read-only-audit.json`と`generated/neon-boss-depth/m1-depth/normal_i_style1/result-return-reentry-ui.json`に根拠を保存。通常Release完走、人の操作感／面白さ、physical mouse aimの実証ではない。
- 最終All `all_a_20261005`は2026-10-05 13:19:51.4373114～14:33:23.4443617 UTCに完了。63interface由来の69command resultsが69/69 PASS・0FAIL、run中sourceChangesは空。完了後のcounts／hash／nested-suite postcheckと907/907入力のfreeze再照合もPASS。実hardware／WARP・ASAN semantic review、Depth 82実行・2parity比較、package自動試験の範囲は[validation.md](validation.md)に記録した。

| 条件 | 必要な結果 | 現在の判定・証拠 |
| --- | --- | --- |
| C01 | 通常本編の対象bossで新profile | PASS（通常Development）：通常18列ルートから標準Depthへ到達し、実damage900・自然Phase2・自然撃破・結果→Title→再出撃を確認。通常ReleaseはC18で別判定。 |
| C02 | 足元anchorと本体分離、高さ／奥行き／上体動作 | PASS（実表示）：45秒の移動・射撃・回避映像と最終原本で、床coreと浮遊本体の分離、高さ・奥行き・上体動作を確認。 |
| C03 | sprite拡大／模型／introだけで終わらない | PASS：実GLBと生成motionがIntro後の3攻撃でも動作。通常Developmentの自然撃破と最終連続映像を確認。 |
| C04 | 明確なcoreをShooter／Drone／Meleeから攻撃 | PASS（全3style接続）：実core damage234／216／389、primary events39／24／27を各2回確認。全3styleの自然勝利や人間評価は未検証。 |
| C05 | 単一HP、重複damage／見えないcontactなし | PASS（実契約）：単一Enemy HP、damage受付・cooldown・世代管理とterminal停止を確認。Damage各2回は4accepted hits、Dodge各2回はdamage／contact0。 |
| C06 | Volleyの予告→空中→床着弾→硬直 | PASS：両phaseの予告→空中→床着弾→Recoveryを最終All各2回と実原本・連続映像で確認。Phase2画像は制御した別fixture。 |
| C07 | Diveの後退／上昇→lock→降下→着地→反撃 | PASS：上昇・target固定・降下・着地・Recoveryを実証。合法壁際の旧停止FAILを保存し、38壁回帰とfresh Allで修正を確認。 |
| C08 | Beamの予告／床接続／掃射／実hit／終了 | PASS：予告・床接続・掃射・実hazard・終了を確認。選定画像の短いactive帯は内壁でclipされたもの。perfect dodge成立は主張しない。 |
| C09 | Gameplay shape／time／damageから一方向表示 | PASS：snapshotのshape／time／damageをVisualが一方向に読む。actual_multidrawとON／OFF／reduced比較を確認。 |
| C10 | 他boss／敵／操作／経済への意図しない変更なし | PASS（回帰範囲）：旧13scenario×2＝26、設定probe7と関連回帰を最終Allで確認。通常Developmentの経済も照合。baselineとの合算なし。 |
| C11 | camera／mouse／床予告／hit一致 | PASS（契約＋native入力）：VP／client／floor契約と標準Depthの実OS mouseで左右床・core照準、右special移動、HP900→883を確認。既存Developer入口の無敵あり、DemoInput／途中HP注入なし。shot別内訳・実被弾shake・人間評価は未計測。 |
| C12 | intro skip／pause／death／abort／retryでcamera／入力／時計を復元 | PASS（実fixture）：skip／pause／death／abort／retry／Titleでcamera・入力・時計の復元を各2回確認。通常Development帰還とReleaseの実retry／pauseも記録。 |
| C13 | 見分けられるmotion、drift／毎frame restartなし | PASS（実motion・契約）：実GLB／WARP、非累積pose／nonrestart契約と45秒映像・状態原本を確認。全frame無欠点やhuman funの保証ではない。 |
| C14 | HP0停止、Dissolve／消灯／解放／結果が一度 | PASS：56terminal operationsでHP0同tick停止、Dissolve終端、core消灯、資源返却一度、結果優先を確認。854の断片と869の消失を実視。 |
| C15 | 自機と危険範囲の可読性 | PASS（実視範囲）：45秒映像、背後2457／2462、左右205／2699、Phase2、合法下壁400で自機・床予告・barsの可読性を確認。全配置・全人操作の保証なし。 |
| C16 | 演出軽量化とGameplayの独立性 | PASS（実Gameplay／clock比較）：Visual ONに対するOFF／reducedの各2,400実frame比較が一致。各profile2回の反復Depth行hashも一致。reduced=trueの各2,400行で実reducedMotionApplied=trueを確認し、要求／実設定一致とFX error0を照合した。 |
| C17 | 最終版の既存＋新規全件結果 | PASS：同一frozen build_iの最終Allが14:33:23.4443617 UTC完了、69/69・0FAIL・source drift0。Depth82実行＋2parity、旧26＋7probe、hardware／WARP noSkipと限定scope ASAN20group、package自動試験を照合。完了後907/907入力hash一致。原本・semantic scopeはvalidation.md。 |
| C18 | 両build／本編配布確認 | BLOCKED（通常Release通し確認はNOT RUN）：両build・pristine package・別cwd依存解決はPASS。有限native tutorial・retry・Title再出撃は確認したが、Depth勝利を含む通しルートは未実施。新copyは3起動経路で操作対象の画面を取得できず、原因未特定。新旧254配布file一致と初回Titleコード到達は可視画面の証明にしない。3連続Goalターンでも再開できる対象がなく、外部の操作状態復旧または手元確認が必要。forced-clear自動walkthroughは代用にしない。 |
| C19 | 実画像／連続映像の確認と条件 | PASS（代表coverage）：要求phase・背後／左右を26実視原本へ対応し、合法下壁400、45秒／8秒映像、8派生画像を確認。literal viewport床端の非適用理由、Dissolveの白pulse／重複ラベル、再生dropとloop巻き戻りを開示。 |
| C20 | 最終版性能／資源の測定または理由 | PASS（実測範囲）：同じ最終Development・標準cameraでOFF1→ON1→ON2→OFF2、900warmup＋1,200測定＋120tail。各GPU有効1,199／無効1、CPU／GPU mean・median・p95とmodel／FX資源を保存。ON−OFF GPU median＋0.677／＋0.694ms、Scene SRVは各＋1後flat・割当元未特定。録画別child全体191.1304秒／2,700PNG。pure readback／saveは未計測、通常FPSやCPU改善へ換算しない。詳細はperformance.md。 |
| C21 | Pages素材／caption／出典／引継ぎ | PASS（引継ぎ）：8 PNG／8 WebP、45秒動画、8秒loop、clean poster、summary／caption／alt／条件・hash・出典と要確認事項を[portfolio-handoff.md](portfolio-handoff.md)へ保存。公開採否は別判断、Pages未変更。 |
| C22 | 無断Git操作／deploy／取得／変更破棄なし | PASS（保全範囲）：開始branch／HEADを維持し、commit／push／merge／deploy、新素材取得、ユーザー変更破棄なし。907入力・All651・両build計10fileの現hash一致を別receiptで再確認。差分・自己reviewの根拠はvalidation.md。 |

## 最短確認手順

通常本編はRelease packageを起動し、Title→遠征→接続された通常node／service→最深部boss→結果／Title帰還→再出撃を確認する。この通しC18は未完了。再開には通常Releaseの実画面を操作ツールで取得できる状態、または手元で行った同じ通し確認の結果が必要。保存済みpackage／launch手順とblocker監査はvalidation.mdを参照する。Developerで短時間に見る場合は**Title→Return→ノード未入場のmap→F3→Neon Boss / 本編ボス確認→本編ボス戦へ移動 / 再生成**。consoleを閉じると通常更新が進む。この既存入口はplayer無敵を設定し、戦闘中からの開始はDirectorのMap前提を満たさないため、Titleへ戻ってfresh mapから実行する。ReleaseではDeveloper toolsを露出しない。

操作はWASD移動、mouseで床coreを狙い左primary、右special。Volleyの床円を離れ、Diveの固定着地点から退避し、Beamの床帯の外へ出てRecoveryでcoreを攻撃する。Shooterは射線、Droneは移動しながら継続攻撃、Meleeはcore付近の反撃時間を使う。無敵入口の操作確認を自然攻略や面白さ評価に数えない。
