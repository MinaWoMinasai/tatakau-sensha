# 資料案内

作品概要・ビルド・操作は [リポジトリの README](../README.md) から確認できます。この一覧は現在のローグライトゲームを読むための入口です。

## ゲームと制作

| 資料 | 内容 |
| --- | --- |
| [戦闘と作戦ルート](tank-action-and-route-guide.md) | 現在の操作、3系統、遠征中の成長 |
| [作戦マップの編集](tank-expedition-map-editor.md) | ルート・部屋・強化の制作 |
| [制作ツールの案内](tank-expedition-authoring-guide.md) | 調整・保存の手順 |
| [制作版と提出版](developer-tools-switch.md) | 開発 UI の切り替え、ビルド情報 |
| [提出用 Release](submission-package.md) | 実行用フォルダー作成と既存検査 |
| [ソースレビュー単元1](source-review-unit1/README.md) | 1-1〜1-5の判定、修正内容、実装根拠、提出用UML画像 |
| [ソースレビュー単元2](source-review-unit2/README.md) | 2-1〜2-9の判定、修正、回帰検証、残る採点上の懸念 |
| [ソースレビュー単元3](source-review-unit3/README.md) | 3-1〜3-3の判定、日本語コメント、Playerの用途別分離、回帰検証 |
| [ソースの読み方・コメント基準](code-comment-guide.md) | 型の責務、関数の契約、処理の理由と書式 |
| [GameSceneとPlayerの責務分離](gameplay-responsibilities.md) | 小さなシーンの入口、用途別の担当、共有状態と寿命 |
| [起動処理](startup-performance.md) | 起動計測・キャッシュ |
| [性能表示](tank-performance-monitor.md) | フレーム計測 |
| [共通音声テスト](audio-runtime-tests.md) | PCM/WAV・MP3と音声ハンドルの回帰検査 |
| [ネオン風車](neon-windmill.md) | iPhone絵文字の板ポリゴン、動画調査、軌道とHDRブルームの独立デモ |
| [ネオン風車：描画品質比較](neon-windmill-quality.md) | Legacy / Line Art / Hybrid、Core・Halo分離、実機画像・動画・GPU測定と発表用説明 |
| [画像の追加](images/README.md) | README に載せる実プレイ画像 |
| [作者情報](credits.md) | 作者・公式 URL、Version Info の調査 |
| [素材・埋め込み情報の監査](public-assets-audit.md) | 最新の A/B/C/D 分類、削除素材、権利・個人情報の保留事項と検証 |
| [保持素材の確認台帳](public-assets-inventory.md) | 現存素材の分類・サイズ・参照候補。出典照合の対象一覧 |
| [Ink Shooterの除去記録](remove-ink-shooter-audit.md) | 専用部分の除去と共通機能の保持、検証 |
| [Naval Prototypeの除去記録](remove-naval-prototype-audit.md) | 海戦専用部分の除去と水面描画・共用素材の保持、検証 |
| [旧テスト3シーンの除去記録](remove-legacy-test-scenes-audit.md) | Test / PlayerLab / Action3Dと専用部分の除去、共有素材・エンジン保持と検証 |
| [Graphics / Underwater / VFX Labの除去記録](remove-graphics-labs-audit.md) | 旧Labと専用素材の除去、Ocean等の汎用Engine保持、検証 |
| [初回の公開準備記録](public-release-audit.md) | 初回整理時の A/B/C 分類と検証。当時の素材保留事項は最新監査を優先 |

2026-09-30時点の全追跡ファイルの保持判断は[最終クリーンアップ監査](final-public-cleanup-audit.md)を参照してください。出典未確認の素材と汎用Engineの保留事項、音声・旧モード・配布検証をまとめています。

[旧GAME / TANK_RUN起動口の廃止監査](retire-legacy-run-entrypoints-audit.md)に、登録・専用設定/ランチャーの除去と、遠征・タイトルデモが使う共有実装/単体テストの保持を記録しています。旧prototype資料の起動方法は当時の記録です。

## 実装・検証の案内

- [ゲームシナリオ検証](repository-engineering-overhaul/README.md)：実ゲームを使った再現、設定、テスト一覧。
- [3Dネオンボスの接続](neon-boss-gameplay-integration.md)：戦闘と描画の担当、死亡演出と資源の寿命。
- [Depthボス](neon-boss-depth/README.md)：攻撃・カメラ・制作版での確認手順。

## Gitで共有する資料とツール

`docs/`には遊び方、ビルド・配布手順、設計、素材の出典と権利、ソースを読む人向けの資料を置きます。`project/tools/`には依存準備、素材生成、編集、回帰テストなど、別の開発環境でも使うツールを残します。拡張子だけでMarkdownやPythonを一括除外しません。

作業途中の報告、進捗表、古い計画、個別調査の補助スクリプトはローカル用です。既存の対象はルートの`.gitignore`で個別指定し、内容と元の配置をローカルに保持しています。新しい一時資料は`docs/local/`、一時ツールは`project/tools/local/`へ置いてください。ログ・画像・ビルド成果物は引き続き`generated/`へ保存します。

共有したくなった資料はローカル専用フォルダーの外へ移し、必要な説明や再現手順を整えて追加します。個別指定された既存ファイルを共有へ戻す場合は、対応する`.gitignore`の行も削除します。

`.gitignore`はすでに追跡しているファイルには効きません。今回の対象は`git rm --cached`で追跡から外し、実ファイルは残します。GitHubへの反映には、この削除と除外設定のコミット・pushが必要です。過去のコミットの履歴は変更しません。

Git管理に除外対象が残っていないかは次のコマンドで確認できます。同じ確認をGitHub Actionsの`CheckUnwantedFiles`でも行います。

```powershell
git ls-files --cached --ignored --exclude-standard
```

出力が空なら、現在の除外設定に該当する追跡ファイルはありません。
