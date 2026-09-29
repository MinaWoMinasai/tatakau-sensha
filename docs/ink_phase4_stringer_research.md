# 第4段階: トライストリンガーの調査と実装境界

調査日: 2026-09-12。対象は対戦用トライストリンガー、基準バージョンは前段階と同じ **11.3.0**。将来の最新版との一致を意味しない。数値を確認するファイルは `WeaponStringerNormal`。`_Coop`、`_Msn`、`WeaponStringerShort`、`WeaponStringerExplosion` は別用途・別ブキであり、対象値に混ぜない。

## 一次資料と確度

1. [任天堂「個性が光るブキとギア」](https://www.nintendo.com/jp/character/splatoon/fashion/index.html): ストリンガーは2段階までチャージ可能。ジャンプ中は構えと照準が縦向きになる。トライストリンガーは3方向へ発射し、チャージ弾は冷却されて着弾点で破裂する。これは仕様の大枠を確認できる公式説明。
2. [任天堂のゲーム紹介](https://www.nintendo.com/en-ca/whatsnew/splatoon-3-makes-a-big-splash-in-new-video-preview-filled-to-the-gills-with-fresh-gameplay-and-new-details/): 3方向の同時発射と、冷却弾が短時間留まってから爆発することを説明している。
3. [Leanny 11.3.0 対戦用パラメータ](https://github.com/Leanny/splat3/blob/7280ff9cde8bb1c5dcef46c700c326471584d2e6/data/parameter/1130/weapon/WeaponStringerNormal.game__GameParameterTable.json): 以下の値の固定参照元。これは解析者が公開したゲームデータであり、任天堂が公開した実行コードではない。値が存在することと、コード内部の補間・順序・丸めまで分かることは区別する。
4. [任天堂 更新履歴](https://en-americas-support.nintendo.com/app/answers/detail/a_id/61257/): 8.0.0 ではトライストリンガーの落下飛沫の配置を、一直線につながりやすく変更。LACT-450 は空中チャージ速度を地上と同じに変更。後者をトライストリンガーへの直接変更として扱わない。
5. [Sendou のパラメータ説明ソース](https://github.com/sendou-ink/sendou.ink/blob/873c5e941d58049f60de58b50bbc20ea81a04d9b/app/features/params/core/param-explanations.ts): 作者による説明を確認。インク消費量・チャージ中移動速度・発射後隙の意味を補助する。一部の Stringer 項目は説明未記入。ArrowAngleMax/Min の説明は映像・公式説明の収束方向と食い違うため、名前だけから「Max=フルチャージ」と解釈しない。
6. [XarrotD の旧版パラメータ調査](https://splatoonwiki.org/wiki/User%3AXarrotD/610ParamsTest.json): 解析者自身の公開ページ。6.1.0 の記録なので現在の当たり判定や飛沫幅には採用しない。共通既定値を追う手掛かりに留める。`paramtable` 本体は今回403で取得できず、未読内容を根拠にしない。

一般 Wiki の記述は追加調査の手掛かりとして照合したが、生値の代替根拠にはしていない。旧版や発売前映像には、爆発やチャージキープの仕様が異なるものがある。

## 固定データで直接確認した値

時間は60F/秒、長さは既存CG2の縮尺 `S=0.5 world/raw`。速度は `raw × 60 × S`、加速度は `raw × 60² × S`。ダメージ値はゲーム表示値へ `/10`。フレーム名が「発射入力から」なのか「状態遷移後から」なのかは、値だけでは確定しない。

| 項目 | raw: 最小 / 1周 / フル | CG2への変換 |
|---|---|---|
| ChargeFrame | 9 / 30 / 72F | 0.15 / 0.5 / 1.2秒 |
| FreezeFrame | 12 / 15 / 15F | 0.2 / 0.25 / 0.25秒 |
| InkConsume | 0.05 / 0.06 / 0.085 | 1射で5 / 6 / 8.5% |
| DirectHitDamage | 300 / 350 / 350 | **1矢**30 / 35 / 35、3矢なら90 / 105 / 105 |
| SpawnSpeed | 2.1 / 2.1 / 3.85 | 63 / 63 / 115.5 world/秒 |
| Paint WidthHalf | 2.5 / 2.5 / 3.0 | 1.25 / 1.25 / 1.5のブラシ包絡幅 |

| 項目 | raw値 | 使用上の注意 |
|---|---|---|
| PostDelayFrame | 10F | 発射後の潜伏等の待ち時間。再チャージFreezeと別タイマー |
| IsEnableChargeKeep | false | 潜伏・武器変更でチャージを保持しない |
| IsExplosiveBoltMidCharge | true | 1周到達以降に冷却弾 |
| ArrowAngleMax / Mid | 8° / 8° | 最大拡散角、1周角。フル時の角度0はこのoverrideには省略 |
| ArrowMargin | 0.4 | CG2では発射位置間隔0.2 |
| BowTiltDegreeMax | 90° | 地上と空中の向きの差 |
| MoveSpeedFullCharge | 0.068 | 2.04 world/秒。チャージ全域の速度曲線は別途近似 |
| GoStraightToBrakeStateFrame | 4F | 直進区間 |
| BrakeToFreeStateFrame | 1F | brake区間 |
| BrakeAirResist / BrakeGravity | 0.1 / 0.04 | 1F速度損失、加速度72 world/秒² |
| FreeAirResist / FreeGravity | 0.24 / 0.15 | 1F速度損失、加速度270 world/秒² |
| BrakeToFreeVelocityXZ / Y | 20 / 10 | 600 / 300 world/秒。符号・条件・優先順はnativeコード未取得 |
| GoStraightStateEndMaxSpeed | 10 | 有効射程そのものではない |
| Init/EndRadiusForPlayer | 0.205 / 0.205 | 半径0.1025 world |
| DetonationFrame | 45F | 着弾後0.75秒の待機として採用 |
| Blast DistanceDamage | Damage300, Distance2.05 | 爆風30、半径1.025 world |
| Blast PaintRadius | 2.0 | 1.0 world |
| Blast DamageOffsetY / PaintOffsetY | 0.6 / 0.6 | 0.3 world。実装はワールド座標Y方向へ加算。法線方向への加算ではない |
| Splash WidthHalf / Nearest | 1.39 / 1.683 | 0.695 / 0.8415 world |
| Splash DepthScaleMax | 2.7 | 着弾主塗りの値と混ぜない |
| Splash DropInterval / SplashNumMax / SplitNum | 10 / 5 / 5 | シューターの1.5個・8位相と別の設定 |
| Splash RandomSpawnVelXMax | 0.02 | ネイティブの生成方向・乱数分布は未確認 |

出典は上記固定 [WeaponStringerNormal JSON](https://github.com/Leanny/splat3/blob/7280ff9cde8bb1c5dcef46c700c326471584d2e6/data/parameter/1130/weapon/WeaponStringerNormal.game__GameParameterTable.json)。幾何学的なブラシ包絡半径を、そのまま完全な円で塗り潰す義務はない。既存のCPU/GPU共通楕円マスクを使い、実塗りの中心部・縁のローブはCG2の表現として分ける。

## 省略された既定値と空中チャージ

`ArrowNum=3` は公式の3方向同時射撃により確定できる。フル拡散0°、地形当たり半径0.2 raw、インク回復停止20Fは、武器overrideに存在せず共通既定値として報告されているため、ヘッダでその確度を区別した。地形半径と回復停止はCG2初期値に採用するが、固定ファイルに直接書かれていると説明しない。

空中チャージについて `AirChargeRateByInkEmpty=1` だけを読み「地上と同速度」と結論してはいけない。[LACT 7.2.0](https://github.com/Leanny/splat3/blob/7280ff9cde8bb1c5dcef46c700c326471584d2e6/data/parameter/720/weapon/WeaponStringerShort.game__GameParameterTable.json) と [LACT 8.0.0](https://github.com/Leanny/splat3/blob/7280ff9cde8bb1c5dcef46c700c326471584d2e6/data/parameter/800/weapon/WeaponStringerShort.game__GameParameterTable.json) を比較すると、その値だけが1から3になり、6/12/34Fのチャージ時間は同じ。[任天堂8.0.0の空中同速度化](https://en-americas-support.nintendo.com/app/answers/detail/a_id/61257/) と対応するので、値1を保つトライストリンガーは地上の1/3速度と推論できる。`airborneChargeRate=1/3` を採用する。これは版差分と公式変更内容からの推論であり、ネイティブの除算式を読めたという意味ではない。

## 添付動画の観察

対象: `ローカル参考資料（非同梱）`。29.6秒、1920×1080、平均約30.03fps。映像の1枚をゲーム内部の1Fと見なさない。抽出物は `generated/ink_phase4/stringer_reference/`。

| 抽出 | 観察 |
|---|---|
| `contact_sheet.jpg` | 0秒から2秒ごと、左→右・上→下。床に3本の異なる経路が生じ、塗りには途切れがある。後半は縦射ちの帯が見える |
| `stuck_8_11s.jpg` | 8秒から0.25秒ごと。三角形の冷却矢が地面に残り、その後の破裂が発射演出から独立して発生する |
| `jump_12_18s.jpg` | 12秒から0.25秒ごと。ジャンプで縦構えに変わり、落ちる矢が前後方向にずれた帯を作る。上向き射撃でも水平直線に吸着していない |

この映像をもとに、着弾と遅延破裂を別イベントにすること、3方向を示す照準、2段階のチャージリング、水平／垂直の広がりを実装した。原作映像の発光の見え方に対して、CG2の装填矢・冷却矢は通常の液体シェーダーで描き、加算発光は使わない。入力ボタン表示がないため、最小9F前に離した場合の内部入力保留、インクを消費する正確な時点、キャンセル時の払い戻しは確定できない。

## 完成したCG2実装と原作との境界

以下は設計案ではなく、2026-09-12時点の [InkSimulation.Stringer.cpp](../project/game/ink/InkSimulation.Stringer.cpp)、[InkSimulation.cpp](../project/game/ink/InkSimulation.cpp)、シーン描画の実装内容である。入力順序や積分方法まで原作と同一だとは扱わない。

### チャージ、解放、インク

[StringerWeaponParams.h](../project/game/ink/StringerWeaponParams.h) は固有の性能値を持つ。[InkStringerPattern.h](../project/game/ink/InkStringerPattern.h) の `EvaluateStringerCharge(params, chargeSeconds)` は `StringerChargeProfile` を値で返す。結果にはレベル0/1/2、全体・第1・第2段階の進捗、消費、各矢のダメージ・初速・拡散・塗り幅、発射後硬直、冷却弾フラグが含まれる。**端点間の区分線形補間はCG2近似**であり、原作の補間曲線や丸めの解析結果ではない。

| 操作・状態 | 完成実装 |
|---|---|
| 左クリック保持 | 残り硬直が終わった後にチャージ開始。フル到達後も保持でき、自動発射しない |
| ボタンを離す | 1回の斉射。9F実時間より前の短押しは最短待機まで予約する |
| 地上／空中 | 地上は通常進行、空中は進行量が1/3。発射時に地上なら横、空中なら縦の配置を選ぶ |
| Shift・イカ形態・装備変更 | チャージと予約射撃を中断。押し続けていても再開せず、トリガーを一度離す必要がある |
| インク消費 | 発射が成立したとき、斉射全体の消費を1回だけ引く。チャージ中・中断時は消費しない |
| インク不足 | 解放時の段階に必要な量がなければ不発。矢・射撃数・負のタンクは発生せず、そのチャージを終了する。支払える下位段階へ自動で下げる処理は採用していない |
| 次の行動 | 再チャージまでの12/15/15F、射撃後の変身待ち10F、インク回復停止20Fを別々に管理する |

9F未満の空中短押しも待機は9F実時間、チャージの蓄積だけが1/3になる。空中短押しを原作内部で9F実時間と27F相当のどちらで処理するかは一次資料から確定できていない。消費を発射時にまとめること、中断時の非消費、低インクでの不発もCG2で採用した処理であり、原作の消費・払い戻しのタイミングを証明したものではない。

### 矢、着弾、遅延爆発

既定は3矢。編集基盤では同じストリンガークラスの1～3矢にも対応する。発射時に向きとチャージ結果を解決し、`ProjectileKind::StringerArrow`、初速・当たり半径・飛行設定・塗り設定を値で保存する。`Projectile::tuning` は既存の投射体・塗り処理と共有する設定のコピーで、直撃威力、爆発の有無・待機秒数・威力・半径などは矢の専用フィールドに保持する。矢ごとに `WeaponId` を検索して性能を引き直す仕組みではない。

装備や編集値を変えても、飛行中の弾、落下飛沫、刺さった矢の物理・ダメージ・塗りは発射時の設定を使う。装備変更は床の塗り、タンク、位置、既存投射体を消さず、前の射撃の残りクールダウンも維持する。シューターの36→18の経時ダメージ減衰は、ストリンガー直撃へ適用しない。

| 項目 | 完成実装と近似 |
|---|---|
| 配置 | 照準方向を中心に既定で−8/0/+8°、1周も同じ。フルで0°へ収束する。発射位置は隣接0.2 world。2矢では角度・位置の係数が−0.5/+0.5、1矢では0 |
| 発射位置の遮蔽 | 銃口から各矢の横／縦オフセットまで地形を照会し、壁の向こうから生成しない |
| 弾道 | 4Fの直進、1Fの減速、その後の自由落下。抵抗は1Fの速度損失から `pow(1-resistance, dt*60)` に換算。ネイティブの遷移条件・丸め・区間をまたぐ時間の厳密な積分は未再現 |
| 速度閾値 | rawの `BrakeToFreeVelocityXZ/Y` は保持するが、現行ストリンガーの遷移判定は時間を使う。この2値で現在の弾道が変わるとは説明しない |
| 直撃 | 地形への掃引と的への球掃引を比較し、手前のものへ当たる。的に吸収された矢は直撃のみで消え、地形用の冷却矢や爆発を追加しない |
| 地形着弾 | 通常の着弾塗りを先に行う。1周以上の冷却矢は `EmbeddedArrow` に位置、法線、入射方向、面ID、残り時間、爆発・塗り設定を保存する |
| 爆発位置 | `arrow.position + Vec3{0, arrow.offset, 0}`。既定の0.3は**ワールドY方向**。法線は遮蔽照会の開始位置の微小補正と、面に沿う塗り・演出に使う |
| 爆風 | 0.75秒後、既定30ダメージ。的の半径を加えた距離判定と、地形の遮蔽照会を行う。厳密な原作の爆風サンプル点や減衰曲線を再現した処理ではない |
| 爆発塗り | 爆発中心を刺さった有限平面に投影し、同じCPU/GPU共通楕円ブラシで塗る。塗れない面は塗らず、隣接する複数面への爆風塗り伝播は未実装 |
| 飛沫 | ストリンガー側の公開数値を独立した生成器へ渡す。位相、揺らぎ、個数の解決、ブラシ中心部と縁の形は共有プランナーによるCG2近似 |
| 寿命・上限 | 矢の既定寿命3秒と距離60 world等で回収。冷却矢は最大128個、未排出の着弾イベントは256個。これらはCG2の処理上限で、原作の有効射程・個数上限ではない |

### レティクルと見た目

[InkReticleRenderer](../project/game/ink/InkReticleRenderer.h) は中央の固定円・点、外側の4本の斜線、ストリンガーの方向マーカー、2段階のチャージ弧を描く。幅はカメラのFOV・描画高さと拡散角の半角から `H/2 × tan(spread) / tan(FOVy/2)` で求める。地上は横、空中は縦へ回す。1矢なら側点なし、2矢なら半幅、3矢なら全幅に側点を置き、中央の円と点は照準の中心として固定する。

この表示は**角度の投影**である。弾の重力・抵抗を積分した着地点予測、矢の発射位置間隔の視差、各矢と地形の衝突予測は含まない。原作動画の画角・切り抜き条件も不明なため、原作レティクルの全距離でのピクセル一致を主張しない。詳細は [レティクル調査](ink_phase4_reticle_research.md) を参照。

武器の見た目は再利用した箱5個で握り、左右の腕、2本の弦を作る。チャージで弦を引き、空中で縦へ回し、イカ形態では隠す。飛行矢と冷却矢は細長い液体粒子、冷却矢は残り時間に応じた脈動と小さなリングで描く。遅延爆発は大きい中心の塊、面法線に沿うリング、10～15個の飛沫で通常着弾と区別する。描画用の乱数は射撃・塗り判定の乱数を進めず、原作モデルや発光素材を抽出したものではない。

## 検証の範囲

- [チャージの純粋関数テスト](../project/tools/ink_stringer_pattern_tests.cpp): 最小／1周／フルの端点と直前、ダメージと消費、拡散収束、1001点の単調性、NaN・無限・負数、時間順序不整合、値コピーの不変性を確認してPASS。
- [ストリンガーCPU統合テスト](../project/tools/ink_stringer_simulation_tests.cpp): 13グループPASS。短押し9F、60/120Hzのチャージ、空中倍率と縦配置、キャンセル、回復・変身待機、35×3の直撃、低インク不発、45Fの地形爆発、壁の遮蔽、装備変更後の飛行・塗り不変、リソース回収を確認。詳細は [統合テスト記録](ink_phase4_stringer_test_notes.md)。
- [レティクル数学テスト](../project/tools/ink_reticle_tests.cpp): 拡散角の投影・逆投影、FOVと解像度の比、2段階の弧、異常入力を確認してPASS。
- これらは原作との入力同期比較を行ったテストではない。D3D12診断、日本語UI、実画面の描画と操作の結果は親側の最終実装レポートにまとめる。数値端点の一致と、未確定の入力・弾道・塗り処理の完全一致を混同しない。
