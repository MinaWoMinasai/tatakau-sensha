# Public 公開準備の調査・変更記録

調査日：2026-09-29。対象は作業開始時点の追跡ファイル 954 件と現在の作業ツリーです。開始時のブランチは `chore/public-release-prep`、既存の作業差分はありませんでした。

**ゲーム本体を保持したまま、作品紹介・文書・ライセンス同梱を整理しました。素材の権利・埋め込み情報など、公開前に作者が確認する項目が残っています。**

## 作業範囲

- `.git` 配下へ書き込む操作、履歴変更、ブランチ作成・切り替え、commit、push、remote 設定変更は行っていません。Git は状態・追跡一覧・差分の読み取りだけに使用しました。
- 別途保存された Private リポジトリやバックアップは参照・変更していません。
- **削除ファイルはありません。** ソース、resources、既存の第三者 LICENSE を保持しました。
- 起動時の動作、シーン登録、ビルド構成、ゲームパラメーターは変更していません。Windows Version Info / Credits の新規実装は提案に留めています。

## A：Public 版に残すもの

| 範囲 | 根拠 |
| --- | --- |
| `project/main.cpp`, `CG2.sln`, `CG2_testPro.vcxproj`, `.filters` | 起動、プロジェクト参照、3構成のビルドに必要 |
| `project/DirectX/engine/` | DirectX 12 描画、入力、音声、シーン・リソース管理をゲームが使用 |
| `project/game/scene/TitleScene.*`, `GameScene.*` | タイトルから `TANK_EXPEDITION` へ進む。遠征・通常モードが同じ GameScene を共有 |
| `project/game/run/`, `player/`, `enemy/`, `exp/`, `collision/`, `ui/`, `effects/` | 遠征、強化、プレイヤー・敵・衝突・表示の実装 |
| `project/game/runtime/`, `modules/`, `editor/` とその他の参照済みソース | 登録・起動・制作機能の依存。開発機能をOFFにすることとコードを削除することは別 |
| `project/resources/configs/`, `maps/`, `projects/`, `shaders/` | 設定・ルート・部屋・実行時 HLSL。`.obj` もモデル素材のため一括除外しない |
| `project/resources/audio/tank_expedition/`, `fonts/` と参照済み素材 | 専用合成音、表示フォントと OFL。その他の素材は出典確認が終わるまで C として保持 |
| `project/externals/` と既存著作権表示 | Assimp / DirectXTex / ImGui / nlohmann がビルドに必要。Releaseでも全てのライブラリが不要になるわけではない |
| `project/tools/bootstrap_dependencies.ps1`, ビルド・戦車テスト・配布ツール | 依存生成、再現・回帰検証、ライセンスを含む配布物作成 |
| `.github/workflows/`, `.gitattributes`, `.gitignore` | 既存 CI とテキスト・生成物管理。workflows は調査のみで変更していない |
| ルート README、著作権・Third-party 文書、`docs/` の現行ガイド | 初見の閲覧者への説明と出典・利用条件 |

ローカルの Assimp `.lib` / `.dll` はビルドに必要です。Git 管理には含めず、依存準備スクリプトから生成する現行方式を保持します。

## B：削除候補として提示したもの（今回は保持）

| 候補 | 影響・理由 |
| --- | --- |
| `generated/outputs/`, `generated/obj/` 等のビルド成果物 | ソースから再生成可能。削除後は再ビルドが必要。既に Git 管理対象外 |
| `project/.vs/`, ImGui 設定、各種ログ・ダンプ、生成キャッシュ | 実行や IDE が作るローカル情報。公開不要。診断履歴を残すため物理削除しない |
| `project/resources/audio/tank_expedition/preview.wav` | README が試聴用と明記。実ゲームの `TankExpeditionAudio.h` の音声一覧には含まれない。2.75 MiB。生成スクリプト・測定記録・試聴手順からは参照されるので、削除するなら説明も合わせて整理する |

再生成可能な生成物と、素材・検証記録を一括して消すことはしていません。上の候補は作業中に理由とともに提示し、実削除は行っていません。

## C：作者の確認が必要なもの（保持）

| 対象 | 保留理由 / 次の確認 |
| --- | --- |
| `project/game/ink/`, `InkShooterScene.*`, `game/naval/`, 3D / 各 Lab シーン | `BuiltInGameModule.cpp` と `.vcxproj` で参照。旧機能でも現在のビルド対象。除去する場合はシーン登録・共有ロード・素材・全構成の動作検証が必要 |
| 旧アリーナ用部分、`GameScene.TankRun.cpp` など | 遠征とプレイヤー・UI・タイトルデモを共有。ファイル名だけで未使用と判定できない |
| `docs/ink_*.md`, `docs/naval_game/`, 旧プロトタイプ・検証資料 | 現行作品紹介からは切り分けたが、旧実装の根拠や出典を含むため保持。資料索引で過去資料と明記 |
| `docs/public_submission/REPOSITORY_SPLIT_PLAN.md` | 学校課題向けの旧計画。今回と異なる方針のため、実施しない旨を冒頭に追記 |
| `project/tools/player_ship_editor_mobile.zip` | 現在の展開済みファイルと比較し、改行差を除いてもアプリ・HTML等が一致しない。単なる重複として消せない。既存 CI の `*.zip` 禁止に該当。内部の `player_ship_editor/README_mobile.md` にも個人用パス形式の情報あり |
| `project/resources/BGM_shining_star.mp3` | 使用行はコメントアウト。ただし著作権・出典・Public での素材収録条件が未確認。5.27 MiB |
| `project/resources/Player_Mixamo.fbx` | 旧シーン用の素材。出典・再配布条件に加え、個人用の絶対パス形式の埋め込み情報あり。内容は報告に転載しない |
| `project/resources/models/player/`, `models/human/`, `animation/` | VRoid / Mixamo / サンプル素材の権利確認。GLB 最大約11.33 MiB。旧シーンからの参照があるため保持 |
| 海・砂・水・海中テクスチャ、その他旧画像・音声 | 実験シーンと共通初期化からの読み込みを確認する必要がある。未使用・自作と推測しない |
| Assimp の追加由来コード、Hedley、SDK / DXC | 本文の不足・使用版との照合が残る。詳細は [Third-party 調査](../THIRD_PARTY_NOTICES.md) |

## 変更した文書・公開設定

- README を「たたかうせんしゃ」の概要、特徴、3系統、遠征中の成長、実装どおりの操作、環境、ビルド・起動、構造、画像追加場所、作者・権利へ再構成。
- 永続強化があると誤認させず、通貨・強化は遠征ごとであることを明記。旧資料の Shift ダッシュ記載を現在の右クリック操作に修正。
- `COPYRIGHT.md` に指定の著作権表示、公開目的、無断再利用に関する方針、法令・GitHub規約・第三者ライセンスの例外を記載。
- `THIRD_PARTY_NOTICES.md` と `docs/third-party/` を追加。ライセンスはプロジェクト全体に新規適用せず、対象ライブラリを限定。
- `docs/README.md` で現行資料と過去の実験資料を区別。`docs/images/README.md` に実プレイ画像の配置案を記載。画像自体は未追加。
- 11件の既存資料にあった個人PC向け絶対パスを、リポジトリ相対リンクまたは「ローカル参考資料・非同梱」の表記へ変更。過去の生成物への参照はローカル検証記録として保持。
- `.gitignore` にローカルエージェント設定、環境変数ファイル・秘密鍵形式、チュートリアル履修記録の派生ファイルを追加。既に追跡されているファイルを解除する操作は行っていない。
- 配布ツールに著作権・Third-party文書と補完本文の同梱を追加。欠落時は出力前に失敗する。既存のライセンス・フォント保持は維持。

## セキュリティ・大容量ファイル

追跡ファイル、可視の未追跡ファイル、旧ZIPの展開内容について、代表的な APIキー・GitHub / サービストークン・秘密鍵・パスワード代入・個人用パス・メール形式を検査しました。判定はファイル名と種別だけを出力し、秘密情報の候補値をログや文書に出していません。

- 検査した代表的な秘密情報パターンに一致するキー・トークン・秘密鍵・パスワード代入は見つかりませんでした。
- 個人用パス：上記11資料は修正。`project/resources/Player_Mixamo.fbx` の埋め込み情報と `project/tools/player_ship_editor_mobile.zip` 内の `player_ship_editor/README_mobile.md` は保持し、要確認としました。
- `project/resources/models/player/testModel.glb` と `testModel_animated.glb` ではメール形式に各1件一致しましたが、GLB構造を解析して、どちらも埋め込み画像のバイナリ領域内であることを確認しました。テキストの連絡先が保存されているという根拠にはならないため、個人情報の検出とは扱っていません。モデル・画像の権利確認は別途必要です。
- vendor の `imgui.cpp` にもパス形式の一致がありますが、第三者ソースのコメント内です。`docs/third-party/zlib-LICENSE.txt` のメール表記も原文の権利者表示です。これらを作者の秘密情報として削除せず、原文を保持しました。
- `.vcxproj.user`、`imgui.ini`、`.vs/`、履修記録、`.deps/`、`vcpkg_installed/`、ビルド・キャッシュはローカルに存在しますが、追跡対象外です。開始時の追跡一覧には `.exe` / `.dll` / `.lib` / `.pdb` 等はありませんでした。
- 最大の追跡ファイルは約11.33 MiB。50 MiBを超えるファイルはありません。大きなモデル・音楽・海のメッシュも、権利・依存が未確認のためサイズだけで削除していません。

この検査は秘密情報の不存在を保証するものではありません。Git履歴・別ブランチ・Privateバックアップは対象外です。ignored な大量の生成物は一覧と一部設定を確認した範囲で、全キャッシュ・全バイナリ・圧縮テクスチャを意味解析した監査ではありません。フォルダー丸ごとを手動アップロードせず、公開対象は差分と追跡一覧で確認してください。

## ビルド・テスト・最終確認

ローカル：Visual Studio 2026、v145、Windows SDK 10.0.26100.0、既存の Assimp v145 バイナリを使用。依存バイナリの再生成は行っていません。

| 検証 | 結果 |
| --- | --- |
| Release / x64、開発機能OFF | ビルド成功 |
| Development / x64、開発機能ON | ビルド成功 |
| Debug / x64 ソリューション構成 | ビルド成功。実体は Development。独立した Debug プロジェクトの実行確認ではない |
| `test_tank_expedition_map.ps1` | 成功。C++17 / 20、各2048 seed、全経路条件、導入経済、JSON保存等 |
| `test_tank_reward_pool.ps1` | 成功。実カード描画構築の50,400サンプル |
| `test_tank_submission_packaging.ps1` | 成功。従来検査に加え、追加文書の同一性と COPYRIGHT 欠落時の拒否を確認 |
| `test_developer_tools_profile.ps1` | 成功。5通りの設定と Release / Development の EXE・ビルド情報のハッシュ一致。実行時trace引数は指定していない |
| 実 Release からの隔離パッケージ作成 | 成功。310ファイル、約87.8 MiB、初期履修記録なし。公開用の権利確認済み成果物を意味しない |
| `test_tank_submission_runtime.ps1` による隔離パッケージの起動・遷移 | 成功。タイトル→初回訓練→系統選択→改造→修理→敵3種→ボス・結果→タイトル→新規遠征→履修済みスキップ。後半の戦闘は強制クリア等を用いるため、難易度テストではない |
| Git差分・ファイル保持・主要資料リンク | `git diff --check` 成功。開始時の全追跡ファイルが存在し、ゲームソース・resources・externals・workflows の SHA256 は変更なし。README 等8件の主要資料のローカルリンク切れなし（未追加画像のコメントを除く） |
| 既存 `CheckUnwantedFiles` と同じ条件のローカル検査 | 旧エディターZIP 1件を検出。要確認のため保持し、検査条件も緩めていない |

最初の Release ビルドはサンドボックスによる MSBuild FileTracker のアクセス拒否で失敗しました。通常権限で再実行して成功しています。Development には既存のリンク最適化に関するメッセージ、各構成に DirectInput の既定バージョンの通知があります。

VS 2022 / v143 のクリーン環境、GitHub Actions、第三者PCでの手動プレイ、全ての旧実験シーンは今回検証していません。CI は master への push で3構成をビルドする既存設定で、不要ファイル検査のみ手動実行にも対応します。GitHubへの操作は行っていません。

## 公開前に作者が決めること

1. C の旧音楽・モデル・画像それぞれの入手元と、素材をPublicリポジトリにそのまま置ける条件を確認する。
2. FBX の埋め込みパスと旧ZIP内の個人用パスを確認し、必要なら制作元から情報を除いて再エクスポート・再構成する。今回の作業では素材・ZIPを直接改変していない。
3. 内容の異なる旧モバイルエディターZIPを公開から除くか、ソースとして整理して残すか決める。不要ファイル検査の既存失敗を解消する。
4. Third-party の未確認本文・SDK/DXC の配布条件を使用版に合わせて確認する。
5. 作品画像を追加し、必要なら [作者表示の案](credits.md) に沿って Version Info / タイトル下部の表示を実装する。
6. 過去履歴の公開範囲とコミット作者情報は、所有者側で別途確認する。今回、履歴の監査・変更は行っていない。
