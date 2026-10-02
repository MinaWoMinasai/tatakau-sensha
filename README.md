# たたかうせんしゃ

**戦車を操り、強化を組み合わせながら最深部のボスを目指す、見下ろし型ローグライトアクション。**

C++ / DirectX 12 による自作エンジンで制作しています。毎回変わる作戦ルートから進路を選び、戦闘で回収した通貨を改造・進化・修理へ振り分けて攻略します。

作者：**MinaWoMinasai**
公式リポジトリ：[MinaWoMinasai/tatakau-sensha](https://github.com/MinaWoMinasai/tatakau-sensha)

このリポジトリは作品紹介・ソースレビューのために公開するものです。自作部分の利用条件は [COPYRIGHT.md](COPYRIGHT.md)、外部コード・素材については [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) を参照してください。

## ゲームの特徴

- **進路を選ぶ遠征**：戦闘、精鋭、改造工房、進化、修理などをつないだ分岐ルートを進みます。経路と部屋の組み合わせが変化し、地形には制作済みの部屋テンプレートを使います。
- **3つの戦闘スタイル**：導入の訓練・強化の後に、シューター・ドローン・近接から選択します。
- **挑戦中に作るビルド**：敵から得た通貨 Cr で強化を購入。現在の系統に有効な効果を重ね、同系統の進化や修理との配分を考えます。
- **攻撃と回避の手触り**：マウスで狙い、スタミナを使うダッシュや体当たりを組み合わせます。近接には連撃・パリィ、射撃には追尾・反射・貫通などの強化があります。
- **選択を伝える演出**：強化カードのレア度表示と動作プレビュー、ネオン表現、通貨回収のアニメーション、実際のゲーム処理を使ったタイトル背景デモを実装しています。
- **導入チュートリアル**：操作を学ぶ経路と、訓練を省略して進む経路を選べます。

### 戦車タイプと成長

| 系統 | 基本攻撃 | 強化の例 |
| --- | --- | --- |
| シューター | 照準方向への主砲射撃 | 追尾、壁反射、敵の貫通、射撃性能 |
| ドローン | 自機に追従するドローンからの射撃 | 支援機追加、集中射撃、迎撃弾 |
| 近接 | 3段の斬撃コンボ | 間合い、連撃速度、3段目の威力 |

改造の候補には汎用強化と系統別強化があり、取得済み効果や前提条件によって候補が変わります。進化せずに改造を重ねる選択もできます。

**通貨・強化は遠征ごとの成長です。次の遠征に通貨を持ち越す永続強化システムではありません。** チュートリアルの履修状態はローカルに保存します。詳しい遊び方は [戦闘と作戦ルート](docs/tank-action-and-route-guide.md) を参照してください。

## 操作方法

キーボードとマウスを使用します。

| 入力 | 操作 |
| --- | --- |
| WASD | 移動 |
| マウス | 照準 |
| 左クリック／長押し | 攻撃。メニューでは選択・決定 |
| 右クリック | スタミナを使ってダッシュ |
| Esc | ポーズ／開いている情報画面を閉じる |
| G（戦闘中） | 作戦マップを確認 |
| Tab（戦闘中） | ビルド詳細を確認 |
| M / N | BGM / SE の切り替え |

タイトルの「遠征をはじめる」から開始します。制作向けの F1～F6 は通常の Release では無効です。詳細は [制作版と提出版の切り替え](docs/developer-tools-switch.md) にまとめています。

## スクリーンショット

実プレイ画像の掲載場所です。画像を `docs/images/` に追加して、下のコメントを外すと表示できます。撮影する場面の案は [画像の追加案内](docs/images/README.md) を参照してください。

<!-- 実際の画像を追加してから有効にしてください。
![戦闘画面](docs/images/combat.png)
![作戦ルート](docs/images/route.png)
![強化の選択](docs/images/upgrades.png)
-->

## 開発環境・ビルド方法

Windows / x64、C++20、DirectX 12 を使用します。DirectInput、XInput、XAudio2、Media Foundation、DirectWrite など Windows の API に依存します。

- プロジェクト既定：**Visual Studio 2026 / MSVC v145**。
- 既存 CI と依存ライブラリの標準準備手順：**Visual Studio 2022 / MSVC v143**。
- 「C++ によるデスクトップ開発」、Windows SDK、CMake / Ninja をインストールしてください。実行には DirectX 12 対応環境が必要です。
- 同梱の日本語フォントに加え、UI の一部で Windows の Meiryo を使用します。

### 1. 外部依存を準備する

リポジトリのルートで実行します。初回は Assimp のダウンロードにネットワーク接続が必要です。

```powershell
.\project\tools\bootstrap_dependencies.ps1
```

標準スクリプトは VS 2022 の C++ / CMake / Ninja を探し、Assimp v5.4.3 の GLTF importer をビルドします。生成する `.lib` / `.dll` は Git 管理対象外です。既存のローカルバイナリがある場合は処理を省略します。VS 2026 のみの環境や別の importer が必要な場合は [Assimp の案内](project/externals/assimp/README.md) と `project/tools/build_assimp_vs2026.ps1` を確認してください。

初回準備では Assimp ヘッダーもコピーされるため、実行後は差分を確認してください。通常のゲームビルドに `-Force` は不要です。

### 2. Release をビルドする

Visual Studio の Developer PowerShell で、リポジトリのルートから実行します。

```powershell
# VS 2022 / 既存 CI と同じ指定
MSBuild.exe project/CG2.sln /m /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:CG2DeveloperTools=false

# VS 2026 / プロジェクト既定の v145 を使用
MSBuild.exe project/CG2.sln /m /p:Configuration=Release /p:Platform=x64 /p:CG2DeveloperTools=false
```

使用する環境に合う一方を実行してください。GUI では `project/CG2.sln` を開き、Release / x64 を選びます。VS 2022 では両プロジェクトのツールセットを v143 に合わせる必要があるため、上記のコマンド指定が簡単です。

| ソリューション構成 | 実際のプロジェクト構成 | 開発機能の標準値 |
| --- | --- | --- |
| Release / x64 | Release | OFF |
| Development / x64 | Development | ON |
| Debug / x64 | **Development に割り当て済み** | ON |

`.vcxproj` 自体には Debug 定義もありますが、`CG2.sln` から Debug を選ぶと両プロジェクトとも Development をビルドします。既存設定を維持しており、Debug と Development を独立した検証結果として扱わないでください。

Neon Skinned PreviewはPowerShell不要で確認できます。Visual StudioでDevelopment / x64を選び、F5またはCtrl+F5で通常起動 → タイトルの「遠征をはじめる」 → F3「制作ツール」の「Neon Skinned Previewを開く」 → `Preview Enable`をオンにします。F12の「Neon Preview」タブからも開けます。`Normal` / `Neon`で同じAvatarSample_Bの描画を比較できます。Neonの既定表示は暗い本体 + ピンクの外周線 + Texture由来の内部特徴線で、線幅・色・HDR強度・内部線の閾値を調整できます。[操作・モデル情報](project/resources/models/neon_hologram/README.md)。Releaseでは無効です。

### 3. 起動する

出力は `generated/outputs/Release/CG2.exe` です。ソースからの起動では、素材の相対パスを解決できるよう作業ディレクトリを `project` にします。

```powershell
Push-Location project
try {
    & ..\generated\outputs\Release\CG2.exe --project resources/projects/tank_game.project.json
} finally {
    Pop-Location
}
```

実行ファイルだけを別の場所へコピーしても動作しません。Assimp、DXC / DXIL の DLL と `resources` が必要です。配布フォルダーの作成方法は [提出用 Release](docs/submission-package.md) を参照してください。**旧素材を含む再配布条件は調査中です。公開・配布前に [素材監査の要確認項目](docs/public-assets-audit.md) を確認してください。**

### 4. 既存テスト

以下はリポジトリのルートから実行できます。C++ テストには Visual Studio の C++ ツールが必要です。

```powershell
.\project\tools\test_tank_expedition_map.ps1
.\project\tools\test_tank_reward_pool.ps1
.\project\tools\test_tank_submission_packaging.ps1
# Release / Development のビルド後に実行
.\project\tools\test_developer_tools_profile.ps1
```

自動テストの成功は実機での操作感や難易度の評価を意味しません。今回の実行結果と制約は [Lab除去記録](docs/remove-graphics-labs-audit.md)、素材整理時の結果は [素材監査記録](docs/public-assets-audit.md)、初回の結果は [公開準備の監査記録](docs/public-release-audit.md) に記載しています。

## リポジトリ構成・ソースの読み方

| 場所 | 内容 |
| --- | --- |
| `project/main.cpp` | 起動、プロジェクト選択、終了処理 |
| `project/game/scene/TitleScene.*` | タイトルと背景のゲームデモ |
| `project/game/scene/GameScene.Expedition*.cpp` | 作戦マップ、通貨回収、進行・演出 |
| `project/game/run/` | 遠征ルート、部屋、強化候補、チュートリアル |
| `project/game/player/`・`enemy/`・`exp/` | 戦車の攻撃・移動、敵・ボス |
| `project/game/ui/`・`editor/` | 強化カードと制作ツール |
| `project/DirectX/engine/` | 描画、入力、音声、シーン管理などのエンジン |
| `project/resources/` | シェーダー、設定、モデル、画像、音声、フォント |
| `project/externals/` | 第三者ライブラリ。作者の自作部分とは区別 |
| `project/tools/` | 依存準備、既存回帰テスト、配布用ツール |
| `docs/` | [資料一覧](docs/README.md)、遊び方、実装資料、公開監査 |
| `.github/workflows/` | 3構成のビルドと不要ファイル検査 |
| `generated/` | ビルド・テスト成果物。Git 管理対象外 |

まず [作戦ルート](project/game/run/TankExpeditionMap.h)、[強化候補](project/game/run/TankExpeditionContent.h)、[プレイヤー](project/game/player/actor/Player.cpp) を読むと、ゲーム進行と戦闘の関係を追えます。

[ソースレビュー単元1の判定と修正報告](docs/source-review-unit1/README.md)に、カプセル化・ポリモーフィズム・State・その他のデザインパターンの確認先と、提出用UML画像をまとめています。

[ソースレビュー単元2の判定と修正報告](docs/source-review-unit2/README.md)に、データドリブン・外部化・関数化・参照渡し・所有権・定数・名前空間・命名・警告ゼロの確認結果をまとめています。

旧Graphics / Underwater / VFX Labのシーン・独立module・起動設定・専用素材はPublic版から除去しました。Tankが使う素材と共通Engine機能は保持し、Ocean / Animation / Skinning等の汎用実装も今回のLab除去では残しています。[Lab除去の分類と検証](docs/remove-graphics-labs-audit.md)を参照してください。過去の整理記録は[資料一覧](docs/README.md)にまとめています。

Public版の[最終クリーンアップ監査](docs/final-public-cleanup-audit.md)では全追跡ファイル・動的参照・素材の出典・配布内容を再確認しました。用途・出典が未確定の素材は保持し、試聴専用音声だけを実行用配布から除外しています。その後の[旧起動口の整理](docs/retire-legacy-run-entrypoints-audit.md)で旧モードの外部登録と専用設定・ランチャーを除去し、正式なTITLE→TANK_EXPEDITIONと共有Engine・TankRun実装を維持しています。

## 作者・著作権・Third-party software

**Copyright © 2025-2026 MinaWoMinasai. All Rights Reserved.**

この表示は作者が権利を持つ自作部分に適用します。プロジェクト全体に MIT / Apache / GPL などを新規適用するものではありません。自作ソース・自作素材の無断再配布、販売、別作品への流用、改変版の配布を許諾していません。法令上認められる利用、GitHub の利用規約に基づく利用、第三者のライセンスによる権利は妨げません。

- [著作権と利用条件](COPYRIGHT.md)
- [Third-party software・素材の表示と確認事項](THIRD_PARTY_NOTICES.md)
- [作者情報・実行ファイルへの表示案](docs/credits.md)
