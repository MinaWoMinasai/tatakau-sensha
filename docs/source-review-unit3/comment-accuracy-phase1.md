# コメント品質改善 第1段階

実施日: 2026-10-03（日本時間）。既存の説明を現在の実装・呼び出し側・既存テストと照合し、コメントと関連文書だけを修正した。コメント数や監査の不足件数を、内容の正しさの評価には用いていない。

## 作業開始時点

| 確認項目 | 開始時の状態 |
| --- | --- |
| 作業ブランチ | `docs/comment-accuracy-phase1` |
| HEAD | `16273257327c0f91e40f5479b2092122677f65ac` |
| 作業ツリー | `git status --porcelain=v1 -uall` の出力なし。追跡ファイルの変更・未追跡ファイルなし |
| 作業場所 | `C:\Users\k024g\OneDrive\デスクトップ\自作エンジン2` |
| 事前資料 | `docs/code-comment-guide.md`、`docs/source-review-unit3/README.md`、`project/tools/audit_source_comments.py`を先に確認 |
| 本報告 | 同名ファイルは存在せず、新規作成 |

準備された作業ブランチで作業した。終了時もブランチ・HEADは同じ。ブランチの作成・切り替え、commit、push、merge、rebase、reset、clean、stash、clone、リモート同期は行っていない。既存のユーザー変更はなかった。

現在の配置を確認し、開始時の内容・SHA-256・サイズ・行数を`generated/comment-accuracy-phase1/start.json`と同ディレクトリの`baseline/`に保存した。重点対象の開始時の配置は次のとおりで、移動はなかった。

| 対象 | 開始時の行数 | 開始時のバイト数 |
| --- | ---: | ---: |
| `project/game/player/TankSpecialCombat.h` | 434 | 16,891 |
| `project/game/player/actor/Player.h` | 1,483 | 67,149 |
| `project/game/player/actor/BulletManager.h` | 171 | 7,908 |

以前のREADMEの対応済み記録やテスト成功記録は参考資料として読み、変更していない。以下の結果は今回実行した確認と区別して記録している。

## 実施範囲と確認したファイル

重点3ヘッダーのクラス・構造体と関数の説明を全体にわたって確認した。公開APIだけでなく、非公開宣言、ヘッダー内の定義、条件付きの制作UI宣言も含めた。下記の実装・利用箇所は、その説明の確認に必要な範囲を追ったものであり、各ファイル全体をコメント審査済みとは扱わない。

| ファイル・範囲 | 照合した内容 |
| --- | --- |
| `project/game/player/TankSpecialCombat.h` | チャージ・倍率・幾何判定・命中間隔・斬撃波判定・パリィ・ドローン任務の状態遷移・ロック・分散選択・回転攻撃・EMP・壁衝突の全説明 |
| `project/game/player/actor/Player.h` | 型の保持情報、取得値・単位・借用、初期化・更新・攻撃・通知・イベント消費、遠征の装備・成長・部屋リセット、進化・制作UI・内部補助関数の全説明 |
| `project/game/player/actor/BulletManager.h` | 所有・追加拒否、更新・衝突走査の境界、予約とイベント消費、内訳・集計・軌跡設定、内部の対象選択とダメージ適用の全説明 |
| `project/game/player/actor/Player.cpp` | 対応する関数定義、特殊戦闘の判定と生成、攻撃・成長・寿命・描画・設定取得、旧互換処理 |
| `project/game/player/actor/Player.SpecialAbilities.cpp` | ドローン表示の進行度、ロックとダメージ補正、追加能力の更新、壁衝突候補の登録・使用 |
| `project/game/player/actor/BulletManager.cpp` | Add、GetBulletCounts、Update、FlushPendingSplits、ClearAll、軌跡設定、連鎖・起爆・破裂・反射と内部関数 |
| `project/game/collision/CollisionManager.cpp` | NotifyDroneHitの戻り値の利用、元ダメージの復元、命中通知、予約反映、衝突対象登録 |
| `project/game/player/actor/Player.EvolutionUi.cpp`、`Player.ClassEditor.cpp` | Player.hに宣言されたUI関数の役割、入力・選択・設定の保存／読込、表示用の値と資源の扱い |
| `project/game/player/actor/PlayerDrone.h`、`PlayerDrone.cpp` | 任務・射撃の更新、接続レーザーの元位置、弾数制限、借用する管理先 |
| `project/game/player/actor/Bullet.h`、`Bullet.cpp`、`Stage.h`、`Stage.cpp` | 弾の所有者・死亡・成長・特殊情報・軌跡設定、地形衝突・移動の順序 |
| `project/game/exp/ExpEnemy.h`、`ExpEnemy.cpp`、`EnemyManager.h`、`project/game/enemy/actor/Enemy.h`、`Enemy.cpp` | 生存・資源・衝突ID・壁衝突回数・方向別ダメージ・撃破判定 |
| `project/game/scene/GameScene.cpp`、`GameScene.SpecialValidation.cpp` | 特殊戦闘を呼ぶ順序、イベントとレーザー表示、contactの利用、弾数表示・検証 |
| `project/game/player/PlayerClassConfig.h`、`PlayerClassCatalog.h`、`PlayerClassCatalog.cpp` | 設定の保持情報、検索結果の寿命、読込成功／失敗、設定の置換 |
| `project/game/player/TankRunModifiers.h`、`TankExpeditionLoadout.h`、`TankCombatStyleBalance.h`、`project/game/run/TankBuildStyle.h`、`project/game/weapon/CombatTypes.h`、`project/game/collision/CollisionConfig.h` | 能力の条件・強さ、時間と速度、整備の添字・範囲、系統・射撃条件・所有者の定義 |
| `project/tools/tank_additional_abilities_tests.cpp`、`tank_projectile_tests.cpp`、`tank_collision_tests.cpp`、`player_class_config_tests.cpp`と対応する実行スクリプト | 現行の契約を確認する既存検査と、実コード抽出の目印・方式 |
| `project/tools/test_tank_expedition.ps1`、`test_tank_presentation.ps1`、`test_tank_special_runtime.ps1`、`project/CG2.sln`、`CG2_testPro.vcxproj` | 関連検証の実行条件、実際の構成・コンパイル条件 |

変更したファイルは次の6ファイルだけ。外部ライブラリ・素材・設定JSON・ビルド設定・監査ツール・テストのソースや判定は変更していない。

- `project/game/player/TankSpecialCombat.h`: 契約の誤説明・不足・不自然な日本語を修正。
- `project/game/player/actor/Player.h`: 保持情報、状態変更、戻り値、単位、借用と利用順序を修正。
- `project/game/player/actor/BulletManager.h`: 登録条件、集計対象、予約・消費、寿命と内部判定を修正。
- `project/game/player/actor/Player.cpp`: 対象APIに関係する3件の誤説明を修正し、ヘッダーと重複する3件の定義前コメントを統合。
- `docs/code-comment-guide.md`: getterをまとめて説明できる旧記述と今回の形式を合わせるため、関数直前の`/// @brief`、cpp内限定関数、実装側の`//`と重複回避の段落だけを補正。
- `docs/source-review-unit3/comment-accuracy-phase1.md`: 本報告を新規作成。

文書用コメントはDoxygen形式を維持した。通常の処理コメントは`//`のまま。英語の有効な契約情報（短い押下での0、衝突走査後の追加、ClearAll以降の集計、財布の所有者など）は、日本語の説明へ意味を保持して統合した。現在も有効な英語の補足は残した。関数内部への全面的なコメント追加は行っていない。

## 必須確認項目A～H

開始時点では、A～Hの各項目に誤説明または説明不足が残っていた。Dの「負値は発射なし・0も有効」は英語コメントに既に存在したが、日本語のAPI説明には不足していた。

| 項目 | 修正前 → 修正後 | 現行実装・呼び出し側の根拠 |
| --- | --- | --- |
| A: EmitsSlashWave | 「s斬撃WAVEを発生させる」→ コンボ添字2（3段目）の判定だけで、生成・状態変更なし | 定義は`return comboStep == 2;`だけ。Player::UpdateSpecialCombatが能力・予備動作・発生済みフラグも確認して弾を生成する |
| B: DroneLaserLink | 「端点と威力」→ ワールド座標の端点と、その更新での敵への接触有無 | メンバーはstart・end・contactで、威力はない。UpdateSpecialCombatで円との接触と遮蔽を確認し、LinkDamageClock::Claimより前にcontactをtrueにする。接触しても200ms間隔のためダメージを与えない場合がある。GameSceneは端点を描画し、SpecialValidationはcontactで画像取得を判断する |
| C: GetBulletCounts | 「総数」→ 自機・敵・敵対する経験値敵の所有者別内訳。削除待ちの死亡弾も含む | GetBulletCountはbullets_.size()。GetBulletCountsは所有者で集計しIsDeadを検査しない。発射上限の事前判定やシーンの集計にも使われる |
| D: RailCharge::Step | 「1段階更新」＋英語の負値・0の補足 → pressedの継続入力、dtの秒と負値扱い、readyによるリセット、離した際の秒数と状態解除を説明 | ready=falseならResetして-1。押下中は上限まで蓄積して-1。未押下継続状態の解放は-1。押下後の解放はsecondsを返してResetし、0も発射結果。最大時間でも解放まで結果なし。AttackRailCannonが負値を除外して実際に生成する |
| E: BulletManager::Add | 「受け取れば追加」「呼び出し後の所有者は管理クラス」→ 所有権は渡るが登録は条件付き。拒否なら呼び出し内で破棄 | 空のunique_ptrは何もしない。追加する弾のUsesRunProjectileRules()がtrueの場合だけ、同じ所有者の生存弾240以上で拒否。集計には通常弾も含む。falseの弾にはこの条件を適用しない。結果を返さず、衝突走査中に直接追加しない契約も維持 |
| F: NotifyDroneHit | 「命中を通知」→ ロック蓄積・成立時の集計と演出予約を更新し、適用すべき補正後ダメージを返す | 装備系統・能力・対象・記録枠などが対象外ならoriginalDamage。処理する記録があれば倍率を掛け四捨五入、最低1。droneが負なら対象外、32以上は蓄積しないが既存ロック倍率は適用。CollisionManagerは返値で弾のダメージを一時変更し、双方の衝突通知後に復元。突撃・自爆側は返値を直接ダメージに使う |
| G: 比率・時間 | 「レール砲チャージ比率」→ 押下中の蓄積秒数。EMP・回転斬撃・被ダメージ演出も計算と範囲を個別に説明 | GetRailChargeRatioは最大時間で割らない。現在の最大1秒では比率と数値が一致する。GetEmpRatioは残り秒数/2.5で、要求の上限3秒なら1.2。GetSpinBladeRatioは残り時間/.8でgetter自身は制限しない。GetDamageFeedbackRatioは明示的に0～1へ制限。ドローンの表示progressは0～1に制限するが帰還時も突撃制限時間を基準にする |
| H: 日本語 | 「対象Lock」「ダメージTaken」「主攻撃攻撃」など → ロック対象、被ダメージ処理回数、主攻撃の実行回数など実際の意味へ修正 | 通知・実行回数・弾数・表示用の写しを区別して確認。英単語の置換だけでなく、RequestSlowの消費、DroneShootのドローン生成、GetRankFromLevelの変換方向なども照合 |

## 全体確認で修正したその他の契約

- `RailRecovery`: 基準の発射間隔と戻り値はいずれも秒。倍率と上下限を反映する。`RailDamageScale`・`RailSpeedScale`へ渡す値も現在は蓄積秒数で、0～1へ制限する計算を変更していない。
- `LinkDamageClock::Claim`: 読み取り判定ではなく次の許可時刻を予約する。空き枠・待ち時間終了枠を再利用し、同じ更新の複数レーザーで共有する。
- `DroneMission`: Startの失敗時、Arriveの受付段階、イベント消費、時間切れでの帰還、Stepが再構築完了だけtrueを返すことを説明。Availableは「新規任務を開始できる」と同義にしない。
- `PainterLock`・`ChooseSpreadTarget`・`SpinCycle`: ロック成立条件と保持するID、配列の借用と負の戻り値、スタミナ消費と1呼び出し1回までの攻撃タイミングを説明。
- `PlayerStats`・`NeonBodyLayout`・`BarrelModel`・`EvolutionCircuitNodeDefinition`・`WallSmashTarget`: 基礎／実行値、形状と姿勢の違い、描画資源の所有、解放条件の所在、敵の壁衝突候補という実際の保持情報へ修正。
- `SpecialCombatStats::spreadTargets`: 件数ではなく`chosen % 32`のビット集合と明記。
- `Player::Initialize`: 宣言のobjectBulletは実装では自機本体の描画オブジェクト。引数名を変えず、借用・nullptr不可・寿命を説明。
- `Player::RequestSlow`: 要求を予約するのではなく、保留要求を消費して返す。各Consume系も読み取りと消費を区別。
- `Player::Die`: 死亡演出を開始する。生成済みの弾の消去・衝突対象からの除外を保証する説明を撤回。cppの「すべての弾を消す」も実際の動作へ修正。
- `Player::DroneShoot`・`UpdateSummoner`・`UpdateP`: それぞれドローン1機の生成、旧待ち時間だけの更新、未使用引数の空処理であることを説明。互換APIは残した。
- `Player::GetClassConfig`・`GetCurrentClassConfig`・`GetCurrentClassName`・`GetDronePtrs`・`BulletManager::GetBulletPtrs`: コピーされる一覧と借用する要素、nullptr、再読込・削除・装備変更をまたぐ寿命を区別。
- `RecalculateStatsFromBase(false)`: 再計算前が満タンなら新しい最大HPになる分岐を明記。別途HPを保存して復元する呼び出し側の「補充しない」と混同しない。
- `BulletManager`: ClearAllで戦闘参照先も解除すること、予約と成長イベントの回収、特殊命中列の消費、DamageBuildTargetのboolが撃破を示すこと、HasClearLinkが内部サンプルによる判定であることを説明。
- `Player.cpp`のTwin射撃: 「待ち時間を半分」→ 実際の`baseReload / 2.5f`。Ninja: 「3つ拡散」→ 実際の1発・拡散角15度。薬莢・移動演出・バフ粒子の重複説明はヘッダーへ統合した。

## 検証

### 開始時点の検証

編集前に次の4本を実行し、すべて成功した。

- `project/tools/test_tank_additional_abilities.ps1`
- `project/tools/test_tank_projectiles.ps1`
- `project/tools/test_tank_collisions.ps1`
- `project/tools/test_player_class_config.ps1`（20グループ）

ログは`generated/comment-phase1-baseline-abilities.log`、`baseline-projectiles.log`、`baseline-collisions.log`、`baseline-class-config.log`（後ろ3件も同じ`comment-phase1-`接頭辞）に保存した。

開始時の`Development|x64`ビルドも成功。通常の制限環境ではMSBuildのFileTrackerがアクセスエラー（MSB4018）になったため、同じ構成をFileTrackerが利用できる実行環境で再実行して成功した。これは編集前の環境による失敗であり、今回のコメント変更による失敗ではない。通常のpython起動や監査用バイナリの読込にも実行環境の制限があったため、既存の同梱Python 3.12と読込可能な実行環境を用いた。ライブラリを新規導入していない。

編集前のコメント監査も実行した。236ファイル、428型、4,253関数、型不足0・関数不足0・不明な引数0。監査が既に合格していても、上記の誤説明は残っていた。

### 修正後の検証

| 検証 | 今回の結果・条件 |
| --- | --- |
| 差分・範囲 | 開始時はクリーン。差分を読み、変更は前掲6ファイルのみと確認。ブランチ・HEADも一致 |
| `git diff --check` | 成功 |
| C++字句比較 | 変更したC++ 4ファイル、合計50,831トークンが開始時の内容と一致 |
| マクロ等 | プリプロセッサ指令のトークン列とバックスラッシュによる行継続が一致。include・マクロ・条件付きコンパイルの指定は変更なし |
| 既存テストの目印 | 実コード抽出に使う関数シグネチャ・型名を確認。抽出を使う投射物・衝突テストも再実行して成功 |
| 既存コメント監査 | `audit_source_comments.py --check --output generated/comment-accuracy-phase1/final-audit.json`が成功。型不足0・関数不足0・不明な引数0。236ファイル・428型・4,253関数で開始時と同じ |
| Developmentビルド | `MSBuild project/CG2.sln /t:Build /p:Configuration=Development /p:Platform=x64 /m /nologo`成功、警告0・エラー0 |
| Releaseビルド | 同じコマンドの`Configuration=Release`で成功、警告0・エラー0 |
| 追加能力 | `test_tank_additional_abilities.ps1`成功。実際の任務遷移・再構築・ロック・分散選択・回転攻撃・EMP等 |
| 投射物 | `test_tank_projectiles.ps1`成功。実コードを抽出して、チャージ解放・短押し・回復待ち・レーザーの間隔／遮蔽・3段目斬撃波・パリィ、所有者別上限、640通りの組合せ等を検査 |
| 衝突 | `test_tank_collisions.ps1`成功。実コードを抽出した衝突・資源・成長上限・リセット・破棄等 |
| 機体設定 | `test_player_class_config.ps1`成功、20グループ。出力の「current class is missing: Twin」は失敗時保持を確かめる成功した負例 |
| 遠征 | `test_tank_expedition.ps1`成功。進行・初期装備・性能調整の3実行ファイル |
| 表示の遷移 | `test_tank_presentation.ps1`成功 |
| 特殊能力の実行時検証 | `test_tank_special_runtime.ps1 -Configuration Development`成功。今回のDevelopment版で15項目、completed=true・testMode=true・forcedDamage=false・errors空。18枚の画像の存在・更新時刻・最小サイズを既存スクリプトで検査 |

コンパイル条件は名前だけで判断せず、プロジェクトとビルドログの実際のCL引数を確認した。Visual Studio 18 Community、MSVC 14.51.36231（v145）、x64、C++20、`/utf-8`、`/WX`。対象ソースのDevelopmentは`/Od /MT`、`CG2_DEVELOPER_TOOLS=1`、`USE_IMGUI`有効で、`_DEBUG`ではない。一部の指定済みファイルはDevelopmentでも`/O2 /GL`を使う。Releaseは`/O1 /MT`、`NDEBUG`、`CG2_DEVELOPER_TOOLS=0`で、制作UIは無効。

単体テストはソリューションのDebug構成ではなく、各既存スクリプトのCLによるx64ビルド。追加能力・投射物・機体設定・遠征・表示はC++20と`/O2 /W4 /WX /UNDEBUG`（機体設定は既定の非ASan実行）。衝突はC++17と`/O2 /W4 /WX`で、NDEBUGは定義していない。

修正後のログ・監査結果・字句比較結果は`generated/comment-accuracy-phase1/`に保存した。ビルドの詳細ログ`final-development.log`・`final-release.log`はMSBuildのCP932出力。実行時のJSONと画像は`project/generated/special_validation/`。これらと検証補助スクリプトはGitの無視対象に置き、プロジェクトの検証ツールや設定を変更していない。

### 動作・APIを変えていないことの確認と限界

今回専用の検証補助`generated/comment-accuracy-phase1/verify_tokens.py`で、開始時のバイト列から字句解析した列と修正後を比較した。行継続を処理し、コメント・空白だけを除外する。通常の文字列・文字リテラルとエスケープ、接頭辞付き／raw文字列は内容を保持し、識別子・数値・演算子も比較した。コメント記号を含むリテラル、raw文字列、文字のエスケープ、トークン境界、マクロ行継続、文字列変更を検出する例でも比較処理を確認した。文字列リテラルを正規表現で捨てる方式ではない。

| C++ファイル | 一致したトークン数 |
| --- | ---: |
| `project/game/player/TankSpecialCombat.h` | 2,212 |
| `project/game/player/actor/Player.h` | 7,166 |
| `project/game/player/actor/BulletManager.h` | 707 |
| `project/game/player/actor/Player.cpp` | 40,746 |

関数名・引数・戻り値・メンバー・初期値・計算式・分岐・処理順序・文字列リテラルが同じであることを、字句比較と差分の読み合わせで確認した。変更行の改行形式も開始時のCRLFに揃えた。

この比較は独自の字句走査であり、C++規格の全構文を保証する汎用コンパイラではない。トークン一致だけでビルド・動作を保証せず、別に上記のビルドと既存テストを実行した。実コード抽出テストには描画などのアダプターがあり、15項目の実行時検証も全プレイ状況を網羅しない。画像はスクリプトで生成を確認したが、全画像の見た目を人手で審査したとは報告しない。

コメント監査は構文と説明の存在・引数名を確認するもので、役割・単位・寿命・状態変更の説明内容は保証しない。今回、監査を通すための条件緩和やテスト無効化は行っていない。監査ツールの修正も行っていない。

未実施の検証は、実際の`Debug|x64`プロジェクトビルド、ASan、Release版の実行時検証、手動の全プレイ・全UI検証、無関係な機能を含む全テスト。今回の範囲に対応するDevelopment・Releaseのビルドと既存の関連検証を選択したためで、これらの成功をもって未実施分まで成功とは扱わない。なお`CG2.sln`のDebugは両プロジェクトをDevelopmentへ対応付けており、その名前で実行しても真のDebug検証にはならない。

## 残課題と別件候補

今回のコメントでは、確認できる現在の制約を記述した。以下は名前・計算・設計・不具合候補の別件であり、コードは変更していない。

| 候補 | 根拠・条件と次に確認する点 |
| --- | --- |
| 比率APIと秒数の名前 | `Player.h::GetRailChargeRatio`は秒数、`TankSpecialCombat.h::RailDamageScale`・RailSpeedScaleは入力を0～1へ制限する。最大チャージ時間を1秒以外へ変える場合、名前・表示・倍率計算を合わせて検討する必要がある。GetEmpRatioも最大要求3秒で1を超えるため、将来の表示側が0～1を前提にしないか確認する |
| Addの登録成否と発射集計 | `BulletManager.cpp::Add`は拒否時もvoid。`Player.cpp::AttackRailCannon`などはAdd後に発射集計や演出を更新するため、上限で登録されない場合と発射実行を区別する設計の確認候補。今回は戻り値を追加していない |
| 壁衝突のstrength | `Player.SpecialAbilities.cpp::ArmWallSmash`はstrengthを補正して記録するが、UpdateAdditionalAbilitiesの壁衝突ダメージはbaseMeleeとTankEffectPowerから計算し、pending.strengthを使わない。用途・意図の確認候補 |
| 分散対象の集計 | `Player.cpp`は最大48件の対象を扱うが、spreadTargetsは`chosen % 32`で32ビットへ記録する。32件を超える対象は同じビットに重なるため、実際の異なる対象数を示す値としては使えない。集計方式の確認候補 |
| 死亡後の衝突の前提 | `Player.cpp::Die`は衝突登録を解除せず、`CollisionManager.cpp::SetColliders`は自機を登録し、CheckCollisionPairには自機の死亡による一律除外がない。TakeDamageは死亡中を除外する。死亡フレームの呼び出し順やシーン側の停止条件を含む検証が必要で、実プレイの不具合と断定していない |
| 旧射撃の時間単位 | `Player.cpp::DroneShoot`にはbulletCoolTimeを1ずつ減らす処理があり、Attack側にはdeltaTime秒で減らす処理がある。共有する待ち時間の呼び出し順・互換仕様の確認候補。今回は単位や計算を変えていない |
| 表示の進行度 | `Player.SpecialAbilities.cpp::GetDroneAbilityVisuals`の帰還progressは突撃の1秒を基準にし、任務の帰還時間切れは2.5秒。現在の表示用基準をコメントに明記し、段階の完了率と統一するかは別件とした |

対象外の関連ヘッダーにも機械的な説明が残る（例: `TankExpeditionLoadout.h::ReloadScale`）。APIの照合用に参照しただけで、今回の全面審査・修正対象には広げていない。制作UIの表示文言や他の実装ファイルの処理コメントも、今回の確認結果を根拠に全体の改善完了とは扱わない。

重点3ヘッダーで今回確認した契約の修正と検証を完了し、この範囲で終了する。第2段階の関数内部への全面的な処理コメント追加、APIや設計の改善には進んでいない。
