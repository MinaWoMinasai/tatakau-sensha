[![DebugBuild](https://github.com/MinaWoMinasai/CG2/actions/workflows/DebugBuild.yml/badge.svg)](https://github.com/MinaWoMinasai/CG2/actions/workflows/DebugBuild.yml)
[![ReleaseBuild](https://github.com/MinaWoMinasai/CG2/actions/workflows/ReleaseBuild.yml/badge.svg)](https://github.com/MinaWoMinasai/CG2/actions/workflows/ReleaseBuild.yml)
[![DevelopmentBuild](https://github.com/MinaWoMinasai/CG2/actions/workflows/DevelopmentBuild.yml/badge.svg)](https://github.com/MinaWoMinasai/CG2/actions/workflows/DevelopmentBuild.yml)
[![CheckUnwantedFiles](https://github.com/MinaWoMinasai/CG2/actions/workflows/CheckUnwantedFiles.yml/badge.svg)](https://github.com/MinaWoMinasai/CG2/actions/workflows/CheckUnwantedFiles.yml)

## CG4 評価課題2 追加要素

起動時の `Action3DScene` で以下を確認できます。

- GPU Skinningモデルの表示
  - AssimpでglTFのMesh、Skeleton、Animation、Weightを読み込み
  - Vertex Shader Skinning
  - Skinning済みモデルのShadowMap描画
  - WASD／ゲームパッド左スティックで移動
- Animation補間
  - コード生成した `Idle_Breathing` と配布モデルの `Walk` を同時再生
  - 移動開始・停止時に0.28秒でクロスフェード
  - Translate／ScaleはLerp、RotationはQuaternion Slerp
- Skeletonデバッグ表示
  - 親子Jointを3Dライン表示
  - 選択Jointの名前・ワールド座標・ローカルXYZ軸を表示
- Jointを利用した装備アタッチ
  - `mixamorig:RightHand` の現在行列を親として武器を追従
  - 位置だけでなく回転・拡縮もJointから継承
- Joint位置からGPU Particleを発生
  - `mixamorig:RightHand` の現在ワールド座標から連続発生
- GPU Particle拡張
  - GPU FreeList、Compute Shader更新、ExecuteIndirect描画
  - Point／Sphere／Box Emitter
  - 複数Emitter Request、各種制御パラメータ、ImGui Editor

### 操作

- `WASD` / 左スティック: 移動
- マウス / 右スティック: 三人称カメラ旋回
- ImGui `3D Action Scene > CG4 Evaluation Features`: 各加点要素のON/OFFと比較

## 3Dアクション試作

- 同一Skinning Rigを利用した仮ボス
- ロックオン追従カメラ
- スタミナ消費付き回避、無敵時間、残像Particle
- ボスの `Idle/Approach -> Telegraph -> Attack -> Recovery` 状態機械
- Startup/Active/Recovery時間を持つ右手武器の近接攻撃
- 前方角度・距離判定、HP、ダメージ、ヒットParticle、再戦リセット

### 戦闘操作

- `Q` / 右スティック押し込み: ロックオン切替
- `Space` / Bボタン: 回避
- 左クリック / Xボタン: 近接攻撃
- `R`: 戦闘リセット
