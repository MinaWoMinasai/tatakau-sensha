# 遠征ビルド拡張・schema 5 実装記録

2026-09-27。遠征のシューター／ドローン／近接に、11個の行動変更能力、7個の追加成長、新敵3種を追加した変更の記録です。表の数値は標準設定・効果倍率1.0です。実際の最終ダメージは既存強化、整数丸め、敵の防御によって変わります。

## 1. 変更範囲と成長の流れ

最初の3系統選択と「戦闘→Cr回収→工房→カード購入」の流れを維持します。遠征の成長は機体の一括交換から、砲門・射撃方式・ドローン性能・ブレード性能を積み重ねる方式へ変更しました。取得済み効果、HP、Crを機体交換でリセットする処理は使用しません。アリーナ側の既存クラス／進化機能は残します。

コードは既存のPlayer、Bullet、CollisionManager、ExpEnemy、工房のデータと描画に接続しています。新モード・新操作キー・外部素材・新シェーダーは追加していません。主な実装は [Player.SpecialAbilities.cpp](../project/game/player/actor/Player.SpecialAbilities.cpp)、[BulletManager.cpp](../project/game/player/actor/BulletManager.cpp)、[ExpEnemy.cpp](../project/game/exp/ExpEnemy.cpp)、[TankExpeditionContent.h](../project/game/run/TankExpeditionContent.h) です。変更ファイル一覧は付録に記載します。

## 2. 廃止した標準キットと換装

以下の標準複合カード8種を通常候補・移行後のカタログから除外します。

`BankshotKit` / `DashBomberKit` / `BreachBladeKit` / `ArcBladeKit` / `DroneBastionKit` / `SeekingWingKit` / `SeekingRicochetKit` / `PredatorCircuit`

`Refit_*` カードと `refitPlayer` による遠征の機体交換も廃止しました。旧 `players` の8機体は保存・編集できる制作データとして保持し、新規遠征の換装候補にはしません。既知の標準キット以外の独自IDの複合カードは残ります。旧交換カードは読込時に正規化し、交換だけのカードを除外、通常効果を持つ独自カードは交換参照だけを取り除きます。

前回の「換装1ラン1回」という仕様は今回の遠征には適用しません。旧 `Evolution` ノードを工房として扱う互換処理と、18～22列の標準経路は維持しています。

## 3. 7個の追加成長

1砲門から出る弾は1発です。追加砲門は機体の砲門そのものを増やします。

| Effect ID／名前 | 系統 | レア度・基本価格 | 標準効果・前提 |
|---|---|---|---|
| `ExtraBarrel1` 追加砲門 | シューター | アンコモン・28 Cr | 1門→2門。各弾65%威力。丸め前の合計は元の130% |
| `ExtraBarrel2` 追加砲門II | シューター | レア・42 Cr | 追加砲門が必要。2門→3門。各弾50%、丸め前の合計150% |
| `FanMount` 扇形砲架 | シューター | アンコモン・28 Cr | 追加砲門が必要。2門は±8度、3門は−14／0／+14度 |
| `AlternatingFire` 交互射撃機構 | シューター | レア・42 Cr | 追加砲門が必要。1門ずつ順番に発射し、発射間隔を門数で割る。基本の総火力を維持 |
| `HeavyDroneCore` 重装ドローン | ドローン | レア・42 Cr | 威力×1.45、射撃間隔×1.20、弾の見た目×1.20、軌跡幅×1.25。機数を減らさない |
| `LightBladeActuator` 軽量ブレード機構 | 近接 | アンコモン・28 Cr | 威力×0.95、予備動作・攻撃・硬直を各×0.85 |
| `HeavyBladeEdge` 重装ブレード | 近接 | レア・42 Cr | 威力×1.45、予備動作・攻撃・硬直を各×1.18 |

軽量／重装ブレードは同時取得でき、既存の連撃加速などとも乗算します。追加砲門・扇形・交互射撃も重ねて保持します。レールキャノンの交互射撃では1回の解放で1門を使い、砲門数分の補正をその弾へ集約して、次の砲門へ進めます。

## 4. シューターの新能力4種

| Effect ID／名前 | レア度・基本価格 | 標準挙動 |
|---|---|---|
| `ChainLightning` 連鎖放電 | レア・42 Cr | 命中地点から半径7以内の別敵へ最大2回連鎖。各連鎖は元弾48%。同じ敵へ戻らず、壁を越えない |
| `MarkDetonation` 爆裂マーカー | エピック・58 Cr | 同じ敵への射撃4回で起爆。ボスは6回。マークは最終命中から4秒。起爆半径2.8、主対象は元弾×1.75、周囲はその50%、ボスへの起爆はさらに65% |
| `BoomerangShell` 往復弾 | エピック・58 Cr | 標準0.60秒で自機へ折り返す。往路／復路それぞれ同じ敵へ1回。戻ると消滅し、弾寿命も維持。効果倍率は折返しまでの時間へ反映 |
| `KillBurst` 撃破バースト | レア・42 Cr | 射撃撃破で6方向へ元弾35%の小型弾。子弾から再バースト・連鎖・マークを発生させない |

連鎖／起爆による撃破はバーストの起点にもなりますが、子弾へ能力を複製しないため無限連鎖はしません。既存の貫通弾の対象別命中記録を使い、重なったまま毎フレーム効果が発生することを防ぎます。反射装甲による返り弾は45%威力・紫赤色の敵所有弾で、再反射フラグを保持します。

## 5. ドローンの新能力4種

| Effect ID／名前 | レア度・基本価格 | 標準挙動 |
|---|---|---|
| `DroneCharge` 突撃ドローン | レア・42 Cr | 左クリック中、使用可能な1機が0.30秒予告して狙った敵の位置へ突撃。標準2.8秒間隔、命中は基礎ドローン弾×3、押し出し0.14。帰還中は射撃しない |
| `DroneRebuildBomb` 自爆再構築 | レジェンド・78 Cr | 1機が0.65秒予告して突撃・自爆。半径4.5、基礎ドローン弾×8、押し出し0.26。5.5秒の再構築中は射撃・衝突・リンクから外れ、復帰する。発動間隔9秒 |
| `TargetPainter` ターゲットペインター | レア・42 Cr | 2機以上から合計4回命中するとLOCK。蓄積の猶予は最終命中から2秒、LOCKは4秒。対象へのドローンダメージ+35%、ボス+18%。射撃・突撃／爆発・レーザーリンクへ適用 |
| `AutonomousSpread` 自律分散モード | レジェンド・78 Cr | 左クリック中、ドローンごとに近い別対象を優先。対象が少なければ共有し、通常射撃威力は18%低下 |

突撃と自爆の威力はボスへ75%補正します。標的の選択はカーソル付近・自機から26以内を使い、壁越しの突撃開始や爆発ダメージは避けます。突撃が届かない場合は1秒で帰還へ切り替えます。初回の突撃準備は1.5秒、自爆準備は4秒です。

既存 `DroneFocus` は遠征で威力+15%、弾速+15%、照準の追従・誘導を担当します。重装ドローンとの役割を分け、重装だけで機数が減る変更はありません。

## 6. 近接の新能力3種

| Effect ID／名前 | レア度・基本価格 | 標準挙動 |
|---|---|---|
| `DashSlash` ダッシュ斬り | レア・42 Cr | ダッシュ中／直後の攻撃を0.22秒の斬り抜けへ変更。基礎近接1段目×1.20、押し出し0.28。同一対象へ1回、次は通常2段目へ接続 |
| `SpinBlade` 回転ブレード | エピック・58 Cr | 3段目後に左クリックを続けるとスタミナ1を消費。0.8秒で最大4回の360度斬撃。各30%威力、射程85%、非ダッシュ中の移動目標速度55%。パリィ取得時は通常パリィ可能、Perfectは発生しない |
| `WallSmash` 壁砕き | レジェンド・78 Cr | 強い押し出し後1秒以内の実際の壁衝突で、基礎近接×1.75を追加。半径2.8へその40%の衝撃。1回の押し出しにつき1回 |

ダッシュ斬りは通常斬撃を重複生成せず、移動区間による命中判定と専用の対象ID記録を使います。回転斬りは各振りごとに1回命中し、3段目扱いにしないため斬撃波を連発しません。壁砕きは敵の壁衝突カウンターを参照し、壁の近くにいるだけでは発動しません。主対象への壁衝撃はその敵の位置を攻撃源として扱い、正面シールドによる射撃軽減を誤適用しません。ボスは壁砕きの直接トリガー対象外ですが、近くの敵からの衝撃波は届きます。

## 7. 新敵3種と出現時期

| Behavior／名前 | 標準HP／Cr | 挙動と対処 | 標準部屋・UI上の開始列 |
|---|---|---|---|
| `SummonerCommander` 召喚指揮兵 | 75／12 | 約5秒周期、0.75秒のリング予告。HP12の小型突進兵を最大3体同時・累計6体まで召喚。召喚兵はCr／XPなし、16秒寿命、指揮兵死亡時に消滅。指揮兵を先に狙う判断が有効 | `command_post`・8列目以降 |
| `EMPJammer` EMP妨害兵 | 60／12 | 6秒周期、半径8を1秒予告。範囲内かつ壁に遮られていない自機へ2.5秒妨害。ドローン射撃間隔×1.5、誘導旋回×0.55、スタミナ回復×0.60。重複加算せず残り時間を更新 | `emp_patrol`・11列目以降 |
| `ReflectArmor` 反射装甲兵 | 90／14 | 正面100度からの通常弾／レール弾を45%威力の敵弾へ反射。旋回に上限があり側背面を取れる。近接・斬撃波・連鎖・爆発は反射されない | `reflect_bastion`・14列目以降 |

上記は通常戦闘の部屋候補で重み2です。JSONは0始まりのため `firstColumn` は7／10／13になります。導入の訓練部屋には出しません。既存シールド兵・ブレード兵と、その部屋 `guard_patrol` は残しています。

召喚位置は壁・自機の至近距離・他敵との重なりを避けます。世界全体で召喚兵24体、敵128体の追加生成上限も設けています。警告リング・シールド・火花・SEは既存の描画とイベント経路を再利用します。反射された弾をPerfect Parryで返した場合も、再反射禁止の印を引き継ぎます。

## 8. 保存・抽選・編集・プレビュー

保存形式はカタログの **schemaVersion 5**。旧schema1～4を読み込み、18個の新効果の標準カードを不足分だけ補います。schema5で意図的に削除したカードは自動復活しません。Effect IDは末尾追加で既存IDの順序を保ち、不明Effect／Behavior、不正な倍率などは現在の適用済みCatalogを保持したまま読込エラーにします。保存は一時ファイルからの置換です。

新18カードは標準で各1回購入。系統と前提条件、取得済み効果、購入上限を確認します。工房3択の1枠は有効な未取得の挙動変更カードから優先抽選し、残り2枠は通常のレア度重みで選びます。価格表はカードの基本価格であり、実売価格は工房の最低価格との高い方です。

| キー | 編集内容 |
|---|---|
| F2 | 3系統の基礎性能、基本強化率、既存ボス調整 |
| F4 | 敵・壁・開始位置・目標を持つ部屋。新3敵も敵リストから配置可能 |
| F5 | 部屋の出現列と抽選重み、固定／ランダム経路 |
| F6 | 強化の効果・対応系統・レア度・価格・倍率、14種の敵行動を基にした敵調整、旧互換機体データ |

F3のネオン／ブルーム／軌跡調整も維持しています。具体的な編集順と保存先は [制作ツールのガイド](tank-expedition-authoring-guide.md) に記載しています。

カードは既存の3枠とそのプレビュー描画を再利用します。11能力の説明用スナップショットを追加し、砲門の追加・交互射撃・隊形・突撃／再構築・斬り抜けなどを取得前後で比較できます。本編と同じネオン形状・軌跡・HDRブルームを使いますが、本編のPlayerやEnemyManagerをカード内で動かす方式ではありません。ホバー中だけアニメーションを更新し、非ホバーは既存の描画結果を再利用します。効果ごとの別シーン・別モデル・別テクスチャは起動時に生成しません。

## 9. バランスと上限

基礎シューターは6ダメージ／0.30秒、基礎ドローンは3ダメージ／0.50秒×3機を維持しています。重装ドローンは理論DPSで約1.21倍、追加砲門は丸め前で1.30／1.50倍が目安です。自律分散は標的分散との交換で通常射撃威力を下げ、回転斬りはスタミナと移動速度、自爆は一時的な機数減少を代償にします。

連鎖2回、バースト6発、マーク台帳128件、特殊イベント数、召喚数などに上限を置きます。子弾への再バースト、同じ敵への無限多重命中、反射装甲とパリィの無限反射を防ぎます。軽量／重装／連撃加速を重ねた近接の最終テンポや、複数砲門とレールの瞬間火力は実プレイで継続調整する対象です。

## 10. 自動検証とStartupTrace

純粋ロジックの検証に加え、実際のBullet／CollisionManager／Player／ExpEnemy処理を用いた検証と、Release実行ファイルを動かすランタイム検証を行いました。

| 検証 | 結果／内容 |
|---|---|
| `test_tank_enemy_combat.ps1` | **PASS**。pure timing/navigationと、実ExpEnemy／EnemyManager処理を抽出した統合検証。既存盾剣、EMP予告・範囲・壁、反射角度、召喚上限・報酬0・親死亡cleanup、1押し出し1壁衝突を確認 |
| `test_tank_expedition_rooms.ps1` | **PASS**。標準部屋データ、新敵参照・中後半生成範囲、保存再読込、配置／到達性チェック、F4 editorコンパイル |
| 実ヘッダを使う `ExpEnemy.cpp`／`EnemyManager.cpp` syntax check | **PASS**。`cl /Zs` で確認 |
| Release build | **PASS**。最終変更を含むx64ビルド |
| Development build | **PASS**。最終変更を含むx64ビルド |
| `test_tank_projectiles.ps1`／`test_tank_collisions.ps1` | **PASS**。連鎖数・対象重複、マーク寿命と起爆、往復命中、再バースト禁止、装甲反射、追加砲門とHP／Cr保持、既存弾処理 |
| `test_tank_reward_pool.ps1`／`test_tank_reward_cards.ps1` | **PASS**。装飾50,400ケース、プレビュー1,032,192ケース。砲門ごとの比較、追加能力デモ、非ホバー停止と描画上限 |
| `test_tank_run.ps1`／`test_tank_expedition.ps1`／`test_tank_expedition_map.ps1` | **PASS**。既存モード・遠征・マップの回帰検証。C++17／20の各2,048 seedを確認 |
| `test_tank_expedition_content.ps1`／`test_tank_additional_abilities.ps1` | **PASS**。schema1～5、廃止カード除外、独自複合カード保持、18効果・前提・抽選・編集、ドローン状態遷移・LOCK・分散・回転・EMP・追加補正 |
| `test_tank_special_runtime.ps1 -Configuration Release` | **PASS**。既存4＋新11能力の15項目、18キャプチャ。往復2回命中、自爆後の1機離脱と復帰、複数標的への分散、ダッシュ斬り→斬撃波、回転パリィにPerfectが出ないこと、実壁衝突を確認 |
| `test_tank_expedition_map_runtime.ps1` | **PASS**。Release。旧Evolutionを含む固定ルート、4回の追加強化購入、取得済み効果保持、換装0回・進化0回、クリアまでの遷移 |
| `test_tank_experience_runtime.ps1 -Configuration Release -Route Both -Style Melee` | **PASS**。上下それぞれ19地点を通過。導入報酬は両方58 Cr、強化3択も一致。上ルートの実攻撃・回避2回、全5レア度、近接コンボと追加強化保持を確認 |
| `test_title_demo.ps1 -Configuration Release` | **PASS**。4段階の実戦デモ、射撃450サンプル、35撃破、29ダッシュ、3報酬、フェード中停止と初期状態の遠征への遷移 |

特殊能力の実行検証は自動入力と固定の標的を使い、ダメージを直接注入せず、実際の攻撃・衝突処理を通します。検証中の自機は無敵です。マップ／体験検証は後半戦を強制クリアするため、経路・工房・保持データの確認に使い、難易度評価には使いません。体験検証の導入戦闘は強制クリアしません。

[既存の起動高速化](startup-performance.md) を確認して作業しました。シェーダーディスクキャッシュ、生成テクスチャキャッシュ、タイトル／遠征の条件付き初期化、旧UI省略、カード装飾数、タイトル戦闘デモの構造は維持します。新しい能力状態はCPU上の小さな配列・タイマーを使い、演出は既存のライン／軌跡／パーティクルへ追加します。同じPC・Release・同じキャッシュで、ビルド／他ゲーム停止中にWarm条件を変更前後各1回測定しました。

| StartupTrace指標 | 変更前 | 変更後 | 確認 |
|---|---:|---:|---|
| タイトル最初のフレーム | 3.066秒 | 2.909秒 | −0.157秒 |
| ゲーム開始→作戦マップ | 1.969秒 | 1.952秒 | −0.017秒 |
| scene initialize | 1.199秒 | 1.185秒 | −0.014秒 |

各1回のため微小な差は実行ごとの揺らぎを含みます。この測定では秒単位の悪化はありません。前後で全カウンターが一致しました。シェーダーディスクヒット40、メモリーヒット56、生成テクスチャヒット3、読み込みテクスチャ131、graphics PSO47・compute PSO4、RenderTexture122、初期化待機省略122、未使用アリーナUI省略2、カード装飾スプライト384です。

元データは `generated/builds-v5-startup-before/summary.json`・`warm.json` と `generated/builds-v5-startup-after/summary.json`・`warm.json`。能力・ルート・タイトルの実行記録とキャプチャは `project/generated/special_validation`、`expedition_map`、`experience_validation/upper`・`lower`、`title_demo` に保存しています。

## 11. 人による試遊で重点的に見る点

人間による試遊は未実施です。自動テストは操作・命中・状態遷移の確認であり、楽しさや最終的な難易度を保証するものではありません。実行時のキャプチャでは連鎖・起爆・突撃／再構築・斬り抜け・回転などの描画を確認しました。

1. シューターは連鎖向けの敵の並び、マークの集中攻撃、往復弾の帰路、撃破バーストの起点で狙い方が変わるか。反射装甲を正面から撃った結果が理解できるか。
2. ドローンは集中／分散、レーザー網、突撃、自爆の一時的な減員を見分けられるか。EMPの円から出る余地があり、妨害後も攻撃・回避できるか。
3. 近接はダッシュ斬り→2・3段目→回転の接続、通常パリィとPerfectの違い、敵を壁へ押す位置取りに手応えがあるか。攻撃速度強化を重ねても予告や敵弾を読めるか。
4. 召喚兵を無限に狩る展開にならず、指揮兵を倒した際の消滅と無報酬が納得できるか。ボス・盾兵・剣兵が新ビルドで極端に弱く／強くならないか。
5. 工房で新しい能力に出会え、前提未取得や別系統のカードが出ず、カードの演出から実戦の変化が予想できるか。レジェンドの価格と効果を選びたいと感じるか。
6. 強化の多い終盤でも自機・敵弾・危険範囲を見失わず、FPS表示で持続的な低下がないか。起動・タイトルデモ・開始遷移の待ち時間が戻っていないか。

## 付録：変更ファイル一覧

今回の変更ファイルです。生成した実行ファイル、計測JSON、キャプチャは既存の `generated` 配下に保存しています。

### 追跡済みファイルの変更

```text
docs/startup-performance.md
docs/tank-expedition-authoring-guide.md
docs/tank-special-builds.md
project/CG2_testPro.vcxproj
project/DirectX/engine/struct/Struct.h
project/game/collision/Collider.h
project/game/collision/CollisionManager.cpp
project/game/collision/CollisionManager.h
project/game/editor/ExpeditionContentEditor.cpp
project/game/exp/EnemyManager.cpp
project/game/exp/EnemyManager.h
project/game/exp/ExpEnemy.cpp
project/game/exp/ExpEnemy.h
project/game/exp/ExpGuardCombat.h
project/game/player/TankRunModifiers.h
project/game/player/TankSpecialCombat.h
project/game/player/actor/AttackController.cpp
project/game/player/actor/Bullet.cpp
project/game/player/actor/Bullet.h
project/game/player/actor/BulletManager.cpp
project/game/player/actor/BulletManager.h
project/game/player/actor/Player.cpp
project/game/player/actor/Player.h
project/game/player/actor/PlayerDrone.cpp
project/game/player/actor/PlayerDrone.h
project/game/run/TankExpeditionContent.h
project/game/run/TankExpeditionMap.h
project/game/run/TankExpeditionRooms.h
project/game/run/TankRunCopy.h
project/game/run/TankRunDirector.h
project/game/scene/GameScene.ExpeditionBuild.cpp
project/game/scene/GameScene.ExpeditionMap.cpp
project/game/scene/GameScene.ExperienceValidation.cpp
project/game/scene/GameScene.SpecialValidation.cpp
project/game/scene/GameScene.TankRun.cpp
project/game/scene/GameScene.cpp
project/game/scene/GameScene.h
project/game/ui/TankRewardCard.cpp
project/game/ui/TankRewardCardDemo.h
project/game/ui/TankRewardPreviewRenderer.cpp
project/resources/configs/expedition_content.json
project/resources/configs/expedition_map.json
project/resources/maps/expedition_layouts.json
project/tools/tank_collision_tests.cpp
project/tools/tank_enemy_combat_tests.cpp
project/tools/tank_expedition_content_tests.cpp
project/tools/tank_expedition_rooms_tests.cpp
project/tools/tank_guard_integration_tests.cpp
project/tools/tank_projectile_tests.cpp
project/tools/tank_reward_card_tests.cpp
project/tools/tank_run_modifier_tests.cpp
project/tools/test_tank_enemy_combat.ps1
project/tools/test_tank_expedition_map_runtime.ps1
project/tools/test_tank_experience_runtime.ps1
project/tools/test_tank_projectiles.ps1
project/tools/test_tank_special_runtime.ps1
```

### 新規ファイル

```text
docs/tank-build-variety-v5.md
project/game/player/TankShooterAbilities.h
project/game/player/actor/Player.SpecialAbilities.cpp
project/tools/tank_additional_abilities_tests.cpp
project/tools/test_tank_additional_abilities.ps1
```

