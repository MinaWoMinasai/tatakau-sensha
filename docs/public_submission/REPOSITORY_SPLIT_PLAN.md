# 採点用Publicリポジトリ分離計画

作成日: 2026-07-06  
対象: `MinaWoMinasai/CG2`  
目的: 学校のPublicリポジトリ要件を満たしながら、本命ゲーム、独自演出、制作素材を非公開側へ分離する。

## 結論

現在のリポジトリを直接削減しない。現行リポジトリをPrivateの開発本体として保存し、Git履歴を引き継がない新しいPublic採点版を別に構築する。

Public版は「授業で明示された課題の最小実装」と、それらを確認できる専用デモだけを含める。Bloom、Shadow Map、本命2Dシューティング、3Dアクション、海戦企画、VRoid/Mixamo素材は含めない。

## 1. 課題画像から確認できた公開対象

### CG4 00: 環境マップ

- 天球
- 環境マッピング
- 最小の活用デモ

### CG4 01: Effect

- Primitive
- Ring
- Cylinder
- Effectクラス化と最小の組み込み例

### CG4 02: Skinning

- Animation
- Skeleton
- DrawIndexed
- Skinning
- 最小の操作キャラクターまたは再生デモ

### CG4 03: GPU Particle

- GPUの有効活用
- Compute ShaderによるParticle更新
- Particle発生、更新、再利用
- ExecuteIndirectによる描画
- 最小のゲーム内活用

### CG5 00: PostEffect

- Offscreen Rendering
- Grayscale
- Vignette
- Smoothing
- Gaussian Filter
- Outline
- Radial Blur
- Dissolve
- Random

## 2. ファイル分類

### A. Public必須候補

授業課題の成立とビルドに必要な最小部分。

- `project/DirectX/engine/commom/DirectXCommon.*`
- `project/DirectX/engine/commom/WinApp.*`
- `project/DirectX/engine/commom/SrvManager.*`
- `project/DirectX/engine/commom/RtvManager.*`
- `project/DirectX/engine/commom/Resource.*`
- `project/DirectX/engine/commom/LogWrite.*`
- `project/DirectX/engine/struct/Struct.h`
- `project/DirectX/engine/calc/Calculation.*`
- `project/DirectX/engine/input/Input.*`
- `project/DirectX/engine/debugCamera/Camera.*`
- `project/DirectX/engine/debugCamera/DebugCamera.*`
- `project/DirectX/engine/2d/TextureManager.*`
- `project/DirectX/engine/3d/Model.*`
- `project/DirectX/engine/3d/ModelCommon.*`
- `project/DirectX/engine/3d/ModelManager.*`
- `project/DirectX/engine/3d/Object3d.*`
- `project/DirectX/engine/3d/Object3dCommon.*`
- `project/DirectX/engine/3d/Animation.*`
- `project/DirectX/engine/3d/Skeleton.*`
- `project/DirectX/engine/3d/SkinCluster.*`
- `project/DirectX/engine/3d/Skybox.*`
- `project/DirectX/engine/commom/RingManager.*`
- `project/DirectX/engine/commom/CylinderManager.*`
- `project/DirectX/engine/particle/ParticleManager.*`
- 授業PostEffectに必要な最小オフスクリーン描画基盤
- 上記に対応するHLSL
- ImGuiによる課題確認UI
- Visual StudioプロジェクトとGitHub Actions

この一覧は依存解析前の候補であり、Public版では未使用APIと本命ゲーム固有APIを削る。

### B. Public用に簡略化するもの

現在のファイルは独自機能と密結合しているため、そのままコピーしない。

| 現在の実装 | Public版の扱い |
|---|---|
| `Game.cpp` | Bloom/Shadow/本命素材ロードを除いた`SubmissionApp`へ置換 |
| `TestScene.*` | 課題別の小さな`SubmissionScene`へ置換 |
| `ParticleManager.*` | Neon死亡演出等を除き、汎用EmitterとGPU更新だけ残す |
| `EffectSequencer.*` | 就職作品課題に必要な最小の発射・飛翔・着弾のみ |
| `PostEffect.*` | 授業指定9効果だけを選択できる最小実装へ分離 |
| `Object3dCommon.*` | 独自Shadow依存をPublic版では外す |
| `DirectXCommon.*` | Bloom/ObjectPost用PSOをPublic版では外す |
| `TitleScene.*` | 文字画像演出をやめ、テキスト主体の課題メニューへ置換 |

### C. Privateへ移すもの

- `project/game/player/`
- `project/game/enemy/`
- `project/game/exp/`
- `project/game/weapon/`
- `project/game/mapchip/`
- `project/game/level/`
- `GameScene.*`
- `PlayerLabScene.*`
- `Action3DScene.*`
- 現在の`TestScene.*`にあるVRoid、剣、コンボ、斜面、独自演出
- `TriangleMeshGround.*`を含む3Dアクション用キャラクター制御
- `Bloom.*`
- `BloomConstantBuffer.*`
- `ObjectPostEffect.*`
- `Shadow.*`
- `ShadowMap.*`
- `TrailManager.*`、`TrailInstance.*`の独自改良版
- `NeonGridRenderer.*`
- Bloom、Shadow、ObjectPost、Trail用HLSL
- `docs/naval_game/`
- `project/resources/configs/`
- `project/resources/levels/`
- 本命ゲーム固有の画像、音声、OBJ、JSON
- `project/resources/models/player/`
- `project/resources/Player_Mixamo.fbx`
- VRoid/Mixamoリターゲット用ツール
- Assimpの再ビルド用開発資産のうち採点版に不要なもの

## 3. 採点用デモ仕様

Public版は一つの小さなアプリとして起動し、数字キーまたはImGuiで課題を切り替える。

```text
Submission Demo
  1 Environment Map
  2 Primitive / Ring / Cylinder
  3 Animation / Skeleton / Skinning
  4 GPU Particle
  5 PostEffect
```

### Environment Map

- 天球と反射する球またはキューブだけを表示
- カメラ回転で映り込みを確認

### Primitive / Effect

- RingとCylinderを個別表示
- 色、半径、寿命をImGuiで変更
- 独自ネオン演出は含めない

### Skinning

- 再配布可能な教材モデルまたは自作の単純モデルのみ
- Idleまたは1本のAnimationを再生
- Joint表示、DrawIndexed、GPU Skinningの状態を表示
- VRoid、Mixamo、剣、戦闘は含めない

### GPU Particle

- クリック位置または固定位置からEmitterを発生
- GPU更新ON/OFF
- Active数、Dispatch、ExecuteIndirect状態を表示
- ダミー標的への命中でEffectを一度発生
- 本命ゲーム固有のNeon演出は含めない

### PostEffect

- 同じ確認画像または単純3Dシーンへ9種類を適用
- 一度に一効果だけ選択可能
- BloomとShadowは含めない

## 4. 素材方針

Public版に入れる素材は、次のいずれかに限定する。

- 自作の単純形状
- 学校から再配布を許可された教材素材
- CC0素材
- ライセンス上、ソースリポジトリへの同梱が明示的に許可された素材

VRoid、Mixamo FBX、制作中キャラクター、本命ゲーム用画像・音声は含めない。

## 5. AI利用に関する表示

Publicである以上、第三者による取得を技術的に完全防止できない。公開量そのものを最小化することを主対策とする。

Public版には、学校の方針確認後に次の趣旨の`NOTICE`を置く。

```text
This repository is published solely for educational assessment.
Automated analysis is permitted only for course grading.
Use for AI/ML model training, dataset creation, redistribution,
or incorporation into other software is not permitted.
```

これは技術的防止ではなく意思表示である。学校の採点AIによる処理を許可する必要があるため、包括的な「機械処理禁止」にはしない。

GitHub Copilot利用者設定では、`Allow GitHub to use my data for AI model training`をDisabledにする。ただし、この設定だけでPublicコードの第三者収集を防げるとは扱わない。

## 6. 移行手順

1. 現在のワークツリーを変更せず、分類表をレビューする。
2. `C:\tmp`またはワークスペース内の別フォルダへ採点版を複製する。
3. 新しい`SubmissionApp`と課題メニューを作る。
4. Public必須候補を依存順にコピーする。
5. 独自拡張を外した最小版へ置換する。
6. 最小素材だけを追加する。
7. Debug/Development/Releaseをビルドする。
8. `CheckUnwantedFiles`相当の検査を行う。
9. 各課題タグ、操作方法、対応ファイルをREADMEへ記載する。
10. 現行GitHubリポジトリをPrivate開発本体として保全する。
11. 履歴を引き継がない新しいPublic採点リポジトリを作る。
12. 採点版だけを新規履歴でPushする。

## 7. 移行前に学校へ確認する項目

- 採点済み課題について、Public公開を継続する必要があるか。
- 採点AIは現在の既定ブランチだけを見るか、過去コミットやPRも見るか。
- リポジトリURLを変更できるか。
- リポジトリを提出時だけPublicにする運用が許可されるか。
- 教材配布モデルをPublicリポジトリへ同梱できるか。
- 独自の`NOTICE`を追加してよいか。

特に「過去コミットやPRも採点対象か」が未確認のまま、現在のPublicリポジトリを改名・Private化しない。

