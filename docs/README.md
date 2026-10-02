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
| [起動処理](startup-performance.md) | 起動計測・キャッシュ |
| [性能表示](tank-performance-monitor.md) | フレーム計測 |
| [共通音声テスト](audio-runtime-tests.md) | PCM/WAV・MP3と音声ハンドルの回帰検査 |
| [画像の追加](images/README.md) | README に載せる実プレイ画像 |
| [作者情報](credits.md) | 作者・公式 URL、Version Info の調査 |
| [素材・埋め込み情報の監査](public-assets-audit.md) | 最新の A/B/C/D 分類、削除素材、権利・個人情報の保留事項と検証 |
| [保持素材の確認台帳](public-assets-inventory.md) | 現存素材の分類・サイズ・参照候補。出典照合の対象一覧 |
| [Ink Shooterの除去記録](remove-ink-shooter-audit.md) | 専用部分の除去と共通機能の保持、検証 |
| [Naval Prototypeの除去記録](remove-naval-prototype-audit.md) | 海戦専用部分の除去と水面描画・共用素材の保持、検証 |
| [旧テスト3シーンの除去記録](remove-legacy-test-scenes-audit.md) | Test / PlayerLab / Action3Dと専用部分の除去、共有素材・エンジン保持と検証 |
| [Graphics / Underwater / VFX Labの除去記録](remove-graphics-labs-audit.md) | 旧Labと専用素材の除去、Ocean等の汎用Engine保持、検証 |
| [初回の公開準備記録](public-release-audit.md) | 初回整理時の A/B/C 分類と検証。当時の素材保留事項は最新監査を優先 |

2026-09-30時点の全追跡ファイルの保持判断は[最終クリーンアップ監査](final-public-cleanup-audit.md)と[818件の分類表](final-public-cleanup-files.tsv)を参照してください。出典未確認の素材と汎用Engineの保留事項、音声・旧モード・配布検証をまとめています。

[旧GAME / TANK_RUN起動口の廃止監査](retire-legacy-run-entrypoints-audit.md)に、登録・専用設定/ランチャーの除去と、遠征・タイトルデモが使う共有実装/単体テストの保持を記録しています。旧prototype資料の起動方法は当時の記録です。

## 過去の実装・検証資料

以下は開発経緯を残した資料です。記載された試験結果・キー割り当て・数値は、その資料作成時点のものです。現在の仕様と異なる場合はコード・設定と上の現行ガイドを優先してください。

- `tank-*-prototype.md`、`tank-mode-comparison-*.md`、`tank-build-variety-v5.md` など：以前の遠征・アリーナ仕様と検証記録。
- [旧 Public 分離計画](public_submission/REPOSITORY_SPLIT_PLAN.md)：学校課題向けの過去案。今回の「たたかうせんしゃ」公開方針とは異なり、実施対象ではありません。

古い資料内の `generated/` への参照はローカル検証成果物です。GitHub には画像・ログ本体を含めていません。これらの記録だけで現在の全構成の動作確認済みとは扱いません。
