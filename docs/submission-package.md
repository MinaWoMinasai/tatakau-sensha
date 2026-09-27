# 提出用Releaseの作成

Releaseビルド後に、リポジトリのルートから実行します。

```powershell
./project/tools/package_tank_submission.ps1
```

`generated/submission/TatakauSensha_<日時>_<識別子>` に新しいフォルダーを作成します。`-OutputDirectory <新規フォルダー>` で保存先を指定できます。既存フォルダーへの上書き、素材・ビルド・ツール・ドキュメントの中への出力は拒否します。元の調整データ、チュートリアル履修記録、キャッシュは削除・変更しません。

配布物は `CG2.exe`、AssimpのランタイムDLL、`dxcompiler.dll`、`dxil.dll`、`resources`、[提出用README](submission-README.md)、既存のAssimp・ImGuiライセンス表示、相対パスとSHA256だけを記録するマニフェストです。ゲームに必要なモデル、音声、フォントとライセンス、JSON、CSV、HLSLとincludeを保持します。SDK、vcpkg、ソースコード、PDB、開発用LIBはコピーしません。

`resources/configs/expedition_user.json` と派生ファイル、作者の `generated`、`logs`、`Dumps`、一時ファイル、ImGui設定は除外します。シェーダー・生成テクスチャのキャッシュは配布先の初回起動時に必要に応じて生成されます。文字画像に限り、次の手順で作った履歴のない描画成果物を明示的に同梱できます。作者PCのWarm起動時間と配布先の初回時間は区別して測定してください。

引数なしで通常タイトルへ進むように、**配布物の中だけ** `resources/projects/default.project.json` を `tank_game.project.json` の内容へ置き換えます。実行時の資源読み込みは `resources/` 相対パスです。明示の `--project` がなく、作業ディレクトリに `resources` がない場合、実行ファイル隣の `resources` を検出してそのフォルダーへ切り替える起動処理を追加しています。明示プロジェクトの相対パス解釈や、既存の開発時の作業ディレクトリに `resources` がある場合は維持されます。実機検証は開発データを参照しないよう、配布フォルダーまたは `resources` のない専用フォルダーから実施してください。

## 初回用文字テクスチャの準備

文字画像をすべて初回生成すると起動待ち時間が長くなるため、エンジンやゲーム進行を変更せず、クリーンな初期起動で生成した文字PNGだけをビルド成果物として配布できます。

```powershell
$prepared = ./project/tools/prepare_tank_submission_text.ps1
./project/tools/package_tank_submission.ps1 -PreparedTextCacheDirectory $prepared.PreparedTextCacheDirectory
```

準備スクリプトは新規の隔離配布フォルダーを作成し、親プロセスの `CG2_*` 環境変数を一時的に除去して `CG2_STARTUP_AUTOTEST=1` だけを指定します。Hidden起動でタイトルから最初の作戦マップまで進み、終了後に両画面のStartupTraceと履修ファイル・一時履修ファイルの不在を確認します。待機上限は既定360秒、`-TimeoutSeconds` で240秒以上を指定できます。環境変数は成功・失敗いずれも復元します。

出力の `text/` には `text_<16桁の小文字16進数>.png` だけを置き、検証記録は別の `preparation.json` に保存します。元の隔離ステージングも診断用に残します。配布作成には `text/` を指定し、作者の `project/resources/generated/text` は指定しないでください。作者resources配下の指定、名前の異なるファイル、サブフォルダー内のファイル、PNG署名のないファイルは拒否します。

配布監査が許可する例外は、マニフェストで `clean-staging-text-png` と明示された `resources/generated/text/text_<hash>.png` だけです。件数とSHA256も検査します。ログ、履修記録、一般のgeneratedデータはこの指定があっても受け入れません。文字PNGにはゲーム進行データを保存しません。文字内容・フォント設定・フォント更新日時がキーに含まれるため、最終ビルド・素材で準備し、フォントの更新日時を維持するコピー・展開方式を使用してください。

## 検査

```powershell
./project/tools/test_tank_submission_packaging.ps1
./project/tools/test_tank_submission.ps1 -PackageDirectory <作成されたフォルダー>
```

前者は小さな専用データを `generated/submission_tests` に作り、履修済み作者データの保持、履歴の除外、必要ファイル、出力先の保護、ファイル改変の検出、準備済み文字PNGだけの許可、作者キャッシュ・偽PNG・混入ファイルの拒否をテストします。後者は実際の提出物を検査し、必要な実行ファイル・素材・通常タイトル設定、SHA256、履修履歴がないことを確認します。これら2つの検査スクリプトはゲームの実行やビルドを行いません。

検査済みの提出物はそのまま保管し、実機での初回起動確認には別の新規配布フォルダーを作成してください。ゲームを実行するとキャッシュ・診断・履修記録が作られるため、実行後のフォルダーは「未使用の提出物」の検査に合格しません。初回チュートリアルの実際の表示・操作完了は、通常Releaseによる別途の実機検証で確認します。

現行ReleaseのPE依存を調べたところ、CG2は静的CRT、AssimpはKERNEL32のみを参照し、DXC/DXILの追加依存はWindowsのUCRT/API setでした。依存するWindows機能はDirectX 12、Media Foundation、XInputなどです。配布先の動作可否は実機環境でも確認してください。
