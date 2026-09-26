# 遠征の特殊ビルドと工房統合

対象はシューター・ドローン・近接の3系統です。操作は従来どおり移動・照準・左クリック・右クリックのダッシュで、レールキャノン取得時だけ左クリックが「押してチャージ、離して発射」に変わります。数値はF2/F6の効果倍率が1.0のときの標準値です。

## 1. 能力と敵の仕様

| カード | 系統 | レア度 | 基本価格 | 戦い方の変化 |
|---|---|---|---:|---|
| `RailCannon` レールキャノン | シューター | エピック | 58 Cr | 狙いを合わせてチャージし、並んだ敵を撃ち抜く |
| `DroneLaserLink` レーザーリンク | ドローン | エピック | 58 Cr | 機体と隊形を動かしてレーザー網を敵へ重ねる |
| `SlashWave` 斬撃波 | 近接 | レア | 42 Cr | 3段目を当てる方向と、飛ばす先を考える |
| `ParryBlade` パリィブレード | 近接 | エピック | 58 Cr | 弾の到達を見て剣を振り、防御を反撃へつなげる |

実際の購入価格は、カード価格とその工房の最低価格の高い方です。F6で価格やレア度を変更できます。

**レールキャノン**は約1秒で最大チャージ。短押しは通常1発の約0.65倍、最大は約5倍です。敵3体を貫通して次の敵へ届き、取得済みの貫通・壁反射も既存の弾ルールで合成します。1砲門1発を保ち、機体換装で砲門が増えた場合は各砲門から発射します。チャージ中の移動目標速度は78%、発射後の硬直は攻撃間隔に応じて0.12～0.35秒です。砲口へ収束する粒子、最大チャージの紫色リング、太い軌跡、約0.24秒の残光、命中リング、小さな反動とSEを加えています。

**レーザーリンク**は2機なら1本、3機なら三角形、4機以上は隣接する機体同士を輪状に接続します。壁を横切る接続は無効です。敵ごとに0.20秒間隔を設け、複数の線に同時接触しても同じtickに重複ダメージを与えません。1tickはドローン基礎攻撃力・強化・換装倍率から計算した威力の45%を基準にし、ボスにはさらに65%を掛けます。通常の線は細く、接触地点に短い光と粒子が出ます。追加ドローンや精密ドローンも反映されます。

**斬撃波**は3段目の攻撃開始時に1回発生します。ダメージはその3段目本体の55%、発生位置と飛翔距離を合わせた射程は近接の約2倍。2体を貫通して最大3体へ作用します。刃先延長は大きさと射程、終撃増幅は威力、連撃加速は3段目へ到達するテンポに反映されます。三日月状のネオンと残光を描き、敵弾を切った際は専用の火花・SEを出します。通常の斬撃波に画面停止はありません。

**パリィブレード**は斬撃の攻撃判定中に、範囲と向きが合う敵弾だけへ作用します。通常パリィは残り耐久6以下の弾を破壊し、それより高い弾は消せません。攻撃判定開始から0.12秒以内はPerfectとなり、標準で耐久を8削ります。破壊できた場合は逆方向へプレイヤー所有の反射弾を新規生成します。Perfectでは短い `PARRY!` 表示・確認音・約0.035秒の停止を使い、同じ斬撃で停止とPerfect表示を連発しません。

敵弾耐久の基準は通常弾6、遠征ボス弾12以上、狙撃弾24です。斬撃波は耐久を6削るため、通常弾は破壊できても新品のボス弾・狙撃弾は1回で消せません。F6の効果倍率を上げると特殊能力も強くなるため、この基準値は標準倍率で比較してください。

| 敵 | 標準HP | 挙動・対処 |
|---|---:|---|
| `ShieldGuard` シールド兵 | 80 | 正面110度の射撃を85%軽減、近接を60%軽減。側面・背面は通常ダメージ。旋回速度に上限があり、回り込みや跳弾、ドローンの位置取りが有効 |
| `BladeGuard` ブレード兵 | 65 | 接近後、前方110度を0.45秒予告→0.14秒薙ぎ払い→0.90秒以上硬直。標準ダメージ27と強い押し出し。1振りにつき1回だけ命中し、単なる身体接触ではダメージを与えない |

シールドは正面の半透明の弧とブロック時の光・SE、ブレード兵は赤橙の予告扇・大型の刃・硬直時の水色で状態を伝えます。両者は `guard_patrol` の編成に入り、標準では左から9列目以降の通常戦闘候補です。チュートリアルには追加していません。

## 2. 進化を工房の強化カードへ統合

成長は「戦闘→Cr回収→工房でカード購入→戦闘」に統一しました。導入後の3系統選択は残り、以降の独立した進化地点・進化候補の準備・進化専用購入分岐を、通常の工房処理へ置き換えています。

`players` の既存8機体は削除せず、`refitPlayer` で参照する換装カードとして再利用します。標準ではすべてエピック、基本価格56 Crです。

| 系統 | 換装カード |
|---|---|
| シューター | ツイン砲台、扇形砲台、速射機体、反射機体 |
| ドローン | ドローン母艦、重射撃編隊 |
| 近接 | 旋回ブレード、重撃ブレード |

換装は同系統に限り1ラン1回。取得後は別の換装カードを抽選から除外します。現在HP・取得済み強化を保持し、Crは購入価格だけ消費します。プレビューも機体の色・形、砲門数と扇角、交互射撃、ドローン数、攻撃倍率を反映します。

新規ランダム遠征には `Evolution` を生成せず、その枠を工房などに置換しました。標準18～22列、最後のボスへの合流、同種3連続を避ける条件は維持しています。旧固定マップの `evolution` は引き続き読め、実際の遠征開始時に `Upgrade` として扱います。アリーナの既存進化処理と従来19種のドラフト候補は維持します。

## 3. 抽選・保存・制作ツール

戦闘スタイル選択後の工房では、有効かつ未取得の挙動変更カードが残っていれば1枠を優先抽選します。残り2枠は通常抽選です。レア度の重み、重複除外、所持効果との互換性、同じ店の候補の再現性を保っています。換装も挙動変更カードに含みます。

- **F2**：3系統の基礎性能・基本強化率。従来の適用・保存を継続して使えます。
- **F6・強化**：4つの新Effect、系統、レア度、価格、効果倍率。換装カードの名前・説明・価格・レア度もここで調整します。
- **F6・自機／換装**：元となる機体の性能・砲門・形・色。機体を新規作成すると適用時に換装カードを生成します。
- **F6・敵→F4→F5**：敵種を作成し、部屋へ配置し、出現する列範囲と抽選重みを指定します。

`expedition_content.json` はschema4になり、旧schema1～3も読み込みます。不明なEffectや不正な換装参照は読込エラーとし、適用済みデータを壊さない既存の挙動を保っています。ここでいう保存は制作データの保存です。中断したランを復元する新しいセーブ機能は追加していません。

## 4. 基礎バランス

| 系統 | 標準攻撃力 | 標準間隔・機数 | 意図 |
|---|---:|---|---|
| シューター | 4→6 | 約0.333→0.30秒 | 自分で狙う代わりに直接火力を高くする。理論上約20 DPS |
| ドローン | 4→3 | 0.50秒・3機を維持 | 回避・位置取りと並行して攻撃できる。全弾命中なら約18 DPS |
| 近接 | 既存値を維持 | 既存3段コンボを維持 | 接近の危険を負い、3段目・斬撃波・パリィで瞬間的な利益を得る |

DPSは比較用の理論値です。命中率、ドローンの追従、敵の向き、換装・特殊能力の組み合わせを含む実戦の優劣を保証する値ではありません。

## 5. テスト結果

実施済みの確認：

| テスト | 結果・範囲 |
|---|---|
| `test_tank_run.ps1` | PASS。既存アリーナの経済・ドラフト・カード上限・ボス・強化の3スイート |
| `test_tank_expedition.ps1` | PASS。従来の遠征進行、既存進化ロードアウト、基礎性能保存・読込 |
| `test_tank_expedition_content.ps1` | PASS。F6のコンパイル、schema1～4、保存・再読込、不正入力、全新能力と全8換装が実際に抽選されること、系統制限と二重換装防止 |
| `test_tank_expedition_map.ps1` | PASS。C++17/20で各2048シード、18～22列・全経路・3連続回避・旧Evolutionの工房化・既存固定経路 |
| `test_tank_expedition_rooms.ps1` | PASS。部屋・敵・マップの参照、保存、配置、到達性。新敵の中盤以降への登録 |

関連するビルド・描画・実機検証：

| 確認項目 | 最終結果 |
|---|---|
| Release build | PASS。最終コードでビルド成功。`generated/special-combat-release.log` |
| Development build | PASS。最終コードでビルド成功。`generated/special-combat-development.log` |
| `test_tank_projectiles.ps1` | PASS。既存640組＋本番の4能力処理、通常弾6／ボス弾12／狙撃弾24、壁、貫通、同一対象の多重命中防止 |
| `test_tank_reward_pool.ps1` | PASS。50,400表示条件で最大85/96通常スプライト・28/32発光スプライト。起動時確保量を維持 |
| `test_tank_reward_cards.ps1` | PASS。559,104サンプルの範囲・有限値、新能力4種、換装の砲門と扇角、ホバー停止 |
| `test_tank_enemy_combat.ps1` / `test_tank_collisions.ps1` | PASS。本番AI／ダメージ処理で盾の正面・背面、剣の予告・硬直・1振り1命中、壁遮蔽、実際の発射弾耐久、衝突回帰 |
| `test_tank_expedition_map_runtime.ps1` | PASS（Release）。旧固定経路10地点、工房購入4回、換装1回、修理1回、ボス撃破、Lv1／EXP0 |
| `test_tank_experience_runtime.ps1` | PASS（Release、近接、上下両ルート）。各19地点、両導入58 Cr、同じ3候補、換装時の状態保持。上ルートでは実際の回避2回も検証 |
| `test_tank_special_runtime.ps1` | PASS（Release、Development）。最終Releaseで1秒チャージ・2体へ各30ダメージ、3本のリンクと14tick、3段目の波3回と42ダメージ、通常／Perfectパリィと高耐久弾の生存を確認 |
| `test_title_demo.ps1 -Configuration Release` | PASS。4段階、射撃観測451回、実撃破36、ダッシュ32、報酬3回、フェード中停止、新規遠征への遷移 |

マップ／体験のランタイム試験では後半の戦闘を強制クリアするため、難易度の試遊結果とは分けて扱います。体験試験では工房の換装候補の抽選シードを試験用に固定し、実際の抽選関数・購入処理を通してHP・通貨・取得済み強化の保持を確認します。専用の特殊能力試験は実際の射撃・リンク・斬撃・弾パリィを検証するために追加しています。

特殊能力の実行試験は入力と標的配置を固定し、プレイヤーに試験用の無敵を設定しますが、ダメージを強制付与せず本番の攻撃処理を使います。斬撃波の標的は通常の剣が届かない8.6単位先に置いています。結果と6枚の画像は `project/generated/special_validation/` に保存しています。最終Releaseの実画面でチャージの最大色変化、リンク、薄い残光を伴う三日月、通常／Perfectパリィを確認しました。Developmentの実行試験後に変更したのは残光・チャージSEと試験の撮影位置であり、その最終コードでもDevelopmentビルドは成功しています。

## 6. 起動時間と描画コスト

特殊能力の線・リング・三日月・命中光は既存のネオン描画へ頂点を追加する方式で、本編とカードプレビューが描画関数を共有します。追加の外部画像・モデル・シェーダーはありません。未取得能力のUIやエフェクトを種類ごとに起動時生成せず、戦闘中に必要な弾・状態だけを作ります。既存のカード装飾プールとプレビューの描画結果再利用も維持します。

旧UIの条件付き初期化、タイトルの戦闘デモ、シェーダー／生成テクスチャのディスクキャッシュを維持しています。追加能力による秒単位の起動遅延は確認していません。

| StartupTraceの区間 | PR #207の記録 | 今回の変更前 | 今回の変更後 |
|---|---:|---:|---:|
| タイトルの最初のフレーム | 1.899秒 | 3.634秒 | 2.285秒 |
| ゲーム開始要求→作戦マップ | 2.234秒 | 2.307秒 | 2.198秒 |
| 本編scene initialize | 1.456秒 | 1.491秒 | 1.414秒 |

今回の前後は同じPC・Release・同じ保存済みキャッシュで各1回測定し、ビルドや別のゲーム実行を止めています。単発計測の揺らぎは含みます。未使用UI省略2件、シェーダーディスクヒット40、生成テクスチャヒット3、RenderTexture122枚、カード装飾384個、graphics47／compute4 PSOは前後で同数です。

データは `generated/abilities-startup-before/summary.json` と `generated/abilities-startup-after/summary.json`、詳細な区間記録は同じフォルダーの `warm.json` です。測定手順と更新記録は `docs/startup-performance.md` にあります。

## 7. 変更ファイルと人による試遊

以下はリポジトリルートからのパスです。

**戦闘・当たり判定・敵**

```text
project/game/player/TankSpecialCombat.h
project/game/player/TankCombatStyleBalance.h
project/game/player/TankRunModifiers.h
project/game/player/actor/Player.h
project/game/player/actor/Player.cpp
project/game/player/actor/Bullet.h
project/game/player/actor/Bullet.cpp
project/game/player/actor/BulletManager.h
project/game/player/actor/BulletManager.cpp
project/game/collision/CollisionManager.cpp
project/game/exp/ExpGuardCombat.h
project/game/exp/ExpEnemy.h
project/game/exp/ExpEnemy.cpp
project/game/exp/EnemyManager.cpp
project/game/enemy/actor/Enemy.cpp
```

**進行・制作ツール・データ**

```text
project/game/run/TankRunDirector.h
project/game/run/TankRunCopy.h
project/game/run/TankExpeditionContent.h
project/game/run/TankExpeditionMap.h
project/game/run/TankExpeditionRooms.h
project/game/editor/ExpeditionContentEditor.cpp
project/game/editor/ExpeditionMapEditor.cpp
project/game/scene/GameScene.ExpeditionMap.cpp
project/game/scene/GameScene.ExpeditionBuild.cpp
project/game/scene/GameScene.TankRun.cpp
project/resources/configs/expedition_content.json
project/resources/configs/expedition_map.json
project/resources/configs/tankExpeditionBalance.json
project/resources/maps/expedition_layouts.json
```

**演出・カードプレビュー・シーンの接続**

```text
project/game/effects/TankSpecialNeonGeometry.h
project/game/run/TankExpeditionAudio.h
project/game/scene/GameScene.h
project/game/scene/GameScene.cpp
project/game/scene/GameScene.TankExpedition.cpp
project/game/ui/TankRewardCard.h
project/game/ui/TankRewardCard.cpp
project/game/ui/TankRewardCardDemo.h
project/game/ui/TankRewardPreviewRenderer.cpp
```

**テスト・ビルド・説明書**

```text
project/CG2_testPro.vcxproj
project/game/scene/GameScene.ExperienceValidation.cpp
project/game/scene/GameScene.SpecialValidation.cpp
project/tools/tank_collision_tests.cpp
project/tools/tank_enemy_combat_tests.cpp
project/tools/tank_guard_integration_tests.cpp
project/tools/tank_expedition_balance_tests.cpp
project/tools/tank_expedition_content_tests.cpp
project/tools/tank_expedition_map_tests.cpp
project/tools/tank_expedition_rooms_tests.cpp
project/tools/tank_expedition_tests.cpp
project/tools/tank_projectile_tests.cpp
project/tools/tank_reward_card_tests.cpp
project/tools/tank_run_tests.cpp
project/tools/test_tank_collisions.ps1
project/tools/test_tank_enemy_combat.ps1
project/tools/test_tank_expedition_map_runtime.ps1
project/tools/test_tank_experience_runtime.ps1
project/tools/test_tank_projectiles.ps1
project/tools/test_tank_special_runtime.ps1
project/tools/test_tank_combat_runtime.ps1
project/tools/test_title_demo.ps1
project/tools/measure_startup.ps1
docs/tank-expedition-authoring-guide.md
docs/startup-performance.md
docs/tank-special-builds.md
```

人による試遊は未実施です。特に次を確認し、F2/F6で少しずつ調整してください。

1. レールの1秒チャージと離して発射する操作が気持ちよく、短押しにも使い道があるか。多砲門換装で威力・光量が過剰にならないか。
2. ドローンの通常射撃より、レーザー網を重ねる位置取りに意味があるか。壁で切れる接続や命中地点を見分けられるか。
3. 斬撃波を狙って3段目まで出す利益があり、近接の接近リスクを消していないか。通常パリィとPerfectの違いを音と光で理解できるか。
4. シールドの側背面へ回れるか。ブレード兵の予告を見て横や後ろへ抜け、硬直へ反撃する余裕があるか。
5. 特殊能力と換装の価格・頻度が適切か。換装を取らない基本機体でも進めるか。数値強化との選択で迷う余地があるか。
6. 敵弾・自機・ドローン本体が演出に埋もれないか。多数の敵・弾・ドローンがいる場面でもフレーム時間が安定するか。
