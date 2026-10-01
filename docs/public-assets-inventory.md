# 保持素材の確認台帳

2026-10-01追加分：Developer Preview用の第三者モデル`models/neon_hologram/AvatarSample_B.glb`（28,333,772 bytes）と同ディレクトリの`README.md`を新規収録しました。作者提供VRMのbyte-for-byteコピーで、VRoid ProjectのAvatarSample利用条件を記録しています。CC0ではありません。[モデルの出典・条件・SHA-256・用途](../project/resources/models/neon_hologram/README.md)、[第三者表示](../THIRD_PARTY_NOTICES.md)を参照してください。このGLBだけを現在のRelease提出パッケージから除外します。以下の220件の台帳は追加前の監査時点の記録として保持します。

2026-09-30の[最終監査](final-public-cleanup-audit.md)時点では218件を保持しました。出典分類と今回の用途によるA/B/C/Dは異なります。音声の制作記録preview.wavはリポジトリに保持し、実行用配布からだけ除外します。

2026-10-01、旧起動口廃止後の `project/resources/` 追跡ファイル一覧です。[監査本文](public-assets-audit.md)の分類・根拠・制約と合わせて使用してください。最初の素材整理18件の削除は監査本文、続く専用リソース21件の削除は[Ink Shooter除去記録](remove-ink-shooter-audit.md)、7件の削除は[Naval Prototype除去記録](remove-naval-prototype-audit.md)、旧テスト専用4件の削除は[3シーン除去記録](remove-legacy-test-scenes-audit.md)、Lab専用25件の削除は[Lab除去記録](remove-graphics-labs-audit.md)に記録しています。

**A**：自作設定・コード、または制作記録のある遠征用合成音声。**C**：OFL条件・表示を維持するフォント一式。**D**：制作経緯・出典・素材単体の再配布条件を作者が確認するもの。Dを自作・許諾済みと認定していません。ディレクトリ別の分類は既存の説明と実装に基づき、個々の制作経緯を証明するものではありません。

参照欄はソース・設定・モデル定義・制作ツールでの**ファイル名の文字列一致**です。同名ファイル、コメント、テスト用入力を含む場合があり、依存確定やローグライトでの使用を意味しません。文字列一致がなくても動的な名前構築やエディター選択があるため、未使用と断定していません。主要モデルの実際のロード箇所・副ファイル依存は監査本文を参照してください。

素材パスは `project/resources/` 相対、参照パスは `project/` 相対です。画像・バイナリの内容や個人情報候補の値は記録していません。

保持220件（A 97件、C 2件、D 121件）。旧起動用project1件を除去し、前回監査後に追加されたNeonSkinned shader3件を台帳へ補完しています。renderer・shader・pipeline test自体は変更していません。[起動口廃止監査](retire-legacy-run-entrypoints-audit.md)。

| 素材パス | 分類 | bytes | 参照候補（最大3箇所） |
| --- | --- | ---: | --- |
| `BossHP.png` | D | 3,442 | `game/enemy/actor/Enemy.cpp` |
| `Enemy.png` | D | 31,021 | `resources/enemy.mtl`, `resources/enemyBullet.mtl` |
| `HPBarCurrent.png` | D | 337 | `resources/playerHPBar.mtl`, `resources/playerHPBarLong.mtl` |
| `Player.png` | D | 40,809 | `resources/player.mtl` |
| `PlayerBullet.png` | D | 28,053 | `resources/playerBullet.mtl` |
| `UnderwaterCaustics.png` | D | 108,371 | 文字列一致なし／動的使用は未確定 |
| `audio/tank_expedition/README.md` | A | 5,834 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `externals/DirectXTex/DirectXTex_GDK_2019.vcxproj` 他6件 |
| `audio/tank_expedition/armor_break.wav` | A | 36,524 | `game/run/TankExpeditionAudio.h`, `resources/audio/tank_expedition/generate_audio.py`, `resources/audio/tank_expedition/measurements.json` |
| `audio/tank_expedition/dash.wav` | A | 22,604 | `game/run/TankExpeditionAudio.h`, `resources/audio/tank_expedition/generate_audio.py`, `resources/audio/tank_expedition/measurements.json` |
| `audio/tank_expedition/generate_audio.py` | A | 11,690 | 文字列一致なし／動的使用は未確定 |
| `audio/tank_expedition/hit.wav` | A | 16,844 | `game/run/TankExpeditionAudio.h`, `resources/audio/tank_expedition/generate_audio.py`, `resources/audio/tank_expedition/measurements.json` |
| `audio/tank_expedition/kill.wav` | A | 50,924 | `game/run/TankExpeditionAudio.h`, `resources/audio/tank_expedition/generate_audio.py`, `resources/audio/tank_expedition/measurements.json` |
| `audio/tank_expedition/measurements.json` | A | 3,696 | `resources/audio/tank_expedition/generate_audio.py` |
| `audio/tank_expedition/music_base.wav` | A | 2,880,044 | `game/run/TankExpeditionAudio.h`, `resources/audio/tank_expedition/generate_audio.py`, `resources/audio/tank_expedition/measurements.json` 他1件 |
| `audio/tank_expedition/music_intensity.wav` | A | 2,880,044 | `game/run/TankExpeditionAudio.h`, `resources/audio/tank_expedition/generate_audio.py`, `resources/audio/tank_expedition/measurements.json` |
| `audio/tank_expedition/preview.wav` | A | 2,880,044 | `resources/audio/tank_expedition/generate_audio.py`, `resources/audio/tank_expedition/measurements.json`, `tools/TankSubmissionPackage.ps1` 他1件 |
| `audio/tank_expedition/shot.wav` | A | 11,564 | `game/run/TankExpeditionAudio.h`, `resources/audio/tank_expedition/generate_audio.py`, `resources/audio/tank_expedition/measurements.json` 他1件 |
| `audio/tank_expedition/upgrade.wav` | A | 65,324 | `game/run/TankExpeditionAudio.h`, `resources/audio/tank_expedition/generate_audio.py`, `resources/audio/tank_expedition/measurements.json` |
| `audio/tank_expedition/warning.wav` | A | 27,884 | `game/run/TankExpeditionAudio.h`, `resources/audio/tank_expedition/generate_audio.py`, `resources/audio/tank_expedition/measurements.json` |
| `ball.mtl` | D | 245 | `resources/ball.obj`, `resources/bloomBall.obj` |
| `ball.obj` | D | 336,346 | `DirectX/engine/particle/EffectSequencer.cpp`, `DirectX/engine/particle/EffectSequencer.h`, `game/scene/GameScene.cpp` 他1件 |
| `block.mtl` | D | 243 | `resources/block.obj`, `resources/bloomBlock.obj`, `resources/expBlock.obj` 他1件 |
| `block.obj` | D | 1,084 | `game/exp/ExpEnemy.cpp`, `game/player/actor/Stage.cpp`, `game/scene/GameScene.cpp` 他1件 |
| `block.png` | D | 35,158 | `resources/block.mtl` |
| `bloomBall.mtl` | D | 243 | `resources/bloomBall.obj` |
| `bloomBall.obj` | D | 339,860 | `game/scene/GameScene.cpp`, `resources/levels/prefab_dictionary.json` |
| `bloomBall.png` | D | 164 | `resources/bloomBall.mtl`, `resources/bloomBlock.mtl` |
| `bloomBlock.mtl` | D | 236 | `resources/bloomBlock.obj` |
| `bloomBlock.obj` | D | 1,074 | `game/scene/GameScene.cpp` |
| `bossHPGreen.png` | D | 189 | `game/enemy/actor/Enemy.cpp` |
| `bossHPGreen1.png` | D | 207 | `resources/playerHPBarGreen.mtl`, `resources/playerHPBarGreenLong.mtl` |
| `bossHPRed.png` | D | 191 | `game/enemy/actor/Enemy.cpp` |
| `bullet.mtl` | D | 250 | `resources/bullet.obj`, `resources/enemyBullet.obj`, `resources/playerBullet.obj` |
| `bullet.obj` | D | 5,185 | `game/player/actor/Bullet.cpp` |
| `bulletShoot.mp3` | D | 17,553 | `game/scene/Game.cpp`, `tools/test_audio_runtime.ps1`, `tools/test_tank_submission_packaging.ps1` |
| `checkerBoard.png` | D | 1,166 | 文字列一致なし／動的使用は未確定 |
| `circle.png` | D | 27,583 | `resources/plane.mtl` |
| `configs/evolutionTree.json` | A | 1,147 | `game/player/actor/Player.h` |
| `configs/evolutionUiStyle.json` | A | 3,077 | `game/player/actor/Player.h` |
| `configs/expedition_content.json` | A | 30,461 | `game/run/TankExpeditionContent.h`, `game/scene/GameScene.ExpeditionMap.cpp`, `tools/TankSubmissionPackage.ps1` 他2件 |
| `configs/expedition_map.json` | A | 6,646 | `game/run/TankExpeditionMap.h`, `tools/TankSubmissionPackage.ps1`, `tools/test_tank_expedition_map.ps1` |
| `configs/gamePostEffects.json` | A | 7,599 | `game/scene/GameScene.Authoring.cpp`, `game/scene/GameScene.TankRunVisuals.cpp`, `game/scene/GameScene.cpp` 他1件 |
| `configs/gameText.json` | A | 981 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `game/scene/GameScene.cpp` |
| `configs/gameVisuals.json` | A | 7,716 | `game/scene/GameScene.Authoring.cpp`, `game/scene/GameScene.TankRunVisuals.cpp`, `game/scene/GameScene.cpp` 他1件 |
| `configs/playerClasses.json` | A | 45,261 | `game/player/actor/Player.h`, `game/scene/GameScene.cpp` |
| `configs/playerUpgradeHud.json` | A | 1,496 | `game/player/actor/Player.h` |
| `configs/screenEffects.json` | A | 1,371 | `CG2_testPro.vcxproj`, `game/scene/GameScene.TitleDemo.cpp`, `game/scene/GameScene.cpp` |
| `configs/tankButtonUiStyle.json` | A | 1,453 | `game/ui/TankButtonUI.cpp`, `game/ui/TankButtonUI.h` |
| `configs/tankExpeditionBalance.json` | A | 3,370 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `game/run/TankExpeditionBalance.h` 他1件 |
| `configs/tutorial.json` | A | 207 | `game/scene/GameScene.h` |
| `cube.mtl` | D | 264 | `resources/cube.obj`, `tools/test_tank_submission_packaging.ps1` |
| `cube.obj` | D | 639 | `DirectX/engine/3d/Skybox.cpp`, `game/player/actor/Stage.cpp`, `resources/levels/prefab_dictionary.json` 他1件 |
| `cube.png` | D | 1,594 | `resources/cube.mtl` |
| `cubeDamage.mtl` | D | 241 | `resources/cubeDamage.obj` |
| `cubeDamage.obj` | D | 854 | `game/player/actor/Stage.cpp`, `resources/levels/prefab_dictionary.json` |
| `cubeDamage.png` | D | 1,594 | `resources/cubeDamage.mtl` |
| `dashGide.png` | D | 2,019 | `game/scene/GameScene.cpp` |
| `deathParticle.png` | D | 1,899 | 文字列一致なし／動的使用は未確定 |
| `drone.png` | D | 1,233 | `game/player/actor/Player.cpp` |
| `effects/hit_spark.json` | D | 1,135 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `DirectX/engine/particle/ParticleManager.cpp` |
| `enemy.mtl` | D | 239 | `resources/enemy.obj`, `resources/expEnemy.obj` |
| `enemy.obj` | D | 3,157 | `game/exp/ExpEnemy.cpp`, `game/player/actor/PlayerDrone.cpp` |
| `enemy3D.mtl` | D | 447 | `resources/enemy3D.obj` |
| `enemy3D.obj` | D | 92,785 | `game/scene/GameScene.cpp` |
| `enemyBullet.mtl` | D | 239 | `resources/enemyBullet.obj` |
| `enemyBullet.obj` | D | 2,582 | 文字列一致なし／動的使用は未確定 |
| `enemyParticle.mtl` | D | 251 | `resources/enemyParticle.obj` |
| `enemyParticle.obj` | D | 600 | 文字列一致なし／動的使用は未確定 |
| `enemyParticle.png` | D | 168 | `resources/enemyParticle.mtl` |
| `expBlock.mtl` | D | 239 | `resources/expBlock.obj` |
| `expBlock.obj` | D | 1,072 | `game/exp/ExpEnemy.cpp` |
| `expEnemy.mtl` | D | 447 | `resources/expEnemy.obj` |
| `expEnemy.obj` | D | 92,786 | `game/exp/ExpEnemy.cpp` |
| `expPentagon.mtl` | D | 250 | `resources/expPentagon.obj` |
| `expPentagon.obj` | D | 1,457 | `game/exp/ExpEnemy.cpp` |
| `expTriangle.mtl` | D | 250 | `resources/expTriangle.obj` |
| `expTriangle.obj` | D | 599 | `game/exp/ExpEnemy.cpp` |
| `fade.png` | D | 2,780 | `game/player/actor/Player.cpp`, `game/scene/Fade.cpp` |
| `fonts/ZenMaruGothic-Bold.ttf` | C | 3,778,984 | `game/scene/GameScene.cpp`, `game/scene/TitleScene.cpp`, `resources/configs/evolutionUiStyle.json` 他2件 |
| `fonts/ZenMaruGothic-OFL.txt` | C | 4,496 | 文字列一致なし／動的使用は未確定 |
| `gradation.png` | D | 2,371 | `resources/weapon.mtl` |
| `gradationLine.png` | D | 314,004 | `game/scene/GameScene.cpp` |
| `ground.mtl` | D | 246 | `resources/ground.obj`, `tools/test_tank_submission_packaging.ps1` |
| `ground.obj` | D | 399 | `game/scene/GameScene.cpp`, `tools/test_tank_submission_packaging.ps1` |
| `gunBarrel.mtl` | D | 250 | `resources/gunBarrel.obj` |
| `gunBarrel.obj` | D | 9,558 | `game/player/actor/Player.cpp`, `game/weapon/WeaponMount.h`, `resources/configs/playerClasses.json` 他1件 |
| `hpBarFillMask.png` | D | 425 | `game/player/actor/Player.cpp`, `game/scene/GameScene.cpp`, `game/ui/NeonProgressBar.cpp` 他1件 |
| `hpBarFrame.png` | D | 623 | `game/scene/GameScene.cpp` |
| `hpBarMask.png` | D | 383 | `game/scene/GameScene.cpp` |
| `hpBarOutlineMask.png` | D | 636 | `game/scene/GameScene.cpp`, `game/ui/NeonProgressBar.cpp` |
| `jewelry.mtl` | D | 249 | `resources/jewelry.obj` |
| `jewelry.obj` | D | 1,186,446 | `game/scene/GameScene.cpp`, `resources/levels/prefab_dictionary.json` |
| `ka.png` | D | 1,466 | 文字列一致なし／動的使用は未確定 |
| `levels/ai_balance_handoff.md` | D | 1,147 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `game/scene/GameScene.cpp` 他1件 |
| `levels/ai_edit_prompt_template.md` | D | 2,945 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters` |
| `levels/level_test.json` | D | 15,103 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `game/scene/GameScene.cpp` 他3件 |
| `levels/prefab_dictionary.json` | D | 11,110 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters` |
| `levels/tank_dictionary.json` | D | 3,541 | `tools/level_aiditor/level_aiditor.py` |
| `levels/tank_run.json` | D | 3,005 | `game/scene/GameScene.cpp`, `tools/TankSubmissionPackage.ps1`, `tools/test_tank_submission_packaging.ps1` |
| `light.mtl` | D | 246 | `resources/light.obj` |
| `light.obj` | D | 9,586 | 文字列一致なし／動的使用は未確定 |
| `machineGun.png` | D | 2,140 | `game/player/actor/Player.cpp` |
| `map.csv` | D | 2,730 | `game/player/actor/Stage.cpp` |
| `maps/README.md` | A | 1,921 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `externals/DirectXTex/DirectXTex_GDK_2019.vcxproj` 他6件 |
| `maps/expedition_crossfire.csv` | A | 2,730 | `game/scene/GameScene.TankExpedition.cpp`, `resources/maps/expedition_rooms.json` |
| `maps/expedition_final_duel.csv` | A | 2,730 | `resources/maps/expedition_rooms.json` |
| `maps/expedition_hazard_lane.csv` | A | 2,730 | `resources/maps/expedition_rooms.json` |
| `maps/expedition_layouts.json` | A | 25,702 | `game/run/TankExpeditionRooms.h`, `tools/test_tank_expedition_rooms.ps1` |
| `maps/expedition_outskirts.csv` | A | 2,730 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `game/scene/GameScene.cpp` 他1件 |
| `maps/expedition_resource_fork.csv` | A | 2,730 | `resources/maps/expedition_rooms.json` |
| `maps/expedition_rooms.json` | A | 1,576 | 文字列一致なし／動的使用は未確定 |
| `monsterBall.png` | D | 18,232 | `resources/ball.mtl` |
| `neonTriangleParticle.mtl` | D | 214 | `resources/neonTriangleParticle.obj` |
| `neonTriangleParticle.obj` | D | 830 | `DirectX/engine/particle/ParticleManager.cpp` |
| `nn.png` | D | 1,045 | 文字列一致なし／動的使用は未確定 |
| `normalTank.png` | D | 1,224 | `game/player/actor/Player.cpp` |
| `plane.mtl` | D | 237 | `resources/plane.obj` |
| `plane.obj` | D | 372 | `DirectX/engine/particle/ParticleManager.cpp`, `DirectX/engine/particle/ParticleManager.h`, `game/scene/GameScene.cpp` 他1件 |
| `player.mtl` | D | 240 | `resources/player.obj` |
| `player.obj` | D | 3,159 | 文字列一致なし／動的使用は未確定 |
| `player3D.mtl` | D | 250 | `resources/player3D.obj`, `tools/test_tank_submission_packaging.ps1` |
| `player3D.obj` | D | 78,108 | `game/scene/GameScene.cpp`, `tools/test_tank_submission_packaging.ps1` |
| `playerBullet.mtl` | D | 246 | `resources/playerBullet.obj` |
| `playerBullet.obj` | D | 2,583 | 文字列一致なし／動的使用は未確定 |
| `playerHPBar.mtl` | D | 250 | `resources/playerHPBar.obj` |
| `playerHPBar.obj` | D | 390 | 文字列一致なし／動的使用は未確定 |
| `playerHPBarGreen.mtl` | D | 250 | `resources/playerHPBarGreen.obj` |
| `playerHPBarGreen.obj` | D | 392 | 文字列一致なし／動的使用は未確定 |
| `playerHPBarGreenLong.mtl` | D | 250 | `resources/playerHPBarGreenLong.obj` |
| `playerHPBarGreenLong.obj` | D | 393 | `game/enemy/actor/Enemy.cpp` |
| `playerHPBarLong.mtl` | D | 250 | `resources/playerHPBarLong.obj` |
| `playerHPBarLong.obj` | D | 392 | `game/enemy/actor/Enemy.cpp` |
| `playerParticle.mtl` | D | 252 | `resources/playerParticle.obj` |
| `playerParticle.obj` | D | 601 | 文字列一致なし／動的使用は未確定 |
| `playerParticle.png` | D | 169 | `resources/playerParticle.mtl` |
| `projects/default.project.json` | A | 149 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `game/scene/Game.cpp` 他2件 |
| `projects/tank_expedition.project.json` | A | 183 | `tools/measure_tank_performance.ps1`, `tools/run_tank_expedition.ps1`, `tools/test_tank_combat_runtime.ps1` 他5件 |
| `projects/tank_game.project.json` | A | 162 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `tools/TankSubmissionPackage.ps1` 他4件 |
| `rule.png` | D | 40,128 | 文字列一致なし／動的使用は未確定 |
| `se.png` | D | 802 | 文字列一致なし／動的使用は未確定 |
| `sea.mtl` | D | 246 | `resources/sea.obj` |
| `sea.obj` | D | 986,638 | 文字列一致なし／動的使用は未確定 |
| `shaders/BloomBlurH.PS.hlsl` | A | 2,786 | `DirectX/engine/commom/DirectXCommon.cpp` |
| `shaders/BloomBlurV.PS.hlsl` | A | 2,745 | `DirectX/engine/commom/DirectXCommon.cpp` |
| `shaders/BloomDownsample.PS.hlsl` | A | 1,200 | `DirectX/engine/commom/DirectXCommon.cpp` |
| `shaders/BloomExtract.PS.hlsl` | A | 752 | `DirectX/engine/commom/DirectXCommon.cpp` |
| `shaders/Composite.PS.hlsl` | A | 18,966 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `DirectX/engine/commom/DirectXCommon.cpp` |
| `shaders/FullScreen.VS.hlsl` | A | 422 | `DirectX/engine/commom/DirectXCommon.cpp` |
| `shaders/GaussianFilter.PS.hlsl` | A | 1,567 | `DirectX/engine/commom/DirectXCommon.cpp` |
| `shaders/ModelParticle.PS.hlsl` | A | 3,421 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `DirectX/engine/commom/DirectXCommon.cpp` 他1件 |
| `shaders/ModelParticle.Scene.PS.hlsl` | A | 65 | `DirectX/engine/commom/DirectXCommon.cpp` |
| `shaders/ModelParticle.VS.hlsl` | A | 1,240 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `DirectX/engine/commom/DirectXCommon.cpp` |
| `shaders/ModelParticle.hlsli` | A | 219 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `resources/shaders/ModelParticle.PS.hlsl` 他1件 |
| `shaders/MotionVectorResolve.PS.hlsl` | A | 1,452 | `DirectX/engine/commom/DirectXCommon.cpp` |
| `shaders/NeonSkinned.PS.hlsl` | A | 1,584 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `DirectX/engine/3d/neon/NeonSkinnedRenderer.cpp` 他1件 |
| `shaders/NeonSkinned.VS.hlsl` | A | 1,890 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `DirectX/engine/3d/neon/NeonSkinnedRenderer.cpp` |
| `shaders/NeonSkinned.hlsli` | A | 217 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `resources/shaders/NeonSkinned.PS.hlsl` 他1件 |
| `shaders/Object3d.PS.hlsl` | A | 60,683 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `DirectX/engine/commom/DirectXCommon.cpp` 他2件 |
| `shaders/Object3d.Scene.PS.hlsl` | A | 60 | `DirectX/engine/commom/DirectXCommon.cpp` |
| `shaders/Object3d.VS.hlsl` | A | 12,302 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `DirectX/engine/commom/DirectXCommon.cpp` 他1件 |
| `shaders/Object3d.hlsli` | A | 464 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `resources/shaders/Object3d.PS.hlsl` 他2件 |
| `shaders/ObjectPostBloomAdd.PS.hlsl` | A | 1,581 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `DirectX/engine/commom/DirectXCommon.cpp` |
| `shaders/ObjectPostComposite.PS.hlsl` | A | 7,533 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `DirectX/engine/commom/DirectXCommon.cpp` |
| `shaders/ObjectPostOutlineAdd.PS.hlsl` | A | 4,865 | `DirectX/engine/commom/DirectXCommon.cpp` |
| `shaders/Ocean.PS.hlsl` | A | 29,992 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `DirectX/engine/commom/DirectXCommon.cpp` |
| `shaders/Ocean.VS.hlsl` | A | 10,080 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `DirectX/engine/commom/DirectXCommon.cpp` |
| `shaders/OceanCommon.hlsli` | A | 3,607 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `resources/shaders/Ocean.PS.hlsl` 他1件 |
| `shaders/OceanFFT.CS.hlsl` | A | 2,586 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `DirectX/engine/3d/OceanRenderer.cpp` |
| `shaders/OceanFFTCommon.hlsli` | A | 9,456 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `resources/shaders/OceanFFT.CS.hlsl` 他3件 |
| `shaders/OceanFFTOutput.CS.hlsl` | A | 1,685 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `DirectX/engine/3d/OceanRenderer.cpp` |
| `shaders/OceanSpectrumEvolve.CS.hlsl` | A | 2,993 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `DirectX/engine/3d/OceanRenderer.cpp` |
| `shaders/OceanSpectrumInitialize.CS.hlsl` | A | 5,529 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `DirectX/engine/3d/OceanRenderer.cpp` |
| `shaders/Particle.PS.hlsl` | A | 828 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `DirectX/engine/commom/DirectXCommon.cpp` 他1件 |
| `shaders/Particle.VS.hlsl` | A | 611 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `DirectX/engine/commom/DirectXCommon.cpp` |
| `shaders/Particle.hlsli` | A | 175 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `resources/shaders/ModelParticle.PS.hlsl` 他3件 |
| `shaders/ParticleCompute.hlsli` | A | 502 | `resources/shaders/ParticleEmit.CS.hlsl`, `resources/shaders/ParticleEmitBatch.CS.hlsl`, `resources/shaders/ParticleInitialize.CS.hlsl` 他1件 |
| `shaders/ParticleEmit.CS.hlsl` | A | 3,342 | `DirectX/engine/commom/DirectXCommon.cpp`, `DirectX/engine/particle/ParticleManager.h` |
| `shaders/ParticleEmitBatch.CS.hlsl` | A | 883 | `DirectX/engine/commom/DirectXCommon.cpp` |
| `shaders/ParticleInitialize.CS.hlsl` | A | 642 | `DirectX/engine/commom/DirectXCommon.cpp` |
| `shaders/ParticleUpdate.CS.hlsl` | A | 6,455 | `DirectX/engine/commom/DirectXCommon.cpp` |
| `shaders/PbrLighting.hlsli` | A | 7,035 | `resources/shaders/Object3d.PS.hlsl` |
| `shaders/PostEffectCommon.hlsli` | A | 4,825 | `resources/shaders/BloomExtract.PS.hlsl`, `resources/shaders/Composite.PS.hlsl`, `resources/shaders/MotionVectorResolve.PS.hlsl` 他5件 |
| `shaders/ProceduralFlame.PS.hlsl` | A | 16,006 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `DirectX/engine/commom/ProceduralFlameRenderer.cpp` |
| `shaders/ProceduralFlame.VS.hlsl` | A | 560 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `DirectX/engine/commom/ProceduralFlameRenderer.cpp` |
| `shaders/Random.PS.hlsl` | A | 2,783 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `DirectX/engine/commom/DirectXCommon.cpp` |
| `shaders/SSAODenoise.PS.hlsl` | A | 3,617 | `DirectX/engine/commom/DirectXCommon.cpp` |
| `shaders/SSAOResolve.PS.hlsl` | A | 4,828 | `DirectX/engine/commom/DirectXCommon.cpp` |
| `shaders/SSRDenoise.PS.hlsl` | A | 3,773 | `DirectX/engine/commom/DirectXCommon.cpp` |
| `shaders/SSRResolve.PS.hlsl` | A | 5,979 | `DirectX/engine/commom/DirectXCommon.cpp` |
| `shaders/Shadow.PS.hlsl` | A | 14 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters` |
| `shaders/Shadow.VS.hlsl` | A | 423 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `DirectX/engine/commom/DirectXCommon.cpp` 他1件 |
| `shaders/SkinningObject3d.VS.hlsl` | A | 2,242 | `CG2_testPro.vcxproj`, `DirectX/engine/commom/DirectXCommon.cpp`, `tools/neon_skinned_pipeline_tests.cpp` |
| `shaders/SkinningShadow.VS.hlsl` | A | 1,087 | `CG2_testPro.vcxproj`, `DirectX/engine/commom/DirectXCommon.cpp`, `tools/neon_skinned_pipeline_tests.cpp` |
| `shaders/Skybox.PS.hlsl` | A | 2,206 | `DirectX/engine/commom/DirectXCommon.cpp`, `resources/shaders/Skybox.Scene.PS.hlsl` |
| `shaders/Skybox.Scene.PS.hlsl` | A | 58 | `DirectX/engine/commom/DirectXCommon.cpp` |
| `shaders/Skybox.VS.hlsl` | A | 823 | `DirectX/engine/commom/DirectXCommon.cpp` |
| `shaders/TemporalResolve.PS.hlsl` | A | 2,436 | `DirectX/engine/commom/DirectXCommon.cpp` |
| `shaders/Trail.PS.hlsl` | A | 1,167 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `DirectX/engine/commom/DirectXCommon.cpp` 他1件 |
| `shaders/Trail.Scene.PS.hlsl` | A | 57 | `DirectX/engine/commom/DirectXCommon.cpp` |
| `shaders/Trail.VS.hlsl` | A | 685 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `DirectX/engine/commom/DirectXCommon.cpp` |
| `shaders/Trail.hlsli` | A | 130 | `CG2_testPro.vcxproj`, `CG2_testPro.vcxproj.filters`, `resources/shaders/Trail.PS.hlsl` 他1件 |
| `shaders/materials/CrystalMaterial.hlsli` | A | 4,222 | `resources/shaders/Object3d.PS.hlsl` |
| `si.png` | D | 936 | 文字列一致なし／動的使用は未確定 |
| `start.png` | D | 6,021 | 文字列一致なし／動的使用は未確定 |
| `ta.png` | D | 1,239 | 文字列一致なし／動的使用は未確定 |
| `testBox.mtl` | D | 243 | `resources/testBox.obj` |
| `testBox.obj` | D | 847 | 文字列一致なし／動的使用は未確定 |
| `toRule.png` | D | 4,525 | 文字列一致なし／動的使用は未確定 |
| `toTitle.png` | D | 5,659 | `game/scene/GameScene.cpp` |
| `triangleParticle.mtl` | D | 250 | `resources/neonTriangleParticle.obj`, `resources/triangleParticle.obj` |
| `triangleParticle.obj` | D | 604 | `DirectX/engine/particle/ParticleManager.cpp` |
| `twin.png` | D | 1,284 | `game/player/actor/Player.cpp` |
| `u.png` | D | 824 | 文字列一致なし／動的使用は未確定 |
| `ui/salvage_orb.png` | D | 2,878 | `game/scene/GameScene.ExpeditionExperience.cpp`, `game/ui/TankRewardCard.cpp` |
| `uvChecker.png` | D | 106,081 | 文字列一致なし／動的使用は未確定 |
| `wasd.png` | D | 7,767 | `game/scene/GameScene.cpp` |
| `weapon.mtl` | D | 255 | `resources/weapon.obj` |
| `weapon.obj` | D | 194,001 | 文字列一致なし／動的使用は未確定 |
| `white512x512.png` | D | 2,248 | `DirectX/engine/3d/Model.cpp`, `DirectX/engine/3d/SkinCluster.cpp`, `DirectX/engine/commom/TrailStressFixture.h` 他29件 |
| `ya.png` | D | 1,228 | 文字列一致なし／動的使用は未確定 |
