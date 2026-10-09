# Public版の最終クリーンアップ監査

> 2026-09-30時点の監査記録です。その後の[旧起動口廃止](retire-legacy-run-entrypoints-audit.md)でGAME / TANK_RUNの登録と専用起動ファイル2件を除去しました。本文の登録・起動可能性・resource件数は当時の状態です。共有TankRun実装とbulletShootは引き続き保持しています。

2026-09-30、`chore/final-public-cleanup`。開始時の追跡818ファイルすべてをローカルのファイル別分類表へ記録しました。削除前の分類でBは0件です。用途・出典・動的利用に判断が残るものを削除せず、ゲームとEngineのコード・設定・素材・Visual Studio登録を保持します。

## 分類と作業範囲

| 分類 | 開始時件数 | 判断と処置 |
| --- | ---: | --- |
| A | 663 | Tank・ビルド・ツール・Public説明・制作記録・第三者表示として保持 |
| B | 0 | 安全な削除を確定できたファイルなし。ファイル削除なし |
| C | 24 | 現行ゲームから呼ばれない独立Engine実装・shader・汎用制作ツール。保持 |
| D | 131 | 出典未確認のresource 121件と、過去の企画・研究・制作記録10件。保持 |

この分類は今回の保持判断です。[素材台帳](public-assets-inventory.md)のA/C/Dは従来の出典・権利分類であり、CがOFLフォントを表す点などが異なります。台帳の出典分類は変更しません。Aは権利処理済みの認定ではなく、vendorにも既存の第三者ライセンスが適用されます。今回追加した監査文書・TSVはAです。

全追跡ファイルのパス・SHA256・サイズをローカルの `generated/final_public_cleanup/before.json` に保存。参照検索は全追跡ファイルを対象に行い、コード、JSON/CSV、OBJ/MTL、prefab辞書、制作ツール、Packaging、テスト、文書を区別しました。OBJ→MTL→画像68辺はすべて現存先へ接続しています。`.git` は開始時1373ファイルのハッシュを記録し、読取専用Git操作以外は行いません。Privateバックアップ・履歴・remote・commit・pushは対象外です。

## 重点resourceの判断

| 対象（resources相対） | 現在の根拠 | 分類・処置 |
| --- | --- | --- |
| `UnderwaterCaustics.png` | 全追跡検索で現在の実行コード・設定・MTL・ツールからファイル指定なし。Engineのcaustics API・shaderは保持され、画像の制作経緯・独立利用は未確定 | D・保持。Atlas2画像の以前の削除と混同しない |
| `checkerBoard.png`, `deathParticle.png`, `uvChecker.png` | 現在の実行コード・設定・MTL・ツールからファイル指定なし。出典と制作時の用途は未確定 | D・保持。simpleSkinの旧uvCheckerとは別ファイル |
| `light.obj/.mtl`, `testBox.obj/.mtl`, `sea.obj/.mtl` | 各OBJ→対応MTL→`white512x512.png`。起動側の直接指定はなし | D・保持。共通白画像も保持 |
| `weapon.obj/.mtl` | OBJ→MTL→`gradation.png`。起動側の直接指定はなし | D・保持。gradationの出典・利用も保留 |
| `player.obj/.mtl` | OBJ→MTL→`Player.png`。起動側の直接指定はなし | D・保持 |
| `playerBullet.obj/.mtl` | OBJ→MTL→`PlayerBullet.png`。名前が似た現行弾の材質とは別に照合 | D・保持。現行`bullet.obj`と混同しない |
| `playerHPBar.obj/.mtl`, `playerHPBarGreen.obj/.mtl` | それぞれ`HPBarCurrent.png`と`bossHPGreen1.png`へ接続。画像は現在使用するLongモデルからも参照 | D・保持。共用画像を削除しない |
| `playerParticle.obj/.mtl/.png` | OBJ→MTL→PNG。起動側の直接指定はなし | D・保持 |
| `rule.png`, `toRule.png`, `ka.png`, `nn.png`, `se.png`, `si.png`, `start.png`, `ta.png`, `u.png`, `ya.png` | 全追跡検索で実行・設定・MTL・ツールからファイル指定なし。現在のタイトルはTextLabelを使用。古い文字画像の出典・制作用途が未確定 | D・保持。名前だけで旧タイトル専用と断定しない |
| `enemyBullet.obj`, `enemyParticle.obj/.mtl/.png`, `Player.png`, `PlayerBullet.png`, `gradation.png`等 | 全resourceを調べ、孤立候補と材質による間接参照を区別。上記と同様、素材単体の出典または動的利用に判断が残る | D・保持 |

動的ロードの確認：`Object3d::SetModel`は未ロードの任意パスを`ModelManager::LoadModel`へ渡します。PlayerのDevelopment用武器マウント編集欄はモデル名を自由入力でき、EffectSequencerのモデル欄も任意パスを受け付けます。`playerClasses.json`の現行指定は`gunBarrel.obj`等ですが、モデル文字列の不一致だけでは制作時の到達不能を断定しません。Level AI-ditorとBlenderツールはレベルJSONを編集し、prefab辞書のObstacle/Itemはcube・cubeDamage・jewelry・ball・bloomBallへ接続します。任意の画像を自動列挙して読み込む処理は確認できませんでしたが、出典と用途の判断が残る画像はDのままです。

resource総数は218件、出典分類はA 95 / C 2 / D 121です。`player3D`、ground、cube、gunBarrel、敵・弾・HP表示、white512x512、フォント・OFL、遠征の設定/マップ/音声、共通shaderを保持します。`skybox.dds`は開始時から非同梱で、TextureManagerの既存の生成cubemapフォールバックを使用します。

## bulletShootの到達性

| 経路 | 結果 |
| --- | --- |
| 共通起動 | `Game::Initialize`が`bulletShoot.mp3`を必ずロードする |
| GAME | `Player`の通常射撃2経路とGameSceneの強化音経路から`PlayAudioSE`へ到達する |
| TANK_RUN | `SetRunCheckpointEvolution(true)`を設定せず、通常射撃・強化音から到達する |
| TANK_EXPEDITION | 初期化でcheckpointフラグをtrueにし、Playerの旧射撃音を抑制。GameSceneの強化音も遠征専用Audioへ分岐する |
| TITLE背景デモ | `GameScene(false,true)`で作成し、デモ再設定でもcheckpointフラグをtrueにする。旧射撃音を抑制。遠征用Audioの音楽・効果音volumeも0にしてデモを無音にする |
| 音声テスト | `test_audio_runtime`がMP3デコードの入力に使用するが、テストだけが保持理由ではない |

現在の全`PlayAudioSE`呼出しとフラグ設定・解除をコードで照合した結果です。すべての場面を耳で比較した認定ではありません。素材・共通ロード・テストを変更せずDとして保持し、出典・Public再配布条件の作者確認を残します。除去または代替音への変更はGAME/TANK_RUN方針と合わせた別作業です。

## GAME / TANK_RUNの位置付け

通常READMEの起動、default/tank_game project、配布物の通常起動はTITLE→TANK_EXPEDITIONです。タイトルの可視メニューは遠征1項目とF9です。`IsMenuAvailable`にGAME判定は残りますが、現在の可視メニューからGAMEへ移る選択肢はありません。GAMEはbuiltin登録を保持しますが、現存project JSONにGAMEをstartupSceneとするものはありません。起動するにはstartupSceneをGAMEにした独自project JSONを`--project`で指定する必要があります。TANK_RUNは`tank_run.project.json`と`run_tank_run.ps1`で明示起動でき、project JSONも配布対象です。

両モードは独立した作品紹介の中心ではありませんが、GameScene・Player・敵・弾・Stage・TankRunDirector・成長/報酬・UI・描画・タイトルデモと広く共有します。遠征もprototypeRunをtrueにしてTankRun基盤を使います。次のbranchで起動設定/登録だけを閉じる整理と、共有実装の整理を分け、タイトルデモと遠征の依存を先に確認してください。今回はコードも設定も保持します。

## CのEngine機能と共有部分

| 機能 | 現在の状態・次の監査 |
| --- | --- |
| OceanRenderer / FFT・spectrum compute | game側の生成・描画呼出しなし。OceanRendererの初期化を呼んだ時にcompute PSOを作る独立Engine実装。Cで保持 |
| Ocean VS/PS | DirectXCommonの共通起動でgraphics PSOを初期化する。TankでOcean描画は確認できない。Cで保持 |
| WaterPost | 独立したWaterPostファイルは存在しない。IScene→SceneManager→Bloom→Compositeの水面用設定/分岐が共有コードに残る。Tankで専用設定overrideなし。共有ファイルはAとして丸ごと保持し、水面経路はC相当の後続候補 |
| Animation / Skeleton / SkinCluster / SkinnedModel | game側の実生成・更新・描画呼出しなし。SkinnedModelはSkinCluster内の公開クラス。Engine間の相互依存とビルド登録を保持。C |
| Skinning shader | 共通起動時に通常/HDR/両面/Shadow PSOを初期化。現行ゲームでskinned drawは確認できない。C |
| PbrEnvironment | 独立ヘッダーの設定APIはgame側の呼出しなし。TextureManagerは起動時にBRDF LUT・irradiance・prefiltered環境を生成し、通常モデルからも使用する。通常モデルのPBR材質/テクスチャ経路と共通Object3d shaderは現存・共有。独立部分はC、共有ファイルはAで保持 |
| ProceduralFlameRenderer / shader | game側の生成・描画呼出しなし。rendererを初期化した時に専用PSOを作る。起動時の共通PSOではない。C |
| 汎用geometry API | Model / ModelManagerのplane/grid/box/crystal/cylinder/sphere生成API。現行game側の直接呼出しなし。OBJロードと同じファイル内にあるため共有ファイルをAで保持、生成APIはC相当 |
| Particle / Trail / Ring / Cylinder / NeonGrid / Bloom / ObjectPost | 現行Tank・UI・デモから使用。Aで保持 |
| リターゲット / アニメーションpreviewツール | 引数で外部入力を指定する汎用制作ツール。今回の同梱モデルを自動探索しない。Cで保持 |

「ゲームからの呼出しなし」は将来価値の不存在や完全なdead-codeの証明ではありません。公開API、起動PSO、共有shader分岐、Engine内部利用を分けて後続監査し、このbranchでEngineを削除しません。

## Docs / 配布 / Visual Studio

現在のREADME・資料索引・素材監査/台帳・公開準備監査の冒頭・Third-party・配布説明に、この最終監査を追加します。COPYRIGHT、原文LICENSE、OFL、remove-*-audit本文は保持。旧調査資料と`public_submission/REPOSITORY_SPLIT_PLAN.md`は制作/方針の記録としてDで保持し、現在の操作や公開手順とは区別します。作者が次に確認するのは、旧計画・研究・roadmapを作品閲覧者向けに残す価値と、Private記録のみへ移す必要性です。今回、履歴価値のある資料を一括削除しません。

試聴用`audio/tank_expedition/preview.wav`は同梱説明でゲームのロード不要と明記され、音声生成・測定・試聴説明から参照があります。PublicソースではAで保持し、実行用パッケージからだけ正確なパス指定で除外します。実行用の遠征音声9本・MP3・モデル・shader・OFL・音声制作記録は保持。Packaging testで試聴音の混入を検出し、他の音声と共用素材の保持を確認します。過去の削除素材を戻さない除外規則も保持します。

vcxprojとfiltersのXML、実ファイル登録278/261件を調べ、存在しないパスは0件。None項目も設定・資料・shader・制作ツールとして用途があるため登録を変更しません。削除済みシーン・Lab module・Lab shaderの実行コード/登録は残っていません。Releaseのリンクオプションに限り、下記のPDBパス対策を追加します。

CIのCheckUnwantedFilesにはZIPに加えて7z・RAR、Dumpに加えてDumpsを追加し、配布規則との検査漏れを補います。4 workflowに除去済みシーンを要求する処理はありません。ビルドCIはwindows-2022 / v143、今回のローカル検証はVS2026 / v145です。GitHubへ送信していないため、ホストCIの実行結果は今回取得していません。

## 個人情報・第三者表示

開始時の全818追跡ファイルをraw/UTF-16で検査し、個人ユーザーディレクトリ、作者ローカルユーザー名、代表的なtoken/API key/秘密鍵/credential代入パターン、個人向け生成物の追跡は検出しませんでした。メール形式の一致はzlib原文・nlohmannの公開権利表示のみです。Windowsシステムフォント、開発ツールの一般的なインストール先、vendorの例示パス、バイナリ内の偶然一致は作者の秘密と区別し、値を監査ログへ保存しません。履歴・Privateバックアップ・ignoredの全生成物は対象外です。

追加で実行用の4バイナリを調べ、CG2.exeのCodeView/RSDSに作者PCの絶対PDBパス1件を検出しました。ReleaseのLinkへ`/PDBALTPATH:%_PDB%`を追加し、埋め込む情報をファイル名に限定します。このオプションは実際のPDBの保存先を変更しません。[Microsoftの仕様](https://learn.microsoft.com/en-us/cpp/build/reference/pdbaltpath-use-alternate-pdb-path?view=msvc-170)を照合し、再リンク後の実バイナリで確認します。Assimp・DXC・DXIL DLLには作者ユーザーディレクトリ・ローカルユーザー名を検出しませんでした。DXC/DXILのFileVersionは1.8.2502.11、Windows SDKは10.0.26100.0です。

ImGui、stb、DirectXTex、d3dx12、nlohmann/Hedley/Abseil、Assimp/RapidJSON/zlib、Konva、ZenMaruGothic/OFLの現存対象と表示を照合します。既存本文は無改変で保持。Assimpのfast_atof由来のIrrlicht/irrXML条件、実際に配るSDK/DXC DLLの版と再配布条件、D素材121件の出典・素材単体再配布条件は未解決のままです。テスト合格を許諾確認済みとは扱いません。

## 最終検証

最終ReleaseのPDB情報は`CG2.pdb`だけになり、配布するEXE/DLL4件から作者ユーザーパス・ローカルユーザー名は検出されませんでした。以下はPDB対策後のビルド・Runtimeと、今回実行した主要テストの結果です。

| 検査 | 結果 |
| --- | --- |
| Release / x64 | PASS。VS2026 v145、Developer Tools無効 |
| Development / x64 | PASS |
| Debug / x64 solution | PASS。既存のDevelopment対応設定を保持 |
| Debug / x64 vcxproj単体 | PASS。独立Debug出力も検証 |
| Tank主要テスト15スクリプト | すべてPASS。expedition / map / rooms / content / tutorial / run / reward pool / reward cards / collisions / projectiles / enemy combat / rival boss / presentation / additional abilities / trails |
| map / 報酬描画の例 | 2048生成seedと全ルート性質、50,400実フレーム描画サンプルでPASS |
| test_audio_runtime | PASS。PCM/WAV、MP3 decode、音声ハンドル・bank上限・pause/resume・置換/解放。音を出さない回帰検査 |
| test_tank_submission_packaging | PASS。試聴音除外・実音声/モデル保持・履歴/旧素材除外・マニフェスト・prepared text・source非破壊 |
| test_title_demo | PASS。4段階、449射撃サンプル、37撃破、30ダッシュ、報酬3、ルート1、fadeの停止と新規遠征開始。作者の履修状態は不変 |
| TITLE / TANK_EXPEDITION、test_tank_submission_runtime | PASS。引数なし・無関係cwd・初回隔離profile。タイトル2回、遠征2回、errors 0。導入・系統・作業場・回復状態・新敵・結果・再遠征・履修済みskipを検証 |
| 配布フォルダー / test_tank_submission | 未起動の別フォルダーでPASS。236ファイル、34,404,175 bytes。マニフェスト未記載ファイル0、履歴/cache/logs/Dumps/圧縮書庫/作者環境ファイル0 |
| 配布resource | 216ファイルを元ファイルとSHA256照合。除外は制作スクリプトと試聴音のみ。default projectは既存方針どおり配布内だけtank_gameと同一。実行用WAV9本・MP3・OFL・noticeは保持 |
| README / docsリンク | 39文書、ローカル141リンクの実在先を確認。現在の配布説明の素材監査アンカーも確認 |
| vcxproj / filters | XML正常、実ファイル登録278/261件、欠落0。登録リストは開始時と同一。変更はRelease LinkのPDB対策だけ |
| 削除済み機能の残存参照 | 実行コード・起動JSON・プロジェクト登録に旧シーン/module/専用shader参照0。履歴監査・再混入防止リストを区別 |
| Third-party | 既存の第三者本文・OFL等12ファイルがSHA256不変。配布内のnoticeと対応LICENSEも無改変。権利保留は前述のとおり |
| 秘密情報 / CI検査 | 現在の追跡＋新規文書に代表的なキー/秘密鍵/作者パス候補0。CI禁止パターンに該当する公開ファイル0。GitHub CI自体は未実行 |
| 変更対象外SHA256 | 807追跡ファイルが不変。内訳にTankコード131、Engine104、resource218、vendor223を含む。COPYRIGHT・filters・過去のremove監査本文も不変 |
| `.git` / 差分 | 開始時1373ファイルの件数・SHA256が一致。`git diff --check` PASS。branch・履歴・remote・commit・pushは変更なし |

ローカル結果は `generated/final_public_cleanup/` に保存しています。未起動の配布検査用フォルダーは `pristine-package/`、Runtime試験用は `runtime-final-package/` です。後者にはQA履歴が生成されているため、そのまま配布しません。未起動の前者もD素材の権利処理が済んだ認定ではありません。

Runtime walkthroughの後半戦は強制clear、回復検査は境界fixtureを含むため、難易度のプレイテスト結果とは区別してください。操作・バランス・見た目のコードや設定は変更していませんが、作者の通常操作による最終プレイ確認は残します。

## GitHubでmergeする前に作者が確認する点

- D素材121件の制作記録・入手元・Publicでの素材単体再配布条件。特にbulletShootと参照不明の画像/モデル群。未使用候補でも出典が不明なものを今回削除しない判断を確認する。
- AssimpのIrrlicht/irrXML由来条件と、SDK 10.0.26100.0から配るDXC/DXIL 1.8.2502.11の条件・必要表示。
- GAME / TANK_RUNのPublic上の位置付け。別branchで起動設定・登録を閉じる場合も、遠征・タイトルデモが使う共有実装を先に確認する。
- Cの24ファイルと共有ファイル内の水面/PBR/geometry API。起動PSOだけの経路、Engine公開API、現在の描画利用を分けて後続のdead-code監査を行う。
- Dの過去資料10件をPublicに置く価値。旧Public分離計画は現行公開手順として使用しない。
- 最新Releaseを通常操作で確認し、公開先のwindows-2022 / v143 CI結果を確認する。今回はローカル検証を完了し、GitHubへの送信は行っていない。
