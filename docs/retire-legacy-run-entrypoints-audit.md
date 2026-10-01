# 旧GAME / TANK_RUN起動口の廃止監査

2026-10-01、`refactor/retire-legacy-run-entrypoints`。削除前に全827追跡ファイルから指定の識別子・設定・ランチャー・生成箇所を検索しました。正式なPublic経路はTITLE→TANK_EXPEDITIONです。共有GameScene・TankRun基盤・音声を変更せず、外部から旧モードを起動する登録と専用ファイル2件だけを除去しました。

## 削除前のA〜E分類

| 検索対象 / 箇所 | 分類 | 根拠と処置 |
| --- | --- | --- |
| BuiltInGameModuleの`Register<GameScene>("GAME")` | A | 旧GameScene(false,false)の外部登録だけ。正式TITLE・遠征・デモは利用しないので除去 |
| BuiltInGameModuleの`"TANK_RUN"` / `GameScene(true)` | A | 旧単独アリーナGameScene(true,false)の外部登録だけ。除去 |
| `resources/projects/tank_run.project.json` | A | startupScene=TANK_RUN。唯一の実起動参照は専用ランチャー。削除 |
| `tools/run_tank_run.ps1`、その`-Validate` | A | 上記JSONを指定して旧シーンを起動する専用ツール。遠征ランチャー/単体テストから呼ばれない。削除 |
| BuiltInGameModuleの`GameScene(true,true)` | B | 遠征の正式登録。prototypeRunを必要とし保持 |
| TitleSceneの`GameScene(false,true)` | C | タイトルが所有する背景デモ。constructorでprototypeRunもtrueになる。保持 |
| `GameScene(false)` | A / Cと区別 | 単引数falseの直接生成はなし。旧GAMEのtemplate登録による既定false,falseと、デモのfalse,trueを区別 |
| `prototypeRun_` / `expeditionRun_` | B・C | constructorで設定し、その後に代入する箇所・編集する入力欄なし。残す2経路はいずれもtrue,trueの内部状態。共有分岐を保持 |
| `nextSceneName_` / GameScene結果画面のGAME・TANK_RUN文字列 | B・C、旧文字列はA相当の休眠分岐 | 遠征はexpeditionRun=trueなので再遠征はTANK_EXPEDITION、別選択はTITLE。デモは結果入力の前にreturnする。共有関数を変更せず保持 |
| `IsMenuAvailable` | C、GAME判定だけA | 可視メニューはindex 0のみ。index 0と遠征の可用性を判定するよう単純化し、旧GAME判定だけ除去 |
| `startupScene` / GameProject / SceneFactory | B・C | 汎用project読み込み・登録照合・TITLE fallback。default/tank_gameはTITLE、tank_expeditionは遠征。ローダーを保持 |
| `TankRunDirector` / `tankrun::RunDirector` | B・C | TankExpeditionDirectorがincludeし、GameSceneとタイトルデモが同じRunDirectorを生成してカード・報酬・進行を使う。保持 |
| `GameScene.TankRun.cpp` / `.TankRunVisuals.cpp` | B・C | 遠征初期化・UI・カード反映・ボス・結果・Neon描画を共有。保持 |
| TankRunModifiers / Player run API / 報酬・ボス・経済・Run UI | B・C | 遠征/デモのPlayer・敵・成長・描画から利用。保持 |
| `resources/levels/tank_run.json` | B | project JSONとは別のレベルデータ。GameSceneのprototypeRun経路から読む共有設定。保持 |
| `test_tank_run.ps1`と3つのC++テスト | D | CG2.exeや旧projectを起動せず、RunDirector・PrototypeBossCombat・TankRunModifiersの単体テストをビルド/実行する。遠征共有基盤の保証として保持 |
| 遠征・報酬・衝突・ボス・描画等の主要テスト | D | 同じ基盤と現在のTankを保証。保持 |
| 内部の旧単独モード用自動validation分岐 | B・Dとの境界、旧分岐はA相当 | 共用ファイルにあり、遠征用validationも同じフラグを使う。外部ランチャーだけ除去し、内部コードは保持 |
| 過去のprototype資料・以前の除去監査 | Aの起動説明を含む制作記録 | 文書本体を保持。旧コマンドは当時の記録で、現在のPublicでは起動しないことを明記 |
| E | 該当なし | 今回の削除2ファイル・登録2件について判断不明の実参照は見つからない。共有実装全体のdead-code判定は対象外 |

全検索結果はローカルの `generated/legacy_run_entrypoints/references-before.txt`、分類した検索箇所は同ディレクトリのJSONに保存しています。歴史資料の名前を実行用参照と混同しません。開始時のSHA256も全827ファイルと`.git`1436ファイルについて保存しました。

## 実施した変更

- GAME / TANK_RUNの2登録を除去。TITLEと`GameScene(true,true)`によるTANK_EXPEDITION登録を維持。
- 旧project JSONと旧ランチャーの2ファイルを削除。他のproject JSON・遠征ランチャーを維持。
- タイトルの旧GAME可用性判定だけ除去。F9、Enter、Space、click、背景デモを維持。
- Packagingは従来resourcesを再帰コピーするため旧projectも配布していた。元JSON削除に加え、旧JSONの正確なパスを再混入防止規則に追加。共有`levels/tank_run.json`と正式projectを保持するfixtureで検査。
- vcxproj/filtersには削除2ファイルの登録が元からないため、XMLとパスを検査し、登録は変更しない。
- prototype資料に歴史記録の注記を追加。README・資料索引・配布説明・現存素材台帳を現在の2シーン構成へ合わせる。過去の監査本文・LICENSE/OFLは保持。

開始時はresources 221件です。以前の台帳218件との差は、作者が前作業後に追加したNeonSkinned shader3件でした。これらと対応renderer・pipeline testは変更せず、台帳へ補完しました。今回のproject JSON1件の除去後はresource 220件です。

## 戻り先が安全な理由

残すSceneRegistryはTITLEとTANK_EXPEDITIONだけです。TITLEはGameScene(false,true)を直接作成し、遠征はGameScene(true,true)を作成します。GameScene constructorは`prototypeRun_ = prototypeRun || expeditionRun`なので、どちらもexpeditionRun/prototypeRunがtrueです。フラグはその後に書き換えられません。

GameSceneの結果確認は再試行でexpeditionRunを最初に判定してTANK_EXPEDITIONへ進み、別選択はTITLEへ進みます。TITLEデモは`UpdateGameFlow`内で`if (titleDemo_) return;`を通るため、結果入力から旧IDへ進みません。GameScene.cpp内にGAME/TANK_RUNの文字列は残りますが、正式Public起動からは到達しない旧条件分岐です。今回これを単純化せず、共有ソースをSHA256で保持確認します。

旧scene IDを指定した外部JSONはSceneRegistryで見つからず、既存のResolveStartupSceneがTITLEへfallbackします。GAME/TANK_RUNを外部JSONで再び起動する登録は残しません。ローダー自体の汎用性は維持します。

## bulletShootと次の別作業

今回、Game.cppの共通ロード、Playerの旧射撃音2箇所、GameSceneの旧強化音分岐、test_audio_runtime、MP3、音声Packagingを変更しません。

入口を閉じた後の正式経路では、遠征初期化とデモ再設定でcheckpointEvolutionがtrueになります。旧Player再生2箇所は`!runCheckpointEvolution_`のため通らず、GameSceneの強化音はexpeditionRun=trueで遠征専用音へ分岐します。全SetRunModifiers呼出しはenabled=trueを渡し、同フラグを解除するdisabled分岐へ進みません。したがって通常Public runtimeの旧MP3再生は到達不能になるとコード上で確認できます。ただし共通起動時のLoadAudioは残り、素材のロード依存はまだあります。

次の音声整理では、共通ロード・旧再生分岐・MP3単体・Packaging fixtureの保持条件・test_audio_runtimeのMP3デコード入力を一体で調べてください。MP3デコード/PlayAudioSEの汎用Engineテストは、権利確認済みの入力へ置換して回帰保証を残す必要があります。出典・Public再配布条件が未確認である点は変わりません。今回は音声仕様を変更しません。

## 検証結果

2026-10-01、変更後の既存テストとRelease実行で確認しました。ビルド・実行時の自動テスト用環境変数は復元しています。

| 検証 | 結果 |
| --- | --- |
| Release / x64（CG2DeveloperTools=false） | 成功、警告0・エラー0 |
| Development / x64 | 成功、警告0・エラー0 |
| Debug / x64（sln） | 成功。既存のslnではDevelopmentへマップされるため、次行も検証 |
| Debug / x64（vcxproj直接） | 本来のDebug構成も成功、警告0・エラー0 |
| Tank主要テスト15本 | 全成功。test_tank_runのRunDirector / BossCombat / Modifiers 3スイートを含む |
| test_tank_submission_packaging | 成功。旧project拒否、共有レベル・正式project3件・音声の保持、manifest、再混入・改変の検出 |
| test_title_demo（Release） | 成功。4ステージ、実射撃458サンプル、37撃破、32ダッシュ、3報酬、フェード、新しい遠征への遷移 |
| test_tank_submission_runtime（Release配布コピー） | 成功。TITLE 2回・遠征2回、初回tutorial・機体選択・工房・修理・敵3種・boss/result・TITLE復帰・新しい遠征・完了後skip、errors 0 |
| 旧GAME / TANK_RUNを指定する外部JSON | 両方とも成功。実際のScene.InitializeはTITLEとTANK_EXPEDITIONのみ。旧シーンの初期化なし、終了コード0 |
| git diff --check | 成功 |
| vcxproj / filters XMLと全Includeパス | 正常。登録283件 / 266件、存在しない参照0、両ファイルSHA256不変 |
| 残存起動口・削除ファイル参照 | 実行用の登録・旧ランチャー・現存startupSceneから旧IDを除去。残る旧ID1行は共有結果関数の到達不能分岐。過去の記録とPackaging除外fixtureは保持 |
| README / docsリンク | Markdown文書40件のローカルリンク153件を検査、リンク切れ0 |
| 配布物のproject JSON | default / tank_game / tank_expeditionの3件のみ。TITLE / TANK_EXPEDITIONを指定 |
| 配布物の内容 | 未プレイの別コピー238ファイル。共有resource 218件のSHA256一致、levels/tank_run.json・bulletShoot.mp3・実行用WAV9件を保持。旧project・preview・Git履歴・logs・cache・archive・未申告ファイルなし |
| Third-party | 既存LICENSE/OFL等12ファイルのSHA256不変。配布中の表示・ライセンスも元ファイルと一致 |
| 非対象ファイルのSHA256 | 開始時827件から2件だけ削除、変更14件、811件不変。共通Engine106件、登録/可用性を除くgame 129件、残るresources全220件は不変 |
| .git | 開始時1436ファイルの件数・SHA256不変。commit / push / remote変更なし |
| 作者のtutorial進捗 | タイトルデモ・起動検証前後で内容/存在状態が不変。配布runtimeは隔離したコピーで確認 |

15本は `test_tank_expedition`、`test_tank_expedition_map`、`test_tank_expedition_rooms`、`test_tank_expedition_content`、`test_tank_expedition_tutorial`、`test_tank_run`、`test_tank_reward_pool`、`test_tank_reward_cards`、`test_tank_collisions`、`test_tank_projectiles`、`test_tank_enemy_combat`、`test_rival_boss_combat`、`test_tank_presentation`、`test_tank_additional_abilities`、`test_tank_trails`です。主要15本に加え、Packaging、title demo、submission runtimeを実行しています。`test_audio_runtime`はソース・入力素材を変更せず保持し、今回は音声仕様を扱わないため再実行対象にしていません。

ビルドログ・テスト結果・SHA256比較・リンク一覧・配布manifest監査はローカルの`generated/legacy_run_entrypoints/`に保存しています。配布runtimeの後半戦闘には既存の強制clear、修理検査には境界fixtureを使います。操作・遷移・回帰の検証であり、難易度の通しプレイ検証ではありません。実行済みruntimeコピーは進捗/画像/logを含むQA用で、配布には未プレイのコピーを作成してください。

## 作者がmerge前に確認する点

- TITLEから通常操作で遠征を開始し、背景デモ・戦闘・結果の再遠征/TITLE復帰を一度手動確認してください。今回の自動検証では遷移と共有基盤は通っていますが、長時間プレイの体感までは保証しません。
- 自分用の外部projectやショートカットでGAME / TANK_RUNを使っていた場合、今後は登録されずTITLEへ戻ります。正式な遠征projectを指定してください。
- bulletShootの出典・再配布条件は未確認です。次の別branchでロード/旧再生/MP3デコードfixture/Packagingをまとめて整理する候補です。今回MP3を削除したと扱わないでください。
- GameScene内の旧条件分岐や汎用Engineのdead-code整理は別作業です。TankRunの名称だけで共有カード・報酬・ボス・Player API・描画・UI・レベル・テストを削除しないでください。
- CIはローカルからGitHubへ送信していないため未実行です。merge時に通常のCI結果を確認してください。保持素材220件のうち権利分類Dの121件について、既存の作者確認課題は残ります。
