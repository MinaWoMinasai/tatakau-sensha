# 提出版と制作版の切り替え

Visual Studio上部の構成選択だけで切り替えられます。

| 構成 | 標準設定 | 用途 |
|---|---|---|
| Release | 開発機能OFF | 提出・配布 |
| Development | 開発機能ON | F1性能表示、F2～F6制作ツール、ImGuiで調整 |
| Debug | 開発機能ON | 現在のソリューションでは Development へ割り当て |

Development / x64では、タイトル画面または遠征中に **F7** を押すと、チュートリアルや道中を省略して制作済みのボス部屋へ直接移動できます。ボス戦中のF7で再生成、自機死亡後のF7で再出撃してボス戦から開始します。F3制作ツールの「F7 ボス戦へ移動 / 再生成」からも利用できます。

制作確認用に自機は無敵になります。遠征中に使うと現在のルートは確認用の1部屋へ置き換わり、現在の機体・強化を引き継ぎます。タイトルからは初期機体、自機死亡後の再出撃では新しい機体で開始します。マップ・部屋の制作JSONは変更しません。通常の開始・再出撃は従来どおりで、Releaseにはこのショートカットを含めません。

2026-10-06の追加時にDevelopment / Releaseビルドと既存の`test_neon_boss_runtime.ps1`が成功。Releaseは開発機能OFFで、ボス直行の案内文字も実行ファイルに含まれないことを確認しました。F7の実キー操作による目視確認は未実施です。検証ログは`generated/boss-shortcut/`に保存しています。

`CG2.sln` の Debug は、ゲームと DirectXTex の両方で Development を参照します。`.vcxproj` 単体の Debug 定義とは区別してください。公開準備では既存の構成割り当てを変更していません。

切り替え後はビルドしてください。実行中のキー操作や配布先JSONでは開発機能を復活できません。

## 同じ構成のまま変更する場合

MSBuildの `CG2DeveloperTools` プロパティを指定します。

```powershell
# 提出用Release（標準値）
MSBuild.exe project/CG2.sln /p:Configuration=Release /p:Platform=x64 /p:CG2DeveloperTools=false
# 制作用Release：従来のReleaseエディター・性能表示が必要な場合
MSBuild.exe project/CG2.sln /p:Configuration=Release /p:Platform=x64 /p:CG2DeveloperTools=true
# DevelopmentでもUIを無効化できる
MSBuild.exe project/CG2.sln /p:Configuration=Development /p:Platform=x64 /p:CG2DeveloperTools=false
```

提出前は必ずRelease／falseで再ビルドしてください。同じ構成の出力先は上書きされます。

## OFFの範囲

- ImGuiのコンテキスト作成、フレーム更新、描画、入力処理
- F1／Shift+F1の性能表示とGPU計測、性能ストレス表示
- F2～F6の基本性能・見た目・部屋・マップ・強化／敵編集
- 開発用シーンのショートカット、デバッグカメラ、当たり判定表示、チート
- F10の手動デバッグ撮影、旧チュートリアルのF3強制スキップ
- 機体JSONの監視による自動再読込

戦闘、ポーズ、初回チュートリアル、通常のスキップルート、60fps制御、シェーダー／生成テクスチャキャッシュは維持します。エディターで保存したゲーム内容は提出版でも読み込みます。

開発機能ONでの動作は従来の各構成に準じます。Developmentでは通常ImGui、Release／trueでは従来の性能表示・遠征制作UIを有効にします。

## 配布間違いの防止

ビルド時に `CG2.build.json` をexeの隣へ生成します。構成、開発機能の有効状態、exeのSHA256を記録する検証用ファイルであり、編集して動作を切り替える設定ではありません。

`package_tank_submission.ps1`／`test_tank_submission.ps1` は、Release／開発機能OFFかつexeとハッシュが一致するビルドだけを受け入れます。古いビルドや制作機能ONのexeを誤って配布することを防ぎます。

既存の環境変数による自動回帰テストとStartupTraceは、画面に制作UIを出さず提出用Releaseを検証するために維持します。通常起動では自動テストは動きません。StartupTraceの `build.developer_tools`、`ui.imgui_initialized`、`ui.runtime_profiler_allowed` は提出版で全て0です。

添付の提出形式PDFは、実行ファイル（ソースコード含む）・プログラム説明PDF・作品実演MP4・自己PRシートをまとめる指定です。この変更で作る配布フォルダはゲーム実行用部分であり、提出一式のZIPではありません。

## 2026-09-27の検証結果

- Release／Developmentビルド成功。
- `test_developer_tools_profile.ps1`：Release／Development／Debugの標準値、Releaseで明示ON、Developmentで明示OFFの5通りのMSBuild設定を検証。実際にビルドしたReleaseとDevelopmentのexe・ビルド情報・起動時カウンターも一致。
- Releaseは制作機能・ImGui初期化・性能表示許可が全て0。Developmentは全て1。
- `test_tank_submission_packaging.ps1`：履歴除外等に加え、開発機能ONのRelease、exeに一致しない古いビルド情報の拒否を確認。
- `test_tank_expedition_map_runtime.ps1 -Configuration Release`：10地点の進行、強化購入、修理、ボスクリア成功。戦闘は既存テストによる強制クリア。
- Warm StartupTrace：タイトル3.174秒、開始→作戦マップ2.228秒、scene initialize1.446秒。直前の3.051／2.152／1.378秒から秒単位の悪化なし（各1回測定で揺れを含む）。シェーダーディスクヒット40、生成テクスチャヒット3、カードスプライト384、未使用アリーナUI省略2回を維持。
- ゲーム実行用配布物：`generated/submission/TatakauSensha_NoDevTools_20260927`。413ファイル・初期文字PNG113枚・履修履歴なし。配布監査成功。以前の配布フォルダは上書きしていない。

今回はマウス／Fキーによる目視操作確認は実施せず、ビルド・実起動カウンター・自動テストで確認した。
