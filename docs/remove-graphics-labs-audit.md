# Graphics / Underwater / VFX Lab の除去記録

2026-09-30。開始ブランチは `refactor/remove-graphics-labs`。開始時の追跡851ファイルに作業差分なし。Public用作業ツリーから旧Lab環境を除去するため、以下を削除前に分類しました。.git・履歴・remote・Privateバックアップは変更せず、commit・pushも行っていません。

## 削除前のA / B / C / D分類

本書は用途分類です。[素材台帳](public-assets-inventory.md)の権利確認分類とは別です。専用との判断から出典・素材単体の公開許諾を認定しません。全追跡ファイルを分割してテキスト・バイナリ双方の参照を検索し、実ロード、project JSON、OBJ→MTL→画像、glTF / GLB内部、共通エンジンと動的探索を調べました。開始時SHA256と参照361行はGit管理外の `generated/graphics_labs_removal/` に保存しています。

| 分類 | 対象 | 参照元と判断根拠 |
| --- | --- | --- |
| A：専用シーンを削除 | `project/game/scene/GraphicsLabScene.cpp` / `.h` | BuiltInGameModuleとGraphicsLabGameModuleのinclude・GRAPHICS_LAB登録、TitleSceneのF5、VS登録のみ。Tankからの呼び出しなし |
| A：専用シーンを削除 | `project/game/scene/UnderwaterLabScene.cpp` / `.h` | BuiltInGameModuleのinclude・UNDERWATER_LAB登録、TitleSceneのF6、VS登録のみ |
| A：専用シーンを削除 | `project/game/scene/VfxLabScene.cpp` / `.h` | BuiltInGameModuleのinclude・VFX_LAB登録、TitleSceneのF7、VS登録のみ。背景ラッパーVfxLabBackgroundRendererはこのcpp内に定義 |
| A：専用module・起動設定を削除 | `project/game/modules/GraphicsLabGameModule.cpp` / `.h`、`resources/projects/graphics_lab.project.json`、`vfx_lab.project.json` | GameModuleBootstrapだけが独立moduleを登録。JSONの開始シーンが各Labで、他のコード・設定・ツールに起動依存なし。default / tank_game / tank_expedition / tank_runは別ファイル |
| A：専用資料を削除 | `docs/vfx_flame.md` | F7 / F6とvfx_lab JSONによるLab操作案内。資料索引だけが掲載。ProceduralFlame本体の汎用API・shaderは保持する。他のLab専用の現行資料は見つからず、過去の除去監査・分離計画は履歴資料として保持 |
| A：専用shaderを削除 | `resources/shaders/VfxLabBackground.VS.hlsl` / `.PS.hlsl` | VfxLabScene内の専用ラッパーだけがCompileShaderする。VSのFxCompile登録も除去。汎用Root / DirectXCommonからの利用なし |
| A：専用画像を削除 | `resources/UnderwaterCausticsAtlas.png`、`UnderwaterCausticsDeepBroadAtlas.png` | UnderwaterLabSceneの定数が2画像を明示指定。Object3dの汎用caustics APIへ渡す。別のロード・設定・モデル・ツールの参照なし |
| A：未確認の専用モデルを削除 | `resources/models/player/testModel_animated.glb`、`models/human/walk.gltf` / `walk.bin` / `white.png` | 実ロードはGraphicsLabのskinningサンプルのみ。GLBに外部URIなし、humanのglTFが同フォルダーのBIN・PNGを相対参照。Tank / Editor / Engineはこれらを固定指定せず、ModelManagerも全モデルを一括ロードしない。Packaging testのGLB・BIN参照は仮のテキストファイルを作る旧構成の保持テストで、実runtime依存ではない |
| A：専用PBR見本を削除 | `resources/TestBlock.obj` / `.mtl`、`material_tests/TestBlock_albedo.png`, `_normal.png`, `_roughness.png`, `_metallic.png`, `_ao.png` | GraphicsLabだけがTestBlockをロード・描画。MTLがこの5画像を指定し、他のモデル・設定・ツールからの参照なし。PBR loader・shader・material debug APIは独立して保持 |
| A：専用地形モデルを削除 | `resources/graphicsSand.obj` / `.mtl`、`graphicsBeach.obj` / `.mtl` | GraphicsLabだけがロード。MTL内にGraphics lab sand / beach materialの専用表記あり。参照するwhite512x512.pngは共有画像なので保持 |
| A：専用生成メッシュを削除 | `resources/graphicsOcean.obj` / `.mtl`、`graphicsWater.obj` / `.mtl` | 現在の明示ロードなし。OBJにGenerated graphics lab grid / Graphics lab dedicated water grid、MTLにもGraphics lab ocean / water materialと明記。現在のLabはModelManagerのCreateGridModelで別のメッシュを生成し、OceanRendererも独立生成。Tank / Engine / 設定 / ツールにこのOBJの依存なし。名前だけで判断していない |
| B：Tankと共用なので保持 | `player3D.obj` / `.mtl`、`ground.obj` / `.mtl`、`cube.obj` / `.mtl`、`ball.obj` / `.mtl`、`jewelry.obj` / `.mtl`、`white512x512.png`、`gradationLine.png`等 | GameScene / Stage / Skybox / Tank描画・UI・Trail / Ring等の参照を維持。専用MTLが白画像を使うことを理由に画像を削除しない |
| B：Tank / Engine共通機能を保持 | Object3d / Model / ModelManager / Skybox / Camera / DebugCamera / Input / Audio、ParticleManager / TrailManager / RingManager / NeonGridRenderer / ObjectPostEffect / Bloom、共通shader | Tank・Title・Editor・共通描画ループで使用。ファイル全体を無変更で保持。skybox.dds自体は開始時から存在せず、TextureManagerのBuildProceduralEnvironmentCubeによる代替生成を使う既存構成も維持。Object3dの水面・caustics・PBR分岐が同居するshaderも分割・削除しない |
| C：現在のLab用途を失う汎用機能を保持 | `DirectX/engine/3d/OceanRenderer.*`、Ocean VS / PS、spectrum / FFT computeと共通include、Object3dの高密度水面経路、WaterPost API / shader / Root / PSO | OceanRendererを所有する実シーンはGraphicsLabのみ。Tankの直接描画利用は確認できないが、カメラ・メッシュ・spectrum・FFT・投影グリッド等を持つ独立したEngine実装。DirectXCommon / Gameの共通初期化・ポスト処理との参照も残る。今回は削除提案に留め、実装・shader・VS登録を保持 |
| C：汎用Animation / Skinningを保持 | Animation / Skeleton / SkinCluster / SkinnedModel、Object3dのDrawSkinned / shadow、Skinning shader / Root / PSO、Assimpと汎用Blenderツール | ファイル読み込みと骨格・GPU palette処理は独立したEngine機能。現在のモデル見本はLabのみだが、Object3dや共通描画からAPI参照あり。将来のTank利用は未実装であり、現在の使用と同一視しない。素材削除とEngine削除を分ける |
| C：汎用描画・生成APIを保持 | `PbrEnvironment.h`、PbrLighting / CrystalMaterial、ProceduralFlameRendererとProceduralFlame VS / PS、CylinderManager / EffectSequencer、見本用に呼ばれていたgeometry生成API | PbrEnvironmentとProceduralFlameの実所有はLabのみ。汎用の環境filter / BRDF LUT生成、パラメーター付きビルボード描画として独立。Cylinder / Effect / procedural geometryもEngineに残す。後続で実到達性と再利用方針を確認する |
| D：用途・出典が不明なので保持 | `resources/UnderwaterCaustics.png` | 全追跡ファイルに明示参照なし。PNGにも生成元・専用性を確定できるテキスト表記を検出できず、Lab用旧単一画像と断定しない。2つのAtlasとは別ファイルとして保留 |
| D：保持 | light / weapon / ルートuvChecker、sea等の旧汎用素材、既存ローカルキャッシュ・バックアップ | 先行監査の保留を引き継ぐ。専用とは確定できない素材やGit管理外の履歴・コピーは今回の削除対象外 |

Aの34ファイル（scene 6、module 2、project JSON 2、資料1、shader2、その他resource21）、29,812,206 bytesを削除しました。うちresource25件は29,710,488 bytesです。削除前にこの分類を記録し、各候補のSHA256一致と作業ディレクトリ内の絶対パスを確認してから個別に削除しました。共通Engineコード、ライブラリ本体、既存LICENSE / copyright noticeは削除していません。

## 変更範囲

- BuiltInGameModule：Lab3つのinclude・登録と空になる条件ブロックを除去。TITLE / GAME / TANK_RUN / TANK_EXPEDITIONを維持。
- GameModuleBootstrap：GraphicsLabGameModuleのincludeと登録のみ除去。BuiltInの登録・汎用registryは維持。
- TitleScene：F5 / F6 / F7のLab導線と空になる条件ブロックだけ除去。F9、Enter / Space / click、タイトル背景デモは維持。Tank本編の制作キーは変更しない。
- vcxproj / filters：削除するscene・module・project JSON・VfxLab背景shaderの計12項目をそれぞれ除去。残すEngine・共通shaderの登録は維持。
- Packaging：旧GLB / human BINの保持fixtureを現行Tank共有素材（player3D / ground / cubeのOBJ・MTL、white512x512.png）に置換。削除するLab resource25件だけを完全一致で除外し、ローカルに旧素材を復元しても配布へ混入しないことを既存テストで検証する。
- 指定7文書と資料索引・素材台帳・Third-partyを更新。過去の除去監査は当時の履歴として無変更で残す。

## 検証結果

Visual Studio 2026 / MSVC v145、Windows SDK 10.0.26100.0、既存のローカルAssimpを使用。前回の検証でサンドボックス内のFileTracker初期化にアクセス拒否があったため、ビルドは必要な権限で実行しました。依存のダウンロード・再生成、設定・ツールセット変更は行っていません。

| 検査 | 結果 |
| --- | --- |
| 削除と差分 | 分類Aの34件だけを削除。既存追跡ファイルの変更は指定7文書・VS2ファイル・BuiltInGameModule / GameModuleBootstrap / TitleScene・Packagingポリシー / 既存Packaging testの計14件だけ。新規文書は本書 |
| Tank / Engine / resourceのSHA256 | 803ファイルが開始時と同一。残存resources218件、Engine104件、game128件、vendor223件を含む。GameScene、Tank、Editor、Ocean / Animation / Skinning / PBR / Flame、共通shader・既存ライセンス本文を無変更で保持 |
| .gitのSHA256 | 内部1,342ファイルの件数とSHA256すべてが開始時と一致。開始ブランチを維持。履歴・remote・バックアップの変更、commit・pushは未実施 |
| 登録・include・起動設定・削除ファイル参照 | 全追跡ファイルを再検索。実行コード・設定・モデル・Actions・VS項目に削除scene / module / resource / shaderへの参照なし。履歴文書と配布除外リストは記録として保持 |
| 共通shader内の名称 | `GetGraphicsLabWaterMask`、`SampleProceduralGraphicsLabEnvironment`等の補助関数名はCの水面・PBR機能として保持。Labへの起動登録・導線ではなく、Bloom / Composite / TemporalResolve / PbrLighting内の汎用描画コードの一部。名称だけを理由に変更・削除していない |
| BuiltInGameModule / GameModuleBootstrap / TitleScene | HEADから指定のLab include・登録・導線と空条件ブロックだけを除いた結果と一致。TITLE / GAME / TANK_RUN / TANK_EXPEDITION、F9、Enter / Space / click、背景デモ、Tank本編の制作キーを維持 |
| vcxproj / filters | XML正常。ファイル項目278件／261件に存在しないパスなし。各12項目を除いた残りの登録はHEADと同一 |
| Release / x64 | 成功。開発機能OFF、警告0・エラー0 |
| Development / x64 | 成功。既存の開発機能ON、警告0・エラー0 |
| Debug / x64（ソリューション） | 成功。既存CG2.slnは両プロジェクトのDebugをDevelopmentへ割り当てるため、独立Debugとは区別 |
| Debug / x64（vcxproj自身） | 成功。`CG2_testPro.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:SolutionDir=<projectの絶対パス>/` を別途指定し、`generated/outputs/Debug/CG2.exe`も生成。警告0・エラー0 |
| 実行ファイル | Release / Development / DebugのEXEに3シーンID・3クラス名・GraphicsLabGameModule・VfxLabBackground・独立module IDのASCII / UTF-16文字列なし |
| Tank主要テスト15本 | 下記すべて成功。マップはC++17 / C++20で各2048 seed、報酬プールは50,400実カード描画サンプルも確認 |
| `test_tank_submission_packaging.ps1` | 成功。現行Tank共有素材の保持、Lab resource25件の除外、元データ保持、初回履修状態、依存・ライセンス同梱、文字PNG / 履歴除外、保護・改変検出。最終fixture更新後にも再実行して成功 |
| Packagingポリシー | 除外25パスと実際の削除resource一覧が完全一致。resources相対・配布物相対の両形式を確認し、残存素材は既存仕様の音声生成スクリプト1件を除いて除外されないことを照合 |
| `test_developer_tools_profile.ps1` | 成功。5通りの設定とRelease / Developmentの実行ファイル・構成情報SHA256。実行時UI trace引数は指定なし |
| `test_title_demo.ps1 -Configuration Release` | 成功。4段階の実戦背景、454発の発射サンプル、37撃破、39ダッシュ、3強化、1経路、フェード中の停止と新規遠征を確認。作者のチュートリアル履修ファイルの状態・SHA256も無変更 |
| 実Release配布コピー | 新規237ファイル、37,283,988 bytesを作成し既存提出物監査に合格。削除resource25件の不在、Tank共有素材・保持するOcean / Skinning / Flame shader・DのCaustics単一画像のコピーSHA256一致を確認 |
| TITLE起動 / TANK_EXPEDITION開始 | 隔離配布コピーをresourcesのない別作業ディレクトリから引数なしで起動。既存`test_tank_submission_runtime.ps1`成功：タイトル→初回訓練→系統選択→改造→修理→敵3種→ボス・結果→タイトル→新規遠征→履修済みスキップ。タイトル2回・遠征2回・エラー0 |
| README / docsリンク | 関連Markdown38件、コメント・コード例を除いたローカルリンク131件に欠落なし。削除したvfx_flameへの資料索引も除去 |
| 素材台帳 / Third-party | 218行のパス・サイズを現存resourcesと照合。権利分類A95件、C2件、D121件。参照候補欄も現在のソース・設定・モデル・shader・ツールから更新。GLB / human等を現在の同梱対象から外し、残存素材の保留と既存LICENSEを維持 |
| `git diff --check` | 成功 |

実行したTank主要テスト：

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

検証スクリプトの初期版はskybox.ddsを実在必須と誤って扱い、配布後の追加ハッシュ照合で停止しました。開始時から当該ファイルが存在せず、TextureManagerが任意のskybox名についてcubemapを代替生成する既存構成であることを確認し、実在する共有素材を照合するよう修正しました。title demoの成功結果を保持し、未実行の隔離配布コピーの監査から再開して実行テストを完了しています。ゲーム・Engineへの修正は不要でした。

ログ、SHA256基準、参照検索、比較結果、検証スクリプトと画像は `generated/graphics_labs_removal/` に保存しています。実行済みの配布コピーにはキャッシュ・履修記録があるため、そのまま提出する成果物ではありません。後半戦闘は強制クリア、修理は境界条件fixtureを使う既存テストで、難易度・音量・全系統の手動プレイ評価ではありません。過去のコミットや既存のローカル生成物・バックアップ内のLab実装は今回の作業対象外です。

## 作者が次に確認するもの

- CのOcean / water post / Animation / Skinning / PbrEnvironment / ProceduralFlame等は今回は保持。実際にTankで使う予定があるか、Engine機能としてPublicに提示するか、後続のdead-code整理へ回すかを個別に決める。呼び出しが減ったことだけで今回削除しない。
- Cの起動時PSO・shader初期化がTankに必要かは別の最適化課題。機能削除・初期化変更は今回行わない。
- DのUnderwaterCaustics.png、light / weapon / uvChecker / sea等の制作経緯・出典と動的利用。今回削除したGLB・humanのPublic素材単体の許諾は確認済みと認定せず、Private保管側での利用条件確認も別途必要。
- 通常操作・音量・3系統の手動プレイ、クリーンなVS2022環境とGitHub Actionsの確認は自動回帰テストと区別する。
