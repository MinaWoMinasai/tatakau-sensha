# コメント品質改善・第2段階：戦闘・弾・衝突

実施日: 2026-10-03。今回の範囲では実装を読み、コメントと必要な書式を修正した。全体の列挙と今回の内容審査は区別し、次の機能グループは編集していない。

## 開始時点と方針

- 作業ブランチ: `docs/comment-readability-phase2-combat`
- 開始HEAD: `071c95fbf7634735450ae87b9ea98dbba9e34f13`
- 作業ツリー: 追跡対象の変更・未追跡ファイルともになし。現在のブランチで作業し、終了時もブランチ・HEADは同じ。
- 開始時に対象の現在の配置・内容を読み、比較用のバイト列・SHA256を`generated/comment-readability-phase2/start.json`と`baseline/`へ保存した。
- `docs/code-comment-guide.md`、単元3のREADME、第1段階報告、`.clang-format`、関連する既存テストの実行脚本を先に読んだ。第1段階の契約と現行のDoxygen形式に矛盾はなく、これらの文書・設定は変更不要と判断した。
- 本報告・進捗表は開始時に存在しなかったため新規作成した。過去のREADME・第1段階報告の検証記録を書き換えていない。

ブランチの作成・切り替え、commit、push、merge、rebase、reset、clean、stash、clone、リモート同期は行っていない。変更は自作C++のコメント・空白・改行と今回の記録文書だけで、検証補助・ログ・実行時出力は既存の無視対象であるgenerated配下へ置いた。テスト・監査ツール・ビルド設定・制作JSONは変更していない。

## 棚卸しと審査範囲

[継続用の進捗表](comment-improvement-progress.md)に、各ファイルの機能区分、文書・内部処理の確認状態、確認した範囲、候補と優先度、今回の内容ハッシュ、過去報告と今回確認の区別を記録した。

| 区分 | 列挙数 | 今回の扱い |
| --- | ---: | --- |
| 実行用C++ | 236 | engine 107、game 128、main 1。h 133・cpp 103。全体の内容審査ではない |
| 自作シェーダー | 66 | hlsl 53・hlsli 13。列挙のみ、内容は未確認 |
| 制作・検証ツール | 85 | 検証C++ 27、スクリプト58。関連脚本・アサーションなどの参照範囲のみ確認 |
| 合計 | 387 | 列挙を改善完了数として扱わない |

自作.hpp/.inl/.ipp等も列挙対象に含めたが、今回の実行用C++には該当ファイルはなかった。`git ls-files`と、無視対象の生成物を除いた`rg --files`の該当ソース集合が一致することを確認した。外部ライブラリ・vendor・生成コード・素材・設定JSONは除外。シェーダーは素材と混同せず別区分とした。GeneratedTextureCache.hは自作の管理コード、level_aiditor.pyのgenerated表記は出力説明の文字列であることも確認し、名前だけで除外していない。

英語説明・長行・旧処理らしい行の検索結果は「未判定の候補」として扱い、不適切と自動判定しない。候補がないファイルも未確認のままにした。

### 全体を確認・修正した6cpp

| ファイル | 確認した機能のまとまり |
| --- | --- |
| `project/game/player/actor/Player.SpecialAbilities.cpp` | 追加砲身、能力のリセット、回転斬撃、ドローン任務表示・命中ロック・壁衝突候補・追加能力の更新 |
| `project/game/player/actor/BulletManager.cpp` | 所有と追加拒否、生成予約、地形通知と削除、軌跡、所有者別集計、射撃強化・マーキング・装甲反射 |
| `project/game/player/actor/Bullet.cpp` | 初期化、移動と寿命、双方通知時の耐久度、命中履歴、帰還、壁反射、分裂予約と生成、外観と軌跡 |
| `project/game/player/actor/PlayerDrone.cpp` | 通常・遠征の射撃、照準、任務遷移と到着、地形移動、描画、接触によるHP減少と死亡 |
| `project/game/player/actor/AttackController.cpp` | 中心/銃口指定、入力の除外、度とラジアン、拡散、所有者別設定とAddへの所有権移譲 |
| `project/game/collision/CollisionManager.cpp` | 借用一覧、重複しないペア、死亡・履歴・形状・マスクの判定、ダッシュ/装甲の例外、双方通知と追加生成 |

各ファイルの全関数名は進捗表に記録した。空関数、単純な取得、初期化も読んだ上で、不要な逐語説明は追加していない。関数の契約は対応ヘッダーと照合し、cppへ丸ごと複製しなかった。

### 部分対象と対応ヘッダー

`project/game/player/actor/Player.cpp`では、開始時の調査で次の25関数を対象として確定した。

- 攻撃: `Attack`、`AttackRailCannon`、`FireConfiguredClass`、`DroneShoot`、`Smash`
- 特殊戦闘: `UpdateSpecialCombat`、`UpdateRunProjectiles`、`ApplyRunProjectileRules`、`GetRunFireIntervalScale`、`GetRailChargeMuzzle`、`ConfigureRunDrone`
- 命中・特殊行動: `OnCollision`、`TryDashImpact`、`Damage`、`TakeDamage`、`TryActivateSpecialAction`、`ActivatePerfectDodge`、`ActivateSaberCounter`、`TriggerSaberCounter`
- 消費: `ConsumePrimaryAttackPerformedEvent`、`ConsumeDashImpactEvents`、`ConsumeSpecialCombatEvents`、`ConsumeLaserShotEvents`、`ConsumeMineDropEvents`、`ConsumeMeleeSlashEvents`

HUD・移動全般・編集・進化UIなどは今回の部分審査に含めない。Player::Updateはドローン射撃の集計、ダッシュ追加射撃、誘導更新の呼び出し順を参照しただけで、整形・コメント修正の対象にしていない。

`project/game/player/TankSpecialCombat.h`は全型・全関数を再確認した。RailCharge、線分/矩形の区間計算、LinkDamageClock、DroneMission、PainterLock、ChooseSpreadTarget、SpinCycleの内部に補足を加え、第1段階の契約は保持した。

対応する`Bullet.h`、`PlayerDrone.h`、`AttackController.h`、`CollisionManager.h`は宣言・インライン定義を読み、空の描画処理、通知と判定、位置補正、登録と所有権、借用の寿命、利用可能と任務開始可能の区別などを局所修正した。`Player.h`と`BulletManager.h`は今回の対象に対応する宣言・関連型を照合した。Player.hは変更不要、BulletManager.hは空のBullet::Drawを呼ぶDrawの説明だけを局所修正した。今回その2ヘッダー全体を再審査したとは扱わない。

敵・地形・シーン・軌跡管理・関連型・テストは処理の意味と順序を調べる参照先であり、全面改善済みにはしていない。実際の部分参照範囲と変更不要の理由も進捗表に残した。

## 重要な説明改善と根拠

| 箇所 | 修正前 → 修正後 | 実装・利用先の根拠 |
| --- | --- | --- |
| CheckCollisionPair | 英語の双方通知の注意 → 片方の通知で弾が死亡しても、成立済みの接触を双方へ通知することを日本語で明記 | 死亡判定は入口。末尾の2つのOnCollisionと、Bulletの相手弾が死亡していても貫通力を受け取る分岐 |
| 衝突中の弾生成 | 処理が詰まって読み取りにくい → 通知中は予約し、ペア走査後に追加する境界と、親弾走査中に子を別配列へ集める理由を明記 | QueueKillBurst/QueueArmorReflection、CheckAllCollisions末尾、FlushPendingSplitsのchildrenとAdd |
| ドローン弾のロック補正 | 一時補正の範囲が追いにくい → originalDamageの保存、双方通知、射撃強化通知、元の値への復元を順に説明 | CollisionManagerのNotifyDroneHit・NotifyPlayerHit・SetDamage。貫通して次の対象へ進む弾の基礎値を保持 |
| ダッシュ・斬撃の履歴 | 英語や説明不足 → 衝突IDで同じ動作中の重複命中を防ぎ、どこで履歴を解除するかを明記 | TryDashImpact/ActivatePerfectDodge、dashSlashTargets_、specialParriedBullets_、Bullet::hitActorIds_ |
| ドローン任務 | 一行に詰まった遷移と英語 → 予告は停止、突撃は記録座標へ、到着はイベント、帰還と再構築は別、再構築完了回は移動/射撃を再開しないことを説明 | DroneMission::Start/Arrive/StepとPlayerDrone::Update、ConsumeRunMissionImpact/ConsumeRunRebuilt |
| 未処理の壁衝突 | 英語の注意 → 同じフレームの次の斬撃でも、登録時の回数と現在回数が異なる未処理情報を上書きしない条件を説明 | ArmWallSmashのcollision比較とUpdateAdditionalAbilitiesの回数比較・消費、ExpEnemy::MoveCombatActorの回数増加 |
| 接続レーザー | contactと適用許可の関係が不明瞭 → 接触表示を先に記録し、対象別の0.20秒間隔は全線で共有することを明記 | UpdateSpecialCombatのcontact=trueとLinkDamageClock::Claimの別処理。型は端点とcontactのみを保持 |
| 斬撃波・パリィ | 境界条件の説明不足 → 壁で生成できなくても試行を消費、耐久ダメージ0でも同じ振りでは再判定しないことを説明 | specialWaveEmitted_とspecialParriedBullets_を、遮蔽/耐久計算の結果より先に更新する現在の順序 |
| 単位・計算 | 「弾の重さ」「往復の時計」など曖昧な説明 → 速度への反動加算、帰還開始の時計、60基準フレームと秒、回頭のラジアン/秒と度、線分の0～1区間を区別 | Attack/Smash、Bullet::Update、UpdateRunProjectiles、SegmentCrossesBoxの実際の計算 |
| ヘッダーの役割 | Bullet::Drawを描画として説明、SetCollidersの効果が不明 → Drawは互換用の空処理、SetCollidersは既存一覧の末尾へ追加と明記 | 空のDraw定義、SetCollidersにはclearがなくCheckAllCollisions側にある |
| 軌跡の寿命 | Attach/Releaseと破棄の違いが曖昧 → 軌跡を非アクティブにして借用を外すだけ。設定の借用はRelease後も残ると明記 | Bullet::ReleaseTrail、GetBulletColor、BulletManager::ClearAllの解除→弾破棄→軌跡破棄 |

既存英語コメントの有効な情報を日本語へ統合した。仕様として保証されない「Assassinに弾速ボーナスがあっても良い」「charge_beamを後から実装」「金色っぽく変える例」などは実際の分岐へ説明し直した。今回の対象内に、削除対象と確定できる無効化された旧処理の残骸は見つからなかった。互換分岐・空関数・条件付きコンパイルは有効な入口として保持した。

## 書式とコードを変えていない確認

既存`.clang-format`をclang-format 22.1.3で使用した。6cppは全体、Player.cppは上記25関数の断片だけを整形。4スペース、長い条件の改行、独立文の分離を行った。ヘッダーとTankSpecialCombat.hはコメントの局所修正であり、全体整形していない。CRLFを保った。

整形結果は書き込む前に字句列を比較し、include/usingの順序、波括弧、識別子、数値、演算子、文字列等の変更を拒否した。整形・説明追加のまとまりごとにも開始時の保存内容と比較した。

最終の変更C++は13ファイル、合計59,500トークンが開始時と一致。独自の字句走査は行連結を処理し、通常・接頭辞付き・raw文字列、文字リテラル、エスケープを保持した。コメント記号を含むリテラルや文字列変更、トークン境界、マクロの行継続を検出する既存の検証例も再利用した。文字列を正規表現で消す判定ではない。

さらにPlayer.cppの対象25関数を構文解析で取り出し、各関数の範囲を同じ目印へ置換した残りのバイト列が開始時と一致することを確認した。対象外のHUDなどを整形していない根拠となる。

プリプロセッサ指令のトークン列と物理的なバックスラッシュ行継続も一致した。今回の対象に動作へ使う__LINE__/source_locationや文字列化マクロは見つからず、条件付きコンパイルの指定は保持。標準assertの診断やコンパイラ/デバッガに表示される行番号は整形・コメント追加で移動するため、ゲームの処理変更とは分けて扱う。

既存テストの関数シグネチャ・`struct AttackParam {`・`struct BulletTrailSettings`等の抽出目印を確認した。抽出を使う投射物・衝突テストも成功した。コメント内の記号を含めた単純な波括弧走査に依存する脚本があるため、今後の編集でも目印への配慮が必要だが、今回は脚本を改修・緩和していない。

字句一致はC++規格の全構文や全動作の保証ではない。ビルド・関連テストを別に行い、説明内容は実装と利用先を読むことで確認した。診断行番号とバイナリの同一性は保証しない。

## 今回実行した検証

編集前にも追加能力・投射物・衝突の3脚本とDevelopment x64を実行して成功した。これは今回の開始点での検証であり、第1段階の成功記録から流用したものではない。実行環境の選択では、第1段階に記録された通常の制限環境でのFileTrackerアクセスエラーを参考にした。今回の編集前後は同じ、FileTrackerと既存の監査用ネイティブライブラリを利用できる実行環境で検証した。前回の環境上の失敗を、今回のコメント変更による失敗や今回の再検証結果として扱っていない。新規ライブラリの導入は行っていない。

| 検証 | 結果と対象 |
| --- | --- |
| 開始時との差分範囲 | C++13ファイルと記録文書2ファイルのみ。既存ガイド・過去報告・テスト・設定・監査ツールに差分なし |
| git diff --check | 成功 |
| 字句・指令・行継続 | 13ファイル・59,500トークン一致。Player.cppの対象外バイト列も一致 |
| 進捗表の整合 | 387行のファイル集合がgit/rgの現在の集合と一致、各内容SHA256も一致 |
| 既存コメント監査 | `audit_source_comments.py --check`成功。236ファイル・428型・4,253関数、型不足0・関数不足0・不明な引数0。説明の存在検査で、内容の正しさとは別 |
| Development x64 | `MSBuild project/CG2.sln /t:Build /p:Configuration=Development /p:Platform=x64 /m /nologo`成功、警告0・エラー0 |
| Release x64 | 同コマンドで`Configuration=Release`、成功、警告0・エラー0 |
| 追加能力 | `test_tank_additional_abilities.ps1`成功。任務・到着・帰還・再構築・ロック・分散対象・回転・EMP・追加砲身等 |
| 投射物 | `test_tank_projectiles.ps1`成功。実コードの弾・衝突・追加能力・一部Player関数、レールの押下/解放・0秒・レーザー間隔/遮蔽・斬撃波/パリィ・上限・寿命・貫通・非再帰の生成等 |
| 衝突 | `test_tank_collisions.ps1`成功。実コードの双方通知・ダッシュ衝撃・敵命中・資源/所有・成長上限・破棄/リセット等 |
| 軌跡 | `test_tank_trails.ps1`成功。実装抽出の幾何・寿命・キャッシュ・消去/再利用・GPU参照保持・容量等 |
| 特殊能力の実行時検証 | `test_tank_special_runtime.ps1 -Configuration Development`成功。15プローブ、completed=true、testMode=true、forcedDamage=false、errors空。18画像の存在・鮮度・最小サイズを既存脚本で検査 |

実際のCL引数とプロジェクトを確認した。Visual Studio 18 Community、MSVC 14.51.36231/v145、x64、C++20、`/utf-8 /WX /MT`。対象ソースのDevelopmentは`/Od`、`CG2_DEVELOPER_TOOLS=1`、USE_IMGUI有効、_DEBUGなし。既存の個別指定で一部の別ソースはDevelopmentでも/O2 /GLを使う。Releaseは`/O1 /GL`、NDEBUG、`CG2_DEVELOPER_TOOLS=0`で制作UIは無効。

CPUテストはソリューションのDebugではなく、各脚本がCLで構築するx64実行ファイル。追加能力・投射物・軌跡はC++20と`/O2 /W4 /WX /UNDEBUG`、衝突はC++17と`/O2 /W4 /WX`でNDEBUGの定義なし。抽出テストには描画等のアダプターがあり、全ゲームの実行を置き換えるものではない。

ログ・開始点・字句結果・今回の監査結果は`generated/comment-readability-phase2/`。最終ビルドは`validated-development.log`・`validated-release.log`、実行時は`final-state-runtime-development.log`、CPUテストは`final-tank_*.log`と、最後のヘッダー補足後の`validated-projectiles.log`。MSBuild詳細ログはCP932。実行時のJSONと画像は`project/generated/special_validation/`で、既存の無視対象。第1段階のログは上書きしていない。

未実施: 真のDebug x64ビルド、ASan、Release版の実行時検証、無関係な機能を含む全テスト、全画像の人手による外観審査、全プレイ/全UIの手動検証。指定された2構成のビルド・関連回帰・Developmentの実行時検証を今回の範囲として選んだためで、成功をこれら未実施分へ一般化しない。今回の検証でコード変更による新しい失敗は確認しなかった。

## 残る説明と別件候補

対象6cpp・指定25関数・TankSpecialCombat.hの今回の審査と補足は完了した。参照先や未確認部分を含むプロジェクト全体の改善完了ではない。

| 種別・優先度 | 根拠と再確認する条件 |
| --- | --- |
| 説明 P1 | `TankShooterAbilities.h::ReturnFlight`はageとreturningを持つだけで、移動方向を計算しない。`Damage`はダメージ値の計算だけで「戦闘状態へ反映」と矛盾する。MarkLedgerのUpdate/Hitも寿命・蓄積・起爆条件の説明が不足。今回は関連型の参照にとどめ、次の局所修正候補へ記録 |
| 説明 P1 | `CombatTypes.h::AttackParam`は発射設定で、位置・方向のメンバーを持たない。現在の型説明と食い違う。今回の対応4ヘッダー以外の参照型として、次の局所修正候補へ記録 |
| 名前・計算 | 第1段階から継続: GetRailChargeRatioは秒数、GetEmpRatioは1を超え得る。定数や表示側を変える場合に名前・正規化の契約を再検討。今回計算は変更しない |
| 登録と発射の記録 | Addは上限による拒否を返さない。レール砲・一部射撃の演出/集計は登録成否と一致を保証しない。上限到達時に求める集計を別途確認 |
| 壁衝突 | ArmWallSmashに記録するstrengthは壁ダメージ計算に使われない。入力する強さと求める効果を別途確認。未処理の回数を保持する条件は今回説明した |
| 接触ダメージ | PlayerDrone::OnCollisionのHP減少はDamageの無敵受付と別経路。複数接触や点滅中のHP減少の期待仕様を確認。Player::OnCollisionは中心間方向がほぼ0ならTakeDamageにも進まないため、同位置の接触を確認 |
| 帰還・分裂 | Bullet::BeginReturnは新しい分裂数を0にするが、既に保留した分裂を消去しない。直前命中/壁衝突後に帰還開始する場合の期待仕様を確認。現在の保持をコメントに記載 |
| 表示・近似 | 帰還表示progressは突撃の1秒基準で、実際の帰還時間切れは2.5秒。HasClearLinkは内部サンプルによる近似で、端点や短い線分を検査しない。表示基準/遮蔽に求める精度を別途確認 |
| API名・衝突半径 | ConfigureVisualScaleは外観だけでなく弾の衝突半径を変更する。設定を見直す場合は命中への影響も確認。今回ヘッダーの説明を現状に合わせた |
| 既存の順序・単位 | 第1段階から継続: 自機の死亡と衝突登録の関係、旧DroneShootとAttackが同じ待ち時間を異なる単位で減らす経路、32ビットの分散対象集計。呼び出し条件と期待値の確認が必要で、今回不具合とは断定しない |

次回は上記の共有型の説明を補正し、「敵・地形・シーンの命中側」（ExpEnemy、Enemy、EnemyManager、Stage、GameSceneの更新/命中/演出）を対象範囲として定めて進める候補とした。PlayerのHUD・編集、エンジン、シェーダー、他のツールは未確認として進捗表に残した。この報告では次のグループを編集せず終了する。
