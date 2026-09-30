# Naval Prototype の除去記録

2026-09-29。開始ブランチは `refactor/remove-naval-prototype`。開始時の追跡ファイル874件に既存差分なし。削除前に全追跡ファイルから候補名・シーンID・include・設定・モデルと材質の依存を検索し、実際のロードと描画経路を確認しました。以下の分類・削除候補を先に記録・提示し、専用13ファイルを除去しました。共用・判断不能の機能は保持しています。

## 削除前の分類と参照元

本書のA/B/C/Dは今回の用途分類です。[素材台帳](public-assets-inventory.md)の権利確認分類とは別です。海戦専用であることは、素材を自作・再配布許諾済みと認定する意味ではありません。

| 分類 | 対象 | 参照・判断根拠 |
| --- | --- | --- |
| A：専用なので削除 | `project/game/naval/scene/NavalBattleScene.cpp` / `.h` | 専用シーン。外部接続はBuiltInGameModuleのinclude・登録、TitleSceneのF4、project JSONとVS登録。Tank / Labから呼び出しなし |
| A：専用なので削除 | `project/game/naval/rendering/NavalOceanRenderer.cpp` / `.h` | NavalBattleSceneだけが所有・使用するObject3dのラッパー。共通OceanRendererとは別のクラス。参照は専用ヘッダー・実装とvcxprojのみ |
| A：専用なので削除 | `project/resources/projects/naval.project.json` | 開始シーンが `NAVAL_BATTLE`。VSのNone登録のみ。既定のTITLE設定は別ファイル |
| A：専用なので削除 | `project/resources/navalHullBox.obj` / `.mtl` | NavalBattleSceneの敵艦だけがモデルをロード・使用。OBJが同名MTLを指定 |
| A：専用なので削除 | `project/resources/testShip.obj` / `.mtl` | NavalBattleSceneの自艦だけがモデルをロード・使用。OBJが同名MTLを指定 |
| A：専用なので削除 | `project/resources/navalWhiteBlock.obj` / `.mtl` | ロードするコードなし。ファイル内の説明も海戦プロトタイプ用ブロック・デバッグ材質と明示。OBJが同名MTLを指定 |
| A：専用資料を削除 | `docs/naval_game/ENGINE_ASSESSMENT.md`、`GAME_SPEC_AND_ROADMAP.md` | 海戦作品の企画・エンジン調査。資料索引と過去の分離計画・監査にディレクトリ記載あり。共通エンジンの唯一の利用手順や第三者LICENSEではない |
| B：共用なので保持 | `bullet.obj` / `.mtl` | Tankの `game/player/actor/Bullet.cpp` がロード・使用 |
| B：共用なので保持 | `cube.obj` / `.mtl`、`ground.obj` / `.mtl`、`white512x512.png` | Stage、Skybox、GraphicsLab、GameScene、TitleScene等が使用。削除する3モデルのMTLも共通の白テクスチャを参照するが、画像は削除しない |
| B：共用なので保持 | Input / Audio / TextureManager / Object3d、Bloom / Shadow / Particle / Trail、共通HLSL | 現行Tank / Lab / エンジンの描画・入力・音声を支える。全ファイルを無変更で保持 |
| C：水面技術として共有・発展しているため保持 | 共通 `DirectX/engine/3d/OceanRenderer.*`、Ocean系shader、GraphicsLabScene、Object3d shaderの高密度水面経路 | Graphics Labが独立して利用。ArcBlanc・FFT、水面診断UI、Calm / Naval / Arc Blancの描画モードを保持。「Naval」は水面プリセット名であり、削除する海戦シーンへの導線ではない |
| D：専用と断定せず保持 | `sea.obj` / `.mtl`、`graphicsOcean.obj` / `.mtl` 等の旧汎用素材 | `sea.obj` の明示ロードはNavalのみだが、専用の制作物と確定できず、素材・動的選択・出典の判断を保留。他の旧素材も名前だけで削除しない |
| D：共通API・shaderとの関係を保留して保持 | Object3dのwake定数バッファ、Rootのslot 17、古い係数範囲のwater分岐、船反射用パラメーター | 専用ラッパーが設定する値を含むが、共通描画API・shader・root layoutまでの除去は今回行わない。詳細は次表 |

削除対象は上記Aの13ファイル、161,505 bytes（コード4、resources7、資料2）です。専用のaudio・単独shader・ツール・テスト・GitHub Actions処理は見つかりませんでした。削除するファイル内に第三者LICENSE・copyright noticeは見つからず、共用ライブラリ本体と既存ライセンス本文はすべて保持します。

## 水面描画を残す根拠

| 経路 | Navalでの使用 | 他の使用・保持理由 |
| --- | --- | --- |
| Object3d VS/PS：`environmentCoefficient >= 2.5` | 専用ラッパーが3.15を設定 | GraphicsLabのriverも3.15を設定。`navalOceanMode`（2.85以上）とArcBlanc（3.10以上）の分岐を実際に通る。削除しない |
| 共通OceanRenderer / Ocean VS・PS / spectrum・FFT compute | 専用ラッパーとは独立 | GraphicsLabが生成・描画し、既定モードはArcBlanc。UIでCalm / Naval / Arc Blancを選択できる。RootのOcean / OceanComputeも残す |
| VSのwakeとRoot slot 17 / b3 | wake setterの明示呼び出しは専用ラッパー | 共通Object3dが常にバッファを生成・初期化・bindする。Tankも同じ描画クラスを使用。値の設定元だけを理由にAPI・root signature・shader契約を崩さない |
| Object3d VS/PS：`1.5 <= environmentCoefficient < 2.5` | 旧Naval waterというコメントあり | 現在のNaval / GraphicsLab水面は3.15。その他の環境係数は編集可能で、専用との証明が不十分。保留して残す |
| PSの船反射相当の処理 | 専用ラッパーが材質値を設定 | 共通高密度水面内で汎用材質フィールドを解釈する分岐。単独除去の影響を確定せず、今回は残す |
| TextureManagerのoptional skybox fallback | Navalは `skyboxSky.dds` を要求していた | `skyboxSky.dds` 自体は追跡ファイルにも作業ツリーにも存在しない。通常の `skybox.dds` と共通の失敗時cubemap生成処理があり、Tank / Labも利用。名前の互換処理を含め無変更で保持 |

## 共通ファイルの変更範囲

- BuiltInGameModule：Navalのincludeとシーン登録だけ除去。TITLE / GAME / TANK_RUN / TANK_EXPEDITION等は保持。
- TitleScene：F4 → Navalの1行だけ除去。F2 / F3 / F5 / F6 / F7 / F9、Enter / Space / クリック、タイトル背景デモは保持。
- vcxproj：専用ClCompile 2件、ClInclude 2件、None 1件を除去。filters：シーンのClCompile / ClIncludeとproject JSONのNone、計3件を除去。共通renderer・shaderの登録は保持。
- Game.cpp / RuntimeProfiler / 共通エンジン / shader / workflows / 既存ツール：変更しない。
- README、資料索引、公開監査、素材台帳、Third-party案内：現行の同梱範囲へ更新。過去の監査は実施時点の記録として保持する。

## 検証結果

ローカルのVisual Studio 2026 / v145、Windows SDK 10.0.26100.0、既存Assimpを使用。依存の再生成は行っていません。

| 検査 | 結果 |
| --- | --- |
| 削除一覧・差分 | 分類Aの13件、161,505 bytesを削除。各対象の絶対パスが作業ディレクトリ内に収まり、開始時のSHA256と一致することを確認してから削除。想定外の削除・変更なし |
| 保持対象の比較 | 戦車ゲーム・共通エンジン・shader・resources・vendor・workflows・第三者文書の732ファイルをSHA256で照合し無変更。残存resourcesは247件すべて無変更 |
| シーン登録・タイトル | BuiltInGameModuleはNavalの2行、TitleSceneはF4導線の1行を除いた残りが開始時と同一。他の登録・通常操作・開発用キー・タイトル背景デモを保持 |
| 削除後の参照検索 | ソース・設定・モデル・shader・ツール・Actions・VSプロジェクトに、削除ファイル名／`NAVAL_BATTLE`／専用クラスへの参照なし。監査と過去計画に削除記録は残す |
| vcxproj / filters | XML正常。ファイル項目297件／281件に存在しない参照なし。指定した5項目／3項目以外の登録は開始時と同一 |
| Release / x64 | 成功。`CG2DeveloperTools=false` |
| Development / x64 | 成功。既存の開発機能ON。DirectInput既定値・リンク最適化の通知あり |
| Debug / x64 | ソリューションのDebug指定で成功。既存 `CG2.sln` は両プロジェクトのDebugをDevelopmentへ割り当てているため、独立したDebugバイナリの検証ではない。設定は変更していない |
| 実行ファイル | Release / DevelopmentのEXEに削除シーンID・専用クラス名のASCII / UTF-16文字列なし |
| `test_tank_expedition_map.ps1` | 成功。C++17 / C++20の各2048 seed、全経路条件、導入経済、練習・旧経路・保存等 |
| `test_tank_reward_pool.ps1` | 成功。50,400実カード描画サンプル |
| `test_tank_submission_packaging.ps1` | 成功。元データ保持、初回履修状態、実行依存、ライセンス同梱、キャッシュ・履歴除外、改変検出等 |
| `test_developer_tools_profile.ps1` | 成功。5通りの設定、実行ファイル・構成情報のハッシュ。実行時UI trace引数は指定していない |
| 実Release配布コピー | 作成成功。266ファイル、67,384,537 bytes。削除素材7件の混入なし、現在のRelease EXEと一致。resources 246件のうち245件は元データと同一、既定projectのみ既存仕様どおり戦車用設定へ置換。制作用音声生成スクリプト1件は既存仕様で配布対象外 |
| TITLE / TANK_EXPEDITION | 新規配布コピーを別の作業ディレクトリから起動。`test_tank_submission_runtime.ps1` 成功：タイトル→初回訓練→系統選択→改造→修理→敵3種→ボス・結果→タイトル→新規遠征→履修済みスキップ。タイトル2回・遠征2回・エラー0。タイトルと作戦マップのキャプチャも目視確認 |
| README / docsリンク | Markdown 46件、ローカルリンク117件に欠落なし。現存素材台帳は247件（権利分類A 99、C 2、D 146）へ更新 |
| Third-party | 既存のライブラリ・フォント・LICENSE本文・copyright noticeは無変更。案内文に除去記録へのリンクを追加し、素材台帳と整合 |
| `git diff --check` | 成功 |

ログ・SHA256基準・削除前のファイル別参照行・検査スクリプト・画像はGit管理外の `generated/naval_removal/` に保存しています。実行テスト後の配布コピーは履修・キャッシュを含むため、そのまま提出・公開には使用しません。後半の戦闘は強制クリア、修理は境界条件用データを使う既存の自動テストです。難易度・聴感・全系統の手動プレイ評価ではありません。

## 制約・作者確認

既存素材の再配布条件は[素材監査](public-assets-audit.md)の保留事項を引き継ぎます。`sea.obj` 等の旧素材や保留したwater分岐を今後整理する場合は、動的選択・Graphics Lab・共通描画経路を別途確認してください。

.git・履歴・remote・Privateバックアップの変更、commit、pushは行っていません。過去コミットや既存ローカル生成物には旧実装が残ります。今回の除去は現在のPublic用作業ツリーが対象です。

作者による通常操作・音量・3系統それぞれのプレイ確認、Graphics Labの各水面モードの目視確認は残ります。Graphics Labの実装・素材はハッシュで同一と確認していますが、今回Labの全モードを実起動してはいません。VS2022のクリーン環境とGitHub Actionsも未実行です。
