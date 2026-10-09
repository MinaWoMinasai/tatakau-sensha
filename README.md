# たたかうせんしゃ

**戦車を操り、強化を組み合わせながら最深部のボスを目指す、見下ろし型ローグライトアクション。**

C++ / DirectX 12 による自作エンジンで制作しています。毎回変わる作戦ルートから進路を選び、戦闘で回収した通貨を改造・進化・修理へ振り分けて攻略します。

作者：**MinaWoMinasai**
公式リポジトリ：[MinaWoMinasai/tatakau-sensha](https://github.com/MinaWoMinasai/tatakau-sensha)

作品紹介とソースレビューのために公開しています。自作部分の利用条件は[COPYRIGHT.md](COPYRIGHT.md)、第三者製コード・素材の表示と確認事項は[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)を参照してください。

## ゲームの特徴

- 戦闘、精鋭、改造工房、進化、修理などをつなぐ分岐ルート。地形には制作済みの部屋テンプレートを使います。
- シューター、ドローン、近接の3系統と、それぞれの攻撃・強化・進化。
- マウスでの照準、スタミナを使うダッシュ、体当たり。近接には連撃とパリィがあります。
- 強化カードのプレビュー、ネオン表現、通貨回収、実際のゲーム処理を使ったタイトル背景デモ。
- 操作を学ぶチュートリアルと、訓練を省略して進む経路。

| 系統 | 基本攻撃 | 強化の例 |
| --- | --- | --- |
| シューター | 照準方向への主砲射撃 | 追尾、壁反射、敵の貫通、射撃性能 |
| ドローン | 自機に追従するドローンからの射撃 | 支援機追加、集中射撃、迎撃弾 |
| 近接 | 3段の斬撃コンボ | 間合い、連撃速度、3段目の威力 |

通貨・強化は遠征ごとの成長です。次の遠征へ通貨を持ち越す永続強化システムではありません。チュートリアルの履修状態はローカルに保存します。

## 操作方法

タイトルの「遠征をはじめる」から開始します。キーボードとマウスを使用します。

| 入力 | 操作 |
| --- | --- |
| WASD | 移動 |
| マウス | 照準 |
| 左クリック／長押し | 攻撃。メニューでは選択・決定 |
| 右クリック | スタミナを使ってダッシュ |
| Esc | ポーズ／開いている情報画面を閉じる |
| G（戦闘中） | 作戦マップを確認 |
| Tab（戦闘中） | ビルド詳細を確認 |
| M / N | BGM / SEの切り替え |

制作向けのF1～F6は通常のReleaseでは無効です。制作ツールとNeon Skinned PreviewはDevelopmentで使用します。Previewのモデル・素材の情報は[モデルREADME](project/resources/models/neon_hologram/README.md)にあります。

## 開発環境・ビルド方法

Windows / x64、C++20、DirectX 12を使用します。「C++によるデスクトップ開発」、Windows SDK、CMake / Ninjaをインストールしてください。実行にはDirectX 12対応環境が必要です。同梱の日本語フォントに加え、UIの一部でWindowsのMeiryoを使用します。

プロジェクト既定はVisual Studio 2026 / MSVC v145、GitHub ActionsはVisual Studio 2022 / MSVC v143です。

### 1. 外部依存を準備する

リポジトリのルートで実行します。初回はAssimpのダウンロードにネットワーク接続が必要です。

```powershell
.\project\build\bootstrap_dependencies.ps1
```

標準スクリプトはVS 2022のC++ / CMake / Ninjaを探し、Assimp v5.4.3のglTF importerをビルドします。生成するライブラリとDLLはGit管理対象外です。既存のローカルバイナリがある場合は準備を省略します。VS 2026のみの環境では、[Assimpの案内](project/externals/assimp/README.md)と[VS 2026用スクリプト](project/build/build_assimp_vs2026.ps1)を確認してください。

初回準備ではAssimpヘッダーもコピーされるため、実行後に差分を確認してください。通常の準備に`-Force`は不要です。

### 2. Releaseをビルドする

Visual StudioのDeveloper PowerShellで、環境に合う一方を実行します。

```powershell
# VS 2022 / CIと同じ指定
MSBuild.exe project/CG2.sln /m /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:CG2DeveloperTools=false

# VS 2026 / プロジェクト既定のv145
MSBuild.exe project/CG2.sln /m /p:Configuration=Release /p:Platform=x64 /p:CG2DeveloperTools=false
```

GUIでは`project/CG2.sln`を開き、Release / x64を選びます。VS 2022では両プロジェクトのツールセットをv143へ合わせる必要があります。

| ソリューション構成 | 実際のプロジェクト構成 | 開発機能の標準値 |
| --- | --- | --- |
| Release / x64 | Release | OFF |
| Development / x64 | Development | ON |
| Debug / x64 | Developmentに割り当て済み | ON |

`.vcxproj`自体にはDebug定義もありますが、ソリューションのDebugはDevelopmentをビルドします。

### 3. 起動する

出力は`generated/outputs/Release/CG2.exe`です。素材の相対パスを解決できるよう、作業ディレクトリを`project`にします。

```powershell
Push-Location project
try {
    & ..\generated\outputs\Release\CG2.exe --project resources/projects/tank_game.project.json
} finally {
    Pop-Location
}
```

実行ファイルだけをコピーしても動作しません。配布にはAssimp、DXC / DXILのDLL、`resources`と対応する第三者表示が必要です。同梱素材の出典・再配布条件に未確認項目があるため、[第三者表示と確認事項](THIRD_PARTY_NOTICES.md)を確認してください。

## リポジトリ構成・ソースの入口

| 場所 | 内容 |
| --- | --- |
| `project/main.cpp` | 起動、プロジェクト選択、終了処理 |
| `project/game/scene/` | タイトル、ゲームへの入口、制作向けシーン |
| `project/game/session/`・`run/session/` | 更新順、作戦マップ、通貨回収、遠征進行 |
| `project/game/run/` | ルート、部屋、強化候補、チュートリアル |
| `project/game/player/`・`enemy/`・`exp/` | 攻撃・移動、敵・ボス |
| `project/game/ui/`・`editor/` | 強化カード、制作ツール |
| `project/DirectX/engine/` | 描画、入力、音声、シーン管理 |
| `project/resources/` | シェーダー、設定、モデル、画像、音声、フォント |
| `project/externals/` | 第三者ライブラリ |
| `project/build/` | クローン後のビルドに必要な依存準備 |
| `licenses/third-party/` | 補完した第三者ライセンス本文 |
| `.github/workflows/` | ビルドと不要ファイルの検査 |
| `generated/` | ローカルのビルド・テスト成果物 |

[作戦ルート](project/game/run/TankExpeditionMap.h)、[強化候補](project/game/run/TankExpeditionContent.h)、[プレイヤー](project/game/player/actor/Player.cpp)がゲーム進行と戦闘を読む入口です。プレイヤーは`actor/`、`combat/`、`progression/`、`ui/`、`editor/`へ役割ごとに分離しています。

`docs/`、`project/tools/`、ルートの`tools/`は**フォルダー全体をローカル専用**にしています。既存の調査資料・補助ツール・回帰テストは作者の手元に残し、新しく追加するファイルとサブフォルダーも自動でGit対象外になります。クローンには含まれません。GitHub Actionsは共有ソースのビルドと、これらのローカルファイルが混入していないことを検査します。

## 作者・著作権

**Copyright © 2025-2026 MinaWoMinasai. All Rights Reserved.**

作者が権利を持つ自作部分の表示です。第三者のライセンスによる権利は妨げません。詳しくは[COPYRIGHT.md](COPYRIGHT.md)と[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)を参照してください。
