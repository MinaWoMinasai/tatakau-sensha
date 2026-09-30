# Ink Shooter Lab の除去記録

> Ink Shooter整理時点の記録です。この後、[Naval Prototypeの専用実装・資料・素材を除去](remove-naval-prototype-audit.md)しました。以下の「海戦を保持」やファイル件数は当時の結果です。

2026-09-29。開始ブランチは `refactor/remove-ink-shooter`、作業開始時の追跡ファイルは953件、既存差分なし。削除前に全追跡ファイルからファイル名・include・シーンID・namespace・環境変数・設定・リソースパスを検索し、参照を分類しました。専用81ファイルを除去し、共通音声テスト2ファイルは汎用名へ移動しました。

## 削除前の分類

| 分類 | 対象 | 判断根拠・対応 |
| --- | --- | --- |
| A：専用なので削除 | `project/game/ink/` 22件 | シミュレーション、武器、インク描画・レティクル、イベント音声。参照元は専用シーン・専用テスト・プロジェクト登録。戦車ゲームからのincludeや呼び出しなし |
| A：専用なので削除 | `project/game/scene/InkShooterScene.*` 10件 | 本体・Debug・Capture・Visuals・Fidelity・Weapons・WeaponVisuals・WeaponsValidation・FeelValidationとヘッダー。外部接続はBuiltInGameModuleの登録のみ |
| A：専用なので削除 | 専用project JSON 1件、config 2件、shader 8件、音声9件とmanifest | 専用シーンとrenderer / AudioDirectorだけが明示読み込み。シェーダーのinclude・音声の名前配列・生成スクリプトも照合。共通AudioManagerの列挙は音声を選べる一覧であり、削除対象を必須ロードする処理ではない |
| A：専用なので削除 | 専用ツール・テスト11件 | シミュレーション・発射パターン・武器・レティクル・移動・音声イベントのテスト、生成・起動スクリプト。Capture / Fidelity / Weapons / Feel の起動は専用ランチャーとシーン内に集約されている |
| A：専用資料を除去 | `docs/ink_*.md` 17件 | 研究・実装資料を削除。ただし音声ランタイムの汎用部分は新しい共通音声テストの説明へ整理して残す |
| B：共通テストとして残す | `project/tools/ink_audio_runtime_tests.cpp`、`test_ink_audio_runtime.ps1` | Inkの型を使わず共通 `Audio.cpp` を検証。`audio_runtime_tests.cpp` / `test_audio_runtime.ps1` へ改名し、テスト内容を維持 |
| B：共用機能を残す | Input、WinApp、Audio、Object3dCommon、TextureManager、ModelManager、DirectXCommon、DirectXTex、描画基盤・共通shader | TITLE / GameScene / 戦車UI / TankExpeditionAudio等が利用。追加時期・名前で削除しない。Audioの古い説明コメントのみ汎用表現に変更 |
| B：共用機能を残す | RuntimeProfiler | 戦車・他シーンのCPU/GPU計測・CSV・F1/Shift+F1操作を保持。InkだけのF1競合回避分岐と、そのための引数だけ除去 |
| B：共通素材を残す | `white512x512.png`、NeonGridRenderer、box/sphere生成API | TitleScene、戦車UI・演出、GraphicsLab等の使用を確認。Inkのモデルは実行時に専用名で生成されるため、専用OBJ・画像ファイルの削除はない |
| C：用途判断を保留して残す | `Object3dCommon::SetDebugUiEnabled` / `GetDebugUiEnabled` | シーン側の呼び出しはInkに限られるが、汎用描画クラスのUI制御API。Ink除去を理由にAPI・状態管理まで削らない |
| C：今回の対象外として保持 | 既存の出典未確認モデル・画像・音声、他Lab・海戦・3Dシーン、ローカル生成物 | Ink専用と確認できたもの以外は削除しない。素材の権利確認は[素材監査](public-assets-audit.md)の保留事項を引き継ぐ |

候補83件のうち、Aは81件、Bの改名対象は2件です。分類と一覧を提示し、各ファイルのハッシュが調査開始時と一致することを確認してから削除しました。共通テストは改名先へ保存してから旧パスを除き、テスト内容を比較しました。各パスと参照調査の内訳は末尾に記載しています。

## 共通ファイルの変更範囲

- `BuiltInGameModule.cpp`：Inkのincludeとシーン登録の2行だけ除去。TITLE / GAME / TANK_EXPEDITION等は保持。
- `TitleScene.cpp`：現在のコードにはInk / F8導線なし。変更していません。Enter・Space・クリック、開発用F9、タイトル背景デモを保持。
- `Game.cpp` / `RuntimeProfiler.*`：Ink判定と専用ショートカット分岐を除き、他シーンで従来使われていた処理をそのまま残す。
- `CG2_testPro.vcxproj` / `.filters`：Ink専用ItemGroupを除去し、XMLとファイル参照を検証済み。汎用機能のItemGroupは保持。
- `.github/workflows`：Ink専用処理なし。変更していません。
- README・資料索引・監査・素材台帳：現在もInkを含むような記載、専用ショートカット説明、削除ファイルへの導線を整理しました。過去の監査結果は実施時点を明示しています。
- 第三者ライブラリ・ライセンス本文・著作権表示：削除・改変なし。Ink専用の第三者依存として除去できるライブラリは見つかりませんでした。Third-party一覧には現在の素材台帳への案内を追加しています。

## 検証結果

ローカルのVisual Studio 2026 / v145、Windows SDK 10.0.26100.0、既存Assimpを使用しました。依存の再生成は行っていません。

| 検査 | 結果 |
| --- | --- |
| 削除一覧・差分 | 専用81件、766,341 bytesを削除。別途2件は共通テストへ改名。想定外の削除・追加・コード変更なし |
| 保持ファイル | 戦車・エンジン・resources・vendor・ライセンス・workflows等の保護対象740ファイルをSHA256で照合し無変更。TitleScene、GameScene群、player / run / editor / UI等、残存resources254件を含む |
| Audioと共通音声テスト | `Audio.cpp` は説明コメント1行だけ変更。テストC++は実行スクリプト名のコメント以外が同一。ランチャーはファイル名・成果物名・変数名を汎用化 |
| シーン登録 | Ink以外の登録行は変更前と同一。TITLE / GAME / TANK_EXPEDITIONを保持 |
| 削除後の参照検索 | コード・設定・プロジェクト・シェーダー・ツール・Actionsに削除ファイル名／Ink専用シンボルの参照なし。監査文書には削除記録として旧パスを保持 |
| XML / ファイル参照 | vcxprojから52項目、filtersから38項目を除去。XML正常。残存ファイル項目302件／284件に欠落なし。その他の登録項目は同一 |
| Release / x64、開発機能OFF | ビルド成功 |
| Development / x64、開発機能ON | ビルド成功。既存DirectInput既定値・リンク最適化の通知あり |
| ビルド成果物 | 両構成のEXEに除去したシーンIDの文字列なし |
| `test_tank_expedition_map.ps1` | 成功。C++17 / C++20、各2048 seed、全経路条件・導入経済・保存等 |
| `test_tank_reward_pool.ps1` | 成功。50,400実カード描画サンプル |
| `test_tank_submission_packaging.ps1` | 成功。素材・ライセンス保持、履修・キャッシュの除外、マニフェスト、改変検出等 |
| `test_developer_tools_profile.ps1` | 成功。5通りの設定、Release / DevelopmentのEXEと構成情報のハッシュ。実行時trace引数は指定していない |
| `test_audio_runtime.ps1` | 成功。CPU、ネイティブ無音再生、音声世代・Pause/Resume・上限、MP3互換経路まで実行。SKIPなし |
| 実Release配布コピー | 作成成功。273ファイル、67,471,942 bytes。Ink専用素材の混入なし。残存未確認素材の権利処理を保証するものではない |
| TITLE / TANK_EXPEDITION | 隔離配布コピーを別作業ディレクトリから起動。`test_tank_submission_runtime.ps1` 成功：タイトル→初回訓練→系統選択→改造→修理→敵3種→ボス・結果→タイトル→新規遠征→履修済みスキップ。タイトル2回・遠征2回・エラー0。タイトルと作戦マップのキャプチャも目視確認 |
| README / docsのリンク | 新規文書を含む47件のMarkdown、ローカルリンク108件を確認し欠落なし。以前から非同梱だった生成物へのリンク2件は、記録を消さずローカルパス表記へ変更 |
| Third-party | 既存本文・ヘッダー表示・フォントを保持。共通ライブラリの一覧と現存素材台帳の整合を確認 |
| `git diff --check` | 成功 |

ログ・検査用データはGit管理外の `generated/ink_removal/` にあります。実行テスト後の配布コピーには履修・キャッシュができるため、そのまま公開用には使用しません。後半の戦闘は強制クリア、修理は境界条件用データを使う既存自動テストであり、難易度・聴感・全操作の手動評価ではありません。

## 制約・作者確認

.git・履歴・remote・Privateバックアップの変更、commit、pushは行っていません。削除した実装・素材は過去コミットや既存ローカル生成物には残ります。公開履歴の範囲と既存素材の再配布許諾は作者側の確認事項です。

汎用UI制御APIは保留して保持しています。VS2022のクリーン環境、GitHub Actions、他の旧シーンすべての手動プレイは今回未検証です。公開前には作者の手元でも通常操作・音量・3系統の操作感を確認してください。

## 削除前のファイル別参照一覧

ファイル名での参照検索に加え、上記の実際のロード・登録・呼び出しを確認しました。表の「内部」は今回の候補83件内、「外部」はそれ以外のソース・設定・プロジェクト・資料です。同名ファイルへの文字列一致は依存確定とは扱いません。

| 対象 | 分類 | 内部参照ファイル数 | 外部参照ファイル |
| --- | --- | ---: | --- |
| `docs/ink_phase4_implementation.md` | A | 1 | なし |
| `docs/ink_phase4_reticle_research.md` | A | 3 | なし |
| `docs/ink_phase4_stringer_research.md` | A | 2 | なし |
| `docs/ink_phase4_stringer_test_notes.md` | A | 2 | なし |
| `docs/ink_phase4_weapon_architecture.md` | A | 1 | なし |
| `docs/ink_phase5_audio_research.md` | A | 1 | なし |
| `docs/ink_phase5_audio_runtime.md` | A | 1 | なし |
| `docs/ink_phase5_implementation.md` | A | 0 | なし |
| `docs/ink_phase5_movement.md` | A | 1 | なし |
| `docs/ink_shooter_implementation.md` | A | 1 | なし |
| `docs/ink_shooter_phase2_implementation.md` | A | 0 | なし |
| `docs/ink_shooter_phase2_research.md` | A | 2 | なし |
| `docs/ink_shooter_phase3_implementation.md` | A | 0 | なし |
| `docs/ink_shooter_phase3_movement_research.md` | A | 1 | なし |
| `docs/ink_shooter_phase3_paint_research.md` | A | 1 | なし |
| `docs/ink_shooter_phase3_shooting_research.md` | A | 2 | なし |
| `docs/ink_shooter_research.md` | A | 4 | なし |
| `project/game/ink/InkAudioDirector.cpp` | A | 0 | `docs/public-assets-inventory.md`, `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/game/ink/InkAudioDirector.h` | A | 2 | `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/game/ink/InkEmissionPattern.h` | A | 4 | `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/game/ink/InkLiquidRenderer.cpp` | A | 1 | `docs/public-assets-inventory.md`, `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/game/ink/InkLiquidRenderer.h` | A | 3 | `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/game/ink/InkPaintRenderer.cpp` | A | 1 | `docs/public-assets-inventory.md`, `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/game/ink/InkPaintRenderer.h` | A | 4 | `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/game/ink/InkReticleMath.h` | A | 3 | `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/game/ink/InkReticleRenderer.cpp` | A | 1 | `docs/public-assets-inventory.md`, `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/game/ink/InkReticleRenderer.h` | A | 4 | `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/game/ink/InkSimulation.Stringer.cpp` | A | 5 | `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/game/ink/InkSimulation.cpp` | A | 10 | `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/game/ink/InkSimulation.h` | A | 10 | `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/game/ink/InkSpreadPattern.h` | A | 4 | `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/game/ink/InkStringerPattern.h` | A | 3 | `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/game/ink/InkTypes.h` | A | 6 | `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/game/ink/InkWeaponFields.h` | A | 2 | `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/game/ink/ShooterWeaponParams.h` | A | 6 | `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/game/ink/StringerWeaponParams.h` | A | 3 | `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/game/ink/WeaponCatalog.cpp` | A | 3 | `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/game/ink/WeaponCatalog.h` | A | 4 | `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/game/ink/WeaponDefinition.h` | A | 3 | `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/game/scene/InkShooterScene.Capture.cpp` | A | 1 | `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/game/scene/InkShooterScene.Debug.cpp` | A | 1 | `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/game/scene/InkShooterScene.FeelValidation.cpp` | A | 0 | `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/game/scene/InkShooterScene.Fidelity.cpp` | A | 1 | `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/game/scene/InkShooterScene.Visuals.cpp` | A | 2 | `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/game/scene/InkShooterScene.WeaponVisuals.cpp` | A | 1 | `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/game/scene/InkShooterScene.Weapons.cpp` | A | 1 | `docs/public-assets-inventory.md`, `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/game/scene/InkShooterScene.WeaponsValidation.cpp` | A | 0 | `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/game/scene/InkShooterScene.cpp` | A | 1 | `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/game/scene/InkShooterScene.h` | A | 11 | `docs/public-assets-inventory.md`, `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters`, `project/game/modules/BuiltInGameModule.cpp` |
| `project/resources/audio/ink/arrow_burst.wav` | A | 3 | `docs/public-assets-inventory.md`, `project/CG2_testPro.vcxproj` |
| `project/resources/audio/ink/arrow_stick.wav` | A | 3 | `docs/public-assets-inventory.md`, `project/CG2_testPro.vcxproj` |
| `project/resources/audio/ink/charge_first.wav` | A | 3 | `docs/public-assets-inventory.md`, `project/CG2_testPro.vcxproj` |
| `project/resources/audio/ink/charge_full.wav` | A | 3 | `docs/public-assets-inventory.md`, `project/CG2_testPro.vcxproj` |
| `project/resources/audio/ink/charge_loop.wav` | A | 3 | `docs/public-assets-inventory.md`, `project/CG2_testPro.vcxproj` |
| `project/resources/audio/ink/manifest.json` | A | 3 | `docs/public-assets-inventory.md`, `project/CG2_testPro.vcxproj` |
| `project/resources/audio/ink/shooter_shot.wav` | A | 3 | `docs/public-assets-inventory.md`, `project/CG2_testPro.vcxproj` |
| `project/resources/audio/ink/stringer_full.wav` | A | 3 | `docs/public-assets-inventory.md`, `project/CG2_testPro.vcxproj` |
| `project/resources/audio/ink/stringer_mid.wav` | A | 3 | `docs/public-assets-inventory.md`, `project/CG2_testPro.vcxproj` |
| `project/resources/audio/ink/stringer_tap.wav` | A | 3 | `docs/public-assets-inventory.md`, `project/CG2_testPro.vcxproj` |
| `project/resources/configs/ink_audio.json` | A | 1 | `docs/public-assets-inventory.md`, `project/CG2_testPro.vcxproj` |
| `project/resources/configs/ink_weapons.json` | A | 4 | `docs/public-assets-inventory.md`, `project/CG2_testPro.vcxproj` |
| `project/resources/projects/ink_shooter.project.json` | A | 2 | `docs/public-assets-inventory.md`, `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/resources/shaders/InkLiquid.PS.hlsl` | A | 2 | `docs/public-assets-inventory.md` |
| `project/resources/shaders/InkLiquid.VS.hlsl` | A | 2 | `docs/public-assets-inventory.md` |
| `project/resources/shaders/InkPaint.CS.hlsl` | A | 3 | `docs/public-assets-inventory.md` |
| `project/resources/shaders/InkPaint.PS.hlsl` | A | 3 | `docs/public-assets-inventory.md` |
| `project/resources/shaders/InkPaint.VS.hlsl` | A | 3 | `docs/public-assets-inventory.md` |
| `project/resources/shaders/InkReticle.PS.hlsl` | A | 1 | `docs/public-assets-inventory.md` |
| `project/resources/shaders/InkReticle.VS.hlsl` | A | 1 | `docs/public-assets-inventory.md` |
| `project/resources/shaders/InkReticle.hlsli` | A | 2 | `docs/public-assets-inventory.md`, `project/CG2_testPro.vcxproj` |
| `project/tools/generate_ink_audio.py` | A | 3 | `docs/public-assets-inventory.md` |
| `project/tools/ink_audio_event_tests.cpp` | A | 1 | なし |
| `project/tools/ink_audio_runtime_tests.cpp` | B（共通名へ移動） | 2 | なし |
| `project/tools/ink_emission_pattern_tests.cpp` | A | 2 | なし |
| `project/tools/ink_movement_carry_tests.cpp` | A | 2 | なし |
| `project/tools/ink_reticle_tests.cpp` | A | 2 | なし |
| `project/tools/ink_simulation_tests.cpp` | A | 4 | `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/tools/ink_stringer_pattern_tests.cpp` | A | 2 | なし |
| `project/tools/ink_stringer_simulation_tests.cpp` | A | 3 | なし |
| `project/tools/ink_weapon_catalog_tests.cpp` | A | 2 | なし |
| `project/tools/run_ink_shooter.ps1` | A | 5 | `docs/public-assets-inventory.md`, `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
| `project/tools/test_ink_audio_runtime.ps1` | B（共通名へ移動） | 3 | `docs/public-assets-inventory.md` |
| `project/tools/test_ink_simulation.ps1` | A | 5 | `project/CG2_testPro.vcxproj`, `project/CG2_testPro.vcxproj.filters` |
