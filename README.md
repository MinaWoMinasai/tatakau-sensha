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
# 日本語の設定画面から起動（Development）
.\project\tools\run_ink_shooter.ps1 -Settings
# 45秒の動作・負荷検証と比較画像の保存
.\project\tools\run_ink_shooter.ps1 -Validate
# 27秒の変身・飛沫・敵インク検証と比較画像の保存
.\project\tools\run_ink_shooter.ps1 -Fidelity
# 35秒のレティクル・トライストリンガー検証と画像保存
.\project\tools\run_ink_shooter.ps1 -Weapons
# 23秒のイカ慣性・合成SEの検証
.\project\tools\run_ink_shooter.ps1 -Feel
# DirectXを起動せず、移動・塗り・射撃の回帰テストを実行
.\project\tools\test_ink_simulation.ps1
```

WASD: 移動、マウス: 照準、左クリック: シューターは長押し連射／ストリンガーは溜めて離すと発射、Shift: イカ変身（自インクで潜伏・遊泳）・チャージ中断、Space: ジャンプ。
1: シューター、2: トライストリンガー、Q: 登録した次のブキ。ストリンガーは地上で横3本、空中で縦3本を撃ち、1段階以上の矢は地形着弾から0.75秒後に爆発します。
塗った壁へ Shift + W で登れます。Tab でマウス解放、F1 で日本語調整（Development）、R でリセット。
F10 で画面を `project/generated/ink_phase5/manual.png` に保存します（`-Validate` は `ink_phase2`、`-Fidelity` は `ink_phase3`、`-Weapons` は `ink_phase4`）。
オレンジ色のダミーは HP 100。直前のダメージ・命中距離・撃破弾数を表示し、撃破後2秒で復活します。
未塗装・空中・インク切れでもイカに変身できます。日本語設定の「相手インクの試験帯を置く」で減速・低いジャンプ・塗り返しを試せます。

F1 の「ブキを選ぶ・作る」で、日本語名やブキの数値を編集し、「このブキで試し撃ち」で適用できます。「複製して新しいブキを作る」で同種のブキを増やし、「ブキ一覧をファイルに保存」で `project/resources/configs/ink_weapons.json` に保存します。編集・適用・保存はそれぞれ独立しています。

イカで自色インクから未塗装へ出ると、直前の速度を残して滑らかに減速します。F1 の「イカの慣性」で余韻・減速・切り返しの制動を調整できます。数値はCG2の操作感調整です。
「SE・音量」には独自合成した発射・チャージ・着弾・爆発音の試聴と音量調整があり、音量は `project/resources/configs/ink_audio.json` に保存できます。元動画の波形はSE素材に使っていません。

第5段階の操作・検証・試聴は [慣性と独自合成SEの実装報告](docs/ink_phase5_implementation.md)、詳細は
[移動の調査](docs/ink_phase5_movement.md)、[音の調査と合成](docs/ink_phase5_audio_research.md)、[音声ランタイム](docs/ink_phase5_audio_runtime.md) を参照してください。

第4段階の操作・設計・検証は [レティクルと複数ブキの実装報告](docs/ink_phase4_implementation.md)、根拠は
[レティクル](docs/ink_phase4_reticle_research.md)、[トライストリンガー](docs/ink_phase4_stringer_research.md)、[ブキ編集基盤](docs/ink_phase4_weapon_architecture.md) を参照してください。

第3段階の変更・検証は [追加再現の実装報告](docs/ink_shooter_phase3_implementation.md)、根拠は
[移動](docs/ink_shooter_phase3_movement_research.md)、[飛沫・足元塗り](docs/ink_shooter_phase3_paint_research.md)、[射撃](docs/ink_shooter_phase3_shooting_research.md) に整理しています。

第2段階の変更・検証・比較画像は [再現度向上の実装報告](docs/ink_shooter_phase2_implementation.md)、
塗りパラメータと任意メッシュへの拡張案は [第2段階の調査](docs/ink_shooter_phase2_research.md) を参照してください。

仕様・パラメータ・既知の制約は [実装報告](docs/ink_shooter_implementation.md)、
原作値と独自の近似の区別は [調査資料](docs/ink_shooter_research.md) を参照してください。
