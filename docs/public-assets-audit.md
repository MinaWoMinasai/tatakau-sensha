# Public 素材・埋め込み情報の監査

> 素材整理時点の記録です。その後の[Ink Shooter除去](remove-ink-shooter-audit.md)により、専用リソース21件を追加で除去しました。本書の275件・配布294件などは当時の検証値です。[保持素材の確認台帳](public-assets-inventory.md)は除去後の現存254件へ更新しています。

調査日：2026-09-29。対象は今回の作業開始時の追跡ファイル968件と、その後の追加・変更です。ゲーム本体の機能削除は行わず、素材の読み込み・ビルド・配布・ツールの依存を確認して整理しました。[前回の公開準備記録](public-release-audit.md) に残っていた素材の保留事項は、本書を優先してください。

**残した素材すべての Public 再配布条件が確認できたわけではありません。D の素材は作者による出典・許諾確認が必要です。** .git・履歴・remote・Private バックアップは変更せず、push も行っていません。作業ツリーからの削除は過去コミットからの除去にはなりません。

## 分類と依存関係

A：今回の根拠で Public に残せるもの。B：Public から除外するもの。C：明示された条件を守って残せるもの。D：判断不能・作者確認が必要なもの。サイズや未使用という事実だけでは権利が明確になったとは扱いません。

保持した素材・設定275件のパス、サイズ、分類、参照候補は [確認台帳](public-assets-inventory.md) に記録しました。以下は重点対象の実際の依存関係です。

表中の素材パスは、別記のない限り `project/resources/` からの相対パスです。

| 対象 | 分類・対応 | 参照・実行への影響と権利上の根拠 |
| --- | --- | --- |
| `project/tools/player_ship_editor_mobile.zip` | B・削除対象 | ソース・プロジェクト・Actions・配布処理に参照なし。5メンバー全てに `project/tools/player_ship_editor/` 内の対応ファイルがあり、現在の説明はPC・スマートフォン両対応。アプリ等4件は同一版ではないが現行ソースが代替。Konva は改行を除いて同一。内部 README に個人PCの絶対パスがあり、既存 CI の ZIP 禁止にも該当 |
| `Player_Mixamo.fbx` | B・削除対象 | ファイル名以外に Mixamo 由来と確定できる記録は見つからず、出典・素材単体の再配布許諾は未確認。現行・旧シーン・ビルド・ツールのファイル指定に参照なし。個人PCの絶対パス・ローカルユーザー名の埋め込みを検出。実行用モデルとしてロードされていない |
| `BGM_shining_star.mp3` | B・削除対象 | `Game.cpp` のロードはコメントアウトのみ。音声設定・音声一覧・他ソースに参照なし。出典・Public での素材再配布条件の記録なし。不要なロード・再生コメントも除去 |
| `models/player/testModel.glb` | B・削除対象 | 非アニメーション版。ロード参照なし。`TestScene.cpp` の失敗メッセージのみ残っていたため、実際にロードする `testModel_animated.glb` に修正。ライセンス記録なし。アニメーション版からこのファイルへの外部参照もなし |
| `models/player/animations/*.fbx` 14本 | B・削除対象 | Mixamo 関連の骨格文字列を確認。素材単体の公開条件は未確認。ゲーム・設定・ビルドに参照なし。リターゲットツールは入力を引数で指定する汎用ツールで、このディレクトリを自動読み込みしない。既存GLBの実行に元FBXは不要。モデル再生成には、作者が手元の権利確認済み入力を用意する必要がある |
| `models/player/testModel_animated.glb` | D・保持 | `TestScene.cpp` と `GraphicsLabScene.cpp` がロード。戦車ローグライトの通常起動用ではないが、残存する旧シーンに必要。13アニメーションと23画像を内蔵し、外部 URI なし。VRoid / Mixamo 関連を示す実装があるが、モデル・衣装・画像・アニメーションそれぞれの入手元と素材再配布条件は未確認 |
| `models/human/walk.gltf`, `walk.bin`, `white.png` | D・3件とも保持 | Test / GraphicsLab / Action3D シーンで使用。gltf が残り2件を相対参照するため一体で保持。出典・条件が未記録。ローグライトで使わないという理由だけで削除しない |
| `animation/assimp_test.gltf` | D・保持 | TestScene のアニメーション検証に使用。1ノード・3キー・48バイトの内蔵バッファを持つ小さいテストデータ。外部依存なし。generator はプロジェクト用テストを示すが、制作経緯の最終確認は作者に残す。ファイル名を根拠に Assimp のライセンスが適用されるとは扱わない |
| `models/simpleSkin/` | D・保持 | TestScene が使用するサンプル。gltf のバッファ・画像参照を保持。出典は未確認 |
| `bulletShoot.mp3` | D・保持 | `Game.cpp` の共通起動処理で実際にロードされる。出典・素材再配布条件が不明。権利確認または共通ロードと一緒に代替音への差し替えが必要 |
| `ball.obj`, `ball.mtl`, `monsterBall.png`、`skybox.dds` 等 | D・保持 | GameScene / エフェクト等の参照と、OBJ→MTL→画像の依存がある。自作と断定せず、入手元と利用許諾の確認を残す |
| `graphicsOcean.obj`, `graphicsWater.obj` と対応 MTL | D・保持 | ファイル名によるロード参照は見つからないが、入手元・再生成手段を確認できない。旧編集用途・素材選択機能への影響と制作経緯が不明のため、サイズだけで削除しない |
| 海岸・砂・海中・その他旧画像、モデル、音声 | D・保持 | 旧 Lab シーン等の参照済み素材と、参照不明の素材が混在。共通処理・選択式ツールも残るため一括削除しない。各素材の制作記録・出典照合が必要 |
| `audio/tank_expedition/` | A・保持 | 同梱 README・波形合成スクリプト・測定記録に外部録音・サンプルを使わない旨がある。実行用音声と試聴用 `preview.wav` を区別。後者は約2.75 MiBで説明・試聴に用途あり |
| 自作ゲームソース、設定、HLSL、制作ツール | A・保持 | 現行ビルド・実行・制作のため保持。第三者コードの表示は別途保持し、自作扱いしない |
| ImGui、DirectXTex、nlohmann/json、Assimp、Konva、フォント等 | C・保持 | 各ライセンス・著作権表示を維持する条件。[Third-party一覧](../THIRD_PARTY_NOTICES.md) を参照。Assimp の追加由来コードおよび実際に配る SDK/DXC DLL の条件確認は別途残る |

Adobe の [Mixamo FAQ](https://helpx.adobe.com/creative-cloud/faq/mixamo-faq.html) は商用・非商用の作品でのモデル・アニメーション利用を案内していますが、ここにある各素材を Public GitHub で素材ファイルとして再配布できる根拠や、モデル本体の権利者確認の代わりにはしていません。

## 削除前に確定した一覧

削除前にこの一覧を提示し、以下の18件を削除しました。合計24,310,786 bytes（約23.2 MiB）。開始時のハッシュと一致することを確認してから削除し、別の素材・第三者 LICENSE は削除していません。

```text
project/tools/player_ship_editor_mobile.zip
project/resources/Player_Mixamo.fbx
project/resources/BGM_shining_star.mp3
project/resources/models/player/testModel.glb
project/resources/models/player/animations/Attack1.fbx
project/resources/models/player/animations/Attack2.fbx
project/resources/models/player/animations/Attack3.fbx
project/resources/models/player/animations/DodgeBackward.fbx
project/resources/models/player/animations/DodgeForward.fbx
project/resources/models/player/animations/DodgeLeft.fbx
project/resources/models/player/animations/DodgeRight.fbx
project/resources/models/player/animations/FallingIdle.fbx
project/resources/models/player/animations/FallingToLanding.fbx
project/resources/models/player/animations/Idle.fbx
project/resources/models/player/animations/Jumping.fbx
project/resources/models/player/animations/JumpingUp.fbx
project/resources/models/player/animations/Run.fbx
project/resources/models/player/animations/Walk.fbx
```

調査は `git ls-files` の一覧、全ソース・設定・プロジェクト・Actions・配布・補助ツールの検索、GLB / glTF 内部構造、ZIP のメンバー比較を組み合わせました。`Game::LoadResources()` は素材全体を自動ロードせず、ModelManager は指定されたパスを必要時に読み込みます。AudioManager の素材一覧は `resources/audio/` 配下が対象です。元FBXのロードはゲームではなく引数指定の Blender ツールだけにあります。

既存の配布ツールは `resources` を再帰コピーするため、未参照素材も以前は含まれていました。削除だけでなく、手元で復元した対象素材や ZIP / 7z / RAR が混入しない除外条件を配布処理にも設けました。`.gitignore` は再追加防止であり、追跡ファイルの削除の代わりではありません。

## Third-party 文書

- `THIRD_PARTY_NOTICES.md` から削除素材を現在の同梱物として列挙する記載を除き、残存GLB・human / simpleSkin・共通効果音などの確認事項を具体化しました。削除の経緯は本書へ集約しています。
- Hedley のヘッダーにある CC0-1.0 表示に対応し、[公式原文のコピー](third-party/Hedley-CC0-1.0.txt) を追加しました。出典を `docs/third-party/README.md` に記録し、配布ツールにも同梱を追加しました。
- 既存の vendor、ライセンス本文、権利者表示、現行エディターと Konva は無改変です。ゲーム全体への新たなオープンソースライセンス適用は行っていません。Assimpの追加由来コード・SDK/DXCの未解決事項は保持しています。

## 個人情報・埋め込み情報

代表的な APIキー・GitHub / サービストークン・秘密鍵・パスワード等の代入、個人用ディレクトリ、実行環境のユーザー名、メール形式、絶対パスを検査しました。ZIP は展開内容をメモリ上で検査し、外へ再展開していません。値は報告・監査ログへ出力せず、パス・種別・件数のみ記録しました。

| ファイル・種類 | 結果・対応 |
| --- | --- |
| `Player_Mixamo.fbx`、旧ZIP内部の `player_ship_editor/README_mobile.md` | 個人PCの絶対パス・ローカルユーザー名候補。不要ファイル本体を削除。現行エディター側の README には同候補なし |
| 残存追跡ファイル・今回の追加文書 | 検査した秘密鍵・キー・トークン・非空の認証情報代入パターン、および実行環境のユーザー名の一致なし。全種類の秘密情報の不存在を保証するものではない |
| `project/resources/Player.png` | 絶対パス形式の一致1件は PNG の圧縮画像データ内。個人情報メタデータとは認定しない |
| `models/player/testModel_animated.glb` | パス形式7件・メール形式1件は全て埋め込み画像内。JSON部分の個人用パス・メール形式の一致なし。画像のテキストメタデータも検出なし。画像・モデルの権利判断はDのまま |
| `project/externals/imgui/imgui.cpp` | ユーザーディレクトリ形式の例示が上流コメントに残る。作者のPC情報として削除せず原文保持 |
| nlohmann 内 Hedley / serializer のヘッダー、zlib のライセンス本文 | 権利者の公開連絡先等のメール形式。第三者の表示として保持し、値は本書に転載しない |
| `Game.cpp`、依存ビルドツール、Blender説明、旧資料、vendor例示 | OSフォント・標準インストール先・例示の絶対パスが残る。個人用ディレクトリとは区別。既存の環境前提をこの素材整理で変更しない |

GLB は JSON・埋め込み画像を分けて調べ、PNG のテキストメタデータも確認しました。バイナリの偶然の文字列一致を個人情報と断定せず、vendor の権利者連絡先・例示パスは原文を保持しています。独自バイナリ内の全圧縮データや画像に写った情報まで保証する監査ではありません。Git履歴・別ブランチ・ignored な生成物やバックアップは対象外です。

## 変更後の検証

環境：Visual Studio 2026 / v145、Windows SDK 10.0.26100.0、既存ローカルAssimp。依存を再生成せず通常ビルドを実行しました。

| 検査 | 結果 |
| --- | --- |
| 削除と差分 | 予定18件のみ削除。開始時968件との照合で想定外の欠落なし。保持resourcesの全バイト、vendor・既存ライセンス本文・現行エディター237ファイルのハッシュは無変更 |
| 削除後の参照 | 全ソース・設定・プロジェクト・Actionsを再検索し、実行用ロード参照なし。名前を残すのは除外規則・回帰テスト・監査記録。旧コメントと誤った失敗メッセージを整理 |
| Release / x64、開発機能OFF | 成功 |
| Development / x64、開発機能ON | 成功。既存のDirectInput既定値・リンク最適化の通知あり |
| `test_tank_expedition_map.ps1` | 成功。C++17 / C++20、各2048 seed、全経路条件、導入経済、保存等 |
| `test_tank_reward_pool.ps1` | 成功。50,400実カード描画サンプル |
| `test_tank_submission_packaging.ps1` | 成功。元データ・アーカイブ混入の除外、実行用GLB / BIN / OBJ保持、Hedley本文の同一性を既存検査に追加 |
| `test_developer_tools_profile.ps1` | 成功。5通りの設定と Release / Development のEXEハッシュ。実行時trace引数は指定していない |
| 実Release配布コピー | 成功。294ファイル、67,836,984 bytes（約64.7 MiB）。削除対象の混入なし、ライセンス本文の同一性確認。権利確認済み公開成果物という意味ではない |
| `test_tank_submission_runtime.ps1` | 成功。隔離コピーを別作業ディレクトリから起動し、タイトル→初回訓練→系統選択→改造→修理→敵3種→ボス・結果→タイトル→新規遠征→履修済みスキップ。後半戦闘は強制クリア、修理は境界用fixtureを使うため難易度評価ではない |
| `.gitignore` / CI相当の禁止ファイル検査 | 削除18パス全てに除外規則が適用。現存公開対象に既存 `CheckUnwantedFiles` 条件の違反なし。Gitの追跡解除は行わず削除差分のまま |
| README・主要資料リンク | ルートREADMEを含む関連9文書、ローカルリンク60件を検査。未追加画像のコメントを除くリンク切れなし |
| 大容量ファイル | 残存最大は `testModel_animated.glb` 約11.33 MiB、次が `graphicsOcean.obj` 約7.96 MiB。50 MiB超、追跡中の現存ZIP・EXE・DLL・LIB・PDBなし。Ocean等はDとして保持。不要と確定したものだけ削除 |

`git diff --check` 成功。検査用コピー・ログはGit管理外の `generated/assets_audit/` にあります。起動検証したコピーには履修・キャッシュが生じるため、そのまま配布しません。ビルド・主要テストは通っていますが、VS2022のクリーン環境、GitHub Actions、全ての旧実験シーンの実機起動は今回未検証です。Debugのソリューション構成はDevelopmentへの別名であり、独立した構成としての検証は実施していません。

## 作者が次に確認するもの

1. D のモデル、画像、音声について、元の配布URL・利用規約・権利者・素材単体の再配布許諾を記録する。特に残存GLBの人物・衣装・画像とアニメーション、humanモデル、共通起動音。
2. 許諾が得られない現行素材は差し替え、旧シーン素材はシーン側と一緒に除外する別作業を行う。必要なFBXを公開する場合は、権利確認に加えて個人情報を除いた再エクスポートを行う。
3. Assimp 内の Irrlicht / irrXML 由来の条件と、配布する SDK / DXC の版に対応する条件・告知を確認する。
4. 過去コミット内の削除素材・個人情報候補、コミット作者情報の公開範囲を所有者側で確認する。この作業では履歴を書き換えない。
