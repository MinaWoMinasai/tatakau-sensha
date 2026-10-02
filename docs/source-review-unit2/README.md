# ソースレビュー単元2 判定と修正報告

2026年10月2日、提示された「ソースレビュー単元2.md」の02_01〜02_09と、自作エンジン・「たたかうせんしゃ」の現在の実装を照合しました。資料はレビュー基準として扱い、外部ライブラリの実装を自作の設計実績には数えていません。

**主な不足を修正し、既存テスト21本、実ゲームのデモ、Debug・Development・Releaseの全件リビルドが成功しました。ただし、9項目すべてについて「全箇所が対応済み」「AI採点で減点されない」とは判定しません。特に2-6の演出用数値と2-8の旧メンバー命名には、厳密な採点で指摘される余地が残っています。**

実際の採点プロンプト・点数配分は未提示です。以下では基本条件の成立、今回改善した部分、残る懸念を区別しています。

## 項目別判定

| 項目 | 判定 | 根拠・今回の修正 |
| --- | --- | --- |
| 2-1 データドリブン | 実装あり・改善 | マップ種類のID・名前・アイコン・色を共通表へ集約。5つの戦闘ContextのState選択を配列へ変更。既存の敵・強化・部屋JSONも実行時に利用される |
| 2-2 調整項目の外部化 | 基本条件を満たす | 敵編成・壁・開始位置・部屋・経路・強化・基礎性能・演出をJSONから読み込む。制作ツールに適用・保存・再読込がある |
| 2-3 関数化 | 実装あり・改善 | カードとプレビューの透明度操作、通常モデルとスキニングモデルの文字列処理を共通関数へ集約。既存の共通攻撃・衝突・表示関数も利用される |
| 2-4 const参照渡し | 発見した不要なコピーを修正 | 読取専用の文字列・Vector3・Vector4・入力spanをconst参照へ変更。小さな数値は値渡し。所有権移動と出力用参照は維持 |
| 2-5 自分のコードでnew/delete禁止 | 対象ソースで成立を確認 | ヒープ確保する3つのSingletonを関数内staticへ変更。DXCのCOM実装はWRLで管理。コメント・文字列・`= delete`を除いた検査で明示的new/deleteは0件 |
| 2-6 マジックナンバー | 改善・全体対応は未完了 | ドローンの時間と表示を共通定数にし、レール・リンク・斬撃・パリィの調整値を名前付き定数へ変更。既存JSONで外部化した値も多いが、演出や一部の能力計算に直書き数値が残る |
| 2-7 エンジンnamespace | 対象ソースで成立を確認 | エンジン107個のh/cppに`cg2`名前空間を確認。ゲーム側は`cg2::`で利用。自作ヘッダーの`using namespace`は0件 |
| 2-8 命名規則 | 改善・全体統一は未完了 | 公開APIの誤字・表記ゆれ、Easing関数、DroneMissionのprivate名を整理。旧クラスには`textureIndex`と`position_`などの混在が残る |
| 2-9 警告ゼロ | 全3構成で成立を確認 | 全構成の自作コードをW3・警告エラー扱い、リンカも警告エラー扱いに設定。全件リビルド結果は下表に記載 |

## データ・処理の共通化

[TankExpeditionMap.h](../../project/game/run/TankExpeditionMap.h)の種類別定義を、ゲーム画面・マップエディター・JSONのID変換で共有するようにしました。ゲームとエディターで色が異なる項目は、元の色をそれぞれ保持しています。未知の種類の扱いと、不正なIDを読んだときに出力値を書き換えない性質も維持し、保存形式のテストを追加しました。

[突進敵](../../project/game/exp/ExpEnemyCombatCycle.h)、[射撃敵](../../project/game/exp/ExpEnemyMagazineCycle.h)、[近接敵](../../project/game/exp/ExpGuardCombat.h)、[ボス](../../project/game/enemy/actor/RivalBossCombat.h)、[ドローン](../../project/game/player/TankSpecialCombat.h)のState選択は同じ処理で戻すオブジェクトだけが違うため、配列にしました。列挙値と表の数の対応をstatic_assertで確認し、範囲外には従来の初期Stateを返します。状態ごとの行動・遷移はStateクラスに残しています。

[ColorMath.h](../../project/game/ui/ColorMath.h)で透明度の乗算を共有し、HDRのRGB値を維持します。[StringUtils.h](../../project/DirectX/engine/commom/StringUtils.h)でモデル読込に重複していた小文字化と文字列検索を共有します。

分岐の削除自体を目的にはしていません。射撃と近接など処理内容が異なる行動、入力条件、異常データの検証は分岐が必要です。色や数値を選ぶだけの分岐については、今回の表以外にも整理候補が残ります。

## 外部化の実装根拠

| 調整対象 | 保存先（projectからの相対パス） | 制作機能 |
| --- | --- | --- |
| 自機・ドローン・近接・ボスの基礎性能 | `resources/configs/tankExpeditionBalance.json` | F2で適用・保存 |
| 機体・軌跡・残像・発光・画面効果 | `resources/configs/gameVisuals.json`、`gamePostEffects.json` | F3で適用・保存 |
| 敵配置・壁・開始位置・勝利条件 | `resources/maps/expedition_layouts.json` | F4の部屋エディター |
| 部屋抽選・列範囲・重み・経路 | `resources/configs/expedition_map.json` | F5のマップエディター |
| 敵・強化商品・能力倍率 | `resources/configs/expedition_content.json` | F6のコンテンツエディター |

操作・反映時期は[制作ツールの案内](../tank-expedition-authoring-guide.md)にあります。制作ツールを有効にした構成で確認できます。提出版は既存方針どおり制作UIを無効にできます。今回、これらのJSONの調整値・ID・スキーマを変更していません。

## 引数・所有権・名前空間

読み取り専用の大きな値はconst参照にしました。Spriteのテクスチャパス、音声名・パス、描画補助関数のVector3/Vector4などが対象です。入力を加工する関数は、const参照から必要な作業用の値を明示的に作ります。float・uint32_tなどは値渡しにしています。

`SceneRegistry`・`GameModuleRegistry`の登録名とFactory、`StartupTrace::Scope`の名前、音声データの所有コンテナは、値を受け取って`std::move`する経路を維持しています。これは資料でも認められた所有権移動の用途です。出力先を書き換える参照までconstにはしていません。戻り値のconst参照は、メンバーや静的定義の寿命を利用するものです。

`ModelManager`・`TextureManager`・`TextRenderer`は関数内staticを使い、既存のFinalizeで保持資源を解放します。TextRendererの再度のFinalizeでもGDI+を二重終了しない形です。DXCのIncludeRecorderはWRLのRuntimeClass/Makeを利用し、独自の参照カウントと`delete this`を除去しました。

`ResourceObject`はComPtrを持ちながらデストラクタで手動Releaseも呼んでいたため、二重解放の危険がありました。ComPtrに破棄を任せる形に修正しています。このクラスの現行利用は見つかっておらず、修正前に実ゲームで障害が発生していたという判定ではありません。

エンジンは`cg2`へまとめ、ゲーム専用の`Phase`・`AttackParam`・`BulletOwner`を[CombatTypes.h](../../project/game/weapon/CombatTypes.h)へ移しました。Windowsや外部ライブラリのインクルードは名前空間の外に置き、外部ライブラリのソースは名前空間化していません。

今後エンジンを利用するコードでは`cg2::Vector3`、`cg2::Sprite`などの指定が必要です。今回、既存ゲームとテストの呼び出し元も変更しています。外部の未収録プロジェクトが従来のグローバル名を使っている場合は、同じ移行が必要です。

## 命名と残る採点上の懸念

今回変更したコードでは、型・関数はPascalCase、ローカル変数・値データはcamelCase、privateメンバーはcamelCaseの末尾に`_`、定数は`k`＋PascalCaseを基本にしています。既存のJSONキーや数学の記号は互換性・式の可読性を優先しています。

公開APIは`GetTextureIndexByFilePath`、`CreateDirectionalLight`、`CreateDepthStencilResource`、`D3DResourceLeakChecker`へ修正し、30種類のEasing関数を`EaseInQuad`などへそろえました。円周率のヘッダーマクロも実装内の定数へ移し、式の値を維持しています。

ただし、例えばSpriteの`textureIndex`、ModelManagerの`models`などは末尾`_`のないprivateメンバーです。2-8をプロジェクト全体の完全統一として採点する場合、ここは減点候補です。また、TankSpecialCombatの一部の係数やGameSceneの描画レイアウト・演出係数に、意味を名前にしていない数値が残っています。2-6も「直書き数値をすべてなくした」とは説明できません。

採点説明では、外部JSON、今回の共通表・共通関数・名前付き定数、必要な分岐と所有権移動の理由を根拠として提示してください。命名規則の記述だけで旧コードの混在が解消するわけではありません。残る統一は、クラス単位で呼び出し元と保存形式を確認しながら進める作業です。

## 警告設定と検証

Visual Studio 2026 / MSVC v145、x64で検証しました。自作プロジェクトはDebug・Development・Releaseの全構成で`WarningLevel=Level3`、`TreatWarningAsError=true`、`TreatLinkerWarningAsErrors=true`です。

DirectXTexの自作ではないソースは既存のWallを維持しています。Windows SDK/STLの外部ヘッダーにはW3を設定しました。既定の外部W4ではSDKのC4865が出るため、課題のW3条件を保った設定にそろえています。特定の警告を全体で無効にしたり、SDK・外部ライブラリの実装を書き換えたりしていません。外部ヘッダーの警告レベルは、[MSVCの公式説明](https://learn.microsoft.com/en-us/cpp/build/reference/external-external-headers-diagnostics?view=msvc-170)に沿った設定です。

| 検証 | 結果 |
| --- | --- |
| Development x64・ソリューション全件Rebuild | 成功、警告0・エラー0 |
| Release x64・制作UI無効・全件Rebuild | 成功、警告0・エラー0 |
| Debug x64・ソリューション全件Rebuild | 成功、警告0・エラー0 |
| 既存テスト21本 | 成功。敵AI・ボス・追加能力・弾・衝突・軌跡・マップ・部屋・強化・チュートリアル・描画・音声・キャッシュ・フレーム制御など |
| 弾と特殊能力 | 成功。640通りの組み合わせ、所有者、耐久、反射、貫通、ドローン、レール、パリィ |
| 軌跡 | 14,016頂点が一致。キャッシュ、GPU資源の寿命、容量制限も成功 |
| カード | 1,032,192プレビューサンプル、50,400描画フレームサンプルが成功 |
| 制作版の実際のタイトルデモ | 成功。4段階、457発射サンプル、36撃破、32ダッシュ、フェード、新しい遠征への遷移、正常終了 |
| 静的検索 | エンジン107ファイルにcg2、自作ヘッダーのusing namespace 0件、自作new/delete 0件 |

ビルドログは`generated/source-review-unit2-development-rebuild.log`・`generated/source-review-unit2-release-rebuild.log`・`generated/source-review-unit2-debug-rebuild.log`、テストログは`generated/source-review-unit2-test_*.log`、実行結果は`project/generated/title_demo/validation.json`です。各既存テストは`project/tools/test_*.ps1`から再実行できます。全件リビルドにはMSBuildの`/t:Rebuild`を指定しました。

今回、ゲームの調整ファイルや攻撃力・速度・待ち時間の値は維持しています。検証した範囲でゲームの回帰は確認されていません。長時間プレイ全体の保証や、変更前後のFPS比較を実施したという結果ではありません。
