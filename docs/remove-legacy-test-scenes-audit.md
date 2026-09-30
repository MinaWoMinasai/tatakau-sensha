# 旧テスト・実験3シーンの除去記録

2026-09-30。開始ブランチは `refactor/remove-legacy-test-scenes`、開始時の追跡862ファイルに作業差分なし。現在のPublic用作業ツリーから TestScene / PlayerLabScene / Action3DScene を除去するため、以下を削除前に分類しました。.git・履歴・remote・Privateバックアップは変更せず、commit・pushも行っていません。

## 削除前の分類と根拠

本書のA/B/C/Dは用途分類です。[素材台帳](public-assets-inventory.md)の権利確認分類とは別です。専用であることから素材の出典・許諾を認定しません。

| 分類 | 対象 | 参照・判断根拠 |
| --- | --- | --- |
| A：専用なので削除 | `project/game/scene/TestScene.cpp` / `.h` | BuiltInGameModuleのinclude・TEST登録、TitleSceneのF3、VSのソース登録のみ。現行Tank / Labからの呼び出しなし |
| A：専用なので削除 | `project/game/scene/PlayerLabScene.cpp` / `.h` | BuiltInGameModuleのinclude・PLAYER_LAB登録、TitleSceneのF2、VS登録のみ。Player / UI等の実装は別の共有ファイル |
| A：専用なので削除 | `project/game/scene/Action3DScene.cpp` / `.h` | BuiltInGameModuleのinclude・ACTION3D登録とVS登録のみ。TitleScene、全project JSON、その他の設定・ツール・起動コードに導線なし |
| A：専用補助コードを削除 | `project/game/collision/TriangleMeshGround.cpp` / `.h` | TriangleMeshGroundとGroundRayHitの利用はTestSceneのみ。共通エンジン配下ではなく、旧3Dアクションの足元レイ判定。VS登録も除去 |
| A：専用resourceを削除 | `project/resources/animation/assimp_test.gltf` | TestSceneのAnimationLoaderだけがロード。単一内蔵data URIで外部依存なし。Game::LoadResourcesは一括ロードせず、設定・ツール・他の素材に参照なし |
| A：専用resourceを削除 | `project/resources/models/simpleSkin/simpleSkin.gltf`, `simpleSkin.bin`, `uvChecker.png` | TestSceneだけがglTFをロード。glTFの相対URIが同ディレクトリのBIN・PNGを指定。別の共有素材からの参照なし。ディレクトリ内のこの3ファイルのみ削除 |
| B：共用なので保持 | `player3D.obj` / `.mtl`, `ground.obj` / `.mtl`, `cube.obj` / `.mtl`, `ball.obj` / `.mtl`, `bloomBall.obj`, `white512x512.png`, `gradation.png`, `gradationLine.png`, `skybox.dds`, `neonTriangleParticle.obj` | GameScene、Stage、GraphicsLab、ParticleManager、OBJ→MTL→画像等の依存を維持。PlayerLab等が使用している理由だけで削除しない |
| B：共用なので保持 | Player / Stage / Bullet / Enemy / Collision / TankButtonUI / TextLabel、Particle / Trail / Ring / Cylinder / Effect / Camera / Input / Audio | Tankまたは共通エンジンの機能。シーンが所有するオブジェクトだけを除去し、共有実装・shader・設定は無変更で保持 |
| C：シーンを削除しresourceを保持 | `models/player/testModel_animated.glb`, `models/human/walk.gltf`, `walk.bin`, `white.png` | GraphicsLabSceneのモデル選択肢がGLBとhumanのglTFをロード。humanのBIN・PNGはglTFの相対依存。既存Packaging testのGLB・BIN保持条件も維持 |
| C：共有描画機能を保持 | Animation / Skeleton / SkinCluster / SkinnedModel / Object3dのskinning経路、Skinning shader・PSO・Root、NeonGridRenderer | GraphicsLabのskinningと骨格表示、Tankのネオングリッド、共通描画APIが使用。Action3Dの骨格表示はNeonGridRendererを所有する実装で、専用SkeletonRendererファイルはない |
| D：判断保留・保持 | `light.obj` / `.mtl`, `weapon.obj` / `.mtl`, ルートの `resources/uvChecker.png`、その他の旧汎用素材 | light / weaponの明示ロードは削除シーンにあったが、専用制作物・動的選択・素材の来歴まで確定できない。ルートuvCheckerはsimpleSkin内の同名画像と別ファイル。名前や文字列一致だけで削除しない |
| D：保持 | 既存のローカル生成物・素材コピー・過去資料 | Git管理外の既存キャッシュやバックアップは今回の削除対象外。過去の検証結果・分離計画は履歴資料として維持 |

削除したのは上記Aの12ファイル（シーン6、補助コード2、resource4）、221,866 bytesです。全追跡ファイルを分割してテキスト・バイナリ双方の参照を検索し、コードのロード箇所、全project JSON、glTFのURI、ModelManagerの指定パスによる遅延ロードを確認しました。検索と開始時SHA256はGit管理外の `generated/legacy_scene_removal/` に保存しています。ライブラリ本体・既存LICENSE・copyright noticeは削除していません。

## 変更範囲

- BuiltInGameModule：3つのincludeと3登録だけ除去。TITLE / GAME / TANK_RUN / TANK_EXPEDITION / GRAPHICS_LAB / UNDERWATER_LAB / VFX_LABを保持。
- TitleScene：F2 → PLAYER_LAB、F3 → TESTの2行だけ除去。F5 / F6 / F7 / F9、Enter / Space / クリック、背景デモを保持。
- vcxproj / filters：TestScene、PlayerLabScene、Action3DScene、TriangleMeshGroundのClCompile・ClIncludeをそれぞれ8項目除去。専用resource4件にはVS登録なし。
- README、資料索引、公開監査、素材台帳、Third-party、配布案内を現在の状態へ更新。GLB・humanは保持と明記し、過去の監査数値は当時の記録として扱う。
- `test_tank_submission_packaging.ps1` のfixture・assertionは変更しない。共有GLB・human BINは引き続き配布対象。

## 検証結果

環境はVisual Studio 2026 / MSVC v145、Windows SDK 10.0.26100.0、既存のローカルAssimpです。依存のダウンロード・再生成は行っていません。最初のサンドボックス内ビルドはVisual StudioのFileTracker初期化でアクセス拒否となったため、同じコマンドを制限外で再実行しました。下表は再実行後の結果です。

| 検査 | 結果 |
| --- | --- |
| 削除と差分 | 分類Aの12ファイルだけを削除。作業ディレクトリ内に収まる絶対パスと開始時SHA256を確認してから削除。追跡ファイルの変更は指定7文書・VSの2ファイル・BuiltInGameModule・TitleSceneの11件だけ。新規文書は本書 |
| 保持ファイルのSHA256 | 839ファイルが開始時と同一。resources 243件、共通engine 104件、残存game 137件、vendor 223件を含む。GameScene / GraphicsLab / Tank / shader / 既存テスト・配布処理は無変更 |
| .gitのSHA256 | 内部1,314ファイルの件数・SHA256すべてが開始時と一致。ブランチ・remote・履歴を変える操作、commit、pushは未実施 |
| シーン登録・include・起動設定 | 全追跡ファイルを再検索し、実行対象のコード・設定・モデル・ツール・Actions・VS登録に削除クラス・専用resource・TEST / PLAYER_LAB / ACTION3D登録への参照なし。過去資料・除去記録とvendorの一般的なTEST文字列は保持 |
| BuiltInGameModule / TitleScene | HEADの内容から指定6行／2行だけを除いた結果と一致。他の登録、F5 / F6 / F7 / F9、Enter / Space / クリック、背景デモを保持。Tank本編内のF2 / F3制作キーも変更なし |
| vcxproj / filters | XML正常。ファイル項目290件／273件に存在しない参照なし。それぞれ指定8項目を除いた残りの登録はHEADと同一 |
| Release / x64 | 成功。開発機能OFF、警告0・エラー0 |
| Development / x64 | 成功。既存の開発機能ON、警告0・エラー0 |
| Debug / x64（ソリューション） | 成功。既存CG2.slnは両プロジェクトのDebugをDevelopmentへ割り当てている。設定は変更なし |
| Debug / x64（vcxproj自身のDebug） | 成功。`CG2_testPro.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:SolutionDir=<projectの絶対パス>/` を別途指定し、独立した `generated/outputs/Debug/CG2.exe` も生成。エラー0、無変更のDirectXTexがincludeするWindows SDKのenum基底型にC4865警告28件。今回SDK / vendor / コンパイル設定は変更していない |
| Release / Development / Debug実行ファイル | 削除シーンID（PLAYER_LAB / ACTION3D）・3クラス名・TriangleMeshGroundのASCII / UTF-16文字列なし。一般的なTEST文字列をvendorから除去する処理はしていない |
| Tank既存テスト15本 | 下記すべて成功。マップはC++17 / C++20で各2048 seed、報酬プールは50,400実カード描画サンプルも確認 |
| `test_tank_submission_packaging.ps1` | 成功。元データ保持、初回履修状態、実行依存・ライセンス同梱、文字PNG・履歴除外、保護・改変検出。GLBとhuman BINを保持する既存fixture・assertionは無変更 |
| `test_developer_tools_profile.ps1` | 成功。5通りのビルド設定とRelease / Developmentの実行ファイル・構成情報SHA256。実行時UI trace引数は指定なし |
| 実Release配布コピー | 既存ツールで新規262ファイル、67,193,582 bytesを作成し、既存提出物監査に合格。専用resource4件の不在、共有GLB・human3件・player3Dとground各2件のSHA256一致を確認 |
| TITLE起動 / TANK_EXPEDITION開始 | 隔離配布コピーをresourcesのない別作業ディレクトリから引数なしで起動。既存`test_tank_submission_runtime.ps1`成功：タイトル→初回訓練→系統選択→改造→修理→敵3種→ボス・結果→タイトル→新規遠征→履修済みスキップ。タイトル2回・遠征2回・エラー0。タイトルと作戦マップの画像も目視確認 |
| README / docsリンク | 関連Markdown 38件、コメント・コード例を除いたローカルリンク126件に欠落なし |
| 素材台帳・Third-party | 台帳243行のパス・サイズを現存resourcesと照合。権利分類A 99件、C 2件、D 142件。削除したsimpleSkin / assimp_testを現在の同梱対象から外し、共有GLB・humanの保持と未解決事項を明記。既存ライセンス本文・フォント・権利者表示はSHA256で無変更 |
| `git diff --check` | 成功 |

実行したTank既存テスト：

```text
test_tank_expedition.ps1
test_tank_expedition_map.ps1
test_tank_expedition_rooms.ps1
test_tank_expedition_content.ps1
test_tank_expedition_tutorial.ps1
test_tank_run.ps1
test_tank_reward_pool.ps1
test_tank_reward_cards.ps1
test_tank_collisions.ps1
test_tank_projectiles.ps1
test_tank_enemy_combat.ps1
test_rival_boss_combat.ps1
test_tank_presentation.ps1
test_tank_additional_abilities.ps1
test_tank_trails.ps1
```

ログ・分類時の参照検索・SHA256基準・比較結果・検証スクリプト・実行画像は `generated/legacy_scene_removal/` に保存しています。実行済みの隔離配布コピーにはキャッシュと履修記録があるため、そのまま提出する成果物ではありません。

後半戦闘は強制クリア、修理は境界条件用fixtureを使う既存の自動テストです。通常操作・難易度・聴感・全系統の手動プレイ評価はしていません。GraphicsLab等はソース・共有素材のSHA256を照合しましたが、Labの全モードの実起動は未実施です。VS2022のクリーン環境とGitHub Actionsも未実行です。過去のコミットや既存のローカル生成物・バックアップ内の旧実装は今回の作業対象外です。

## 次のGraphicsLab整理で確認するもの

- humanのglTF・BIN・PNGとアニメーション付きGLB：GraphicsLabのモデル選択、骨格表示、skinning、外部URI・内蔵画像、既存配布テストとの関係。
- GLBのモデル・衣装・画像・13アニメーションとhumanモデルの出典・素材単体の公開条件。[Third-party](../THIRD_PARTY_NOTICES.md)の保留事項を引き継ぐ。
- light / weapon / ルートuvChecker等：エディターの動的選択、OBJ→MTL→画像の依存、制作経緯を確認してから用途を判断。
- Animation / Skeleton / SkinCluster / NeonGridRenderer / Effect / Ocean / 共通shader：Tankと自作エンジンの使用経路を再調査し、Lab除去だけを理由に削除しない。
