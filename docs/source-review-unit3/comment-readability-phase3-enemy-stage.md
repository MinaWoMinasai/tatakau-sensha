# コメント品質改善 第3段階：追補・共有型・敵・地形・シーン

実施日: 2026-10-03（日本時間）。コメント、必要な空白・改行・インデントと記録文書だけを修正する。説明の正確さは実装と利用先を照合して判断し、コメント数・監査の不足件数・テスト成功だけで判断しない。

## 開始時点

- ブランチ: `docs/comment-readability-phase3-enemy-stage`
- HEAD: `83022a1fd3273236a8f76e6e5f4f941e60908426`
- `git status --porcelain=v1 -uall`: 出力なし。追跡ファイルの変更・未追跡ファイルなし。
- 指定の現在の配置と、第1・第2段階の報告・進捗表が存在することを確認。本報告は存在せず新規作成。
- `docs/code-comment-guide.md`、単元3 README、第1・第2段階報告、進捗表、`.clang-format`、関連する実行脚本と監査を事前参照。ガイドは今回の方針と整合しており変更不要。
- リポジトリ内と作業場所の祖先にAGENTS.mdなし。他の作業指示ファイルも検索したが該当なし。`.codex/video_frames`は作業指示ではない。

前回のHEADへ戻していない。準備されたブランチで作業し、ブランチ操作、commit、push、merge、rebase、reset、clean、stash、clone、リモート同期は行っていない。開始時の全ゲームC++と既存資料135ファイルを、無視対象の`generated/comment-readability-phase3/baseline/`へバイト列で保存し、`start.json`にHEAD・サイズ・行数・SHA256を記録した。過去の報告と検証ログは上書きしない。

## 第2段階への追補3点

3点とも開始時点に指摘の説明が残っていた。コードは指摘に合わせて変更していない。

| 対象 | 修正前 → 修正後 | 根拠 |
| --- | --- | --- |
| A: Player::Smash | 「最低速度を設ける」→ 蓄積量から速度の大きさを求め、上限を0.7に制限 | `std::min(recoilPower, 0.7f)`。maxへ変更していない |
| B: Bullet::IsBurstChild/SetBurstChild | 「撃破時の破裂子弾」→ 撃破時破裂と命中・壁衝突の分裂の子弾を識別する記録 | Initializeでfalse、AppendImpactChildrenでtrue、FlushPendingSplitsで`!spec.reflection`。読む箇所はNotifyPlayerHitとQueueKillBurst |
| C: Player::UpdateRunProjectiles | 「元の速さで移動させる」→ 元の速さを保った速度の向きを設定し、位置更新はBullet::Updateが担当 | この関数で弾の位置・速度へ書き込むのはSetVelocity。対象選択/回頭を計算し、Bullet::Updateが`velocity_ * deltaTime * 60`で位置を進める |

BのtrueはNotifyPlayerHitの入口で除外され、同通知の連鎖、マーキング／起爆、撃破時破裂へ進まない。すべての能力を無効にするフラグではなく、壁反射や分裂回数を変更しない。子弾の再分裂を止めるのは、生成時のConfigureGrowthなどの別設定。元弾の分裂予約状態とも区別した。第2段階の他の戦闘ファイルを再執筆・再整形していない。

## 今回の確認範囲

### 全体確認した指定10ファイルと追加1ファイル

| ファイル | 照合した範囲 |
| --- | --- |
| `project/game/player/TankShooterAbilities.h` | ReturnFlight、MarkLedger/Mark、Step/Update/Hit、Damage、全メンバー・定数・内部処理 |
| `project/game/weapon/CombatTypes.h` | Phase、AttackParam全メンバー・既定値、BulletOwnerと利用先 |
| `project/game/exp/ExpEnemy.h` / `ExpEnemy.cpp` | 全型・宣言・インライン・定義。図形、Shooter、移動戦闘役、資源、召喚、盾/刃/EMP、描画も含む |
| `project/game/exp/EnemyManager.h` / `EnemyManager.cpp` | 全型・宣言・定義。生成・領域・更新・召喚・削除・取得・設定消去・描画 |
| `project/game/enemy/actor/Enemy.h` / `Enemy.cpp` | 全型・宣言・定義。通常/試作/ライバル経路、AI、経路、射撃、捕食、HP・死亡、設定/リセット、描画 |
| `project/game/player/actor/Stage.h` / `Stage.cpp` | 全型・宣言・定義。地形読み込み/生成/統合/障害物/描画、軸別・球・弾の衝突、借用取得 |
| `project/game/run/TankExpeditionTransition.h` | シーンが直接利用する遷移型。全メンバー・全8関数の時計、反映通知、進行/終了、表示係数の計算 |

単純なgetter、互換用の空処理、十分な既存説明は確認して保持した。全体確認は全行への説明追加を意味しない。

### 部分確認・編集

- `Player.cpp`: Smash、UpdateRunProjectilesの追補のみ。累積では第2段階の25関数の確認実績を保持。HUD・編集・移動全般などへ広げていない。
- `Bullet.h`: IsBurstChild/SetBurstChildのみ追補。第2段階のヘッダー全体確認実績を保持。
- `GameScene.cpp`: 編集前に現在の全分割cppの関数一覧・呼び出し関係と開始時バイト範囲を`scene-scope.json`へ保存した。Updateの時間倍率・停止条件・戦闘更新・死亡分岐・共通演出消費の区間、UpdateGameplayEventEffects、BeginBossDefeatSequence、BeginGameOver、UpdateGameFlowの死亡/結果移行区間、EnterResultState、SpawnPlayerLaser、UpdatePlayerLasers、SpawnPlayerMine、UpdatePlayerMines、DetonatePlayerMine、MakePlayerMeleeTrailConfig、ComputePlayerMeleeBladeSection、SpawnPlayerMeleeSlash、UpdatePlayerMeleeSlashes、UpdateSpecialCombatPresentation、UpdateLevelBossPhases、ApplyBossPhaseTuning。cpp限定DistancePointToSegment2D/RotateVector2Dも直接利用する範囲として照合。
- `GameScene.TankExpedition.cpp`: StartTankExpeditionRoom、FinishTankExpeditionRoom、UpdateTankExpeditionの戦闘/死亡/目的判定区間。
- `GameScene.ExpeditionMap.cpp`: UpdateExpeditionPresentationの保留遷移消費、StartAuthoredExpeditionRoom、CompleteExpeditionMapCombat、UpdateExpeditionMapの戦闘完了/資源回収区間。
- `GameScene.h`: 対応する戦闘・部屋遷移・停止/死亡・ボス段階の宣言とレーザー/地雷/爆発/斬撃/特殊演出の状態型。UI・描画設定・制作機能全体は対象外。

### 直接関連する追加ヘッダー7ファイルの局所補正

追加範囲も開始時保存内容と比較し、参照先全体を改善済みと扱わない。

| ファイル | 確認・補正した範囲 |
| --- | --- |
| `ExpEnemyNavigation.h` | FindExpEnemyNextCellの四方向BFS、格子添字、空きセル代替、失敗値、親セル記録 |
| `ExpEnemyCombatCycle.h` | 型の責務、Reset/Advance/GetRecoveryRatio、内部Stateの誤った資源/描画説明、共用状態表 |
| `ExpEnemyMagazineCycle.h` | 型/Timing、Reset/SetIntervalScale/Advance/IsReloading、残弾・容量・再装填集計、内部State/RecoveryState |
| `ExpGuardCombat.h` | PulseCycle、SummonSlots、InFacingCone、ShieldDamage、BladeCycleの更新/受付消費/回復比率、内部Stateの説明 |
| `PrototypeBossCombat.h` | 型/Shot、照準・拡散の単位、Step、弾数・拡散角の計算 |
| `RivalBossCombat.h` | 型/Shot、段階/進行/残弾/移動/ダッシュ/Step、予測判定、7状態Update、Enter/Duration/FireRound |
| `project/game/mapchip/MapChip.h` | 地形種類、行列添字・ワールド座標・セル寸法、行列数の取得 |

上記6敵補助ヘッダーはExpEnemy/Enemyと同じディレクトリ。無効な旧処理として削除できる残骸は今回の対象で確定しなかった。空関数、有効な条件付き処理、抽出目印は保持した。

### 参照のみ

AttackController.cppの発射処理、Bullet.cppの帰還/衝突/分裂、BulletManager.cppのマーキング/命中/反射/予約反映/削除、CollisionManager.cppの双方通知/ダメージ復元、Collider.hの衝突ID発行・コピー/代入、CollisionConfig.hの属性を照合。Player.cpp/PlayerDrone.cppのAttackParam構築と射撃、Player.SpecialAbilities.cppの壁衝撃回数の取得/消費、TankExpeditionContent.hの敵定義も参照した。これらの全面再審査は行っていない。

既存テスト・ビルド設定は今回の検査内容と実行条件を調べる参照先。具体的な参照範囲と累積実績は[進捗表](comment-improvement-progress.md)に記録する。

追加の参照のみは、MapChip.cppの座標・CSV取得、Calculation.cppの球/AABB/線分・OBBと境界、GameScene.TankRun.cppのメニュー/撃破/資源通知、GameScene.Balance.cppのApplyTankExpeditionRoomBalance。GameScene本体のInitialize/デストラクターの通知登録・解除、今回の分割cppの遷移要求/選択/描画利用も契約確認の参照にとどめた。関連テストは脚本と対応アサーション・ソース抽出を参照し、テストソース自体のコメント全面審査には加算しない。

実際に変更したC++は上記全体11＋部分13（追補2、シーン4、小型ヘッダー7）の計24ファイル。文書は本報告と進捗表の2ファイルだけ。既存ガイド、過去の報告、テスト、監査、設定JSON、シェーダー、ビルド設定は変更していない。

## 主な説明改善と根拠

| 対象 | 修正前 → 修正後 | 根拠 |
| --- | --- | --- |
| ReturnFlight | 「帰還の進行と移動方向を計算」→ 累積時間と開始済み状態。Stepのtrueは今回の帰還開始だけ | age/returningのみ。Bullet::Updateは時間倍率でdtを補正し、BeginReturnの後に速度/位置を更新。内部0.60秒を常に実経過0.60秒としない |
| MarkLedger | 「描画に必要な状態」「命中を反映」→ 非0衝突ID、128枠、4秒寿命、通常4回/ボス6回、条件成立時に記録を消費 | Hitのfalseでも蓄積/寿命を更新。新規対象の空きなしは変更せずfalse。Updateは残り0以下で消去。ColliderのIDは生成/コピー時の発行値で、アドレスではない |
| Damage | 「戦闘状態へ反映」→ 値の計算だけ。powerのみ0.1～5、四捨五入、最低1 | HPへの適用はDamageBuildTarget等の呼び出し側。scale/積の上限検査はなく、範囲は呼び出し側の前提 |
| AttackParam | 「弾の位置・方向・速度・威力」→ 発射設定。位置/方向は別引数 | メンバーとFire/FireFromMuzzleを照合。60FPS基準弾速、度の全拡散幅、HPと他弾への貫通力、借用元、負値/0、各フラグと適用条件を区別 |
| ExpEnemyのダメージ | 「平行光」「ダメージ反映」→ 攻撃元と近接条件による盾減衰後のHP適用、boolは新規撃破 | ResolveShieldDamage→ApplyDamage。falseの非致死経路もHP/被弾時間を更新。死亡を通知より先に確定し、再入/後続通知の二重報酬を防ぐ |
| 反射受付 | 「投射物を反射」→ 判定・受付/演出/集計。生成は別の予約処理 | TryReflectProjectileは生成しない。CollisionManager→QueueArmorReflection→FlushPendingSplits。予約/登録制限で生成できなくても元弾は死亡し得る |
| 敵の所有・件数 | 「敵件数」「リスト公開」→ 資源/召喚/削除待ちも含む管理数と借用一覧のコピー | unique_ptr所有、GetEnemyPtrsはget()を値返し、死亡フラグとUpdate/clearの実体削除を区別 |
| 敵の壁衝撃 | 「ノックバック反映」「壁衝突件数」→ 速度設定と位置補正、受付期間内の壁補正による回数増加 | ApplyKnockbackのpower>=0.16、MoveCombatActorの補正/速度条件と受付消費、Playerの回数差による壁衝撃適用 |
| 地形 | 「接地」「滑らない」等→ 経路別の位置・速度・接地の実際の担当 | 全方向球は内向き法線速度だけ除去し接線を保持。Y専用は接地を設定しない、X専用は法線Yを除きZも扱う。敵/資源の軸補正は速度を変えない |
| シーンの攻撃 | 「更新」だけ→ 予約受取時/遅延発動時のHP適用、命中済み記録、表示とダメージの分離 | レーザーはSpawnで適用、地雷は起爆で適用、斬撃は有効時間の対象判定。レーザー/爆発表示型は威力を保持しない |

AttackParamのpenetrate/cooldownは設定箇所と全利用検索を確認し、現行AttackControllerでは参照しないことを記した。別型の同名設定が使われていることと混同せず、プロジェクト全体の同名機能が無効とは説明していない。

実際のシーン更新は、遠征/部屋状態を進める→時間倍率/停止を決める→地形/レベル品→自機/ドローン→攻撃予約を消費→場の攻撃→敵AI/ボス段階→弾の移動/壁/追加予約反映/削除→特殊戦闘→通常接触とその末尾の追加生成→ダッシュ衝撃イベント→HP/死亡演出判定→共通特殊演出の消費→ゲーム状態遷移。Playing/main/メニュー、prototypeRun_が有効なモードの進化画面とフレーム開始時の画面状態、チュートリアル抑制は別条件。チュートリアル抑制中でも予約攻撃受取と弾更新は呼ばれることを説明した。表示用の基準時間と減速を含む戦闘時間も分けた。

## 書式・逆照合・コード不変の確認

ExpEnemy.cppとEnemyManager.cppだけに既存.clang-format（Microsoft、4空白、ColumnLimit 140）を適用した。clang-format 22.1.3の出力を採用する前にも字句比較し、独立した文・長条件式・インデントを整えた。他の22 C++はコメントの局所変更と必要な改行にとどめ、Player.cppやシーンの区間外を再整形していない。CRLFを維持した。

書き終えた説明から設定元・実装・利用/消費先へ戻る逆照合を全担当で行い、共有型とシーンは別担当も独立して確認した。次の草稿の問題を最終検証前に補正した。

- 共有型: 帰還弾はactorPierceCountを消費せず、bulletHp/bulletPenetrationのdamageによる補完はAttackController経由に限る。bulletSpeedは負値も許すため、単に距離の大きさとはしない。
- 敵/管理: 生成上限は弾数ではなく管理アクター数。借用ポインターはUpdateで削除された個体だけ無効になり、全ポインターが毎回無効とはしない。弾倉は一斉射撃回数であり弾個数ではない。Balance設定は図形専用ではなく共用で、未参照decel_を旧設定と断定しない。
- ボス/地形: ScoreDirのサンプル距離は未正規化dirの3倍。無効化された遭遇はフラグを消しても目標座標を保持する。ResetRunEncounterの通常AI/試作時計と、別途EnableExpeditionRivalで扱うライバル状態を分けた。
- シーン: ノックバックは速度設定で、壁衝突は後続更新。停止用タイマーの設定と減速適用の時点も分けた。資源通知は報酬/HP/表示も更新するので「待ち記録だけ」としない。prototypeRun_の進化抑制を遠征だけに限定せず、ResetRunRoomStateの生存/成長有効条件と一次攻撃イベントの遠征時だけの消費も追記した。
- 遷移/表示: Transitionは経過秒と状態を保持し、表示係数は計算する。Advanceのtrueは反映通知であり終了ではない。フラッシュの消去は期限切れ分に限る。Player更新には通常弾の即時生成とレーザー/地雷/斬撃の予約の両方がある。

追補のmin上限制限、max下限制限、falseでも変わる蓄積/時計、フラグの全設定元と参照先、消費と取得、借用と所有、死亡とerase、秒/60FPS基準/度/ラジアン、境界と丸めを再照合した。上記修正後はソースを固定してビルド・テストへ進めた。

開始時保存バイト列と最終24 C++の字句列171,834トークンが一致。既存の第1段階lexerと検査例を再利用し、通常/接頭辞/raw文字列、文字リテラル、エスケープ、演算子・数値を保持して比較した。文字列まで除去する正規表現による判定ではない。プリプロセッサ指令と物理的行継続も一致した。

事前に記録したPlayerの2関数、Bulletの2API文書、シーン4ファイル、小型ヘッダー7ファイルの許容範囲へ全差分が収まることを確認し、その範囲外バイト列は不変。検証補助の14範囲チェックのうち1つは追加全体確認のTransitionであり、実際の部分ファイルは13。__LINE__/source_location、文字列化、raw文字列、行継続は今回の変更範囲にはなく、既存のinclude/pragma/NOMINMAX/条件付き処理/static_assertを保持。テストの単純な波括弧抽出の署名・目印を保持し、新コメントに抽出を乱す波括弧を加えていない。

トークン一致はコメントの意味、全プレイ経路、バイナリ同一性の保証ではない。コメントと改行に伴ってコンパイラー診断・デバッガ・標準assertなどの表示行番号は変わる。ゲームの計算や公開APIを変更していないことは字句・指令・範囲比較で確認し、ビルドと既存テストは別に実行した。

## 検証結果

開始時点でtest_tank_enemy_combat.ps1を差分なしの状態で実行し、敵周期と実装統合の2スイートが成功した（baseline-enemy-combat.log）。以下は最終ソースに対する今回の実行であり、過去の成功記録を流用していない。テスト/監査の検査条件は変更していない。

| 検証 | 結果と実行条件 |
| --- | --- |
| 開始時差分・字句・範囲比較 | 24 C++、171,834トークン、プリプロセッサ/行継続、部分13ファイルの範囲外バイト一致。ブランチ/HEADも開始時と同じ |
| git diff --check | 成功。最終文書保存後にも確認 |
| 既存コメント監査 | audit_source_comments.py --check --outputで成功。236 C++、428型、4,253関数、型/関数の文書不足・不明@param各0。形式/存在の検査であり説明意味の保証ではない |
| Development x64 | VS 18 Community MSBuild /t:Build /p:Configuration=Development /p:Platform=x64、成功 |
| Release x64 | 同じMSBuildのConfiguration=Release / Platform=x64、成功。Development/Releaseとも警告0・エラー0 |
| CPU: 追加能力・投射物・衝突 | test_tank_additional_abilities.ps1、test_tank_projectiles.ps1、test_tank_collisions.ps1すべて成功 |
| CPU: 敵/ボス/通常モード | test_tank_enemy_combat.ps1、test_rival_boss_combat.ps1、test_tank_run.ps1すべて成功。run脚本には試作ボスとmodifiersのスイートも含む |
| CPU: 遠征/地形/部屋/演出 | test_tank_expedition.ps1、test_tank_expedition_rooms.ps1、test_tank_expedition_map.ps1、test_tank_expedition_content.ps1、test_tank_expedition_tutorial.ps1、test_tank_presentation.ps1すべて成功。rooms/contentへWriteDefaultsを渡していない |
| Development特殊能力実行時 | test_tank_special_runtime.ps1 -Configuration Development、成功。completed/testMode=true、forcedDamage=false、errors=0、15 probes、新規PNG18枚 |
| Development戦闘実行時 | test_tank_combat_runtime.ps1 -Configuration Development、成功。completed/testMode=true、forcedCombatClear=false、errors=0、6 probes、新規PNG10枚。壁交差/通過/再装填違反0、Rivalの第2段階・3射撃パターン・ダッシュ成立 |

ビルドログ、監査JSON、各脚本のfinal-*.log、tokens.jsonと最終ソースSHAを無視対象のgenerated/comment-readability-phase3へ保存。MSVCはv145/14.51、C++20、/utf-8、/MT。Developmentは/WX、基本/Od、CG2_DEVELOPER_TOOLS=1とUSE_IMGUI（_DEBUGなし、一部既存ファイルは/O2/GL）、Releaseは/O1/GL、NDEBUG、CG2_DEVELOPER_TOOLS=0。ソリューションのDebugはDevelopmentへ割り当てられるため、独立したDebugビルドとして報告しない。

CPU脚本は実際のコンパイル/アサーション/ソース抽出を確認して選択した。追加能力・投射物・敵/ボス・run等は主にC++20 /O2 /W4 /WX /UNDEBUG、衝突はC++17、地図はC++17と20の両方を実行する。抽出/描画代替を使うテストの成功を、ゲーム全体の描画・全状態遷移の保証にはしない。

実行時は実際に構築したDevelopment版CG2.exeを既存脚本で起動した。特殊能力は強制ダメージなし。敵戦闘は無敵の移動標的・自機射撃なし・強制撃破なしで、ボスのHPを注入して第2段階を検査する既存fixture（phase2HpInjection=true）である。通常プレイ条件そのものとはせず、対象の実移動/実弾/有限弾倉と段階到達の証拠として扱う。各脚本は画像の存在・実行開始後の鮮度・512バイト以上を検査し、JSONと実行ログを今回の保存先へ残した。

初回の実行時2脚本の起動補助に誤りがあった。PowerShell脚本では設定されない$LASTEXITCODEをネイティブ実行用に比較したため、内部検査が成功した後に補助だけが失敗した。ゲーム/テストの失敗ではなく今回の補助の問題として記録し、その判定を除いて既存脚本を再実行した。初回ログruntime-development-*.logを残し、正常な起動での再検証をruntime-final-*.logへ分けた。既存脚本の判定は緩めていない。

未実施: 全手動プレイ経路と全プロジェクトの全テストは指定グループ外のため未実施。独立Debug構成はソリューションがDevelopmentへ割り当てているため未実施。Release版の自動実行時検証は今回は指定されたDevelopment版を選択したため未実施。画像の見た目の審査は行っておらず、既存脚本のスクリーンショット生成/鮮度/容量検査と、見た目の評価を区別する。

## 別件の設計・動作候補（今回コードは未修正）

| 根拠となる範囲 | 現状と再確認条件 |
| --- | --- |
| TankShooterAbilities.h::Damage / MarkLedger::Hit | scale/積の上限・有限性、id=0を内部で検査しない。現在の有限倍率/非0衝突IDの利用と別に、新しい入力元や大倍率を追加するとき再確認 |
| CombatTypes.h::AttackParam、AttackController | penetrate/cooldownは現行発射経路で参照しない。別型の同名機能とは区別し、将来の命名/適用整理の候補 |
| ExpEnemy::Initialize | 外部速度/戦闘速度/無敵等を全消去しない。現在の新規生成以外に同じ実体を再初期化する場合の期待状態を再確認 |
| ExpEnemy::Update/UpdateExpeditionCombat | AI時計は最大0.20秒、先行寿命/演出/壁受付時計は制限前の時間。長いフレームや非有限入力の進行を再確認 |
| ExpEnemy::TryReflectProjectile / CollisionManager / BulletManager::QueueArmorReflection | 反射受付/集計と追加生成の成功は別。予約上限やほぼ静止した弾で生成できなくても元弾が死亡し得る条件を再確認 |
| EnemyManager::FindNearestEnemy / Update | includeShooters=falseは全移動戦闘役も除外し資源は残す。召喚元が個体更新中に死亡すると、召喚解除は次回管理更新。呼び出し側の期待対象/時点を再確認 |
| Enemy::TakeDamage/OnCollision/Update | uint32_tからintと減算の範囲、大威力や多重命中を再確認。致死HPでもUpdateで行動/射撃/移動を先に行ってからDieする経路の期待を確認 |
| Enemy::HasLineOfSightToTarget/HasClearMoveRouteToTarget | ブロック中心距離や機体幅3線分による近似。大型障害物/壁内目標で期待精度を再確認 |
| Stage::AddLevelObstacle/GenerateBlocks/ResolveBulletsCollision | 描画回転と軸平行衝突の差、直接再生成時の空白セルの旧ブロック保持、現在位置だけの弾判定による高速通過を再確認。LoadRunMapは再生成前にClearする |
| GameScene::UpdatePlayerMeleeSlashes | hitTargetsは対象アドレス。斬撃残存中に削除/再生成でアドレスが再利用されると別個体を命中済みにし得る。履歴からメンバーへアクセスはせず、直ちに解放済み参照とは断定しない |
| GameScene::StartTankExpeditionRoom | 地形読み込み失敗で診断後も配置を続ける旧経路と、制作部屋の失敗returnの差。失敗入力で再確認 |
| GameScene::EnterExpeditionMapNode | catchの進行2オブジェクトの復元が、地形差し替え/実体消去後の例外まで戻す保証ではない。生成失敗の注入で再確認 |

第1・第2段階のGetRailChargeRatioの単位/正規化等の別件も元報告に保持し、今回解決済みとはしない。上記は設計変更の承認や不具合の再現完了を意味せず、根拠と再確認条件の記録である。

## 進捗と残課題

現在のファイル集合を既存方式で再列挙した結果、実行C++236（h133/cpp103）、自作シェーダー66、制作・検証ツール85、計387。既存表との差分は追加0・削除0・移動0であり、過去の数へ合わせたのではない。列挙と内容確認は別。第1・第2段階の全体/部分確認実績を保ち、今回の全体11と部分13の範囲を追加した。累積の文書全体確認は24、内部全体確認は22ファイル。Player.h/BulletManager.hの第1段階文書全体実績は内部の部分確認と分けて保持し、Player.cppの第2段階25関数とBullet.hの全体確認も残した。更新SHAは最終保存内容を識別するだけで説明の品質を保証しない。

別担当の読み取り検査でも、387行の集合・重複なし・全SHA12桁、全体/部分の集計、第1・第2段階の実績保持が一致した。今回読まなかった過去確認済み範囲を未確認へ戻していない。更新した表の参照行末も修正し、内容識別欄と出典を保った。

次は遠征の進行・報酬・成長を候補とする。`project/game/run/TankRunDirector.h`、`TankExpeditionDirector.h`、`TankExpeditionMap.h`、`TankExpeditionRooms.h`、`project/game/player/TankExpeditionLoadout.h`、`project/game/scene/GameScene.TankRun.cpp`、`GameScene.ExpeditionBuild.cpp`、`GameScene.ExpeditionExperience.cpp`などの、既存表で列挙または参照のみの範囲。今回編集したGameScene本体/分割cppも、UI・制作・描画・サービス選択など指定区間外は未確認のまま。この段階で次のグループを編集しない。
