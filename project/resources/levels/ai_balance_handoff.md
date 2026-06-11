# Level AI-ditor Balance Handoff

このファイルは、ゲーム内の `Level AI-ditor Balance Lab` で調整した値をAIに渡すためのメモです。

## 使い方

1. ゲーム内ImGuiの `Level AI-ditor Balance Lab` でHPやダメージを調整する
2. `Apply Runtime` で実行中のゲームへ反映する
3. よさそうなら `Save level_test.json` で `resources/levels/level_test.json` に保存する
4. AIに相談したい時は `AI Handoff MD` でこのファイルを現在値つきで生成する

## AIへ伝える観点

- 現在のプレイ感:
- 困っていること:
- もっと強くしたい要素:
- もっと弱くしたい要素:
- 残したい体験:

## AIへのルール

- C++の固定値変更ではなく、まず `resources/levels/level_test.json` の `balance` を調整する
- プレイヤーHP、DamageBlock、Shooter弾、敵接触、ボス通常弾は `balance` 内で調整する
- ボスHPフェーズ中の弾性能だけは `bossPhases[].customProperties.bossAttack` で調整する
- 変更後の `balance` JSONと、なぜその値にしたかを短く説明する
