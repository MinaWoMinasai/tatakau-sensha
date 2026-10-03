# コメント改善の継続用進捗表

更新日: 2026-10-03（第2段階・戦闘グループ）。開始HEAD: `071c95fbf7634735450ae87b9ea98dbba9e34f13`、ブランチ: `docs/comment-readability-phase2-combat`。開始時の変更・未追跡ファイルはなし。

## 集計と読み方

| 区分 | 列挙数 | 内容の確認 |
| --- | ---: | --- |
| 実行用C++ | 236（h 133、cpp 103） | cpp全体6、TankSpecialCombat.h全体、対応4ヘッダーを照合。Player.cppは25関数のみ。参照先は部分確認 |
| 自作シェーダー | 66（hlsl 53、hlsli 13） | 今回は列挙のみ。説明・内部処理は未確認 |
| 制作・検証ツール | 85（検証C++ 27、スクリプト58） | 実行脚本・監査・関連アサーションなどを部分参照。コメント品質の全面審査は未実施 |
| 合計 | 387 | 列挙数を改善完了数として扱わない |

対象は`git ls-files`と`rg --files`で現在の配置を照合した。自作.hpp/.inl等も検索対象に含め、今回の実行用C++ではh/cpp以外は見つからなかった。主対象外の自作C++は検証ツール区分へ分けた。外部コード（project/externals、vendor/thirdparty等）、generated、素材・設定JSON・文書を集計から除く。シェーダーはresources配下でも制作したプログラムのソースとして別掲する。

エンジン・ゲームのビルド対象、シェーダーの利用先、ツールの内容を照合して区分した。GeneratedTextureCache.hは生成テクスチャの管理コードであり、生成コードとして除外していない。level_aiditor.pyのgenerated表記は出力ファイルの説明文字列で、ツール本体の生成を示すものではない。帰属不明として黙って除外したソースはない。

- 「確認済み」は今回、記載した範囲の実装・対応宣言・利用先を読んだ状態。「一部確認」はその範囲だけ。「未確認」は列挙・検索のみ。「要修正」は手読みにより具体的な食い違いを確認した状態。
- 文書欄は型・関数の説明、内部欄は処理説明。cppの文書確認にはヘッダー側の説明との照合を含む。ヘッダーは宣言・インライン定義だけであり、対応cpp以外を審査した意味ではない。
- P1: 契約の誤解や命中・寿命・更新順に関わる説明、P2: 単位や処理のまとまり、P3: 書式。英語・長行・旧処理候補は検索の手掛かりで、不適切との断定ではない。記載行は今回時点、後続行は省略している。候補なしも内容の正しさを保証しない。
- 「旧報告未照合」はREADMEの過去の対応報告を確認しただけで、今回の内容審査には加算しない。第1段階で3ヘッダーを確認した事実は同報告に残し、今回の確認範囲と分ける。
- SHA256欄はこの表の作成時のファイル内容（先頭12桁）。今回変更したファイルは開始HEADだけでは変更後を識別できないため併記した。ハッシュの一致は説明の品質を示さない。

## 今回の確認範囲

**P2-P（Player.cppの25関数）**: `Attack`、`AttackRailCannon`、`UpdateSpecialCombat`、`UpdateRunProjectiles`、`FireConfiguredClass`、`TryDashImpact`、`DroneShoot`、`Smash`、`TakeDamage`、`Damage`、`GetRailChargeMuzzle`、`ConsumeSpecialCombatEvents`、`ConsumeDashImpactEvents`、`ConsumePrimaryAttackPerformedEvent`、`ConsumeLaserShotEvents`、`ConsumeMineDropEvents`、`ConsumeMeleeSlashEvents`、`ApplyRunProjectileRules`、`GetRunFireIntervalScale`、`ConfigureRunDrone`、`OnCollision`、`TryActivateSpecialAction`、`ActivatePerfectDodge`、`ActivateSaberCounter`、`TriggerSaberCounter`。移動全般、HUD、編集、進化UI、設定保存などは未確認。

6cppの対象は以下の全関数（空関数・getterも読み取り、長文化が不要な箇所は既存の簡潔な説明を保持した）。

- `project/game/player/actor/Player.SpecialAbilities.cpp`: `RefreshAdditiveArmaments`、`ResetAdditionalAbilities`、`TryStartSpinBlade`、`GetDroneAbilityVisuals`、`NotifyDroneHit`、`GetDroneTargetDamageScale`、`ArmWallSmash`、`UpdateAdditionalAbilities`
- `project/game/player/actor/BulletManager.cpp`: `Initialize`、`SetTrailSettings`、`Add`、`FlushPendingSplits`、`ClearAll`、`Update`、`Draw`、`DrawTrails`、`GetBulletPtrs`、`GetBulletCounts`、`GetTrailInstanceCount`、`BuildEventAt`、`LiveCombatTargets`、`HasClearLink`、`DamageBuildTarget`、`QueueKillBurst`、`NotifyPlayerHit`、`GetMarkVisuals`、`QueueArmorReflection`
- `project/game/player/actor/Bullet.cpp`: `Initialize`、`Update`、`Draw`、`OnCollision`、`ConfigureSpecial`、`ConfigureShooterAbilities`、`BeginReturn`、`ConfigureGrowth`、`CanHitActor`、`QueueImpactSplit`、`OnWallImpact`、`AppendImpactChildren`、`ApplyBulletDurabilityDamage`、`GetWorldPosition`、`Die`、`ReleaseTrail`、`AttachTrail`、`GetBulletColor`、`ApplyVisualSettings`、`MakeTrailConfig`、`UpdateTrail`
- `project/game/player/actor/PlayerDrone.cpp`: `~PlayerDrone`、`ConfigureRunAttack`、`Attack`、`RotateToMouse`、`Initialize`、`Update`、`IsVisualVisible`、`Draw`、`DrawSprite`、`GetWorldPosition`、`OnCollision`、`GetAABB`、`Damage`、`Die`
- `project/game/player/actor/AttackController.cpp`: `Fire`、`FireFromMuzzle`、`FireInternal`
- `project/game/collision/CollisionManager.cpp`: `CheckAllCollisions`、`CheckCollisionPair`、`SetColliders`

TankSpecialCombat.hは全型・インライン関数を再確認し、RailCharge、線分と矩形の区間計算、LinkDamageClock、DroneMissionの状態更新、PainterLock、ChooseSpreadTarget、SpinCycleの内部へ補足した。第1段階の判定/生成の区別、秒数・0秒発射・リセット等の契約は保持。対応する4ヘッダーはBullet.h、PlayerDrone.h、AttackController.h、CollisionManager.h。Player.h・BulletManager.hは関連宣言を再確認し、Player.hは変更不要、BulletManager.hはDrawの説明だけを局所修正した。今回その2ヘッダー全体を再審査したとは扱わない。

## 次回の優先範囲

まず戦闘共有型の契約（TankShooterAbilities.h、CombatTypes.h）の確認済みの誤説明を局所修正する。その後は「敵・地形・シーンの命中側」を一まとまりにし、ExpEnemy.cpp/.h、Enemy.cpp/.h、EnemyManager.cpp/.h、Stage.cpp/.h、GameScene.cpp、GameScene.hを全体または事前に指定した関数範囲で確認する。参照済みの部分以外は未確認。PlayerのHUD/編集、エンジン、シェーダー、他の制作・検証ツールは別の未確認グループとして残す。今回ここへ編集を広げていない。

実施内容・テスト・設計上の別件候補は[第2段階の戦闘グループ報告](comment-readability-phase2-combat.md)を参照。

## 実行C++の一覧

| ファイルパス | 機能区分 | 文書 | 内部 | 確認範囲 | 要修正・優先度／判断理由 | 内容SHA256 | 根拠 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `project/DirectX/engine/2d/GeneratedTextureCache.h` | engine/2d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L13等・計6箇所（未判定） | `db3748de8bbd` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/2d/Sprite.cpp` | engine/2d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L33等・計6箇所（未判定） | `077dbd7022f7` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/2d/Sprite.h` | engine/2d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L206等・計1箇所（未判定） | `92f3a57d68f7` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/2d/SpriteCommon.cpp` | engine/2d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L26等・計2箇所（未判定） | `bbcbd28757e9` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/2d/SpriteCommon.h` | engine/2d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L30等・計1箇所（未判定） | `a4de2bea7915` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/2d/TextLabel.cpp` | engine/2d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L52等・計2箇所（未判定） | `e8bf7ec0a904` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/2d/TextLabel.h` | engine/2d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L91等・計1箇所（未判定） | `91010041fd7f` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/2d/TextRenderer.cpp` | engine/2d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L59等・計2箇所（未判定） | `e16898e9471b` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/2d/TextRenderer.h` | engine/2d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L20等・計2箇所（未判定） | `dd32c8210141` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/2d/TextureManager.cpp` | engine/2d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L13等・計4箇所（未判定） | `96d6e912e431` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/2d/TextureManager.h` | engine/2d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L47等・計4箇所（未判定） | `38a1910f4080` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/3d/Animation.cpp` | engine/3d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L85等・計2箇所（未判定） | `61b36dbacd51` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/3d/Animation.h` | engine/3d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L113等・計1箇所（未判定） | `cc64256ef486` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/3d/Model.cpp` | engine/3d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L552等・計7箇所（未判定） | `647fb1ffc6fa` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/3d/Model.h` | engine/3d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L72等・計2箇所（未判定） | `8aa96643f8a0` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/3d/ModelCommon.cpp` | engine/3d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L9等・計1箇所（未判定） | `7a5d0e1bf4b1` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/3d/ModelCommon.h` | engine/3d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L22等・計1箇所（未判定） | `4288a7a5c706` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/3d/ModelManager.cpp` | engine/3d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L140等・計1箇所（未判定） | `01224e078ea9` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/3d/ModelManager.h` | engine/3d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L60等・計1箇所（未判定） | `0176e4ff26a1` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/3d/Object3d.cpp` | engine/3d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L385等・計2箇所（未判定） | `80511e392921` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/3d/Object3d.h` | engine/3d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L20等・計2箇所（未判定） | `5ca4c7a47e91` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/3d/Object3dCommon.cpp` | engine/3d | 未確認 | 未確認 | 列挙のみ | 候補 P2:旧処理候補 L24等・計4箇所、P2:英語説明候補 L37等・計3箇所（未判定） | `7a2d635d3e29` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/3d/Object3dCommon.h` | engine/3d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L143等・計1箇所（未判定） | `27c37f08a963` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/3d/OceanRenderer.cpp` | engine/3d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L675等・計3箇所（未判定） | `ae8815235668` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/3d/OceanRenderer.h` | engine/3d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L445等・計1箇所（未判定） | `316815e9ca7a` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/3d/PbrEnvironment.h` | engine/3d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L91等・計1箇所（未判定） | `e8915a9b442e` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/3d/Skeleton.cpp` | engine/3d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L122等・計2箇所（未判定） | `be28145cb77f` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/3d/Skeleton.h` | engine/3d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L61等・計1箇所（未判定） | `f5d199516643` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/3d/SkinCluster.cpp` | engine/3d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L544等・計4箇所（未判定） | `21db7714542c` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/3d/SkinCluster.h` | engine/3d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L128等・計7箇所（未判定） | `25302d1cb1f0` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/3d/Skybox.cpp` | engine/3d | 未確認 | 未確認 | 列挙のみ | 候補 P2:旧処理候補 L13等・計7箇所、P2:英語説明候補 L20等・計5箇所（未判定） | `c9a004d9edcb` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/3d/Skybox.h` | engine/3d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L30等・計1箇所（未判定） | `dae71164f769` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/3d/neon/NeonSkinnedRenderer.cpp` | engine/3d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L92等・計7箇所（未判定） | `357d9968835f` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/3d/neon/NeonSkinnedRenderer.h` | engine/3d | 未確認 | 未確認 | 列挙のみ | 候補 P2:旧処理候補 L40等・計1箇所、P2:英語説明候補 L16等・計9箇所（未判定） | `ce70425380a2` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/audio/Audio.cpp` | engine/audio | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L79等・計12箇所（未判定） | `c66b88240999` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/audio/Audio.h` | engine/audio | 未確認 | 未確認 | 列挙のみ | 候補 P2:旧処理候補 L67等・計1箇所、P2:英語説明候補 L30等・計6箇所（未判定） | `dcf4e0099538` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/audio/AudioManager.cpp` | engine/audio | 未確認 | 未確認 | 列挙のみ | 候補 P2:旧処理候補 L211等・計5箇所、P2:英語説明候補 L17等・計10箇所（未判定） | `d42307e1266b` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/audio/AudioManager.h` | engine/audio | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L18等・計3箇所（未判定） | `2d7fff2c488e` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/calc/Calculation.cpp` | engine/calc | 未確認 | 未確認 | 列挙のみ | 候補 P2:旧処理候補 L842等・計4箇所、P2:英語説明候補 L1092等・計3箇所、P3:長行候補 L891等・計2箇所（未判定） | `46148e7fffbd` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/calc/Calculation.h` | engine/calc | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L98等・計6箇所（未判定） | `197f9b252de6` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/CylinderManager.cpp` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L114等・計1箇所（未判定） | `cf70eaad0349` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/CylinderManager.h` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L74等・計1箇所（未判定） | `5fd10d1cf5d9` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/DeveloperTools.h` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L3等・計2箇所（未判定） | `1d078d364bcf` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/DirectXCommon.cpp` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:旧処理候補 L114等・計8箇所、P2:英語説明候補 L23等・計44箇所、P3:長行候補 L679等・計2箇所（未判定） | `f694c20c7121` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/DirectXCommon.h` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L114等・計4箇所（未判定） | `306d4a79aeac` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/FramePacer.h` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L8等・計6箇所（未判定） | `3641281d4428` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/InputDesc.cpp` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L33等・計4箇所（未判定） | `d9b0852de58d` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/InputDesc.h` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L31等・計2箇所（未判定） | `575f3ae08c41` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/LogWrite.cpp` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L58等・計2箇所（未判定） | `ef281f5371f5` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/LogWrite.h` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L37等・計1箇所（未判定） | `05d32862f60f` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/NeonGridRenderer.cpp` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L456等・計1箇所、P3:長行候補 L215等・計1箇所（未判定） | `657bff370acd` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/NeonGridRenderer.h` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L88等・計1箇所（未判定） | `b7ed02cdac7a` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/ProceduralFlameRenderer.cpp` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L29等・計9箇所（未判定） | `2807e466e6dc` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/ProceduralFlameRenderer.h` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L13等・計3箇所（未判定） | `b783d7326332` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/Resource.cpp` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L11等・計3箇所、P3:長行候補 L5等・計1箇所（未判定） | `b612af96d13f` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/Resource.h` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L33等・計1箇所（未判定） | `e3899dc62bab` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/RingManager.cpp` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L115等・計1箇所（未判定） | `2cb34affb42b` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/RingManager.h` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L74等・計1箇所（未判定） | `647aa6b4bc95` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/Root.cpp` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:旧処理候補 L763等・計3箇所、P2:英語説明候補 L51等・計39箇所（未判定） | `9256a5bd7326` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/Root.h` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L84等・計1箇所（未判定） | `263102b28f28` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/RtvManager.cpp` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L33等・計1箇所（未判定） | `58889776ab32` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/RtvManager.h` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L28等・計1箇所（未判定） | `26eb7335e558` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/RuntimeProfiler.cpp` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L137等・計5箇所、P3:長行候補 L240等・計1箇所（未判定） | `428c55122e94` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/RuntimeProfiler.h` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L15等・計2箇所（未判定） | `747e718db2a2` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/ShaderDiskCache.h` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L3等・計6箇所（未判定） | `cb3fe26e01c8` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/ShadowMap.cpp` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:旧処理候補 L8等・計3箇所、P2:英語説明候補 L15等・計4箇所（未判定） | `9bb337c4ab43` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/ShadowMap.h` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L36等・計1箇所（未判定） | `17c3677444f2` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/SrvManager.cpp` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L14等・計3箇所（未判定） | `61e072fccc6e` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/SrvManager.h` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L33等・計8箇所（未判定） | `48b55ed2109a` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/StartupTrace.h` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:旧処理候補 L16等・計1箇所、P2:英語説明候補 L15等・計4箇所（未判定） | `272f66cf7bec` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/StringUtils.h` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L21等・計1箇所（未判定） | `dfdb49769ea9` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/Texture.cpp` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L10等・計22箇所、P3:長行候補 L117等・計1箇所（未判定） | `f5df747c0b55` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/Texture.h` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L39等・計1箇所（未判定） | `d4e8458c1362` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/TrailInstance.cpp` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L6等・計3箇所（未判定） | `f8f960f628c1` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/TrailInstance.h` | engine/commom | 一部確認 | 一部確認 | SetActive・Configと軌跡の寿命 | 今回変更不要: 今回は利用側を修正。生成/描画の全体は未確認 | `fecc3dcf9569` | 今回参照のみ |
| `project/DirectX/engine/commom/TrailManager.cpp` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L101等・計6箇所（未判定） | `b835fd64a010` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/TrailManager.h` | engine/commom | 一部確認 | 一部確認 | CreateTrail/管理・消去の寿命（軌跡の借用先） | 今回変更不要: 今回は利用側の寿命説明を修正。管理実装は未確認 | `5980ccb31db4` | 今回参照のみ |
| `project/DirectX/engine/commom/TrailStressFixture.h` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:旧処理候補 L12等・計1箇所、P2:英語説明候補 L10等・計9箇所（未判定） | `4c9207c6a0dd` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/WinApp.cpp` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L41等・計2箇所（未判定） | `365a37d9f444` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/commom/WinApp.h` | engine/commom | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L20等・計2箇所（未判定） | `c23d084e127b` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/debugCamera/Camera.cpp` | engine/debugCamera | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L37等・計1箇所（未判定） | `acb2128fd84f` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/debugCamera/Camera.h` | engine/debugCamera | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L17等・計3箇所（未判定） | `d811b0c17602` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/debugCamera/DebugCamera.cpp` | engine/debugCamera | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L23等・計6箇所（未判定） | `d6c122be46a5` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/debugCamera/DebugCamera.h` | engine/debugCamera | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L124等・計1箇所（未判定） | `8e73ec0cbb32` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/dumpClass/Dump.cpp` | engine/dumpClass | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L17等・計3箇所（未判定） | `5a40e22eff86` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/dumpClass/Dump.h` | engine/dumpClass | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L16等・計1箇所（未判定） | `ddd0b16b71d2` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/easing/Easing.cpp` | engine/easing | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L214等・計1箇所（未判定） | `cdb13bca3bd0` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/easing/Easing.h` | engine/easing | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L77等・計1箇所（未判定） | `bf314a14c584` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/input/Input.cpp` | engine/input | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L116等・計2箇所（未判定） | `a900ffa65d34` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/input/Input.h` | engine/input | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L28等・計2箇所（未判定） | `cba50a650fd6` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/particle/EffectSequencer.cpp` | engine/particle | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L415等・計1箇所（未判定） | `7f22be267a5a` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/particle/EffectSequencer.h` | engine/particle | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L194等・計1箇所（未判定） | `b66c5209b4dc` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/particle/ParticleManager.cpp` | engine/particle | 未確認 | 未確認 | 列挙のみ | 候補 P2:旧処理候補 L95等・計5箇所、P2:英語説明候補 L129等・計6箇所（未判定） | `057ea0f12eb0` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/particle/ParticleManager.h` | engine/particle | 未確認 | 未確認 | 列挙のみ | 候補 P2:旧処理候補 L153等・計1箇所、P2:英語説明候補 L90等・計3箇所（未判定） | `1e30f3bd1d37` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/postEffect/Bloom.cpp` | engine/postEffect | 未確認 | 未確認 | 列挙のみ | 候補 P2:旧処理候補 L910等・計2箇所、P2:英語説明候補 L71等・計9箇所（未判定） | `434492f923d8` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/postEffect/Bloom.h` | engine/postEffect | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L20等・計2箇所（未判定） | `0293493abc84` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/postEffect/BloomConstantBuffer.cpp` | engine/postEffect | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L27等・計2箇所（未判定） | `59874d67353c` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/postEffect/BloomConstantBuffer.h` | engine/postEffect | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L21等・計1箇所（未判定） | `535a793a6037` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/postEffect/ObjectPostEffect.cpp` | engine/postEffect | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L156等・計3箇所（未判定） | `643b56e4b9ed` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/postEffect/ObjectPostEffect.h` | engine/postEffect | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L90等・計1箇所（未判定） | `1f7ef4dd31cf` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/postEffect/PostEffect.cpp` | engine/postEffect | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L290等・計1箇所（未判定） | `635a7b2455d5` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/postEffect/PostEffect.h` | engine/postEffect | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L50等・計1箇所（未判定） | `78025e1222e4` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/postEffect/RenderTexture.cpp` | engine/postEffect | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L37等・計9箇所（未判定） | `f9c3032c6ec8` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/postEffect/RenderTexture.h` | engine/postEffect | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L71等・計1箇所（未判定） | `cb70ffa64004` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/postEffect/Shadow.cpp` | engine/postEffect | 未確認 | 未確認 | 列挙のみ | 候補 P2:旧処理候補 L19等・計1箇所、P2:英語説明候補 L25等・計2箇所（未判定） | `a0a80db5f7bf` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/postEffect/Shadow.h` | engine/postEffect | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L36等・計1箇所（未判定） | `06a88d474c2c` | 今回列挙／旧報告未照合 |
| `project/DirectX/engine/struct/Struct.h` | engine/struct | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L188等・計45箇所、P3:長行候補 L691等・計1箇所（未判定） | `930bdb22f4ec` | 今回列挙／旧報告未照合 |
| `project/game/collision/Collider.cpp` | game/collision | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `8079b9edb932` | 今回列挙／旧報告未照合 |
| `project/game/collision/Collider.h` | game/collision | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L127等・計2箇所（未判定） | `d21d56ef4927` | 今回列挙／旧報告未照合 |
| `project/game/collision/CollisionConfig.cpp` | game/collision | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `5dce83120f39` | 今回列挙／旧報告未照合 |
| `project/game/collision/CollisionConfig.h` | game/collision | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `dc19a90ebab1` | 今回列挙／旧報告未照合 |
| `project/game/collision/CollisionManager.cpp` | game/collision | 確認済み | 確認済み | 全関数・型（前掲範囲） | 今回修正／自明な取得・空処理は長文化不要。設計候補は別件報告参照 | `d7729233d5d4` | 今回照合 |
| `project/game/collision/CollisionManager.h` | game/collision | 確認済み | 確認済み | 全関数・型（前掲範囲） | 今回修正／自明な取得・空処理は長文化不要。設計候補は別件報告参照 | `5e0165f5d20b` | 今回照合 |
| `project/game/debug/NeonSkinnedPreview.cpp` | game/debug | 未確認 | 未確認 | 列挙のみ | 候補 P2:旧処理候補 L93等・計1箇所、P2:英語説明候補 L99等・計5箇所（未判定） | `9eb5106164e8` | 今回列挙／旧報告未照合 |
| `project/game/debug/NeonSkinnedPreview.h` | game/debug | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L4等・計3箇所（未判定） | `e92a63ae1a10` | 今回列挙／旧報告未照合 |
| `project/game/editor/ExpeditionContentEditor.cpp` | game/editor | 未確認 | 未確認 | 列挙のみ | 候補 P3:長行候補 L16等・計16箇所（未判定） | `2916c8b25177` | 今回列挙／旧報告未照合 |
| `project/game/editor/ExpeditionContentEditor.h` | game/editor | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L4等・計3箇所（未判定） | `062fc7d07eb0` | 今回列挙／旧報告未照合 |
| `project/game/editor/ExpeditionMapEditor.cpp` | game/editor | 未確認 | 未確認 | 列挙のみ | 候補 P3:長行候補 L161等・計2箇所（未判定） | `9c4f5b45f61a` | 今回列挙／旧報告未照合 |
| `project/game/editor/ExpeditionMapEditor.h` | game/editor | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L6等・計3箇所（未判定） | `9f1286b4854b` | 今回列挙／旧報告未照合 |
| `project/game/editor/ExpeditionRoomEditor.cpp` | game/editor | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L38等・計4箇所、P3:長行候補 L70等・計4箇所（未判定） | `99cbe18235b9` | 今回列挙／旧報告未照合 |
| `project/game/editor/ExpeditionRoomEditor.h` | game/editor | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L27等・計1箇所（未判定） | `83f62eee6f03` | 今回列挙／旧報告未照合 |
| `project/game/effects/ScreenEffectDirector.cpp` | game/effects | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L29等・計1箇所（未判定） | `e05ea9ddd4b5` | 今回列挙／旧報告未照合 |
| `project/game/effects/ScreenEffectDirector.h` | game/effects | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L7等・計3箇所（未判定） | `c633009de2b6` | 今回列挙／旧報告未照合 |
| `project/game/effects/TankSpecialNeonGeometry.h` | game/effects | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L6等・計3箇所（未判定） | `d3bf42c32df1` | 今回列挙／旧報告未照合 |
| `project/game/enemy/actor/Enemy.cpp` | game/enemy | 一部確認 | 一部確認 | OnCollision、TakeDamage、ApplyKnockbackと死亡/遭遇の条件 | 次回P1: ボス側の命中・死亡遷移を確認 | `ec1643aa72b4` | 今回参照のみ |
| `project/game/enemy/actor/Enemy.h` | game/enemy | 一部確認 | 一部確認 | GetHp・IsDead・IsRunEncounterEnabledなどの利用条件 | 次回P1: 対応cppと併せて確認 | `476fafdfcb5b` | 今回参照のみ |
| `project/game/enemy/actor/PrototypeBossCombat.h` | game/enemy | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L5等・計2箇所（未判定） | `e58ea60cab37` | 今回列挙／旧報告未照合 |
| `project/game/enemy/actor/RivalBossCombat.h` | game/enemy | 未確認 | 未確認 | 列挙のみ | 候補 P2:旧処理候補 L161等・計1箇所、P2:英語説明候補 L6等・計8箇所（未判定） | `13e8157b8c54` | 今回列挙／旧報告未照合 |
| `project/game/exp/EnemyManager.cpp` | game/exp | 一部確認 | 一部確認 | Updateの削除時点、GetEnemyPtrsの借用一覧 | 次回P1: 敵生成・破棄と借用の全体を確認 | `97e14555974d` | 今回参照のみ |
| `project/game/exp/EnemyManager.h` | game/exp | 一部確認 | 一部確認 | GetEnemyPtrsの宣言・借用 | 次回P1: 管理APIの全体を確認 | `436d532d6346` | 今回参照のみ |
| `project/game/exp/ExpEnemy.cpp` | game/exp | 一部確認 | 一部確認 | MoveCombatActor、ApplyKnockback、TakeDirectionalDamage、ApplyDamage、TryReflectProjectile、OnCollisionの命中関連 | 次回P1: 敵側の命中・防御・壁衝突を全体で確認 | `1d3bbd38fbfc` | 今回参照のみ |
| `project/game/exp/ExpEnemy.h` | game/exp | 一部確認 | 一部確認 | GetWallCollisionCount・命中/押し出し関連の宣言 | 次回P1: 対応cppと併せて確認 | `1af4f708b707` | 今回参照のみ |
| `project/game/exp/ExpEnemyCombatCycle.h` | game/exp | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L6等・計7箇所（未判定） | `cb51715cd392` | 今回列挙／旧報告未照合 |
| `project/game/exp/ExpEnemyMagazineCycle.h` | game/exp | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L5等・計2箇所（未判定） | `83b7eea4282c` | 今回列挙／旧報告未照合 |
| `project/game/exp/ExpEnemyNavigation.h` | game/exp | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L6等・計2箇所（未判定） | `98796b5bc764` | 今回列挙／旧報告未照合 |
| `project/game/exp/ExpGuardCombat.h` | game/exp | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L7等・計5箇所（未判定） | `6fb92b36e612` | 今回列挙／旧報告未照合 |
| `project/game/level/LevelLoader.cpp` | game/level | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L162等・計1箇所（未判定） | `b8c6e9a7c104` | 今回列挙／旧報告未照合 |
| `project/game/level/LevelLoader.h` | game/level | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `0078bda3d9c5` | 今回列挙／旧報告未照合 |
| `project/game/mapchip/MapChip.cpp` | game/mapchip | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L45等・計1箇所（未判定） | `d8f2a1f7c4d5` | 今回列挙／旧報告未照合 |
| `project/game/mapchip/MapChip.h` | game/mapchip | 一部確認 | 一部確認 | ドローンのマップ範囲で使うサイズ定数 | 今回変更不要: 参照した定数の意味は確認。その他未確認 | `37483c92da65` | 今回参照のみ |
| `project/game/modules/BuiltInGameModule.cpp` | game/modules | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `ad18c3078c76` | 今回列挙／旧報告未照合 |
| `project/game/modules/BuiltInGameModule.h` | game/modules | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `e6a3bf2cd86d` | 今回列挙／旧報告未照合 |
| `project/game/modules/GameModuleBootstrap.cpp` | game/modules | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `16b01dc8b98c` | 今回列挙／旧報告未照合 |
| `project/game/modules/GameModuleBootstrap.h` | game/modules | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `d580c95ed9c9` | 今回列挙／旧報告未照合 |
| `project/game/player/PlayerClassCatalog.cpp` | game/player | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L185等・計1箇所（未判定） | `bd0d6c84843b` | 今回列挙／旧報告未照合 |
| `project/game/player/PlayerClassCatalog.h` | game/player | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `3f14b624fb14` | 今回列挙／旧報告未照合 |
| `project/game/player/PlayerClassConfig.h` | game/player | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `81c7d2dcd9e0` | 今回列挙／旧報告未照合 |
| `project/game/player/TankCombatStyleBalance.h` | game/player | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L7等・計2箇所（未判定） | `de48bbce4d83` | 今回列挙／旧報告未照合 |
| `project/game/player/TankExpeditionLoadout.h` | game/player | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L7等・計4箇所、P2:説明の照合候補 L57等・計1箇所（未判定） | `51e3a67fd32e` | 今回列挙／旧報告未照合 |
| `project/game/player/TankRunModifiers.h` | game/player | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L12等・計9箇所（未判定） | `65dbefaf3304` | 今回列挙／旧報告未照合 |
| `project/game/player/TankShooterAbilities.h` | game/player | 要修正（部分） | 一部確認 | ReturnFlight、MarkLedger、Damageの状態・計算 | 要修正 P1: ReturnFlightは方向を計算せず、Damageは値の計算だけ。Update/Hitの説明も不足 | `a10085a03d67` | 今回参照のみ |
| `project/game/player/TankSpecialCombat.h` | game/player | 確認済み | 確認済み | 全関数・型（前掲範囲） | 今回修正／自明な取得・空処理は長文化不要。設計候補は別件報告参照 | `3fd2f153cae1` | 今回照合 |
| `project/game/player/actor/AttackController.cpp` | game/player | 確認済み | 確認済み | 全関数・型（前掲範囲） | 今回修正／自明な取得・空処理は長文化不要。設計候補は別件報告参照 | `f8a08d506b0b` | 今回照合 |
| `project/game/player/actor/AttackController.h` | game/player | 確認済み | 確認済み | 全関数・型（前掲範囲） | 今回修正／自明な取得・空処理は長文化不要。設計候補は別件報告参照 | `67126d724737` | 今回照合 |
| `project/game/player/actor/Bullet.cpp` | game/player | 確認済み | 確認済み | 全関数・型（前掲範囲） | 今回修正／自明な取得・空処理は長文化不要。設計候補は別件報告参照 | `c4f34860ada8` | 今回照合 |
| `project/game/player/actor/Bullet.h` | game/player | 確認済み | 確認済み | 全関数・型（前掲範囲） | 今回修正／自明な取得・空処理は長文化不要。設計候補は別件報告参照 | `cec1715edde4` | 今回照合 |
| `project/game/player/actor/BulletManager.cpp` | game/player | 確認済み | 確認済み | 全関数・型（前掲範囲） | 今回修正／自明な取得・空処理は長文化不要。設計候補は別件報告参照 | `51f4b591dbb2` | 今回照合 |
| `project/game/player/actor/BulletManager.h` | game/player | 一部確認 | 一部確認 | 今回の対象API・関連型/インライン状態。第1段階は文書全体を確認 | Drawの空描画経路の説明を局所修正。他の対象APIは既存契約と一致。範囲外は今回未確認 | `0064d1cebfe6` | 今回部分照合＋第1段階報告 |
| `project/game/player/actor/Player.ClassEditor.cpp` | game/player | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L45等・計3箇所（未判定） | `7e4ac007b436` | 今回列挙／旧報告未照合 |
| `project/game/player/actor/Player.EvolutionUi.cpp` | game/player | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L42等・計3箇所（未判定） | `3904ad16401b` | 今回列挙／旧報告未照合 |
| `project/game/player/actor/Player.SpecialAbilities.cpp` | game/player | 確認済み | 確認済み | 全関数・型（前掲範囲） | 今回修正／自明な取得・空処理は長文化不要。設計候補は別件報告参照 | `4e96faa1b593` | 今回照合 |
| `project/game/player/actor/Player.cpp` | game/player | 一部確認 | 一部確認 | P2-Pの25関数のみ | 今回修正／対象外のHUD・移動・編集等は未確認 | `51c6baa16077` | 今回照合 |
| `project/game/player/actor/Player.h` | game/player | 一部確認 | 一部確認 | 今回の対象API・関連型/インライン状態。第1段階は文書全体を確認 | 今回変更不要: 既存の契約説明と実装が一致。範囲外は今回未確認 | `903dfd8d66db` | 今回部分照合＋第1段階報告 |
| `project/game/player/actor/PlayerDrone.cpp` | game/player | 確認済み | 確認済み | 全関数・型（前掲範囲） | 今回修正／自明な取得・空処理は長文化不要。設計候補は別件報告参照 | `ab24e33e05ef` | 今回照合 |
| `project/game/player/actor/PlayerDrone.h` | game/player | 確認済み | 確認済み | 全関数・型（前掲範囲） | 今回修正／自明な取得・空処理は長文化不要。設計候補は別件報告参照 | `9a318e2740b1` | 今回照合 |
| `project/game/player/actor/PlayerUiHelpers.cpp` | game/player | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L43等・計1箇所（未判定） | `bb3b84902943` | 今回列挙／旧報告未照合 |
| `project/game/player/actor/PlayerUiHelpers.h` | game/player | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L8等・計2箇所（未判定） | `f7a4d1fdef30` | 今回列挙／旧報告未照合 |
| `project/game/player/actor/Stage.cpp` | game/player | 一部確認 | 一部確認 | ResolveBulletsCollision、ResolvePlayerDroneCollision、ResolveExpEnemyCollision、IsCollisionWithAnyBlock | 次回P1: 地形通知の流れ・近似遮蔽との使い分けを確認 | `8dc00fb8da5c` | 今回参照のみ |
| `project/game/player/actor/Stage.h` | game/player | 一部確認 | 一部確認 | 上記地形API・GetMergedBlocksの宣言 | 次回P1: 実装側と併せて範囲を広げて確認 | `c300e1368798` | 今回参照のみ |
| `project/game/run/TankBuildStyle.h` | game/run | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L56等・計1箇所（未判定） | `10a9331fc06f` | 今回列挙／旧報告未照合 |
| `project/game/run/TankExpeditionAudio.h` | game/run | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L11等・計9箇所（未判定） | `a851f3773dea` | 今回列挙／旧報告未照合 |
| `project/game/run/TankExpeditionBalance.h` | game/run | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L133等・計7箇所（未判定） | `79f86a7c8581` | 今回列挙／旧報告未照合 |
| `project/game/run/TankExpeditionContent.h` | game/run | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L22等・計14箇所（未判定） | `95ab2dc577bf` | 今回列挙／旧報告未照合 |
| `project/game/run/TankExpeditionDirector.h` | game/run | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L32等・計4箇所（未判定） | `20aaee01661d` | 今回列挙／旧報告未照合 |
| `project/game/run/TankExpeditionEncounters.h` | game/run | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L63等・計3箇所（未判定） | `2039a2850ddc` | 今回列挙／旧報告未照合 |
| `project/game/run/TankExpeditionMap.h` | game/run | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L159等・計11箇所（未判定） | `24d7a2eb63b9` | 今回列挙／旧報告未照合 |
| `project/game/run/TankExpeditionRooms.h` | game/run | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L484等・計1箇所（未判定） | `46bcfc202c66` | 今回列挙／旧報告未照合 |
| `project/game/run/TankExpeditionTransition.h` | game/run | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L7等・計2箇所（未判定） | `cb9f7f797607` | 今回列挙／旧報告未照合 |
| `project/game/run/TankExpeditionTutorial.h` | game/run | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L8等・計7箇所（未判定） | `233210bcdd54` | 今回列挙／旧報告未照合 |
| `project/game/run/TankGuidedCombatTutorial.h` | game/run | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L5等・計5箇所（未判定） | `4291268dc55f` | 今回列挙／旧報告未照合 |
| `project/game/run/TankRunCopy.h` | game/run | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L62等・計2箇所（未判定） | `149ef3e7f154` | 今回列挙／旧報告未照合 |
| `project/game/run/TankRunDirector.h` | game/run | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L71等・計11箇所（未判定） | `03f88a5fb794` | 今回列挙／旧報告未照合 |
| `project/game/run/TankSubmissionValidation.h` | game/run | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L9等・計3箇所（未判定） | `e78576cdaf3d` | 今回列挙／旧報告未照合 |
| `project/game/run/TankTutorialCopy.h` | game/run | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L10等・計2箇所（未判定） | `1397078482ac` | 今回列挙／旧報告未照合 |
| `project/game/runtime/GameModuleRegistry.cpp` | game/runtime | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `fdb6ea842e63` | 今回列挙／旧報告未照合 |
| `project/game/runtime/GameModuleRegistry.h` | game/runtime | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `d22ef164cdfa` | 今回列挙／旧報告未照合 |
| `project/game/runtime/GameProject.cpp` | game/runtime | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `f773288cae29` | 今回列挙／旧報告未照合 |
| `project/game/runtime/GameProject.h` | game/runtime | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `f6b5f51800db` | 今回列挙／旧報告未照合 |
| `project/game/runtime/IGameModule.h` | game/runtime | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `8e2393350dd9` | 今回列挙／旧報告未照合 |
| `project/game/runtime/SceneRegistry.cpp` | game/runtime | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `b563c7698c7b` | 今回列挙／旧報告未照合 |
| `project/game/runtime/SceneRegistry.h` | game/runtime | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `5e711a0f6659` | 今回列挙／旧報告未照合 |
| `project/game/scene/AbstractSceneFactory.h` | game/scene | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `ea877db29861` | 今回列挙／旧報告未照合 |
| `project/game/scene/Fade.cpp` | game/scene | 未確認 | 未確認 | 列挙のみ | 候補 P2:旧処理候補 L27等・計2箇所（未判定） | `a399ab68284a` | 今回列挙／旧報告未照合 |
| `project/game/scene/Fade.h` | game/scene | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `7daca84f5c62` | 今回列挙／旧報告未照合 |
| `project/game/scene/Game.cpp` | game/scene | 未確認 | 未確認 | 列挙のみ | 候補 P2:旧処理候補 L382等・計1箇所、P2:英語説明候補 L322等・計20箇所（未判定） | `bee68ff23c2d` | 今回列挙／旧報告未照合 |
| `project/game/scene/Game.h` | game/scene | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `a7c2f4b0d0a2` | 今回列挙／旧報告未照合 |
| `project/game/scene/GameScene.Authoring.cpp` | game/scene | 未確認 | 未確認 | 列挙のみ | 候補 P3:長行候補 L110等・計1箇所（未判定） | `8d183f37a86f` | 今回列挙／旧報告未照合 |
| `project/game/scene/GameScene.Balance.cpp` | game/scene | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L17等・計4箇所、P3:長行候補 L193等・計1箇所（未判定） | `d1f7ff8911be` | 今回列挙／旧報告未照合 |
| `project/game/scene/GameScene.CombatValidation.cpp` | game/scene | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L14等・計6箇所（未判定） | `fec2b4b66fbe` | 今回列挙／旧報告未照合 |
| `project/game/scene/GameScene.ExpeditionBuild.cpp` | game/scene | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L18等・計2箇所、P3:長行候補 L21等・計1箇所（未判定） | `63aa48fb14fe` | 今回列挙／旧報告未照合 |
| `project/game/scene/GameScene.ExpeditionExperience.cpp` | game/scene | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L150等・計1箇所、P3:長行候補 L20等・計4箇所（未判定） | `714bf38243b4` | 今回列挙／旧報告未照合 |
| `project/game/scene/GameScene.ExpeditionMap.cpp` | game/scene | 未確認 | 未確認 | 列挙のみ | 候補 P2:旧処理候補 L474等・計1箇所、P2:英語説明候補 L112等・計16箇所、P3:長行候補 L256等・計12箇所（未判定） | `134077de3e27` | 今回列挙／旧報告未照合 |
| `project/game/scene/GameScene.ExperienceValidation.cpp` | game/scene | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L33等・計5箇所、P3:長行候補 L160等・計5箇所（未判定） | `c407bd0c8978` | 今回列挙／旧報告未照合 |
| `project/game/scene/GameScene.SpecialValidation.cpp` | game/scene | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L112等・計3箇所、P3:長行候補 L77等・計16箇所（未判定） | `82ad8ed1f417` | 今回列挙／旧報告未照合 |
| `project/game/scene/GameScene.TankExpedition.cpp` | game/scene | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L79等・計10箇所、P3:長行候補 L195等・計5箇所（未判定） | `bbefb3b93b28` | 今回列挙／旧報告未照合 |
| `project/game/scene/GameScene.TankRun.cpp` | game/scene | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L274等・計4箇所、P3:長行候補 L158等・計1箇所（未判定） | `b74659c35604` | 今回列挙／旧報告未照合 |
| `project/game/scene/GameScene.TankRunVisuals.cpp` | game/scene | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L8等・計10箇所（未判定） | `f0e9aca91f6a` | 今回列挙／旧報告未照合 |
| `project/game/scene/GameScene.TitleDemo.cpp` | game/scene | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L24等・計10箇所（未判定） | `4d536b737109` | 今回列挙／旧報告未照合 |
| `project/game/scene/GameScene.cpp` | game/scene | 一部確認 | 一部確認 | Updateの攻撃予約消費→敵→弾/地形→特殊戦闘→衝突、UpdateSpecialCombatPresentationのイベント消費 | 次回P1: 呼び出し順と演出の全体を確認 | `5a55bd6c9142` | 今回参照のみ |
| `project/game/scene/GameScene.h` | game/scene | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L315等・計2箇所（未判定） | `364fd578eba7` | 今回列挙／旧報告未照合 |
| `project/game/scene/GameStartMode.h` | game/scene | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L24等・計1箇所（未判定） | `826b7b347dbd` | 今回列挙／旧報告未照合 |
| `project/game/scene/IScene.h` | game/scene | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L22等・計1箇所（未判定） | `830eee759cdf` | 今回列挙／旧報告未照合 |
| `project/game/scene/SceneFactory.cpp` | game/scene | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `6e27a01c927d` | 今回列挙／旧報告未照合 |
| `project/game/scene/SceneFactory.h` | game/scene | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `da6074373fa6` | 今回列挙／旧報告未照合 |
| `project/game/scene/SceneManager.cpp` | game/scene | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L80等・計4箇所（未判定） | `bc517be071f2` | 今回列挙／旧報告未照合 |
| `project/game/scene/SceneManager.h` | game/scene | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `987167a487d4` | 今回列挙／旧報告未照合 |
| `project/game/scene/TitleScene.cpp` | game/scene | 未確認 | 未確認 | 列挙のみ | 候補 P2:旧処理候補 L111等・計1箇所、P2:英語説明候補 L38等・計8箇所（未判定） | `d41f5d2c516c` | 今回列挙／旧報告未照合 |
| `project/game/scene/TitleScene.h` | game/scene | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `2b49bc38b7cd` | 今回列挙／旧報告未照合 |
| `project/game/ui/ColorMath.h` | game/ui | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L5等・計2箇所（未判定） | `70e4a2e445d5` | 今回列挙／旧報告未照合 |
| `project/game/ui/NeonProgressBar.cpp` | game/ui | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L83等・計1箇所（未判定） | `08291b854be3` | 今回列挙／旧報告未照合 |
| `project/game/ui/NeonProgressBar.h` | game/ui | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `0368ed567cba` | 今回列挙／旧報告未照合 |
| `project/game/ui/NeonSegmentedBar.cpp` | game/ui | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L51等・計1箇所（未判定） | `b557154d15dc` | 今回列挙／旧報告未照合 |
| `project/game/ui/NeonSegmentedBar.h` | game/ui | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `91b7c51388ff` | 今回列挙／旧報告未照合 |
| `project/game/ui/NeonTextEffect.cpp` | game/ui | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L19等・計1箇所（未判定） | `37a1eb346db8` | 今回列挙／旧報告未照合 |
| `project/game/ui/NeonTextEffect.h` | game/ui | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `05ef250abc94` | 今回列挙／旧報告未照合 |
| `project/game/ui/TankButtonUI.cpp` | game/ui | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L113等・計3箇所（未判定） | `83b359a3d8e6` | 今回列挙／旧報告未照合 |
| `project/game/ui/TankButtonUI.h` | game/ui | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `b419e2852afa` | 今回列挙／旧報告未照合 |
| `project/game/ui/TankCombatNeonGeometry.h` | game/ui | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L6等・計3箇所（未判定） | `bf8bcd42ff90` | 今回列挙／旧報告未照合 |
| `project/game/ui/TankRewardCard.cpp` | game/ui | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L138等・計17箇所、P3:長行候補 L207等・計3箇所（未判定） | `75633345f166` | 今回列挙／旧報告未照合 |
| `project/game/ui/TankRewardCard.h` | game/ui | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L22等・計5箇所（未判定） | `3561a0e68938` | 今回列挙／旧報告未照合 |
| `project/game/ui/TankRewardCardDemo.h` | game/ui | 未確認 | 未確認 | 列挙のみ | 候補 P2:旧処理候補 L481等・計1箇所、P2:英語説明候補 L12等・計13箇所（未判定） | `2c5b314458a1` | 今回列挙／旧報告未照合 |
| `project/game/ui/TankRewardPreviewRenderer.cpp` | game/ui | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L74等・計8箇所、P3:長行候補 L239等・計2箇所（未判定） | `35ea221f5fc3` | 今回列挙／旧報告未照合 |
| `project/game/ui/TankRewardPreviewRenderer.h` | game/ui | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L30等・計4箇所（未判定） | `740f6b7b41bf` | 今回列挙／旧報告未照合 |
| `project/game/weapon/CombatTypes.h` | game/weapon | 要修正（部分） | 一部確認 | AttackParamのメンバー・既定値、BulletOwner | 要修正 P1: AttackParamの説明にある位置・方向をこの型は保持しない | `a8ab7e69d0cb` | 今回参照のみ |
| `project/game/weapon/WeaponMount.h` | game/weapon | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `0f0806e8a689` | 今回列挙／旧報告未照合 |
| `project/main.cpp` | 起動 | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L59等・計1箇所（未判定） | `f946df15eb07` | 今回列挙／旧報告未照合 |

## シェーダーの一覧

| ファイルパス | 機能区分 | 文書 | 内部 | 確認範囲 | 要修正・優先度／判断理由 | 内容SHA256 | 根拠 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `project/resources/shaders/BloomBlurH.PS.hlsl` | 描画/BloomBlurH | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L49等・計4箇所（未判定） | `fc8ddc9e208a` | 今回列挙 |
| `project/resources/shaders/BloomBlurV.PS.hlsl` | 描画/BloomBlurV | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L49等・計5箇所（未判定） | `fb7e039d4b87` | 今回列挙 |
| `project/resources/shaders/BloomDownsample.PS.hlsl` | 描画/BloomDownsample | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `e957e7e0756b` | 今回列挙 |
| `project/resources/shaders/BloomExtract.PS.hlsl` | 描画/BloomExtract | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `a675a792f758` | 今回列挙 |
| `project/resources/shaders/Composite.PS.hlsl` | 描画/Composite | 未確認 | 未確認 | 列挙のみ | 候補 P2:旧処理候補 L445等・計9箇所、P2:英語説明候補 L183等・計4箇所（未判定） | `359716b79de4` | 今回列挙 |
| `project/resources/shaders/FullScreen.VS.hlsl` | 描画/FullScreen | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `94e6ed16503c` | 今回列挙 |
| `project/resources/shaders/GaussianFilter.PS.hlsl` | 描画/GaussianFilter | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `351b339f5bb2` | 今回列挙 |
| `project/resources/shaders/ModelParticle.PS.hlsl` | 描画/ModelParticle | 未確認 | 未確認 | 列挙のみ | 候補 P2:旧処理候補 L58等・計9箇所（未判定） | `06047ba7030b` | 今回列挙 |
| `project/resources/shaders/ModelParticle.Scene.PS.hlsl` | 描画/ModelParticle | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `b2471f1660bb` | 今回列挙 |
| `project/resources/shaders/ModelParticle.VS.hlsl` | 描画/ModelParticle | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `6eaa93ad775e` | 今回列挙 |
| `project/resources/shaders/ModelParticle.hlsli` | 描画/ModelParticle | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `e5ef6232aa51` | 今回列挙 |
| `project/resources/shaders/MotionVectorResolve.PS.hlsl` | 描画/MotionVectorResolve | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `20228a97a055` | 今回列挙 |
| `project/resources/shaders/NeonSkinned.PS.hlsl` | 描画/NeonSkinned | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `91db41aca7c1` | 今回列挙 |
| `project/resources/shaders/NeonSkinned.VS.hlsl` | 描画/NeonSkinned | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `1f2add40b948` | 今回列挙 |
| `project/resources/shaders/NeonSkinned.hlsli` | 描画/NeonSkinned | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L4等・計1箇所（未判定） | `e411edcba624` | 今回列挙 |
| `project/resources/shaders/NeonSkinnedOutline.PS.hlsl` | 描画/NeonSkinnedOutline | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L7等・計1箇所（未判定） | `0bab4619a6bf` | 今回列挙 |
| `project/resources/shaders/NeonSkinnedOutline.VS.hlsl` | 描画/NeonSkinnedOutline | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L6等・計1箇所（未判定） | `5ef412943920` | 今回列挙 |
| `project/resources/shaders/NeonSkinnedSkinning.hlsli` | 描画/NeonSkinnedSkinning | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L29等・計1箇所（未判定） | `40b9befc01f5` | 今回列挙 |
| `project/resources/shaders/NeonSkinnedStencilClear.PS.hlsl` | 描画/NeonSkinnedStencilClear | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L1等・計1箇所（未判定） | `baac39bad6e9` | 今回列挙 |
| `project/resources/shaders/NeonSkinnedStencilClear.VS.hlsl` | 描画/NeonSkinnedStencilClear | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L1等・計1箇所（未判定） | `cb5e500b5c33` | 今回列挙 |
| `project/resources/shaders/NeonSkinnedSurface.hlsli` | 描画/NeonSkinnedSurface | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L25等・計1箇所（未判定） | `8ac3bd1c70d8` | 今回列挙 |
| `project/resources/shaders/Object3d.PS.hlsl` | 描画/Object3d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L118等・計10箇所、P3:長行候補 L554等・計5箇所（未判定） | `1655bfd60625` | 今回列挙 |
| `project/resources/shaders/Object3d.Scene.PS.hlsl` | 描画/Object3d | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `b1fd02ba125c` | 今回列挙 |
| `project/resources/shaders/Object3d.VS.hlsl` | 描画/Object3d | 未確認 | 未確認 | 列挙のみ | 候補 P2:旧処理候補 L245等・計1箇所、P2:英語説明候補 L64等・計5箇所（未判定） | `ddfa6f9da410` | 今回列挙 |
| `project/resources/shaders/Object3d.hlsli` | 描画/Object3d | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L1等・計1箇所（未判定） | `d71f3db964d4` | 今回列挙 |
| `project/resources/shaders/ObjectPostBloomAdd.PS.hlsl` | 描画/ObjectPostBloomAdd | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L53等・計4箇所（未判定） | `0667dfdf787e` | 今回列挙 |
| `project/resources/shaders/ObjectPostComposite.PS.hlsl` | 描画/ObjectPostComposite | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `a5fc360f76bf` | 今回列挙 |
| `project/resources/shaders/ObjectPostOutlineAdd.PS.hlsl` | 描画/ObjectPostOutlineAdd | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `5d0f7a00eaa4` | 今回列挙 |
| `project/resources/shaders/Ocean.PS.hlsl` | 描画/Ocean | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `7e43f1faf856` | 今回列挙 |
| `project/resources/shaders/Ocean.VS.hlsl` | 描画/Ocean | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `3a04fb2f21f2` | 今回列挙 |
| `project/resources/shaders/OceanCommon.hlsli` | 描画/OceanCommon | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L52等・計3箇所（未判定） | `5279193f2482` | 今回列挙 |
| `project/resources/shaders/OceanFFT.CS.hlsl` | 描画/OceanFFT | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `284b7320d9df` | 今回列挙 |
| `project/resources/shaders/OceanFFTCommon.hlsli` | 描画/OceanFFTCommon | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `1a3bb71b51d6` | 今回列挙 |
| `project/resources/shaders/OceanFFTOutput.CS.hlsl` | 描画/OceanFFTOutput | 未確認 | 未確認 | 列挙のみ | 候補 P2:旧処理候補 L26等・計1箇所、P2:英語説明候補 L25等・計2箇所（未判定） | `525ba9b8e258` | 今回列挙 |
| `project/resources/shaders/OceanSpectrumEvolve.CS.hlsl` | 描画/OceanSpectrumEvolve | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L45等・計4箇所（未判定） | `f21e41f074b6` | 今回列挙 |
| `project/resources/shaders/OceanSpectrumInitialize.CS.hlsl` | 描画/OceanSpectrumInitialize | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L105等・計4箇所（未判定） | `9fb6f69f37a0` | 今回列挙 |
| `project/resources/shaders/Particle.PS.hlsl` | 描画/Particle | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `997554fdeeac` | 今回列挙 |
| `project/resources/shaders/Particle.VS.hlsl` | 描画/Particle | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `cb1a43a1014d` | 今回列挙 |
| `project/resources/shaders/Particle.hlsli` | 描画/Particle | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L1等・計1箇所（未判定） | `28cb90bd1b98` | 今回列挙 |
| `project/resources/shaders/ParticleCompute.hlsli` | 描画/ParticleCompute | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `f5c021d2b07c` | 今回列挙 |
| `project/resources/shaders/ParticleEmit.CS.hlsl` | 描画/ParticleEmit | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `60c3bc2dba1a` | 今回列挙 |
| `project/resources/shaders/ParticleEmitBatch.CS.hlsl` | 描画/ParticleEmitBatch | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `6905f68eec1f` | 今回列挙 |
| `project/resources/shaders/ParticleInitialize.CS.hlsl` | 描画/ParticleInitialize | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `6b0cdb6b332d` | 今回列挙 |
| `project/resources/shaders/ParticleUpdate.CS.hlsl` | 描画/ParticleUpdate | 未確認 | 未確認 | 列挙のみ | 候補 P2:旧処理候補 L127等・計9箇所、P2:英語説明候補 L17等・計6箇所（未判定） | `170dac4c342e` | 今回列挙 |
| `project/resources/shaders/PbrLighting.hlsli` | 描画/PbrLighting | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `420c49f406b8` | 今回列挙 |
| `project/resources/shaders/PostEffectCommon.hlsli` | 描画/PostEffectCommon | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `ea8270b5652e` | 今回列挙 |
| `project/resources/shaders/ProceduralFlame.PS.hlsl` | 描画/ProceduralFlame | 未確認 | 未確認 | 列挙のみ | 候補 P2:旧処理候補 L270等・計1箇所、P2:英語説明候補 L69等・計24箇所（未判定） | `9e8dfa5ea4c5` | 今回列挙 |
| `project/resources/shaders/ProceduralFlame.VS.hlsl` | 描画/ProceduralFlame | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `7e0e4a0ddebc` | 今回列挙 |
| `project/resources/shaders/Random.PS.hlsl` | 描画/Random | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L4等・計2箇所（未判定） | `885d555fc15f` | 今回列挙 |
| `project/resources/shaders/SSAODenoise.PS.hlsl` | 描画/SSAODenoise | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `14dc264f7a7b` | 今回列挙 |
| `project/resources/shaders/SSAOResolve.PS.hlsl` | 描画/SSAOResolve | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `f3944a40d637` | 今回列挙 |
| `project/resources/shaders/SSRDenoise.PS.hlsl` | 描画/SSRDenoise | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `89b8a0004968` | 今回列挙 |
| `project/resources/shaders/SSRResolve.PS.hlsl` | 描画/SSRResolve | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `91a4248b3401` | 今回列挙 |
| `project/resources/shaders/Shadow.PS.hlsl` | 描画/Shadow | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `340c8ac2b0e6` | 今回列挙 |
| `project/resources/shaders/Shadow.VS.hlsl` | 描画/Shadow | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `afd943d308f0` | 今回列挙 |
| `project/resources/shaders/SkinningObject3d.VS.hlsl` | 描画/SkinningObject3d | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `3a305c710a2d` | 今回列挙 |
| `project/resources/shaders/SkinningShadow.VS.hlsl` | 描画/SkinningShadow | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `5985a76e681f` | 今回列挙 |
| `project/resources/shaders/Skybox.PS.hlsl` | 描画/Skybox | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L7等・計1箇所（未判定） | `cc3b60f2cbd3` | 今回列挙 |
| `project/resources/shaders/Skybox.Scene.PS.hlsl` | 描画/Skybox | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `bd6b8236d2b5` | 今回列挙 |
| `project/resources/shaders/Skybox.VS.hlsl` | 描画/Skybox | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L15等・計1箇所（未判定） | `b3b57b4505f8` | 今回列挙 |
| `project/resources/shaders/TemporalResolve.PS.hlsl` | 描画/TemporalResolve | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `a5f054ca8c48` | 今回列挙 |
| `project/resources/shaders/Trail.PS.hlsl` | 描画/Trail | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `13caaa11e695` | 今回列挙 |
| `project/resources/shaders/Trail.Scene.PS.hlsl` | 描画/Trail | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `1f0f7938c6f1` | 今回列挙 |
| `project/resources/shaders/Trail.VS.hlsl` | 描画/Trail | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L5等・計1箇所（未判定） | `9f6868c8e6bd` | 今回列挙 |
| `project/resources/shaders/Trail.hlsli` | 描画/Trail | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `9a7d9f2fc588` | 今回列挙 |
| `project/resources/shaders/materials/CrystalMaterial.hlsli` | 描画/CrystalMaterial | 未確認 | 未確認 | 列挙のみ | 候補 P2:旧処理候補 L75等・計1箇所、P2:英語説明候補 L57等・計5箇所（未判定） | `96d96db56f7f` | 今回列挙 |

## 制作・検証ツールの一覧

| ファイルパス | 機能区分 | 文書 | 内部 | 確認範囲 | 要修正・優先度／判断理由 | 内容SHA256 | 根拠 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `project/resources/audio/tank_expedition/generate_audio.py` | 補助スクリプト | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `6d6f63037fc1` | 今回列挙 |
| `project/tools/TankSubmissionPackage.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 候補 P3:長行候補 L182等・計1箇所（未判定） | `5a4fac9b51c1` | 今回列挙 |
| `project/tools/audio_runtime_tests.cpp` | 検証C++ | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L1等・計10箇所（未判定） | `693120af49f7` | 今回列挙 |
| `project/tools/audit_source_comments.py` | 制作・検証スクリプト | 未確認 | 一部確認 | inventory/leading_comments/source_paths/mainの存在検査・引数検査 | 今回変更不要: 内容の正しさは判定しない。拡張子対象はh/cppのみ | `875441bd8fcf` | 今回参照／実行（全文の説明審査は未実施） |
| `project/tools/blender/export_level_test.py` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `30e489fd5ea1` | 今回列挙 |
| `project/tools/blender/import_level_test.py` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `fcc60db0c15b` | 今回列挙 |
| `project/tools/bootstrap_dependencies.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L105等・計1箇所、P3:長行候補 L163等・計1箇所（未判定） | `9feaa773fa46` | 今回列挙 |
| `project/tools/build_assimp_vs2026.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `925d27358279` | 今回列挙 |
| `project/tools/compare_retarget_rotations.py` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `54e84c8e379f` | 今回列挙 |
| `project/tools/find_animation_phase_offset.py` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `57cb1699a70c` | 今回列挙 |
| `project/tools/frame_pacer_tests.cpp` | 検証C++ | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L21等・計2箇所（未判定） | `8085c95bc993` | 今回列挙 |
| `project/tools/generated_texture_cache_tests.cpp` | 検証C++ | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `5e2c57e61833` | 今回列挙 |
| `project/tools/inspect_retarget_pose.py` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `9304005954e2` | 今回列挙 |
| `project/tools/inspect_root_rotation.py` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `eec6ef706e58` | 今回列挙 |
| `project/tools/inspect_vrm_materials.py` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `c45c5b7020ec` | 今回列挙 |
| `project/tools/level_aiditor/level_aiditor.py` | 制作・検証スクリプト | 未確認 | 一部確認 | generated表記の2箇所は制作ツールが出力する文字列と確認 | 列挙判定のみ: ツール全体の説明は未確認 | `f3d1bee5eb6d` | 今回参照のみ（全文の説明審査は未実施） |
| `project/tools/list_finger_bones.py` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `fd9a54f3744c` | 今回列挙 |
| `project/tools/measure_startup.cmd` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `8ee1d46b47f0` | 今回列挙 |
| `project/tools/measure_startup.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 候補 P3:長行候補 L18等・計2箇所（未判定） | `649cdb07ca87` | 今回列挙 |
| `project/tools/measure_tank_performance.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 候補 P3:長行候補 L35等・計1箇所（未判定） | `8274b83d0dca` | 今回列挙 |
| `project/tools/neon_skinned_model_tests.cpp` | 検証C++ | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `1ea10b290be4` | 今回列挙 |
| `project/tools/neon_skinned_pipeline_tests.cpp` | 検証C++ | 未確認 | 未確認 | 列挙のみ | 候補 P2:旧処理候補 L44等・計1箇所、P2:英語説明候補 L45等・計5箇所（未判定） | `4ce9761c05c3` | 今回列挙 |
| `project/tools/package_tank_submission.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 候補 P3:長行候補 L6等・計2箇所（未判定） | `b28f4eb6a102` | 今回列挙 |
| `project/tools/player_class_config_tests.cpp` | 検証C++ | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L1等・計10箇所、P3:長行候補 L423等・計1箇所（未判定） | `57d76394b2a9` | 今回列挙 |
| `project/tools/player_ship_editor/app.js` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L558等・計2箇所（未判定） | `56c7be6f29dd` | 今回列挙 |
| `project/tools/prepare_tank_submission_text.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 候補 P3:長行候補 L9等・計2箇所（未判定） | `a9f24166f92c` | 今回列挙 |
| `project/tools/prototype_boss_combat_tests.cpp` | 検証C++ | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L46等・計2箇所（未判定） | `059d30b19faa` | 今回列挙 |
| `project/tools/render_animation_preview.py` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `426446132a0c` | 今回列挙 |
| `project/tools/render_source_review_uml.py` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `2524427f5b2f` | 今回列挙 |
| `project/tools/retarget_mixamo_to_vroid.py` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `5551dbc1e876` | 今回列挙 |
| `project/tools/rival_boss_combat_tests.cpp` | 検証C++ | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L27等・計7箇所（未判定） | `06c42e60cfc3` | 今回列挙 |
| `project/tools/run_tank_expedition.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `a74ff9784427` | 今回列挙 |
| `project/tools/shader_disk_cache_tests.cpp` | 検証C++ | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L79等・計4箇所（未判定） | `740b1390342c` | 今回列挙 |
| `project/tools/startup_trace_tests.cpp` | 検証C++ | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L87等・計1箇所（未判定） | `adf56c805458` | 今回列挙 |
| `project/tools/tank_additional_abilities_tests.cpp` | 検証C++ | 未確認 | 一部確認 | 追加能力・投射物・衝突の関連する既存アサーションを参照 | 今回変更不要: 既存の回帰検査をそのまま使用 | `4727c001737b` | 今回参照／実行（全文の説明審査は未実施） |
| `project/tools/tank_collision_tests.cpp` | 検証C++ | 未確認 | 一部確認 | 追加能力・投射物・衝突の関連する既存アサーションを参照 | 今回変更不要: 既存の回帰検査をそのまま使用 | `322df97fd159` | 今回参照／実行（全文の説明審査は未実施） |
| `project/tools/tank_enemy_combat_tests.cpp` | 検証C++ | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L33等・計20箇所（未判定） | `b094b5cca881` | 今回列挙 |
| `project/tools/tank_expedition_balance_tests.cpp` | 検証C++ | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L31等・計5箇所、P3:長行候補 L123等・計1箇所（未判定） | `58d074583ace` | 今回列挙 |
| `project/tools/tank_expedition_content_tests.cpp` | 検証C++ | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L26等・計6箇所、P3:長行候補 L51等・計6箇所（未判定） | `de22b19c91b2` | 今回列挙 |
| `project/tools/tank_expedition_loadout_tests.cpp` | 検証C++ | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L15等・計3箇所（未判定） | `8637abf559ae` | 今回列挙 |
| `project/tools/tank_expedition_map_tests.cpp` | 検証C++ | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L23等・計15箇所、P3:長行候補 L414等・計1箇所（未判定） | `a53f05d35f52` | 今回列挙 |
| `project/tools/tank_expedition_rooms_tests.cpp` | 検証C++ | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L66等・計9箇所、P3:長行候補 L111等・計1箇所（未判定） | `2539c9818c28` | 今回列挙 |
| `project/tools/tank_expedition_tests.cpp` | 検証C++ | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L231等・計1箇所、P3:長行候補 L254等・計1箇所（未判定） | `8cd58abe8753` | 今回列挙 |
| `project/tools/tank_expedition_tutorial_tests.cpp` | 検証C++ | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L41等・計4箇所（未判定） | `cb0a9e09a4d4` | 今回列挙 |
| `project/tools/tank_guard_integration_tests.cpp` | 検証C++ | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L1等・計25箇所（未判定） | `27a9d92b42a0` | 今回列挙 |
| `project/tools/tank_presentation_tests.cpp` | 検証C++ | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `d5ac826614bc` | 今回列挙 |
| `project/tools/tank_projectile_tests.cpp` | 検証C++ | 未確認 | 一部確認 | 追加能力・投射物・衝突の関連する既存アサーションを参照 | 今回変更不要: 既存の回帰検査をそのまま使用 | `87a5f707fd66` | 今回参照／実行（全文の説明審査は未実施） |
| `project/tools/tank_reward_card_tests.cpp` | 検証C++ | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L16等・計3箇所（未判定） | `4314587d93ca` | 今回列挙 |
| `project/tools/tank_run_modifier_tests.cpp` | 検証C++ | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L28等・計2箇所（未判定） | `ba9d62b670f2` | 今回列挙 |
| `project/tools/tank_run_tests.cpp` | 検証C++ | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L241等・計3箇所、P3:長行候補 L338等・計2箇所（未判定） | `531c755ee9b4` | 今回列挙 |
| `project/tools/tank_trail_tests.cpp` | 検証C++ | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L1等・計8箇所（未判定） | `fb5c000923df` | 今回列挙 |
| `project/tools/test_audio_runtime.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `c67d48ad076f` | 今回列挙 |
| `project/tools/test_developer_tools_profile.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `206d1cc5c219` | 今回列挙 |
| `project/tools/test_frame_pacer.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `7592d113b875` | 今回列挙 |
| `project/tools/test_generated_texture_cache.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 候補 P3:長行候補 L16等・計3箇所（未判定） | `835a9b5eefd0` | 今回列挙 |
| `project/tools/test_neon_skinned_model.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 候補 P3:長行候補 L18等・計1箇所（未判定） | `da65d2ec3c7a` | 今回列挙 |
| `project/tools/test_neon_skinned_pipeline.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 候補 P3:長行候補 L17等・計1箇所（未判定） | `7392188ab0b2` | 今回列挙 |
| `project/tools/test_player_class_config.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 候補 P3:長行候補 L91等・計1箇所（未判定） | `d38b0ffe543c` | 今回列挙 |
| `project/tools/test_rival_boss_combat.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `265b4df9daea` | 今回列挙 |
| `project/tools/test_shader_disk_cache.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 候補 P3:長行候補 L26等・計2箇所（未判定） | `18bce3ae443a` | 今回列挙 |
| `project/tools/test_startup_trace.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 候補 P3:長行候補 L26等・計2箇所（未判定） | `e32884e61b28` | 今回列挙 |
| `project/tools/test_tank_additional_abilities.ps1` | 制作・検証スクリプト | 未確認 | 一部確認 | 既存テストの対象・実行条件・ソース抽出の目印（全脚本を読み取り） | 今回変更不要: 検査条件・抽出対象を保持 | `8f602cde0b92` | 今回参照／実行（全文の説明審査は未実施） |
| `project/tools/test_tank_collisions.ps1` | 制作・検証スクリプト | 未確認 | 一部確認 | 既存テストの対象・実行条件・ソース抽出の目印（全脚本を読み取り） | 今回変更不要: 検査条件・抽出対象を保持 | `426d647a14d8` | 今回参照／実行（全文の説明審査は未実施） |
| `project/tools/test_tank_combat_runtime.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 候補 P3:長行候補 L34等・計2箇所（未判定） | `1a9a862f139d` | 今回列挙 |
| `project/tools/test_tank_enemy_combat.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `aa74e29ba596` | 今回列挙 |
| `project/tools/test_tank_expedition.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `333853e7d9e3` | 今回列挙 |
| `project/tools/test_tank_expedition_content.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 候補 P3:長行候補 L18等・計1箇所（未判定） | `da3dc0ac796a` | 今回列挙 |
| `project/tools/test_tank_expedition_map.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 候補 P3:長行候補 L47等・計1箇所（未判定） | `5326a41983ac` | 今回列挙 |
| `project/tools/test_tank_expedition_map_runtime.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 候補 P3:長行候補 L34等・計1箇所（未判定） | `87929dd202d8` | 今回列挙 |
| `project/tools/test_tank_expedition_rooms.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 候補 P3:長行候補 L31等・計1箇所（未判定） | `06ba3c12be59` | 今回列挙 |
| `project/tools/test_tank_expedition_tutorial.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `9bf6888fbfd2` | 今回列挙 |
| `project/tools/test_tank_experience_runtime.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 候補 P3:長行候補 L44等・計4箇所（未判定） | `fa7b42e6ab12` | 今回列挙 |
| `project/tools/test_tank_presentation.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `e65e43f1e2af` | 今回列挙 |
| `project/tools/test_tank_projectiles.ps1` | 制作・検証スクリプト | 未確認 | 一部確認 | 既存テストの対象・実行条件・ソース抽出の目印（全脚本を読み取り） | 今回変更不要: 検査条件・抽出対象を保持 | `e5fb77b8e461` | 今回参照／実行（全文の説明審査は未実施） |
| `project/tools/test_tank_reward_cards.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 候補 P3:長行候補 L24等・計1箇所（未判定） | `862670f143e5` | 今回列挙 |
| `project/tools/test_tank_reward_pool.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 候補 P2:英語説明候補 L45等・計1箇所（未判定） | `6ea68888ae58` | 今回列挙 |
| `project/tools/test_tank_run.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `689267c9682f` | 今回列挙 |
| `project/tools/test_tank_special_runtime.ps1` | 制作・検証スクリプト | 未確認 | 一部確認 | 既存テストの対象・実行条件・ソース抽出の目印（全脚本を読み取り） | 今回変更不要: 検査条件・抽出対象を保持 | `de0a1874c8e4` | 今回参照／実行（全文の説明審査は未実施） |
| `project/tools/test_tank_submission.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `7fb98b01e0a0` | 今回列挙 |
| `project/tools/test_tank_submission_packaging.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 候補 P3:長行候補 L30等・計8箇所（未判定） | `7ef727f1190c` | 今回列挙 |
| `project/tools/test_tank_submission_runtime.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 候補 P3:長行候補 L44等・計1箇所（未判定） | `acaaa56ba460` | 今回列挙 |
| `project/tools/test_tank_trails.ps1` | 制作・検証スクリプト | 未確認 | 一部確認 | 既存テストの対象・実行条件・ソース抽出の目印（全脚本を読み取り） | 今回変更不要: 検査条件・抽出対象を保持 | `6fb900457ed3` | 今回参照／実行（全文の説明審査は未実施） |
| `project/tools/test_tank_tutorial_runtime.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 候補 P3:長行候補 L29等・計1箇所（未判定） | `45ff4ded261d` | 今回列挙 |
| `project/tools/test_title_demo.ps1` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 候補 P3:長行候補 L7等・計3箇所（未判定） | `0feba91f396b` | 今回列挙 |
| `project/tools/validate_animated_glb.py` | 制作・検証スクリプト | 未確認 | 未確認 | 列挙のみ | 未抽出（精査前） | `afc93c1bd3c9` | 今回列挙 |
