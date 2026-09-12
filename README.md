[![DebugBuild](https://github.com/MinaWoMinasai/CG2/actions/workflows/DebugBuild.yml/badge.svg)](https://github.com/MinaWoMinasai/CG2/actions/workflows/DebugBuild.yml)
[![ReleaseBuild](https://github.com/MinaWoMinasai/CG2/actions/workflows/ReleaseBuild.yml/badge.svg)](https://github.com/MinaWoMinasai/CG2/actions/workflows/ReleaseBuild.yml)
[![DevelopmentBuild](https://github.com/MinaWoMinasai/CG2/actions/workflows/DevelopmentBuild.yml/badge.svg)](https://github.com/MinaWoMinasai/CG2/actions/workflows/DevelopmentBuild.yml)
[![CheckUnwantedFiles](https://github.com/MinaWoMinasai/CG2/actions/workflows/CheckUnwantedFiles.yml/badge.svg)](https://github.com/MinaWoMinasai/CG2/actions/workflows/CheckUnwantedFiles.yml)

## Local dependencies

Generated build outputs and binary libraries are intentionally not committed to
this repository. In particular, Assimp must exist only as local generated files.
After downloading the project ZIP from GitHub, run:

```powershell
.\project\tools\bootstrap_dependencies.ps1
```

Then open `project/CG2.sln` in Visual Studio 2022 and build the solution.

The bootstrap step creates files such as:

- `project/externals/assimp/lib/assimp-vc143-mt.lib`
- `project/externals/assimp/runtime/assimp-vc143-mt.dll`

Do not add generated `.lib` or `.dll` files to Git; the repository health check
expects them to stay untracked.

## Ink Shooter Lab

「撃つ → 塗る → 潜る → 高速移動・補給」を試せるシーンを追加しています。
Development または Release をビルドし、タイトルで **F8** を押すか、リポジトリのルートで起動します。

```powershell
.\project\tools\run_ink_shooter.ps1
# 12秒の動作確認デモ
.\project\tools\run_ink_shooter.ps1 -Demo
# DirectXを起動せず、移動・塗り・射撃の回帰テストを実行
.\project\tools\test_ink_simulation.ps1
```

WASD: 移動、マウス: 照準、左クリック長押し: 射撃、Shift: 自インクで遊泳、Space: ジャンプ。
塗った壁へ Shift + W で登れます。Tab でマウス解放、F1 で調整（Development）、R でリセット。

仕様・パラメータ・既知の制約は [実装報告](docs/ink_shooter_implementation.md)、
原作値と独自の近似の区別は [調査資料](docs/ink_shooter_research.md) を参照してください。
