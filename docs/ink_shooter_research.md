# シューター・塗り・遊泳プロトタイプ調査

調査日: 2026-09-12（日本時間）。対象は **Splatoon 3 の対戦用スプラシューター、ギア効果なし・特殊強化なし**。Salmon Run、ヒーローモード、Splatoon 1/2 の値は基準に混ぜない。

## 版と出典の扱い

任天堂の日本語サポートで確認できた最新バージョンは **Ver. 11.3.0、2026-08-20 配信**。Leanny の公開データにも `1130` があり、今回の数値抽出はこのフォルダーを使用した。したがって「2026年9月現在に近い値」は推測ではなく、調査時点で公式情報と照合した版を指す。[任天堂 更新データ](https://support.nintendo.com/jp/switch/software_support/av5ja/index.html) / [任天堂 英語版の履歴](https://www.nintendo.com/en-gb/Support/Nintendo-Switch/Game-Updates/How-to-Update-Splatoon-3-2266003.html) / [Leanny バージョン一覧](https://raw.githubusercontent.com/Leanny/splat3/main/versions.json)

優先順位は、配信版・変更内容は任天堂、明示的な数値は Leanny の公開解析データ、共通初期値・数値の意味は解析者による説明、補足確認は Inkipedia の順とする。任天堂は武器の全内部パラメータを仕様書として公開していない。公開 JSON にないキーを「0」と解釈しない。省略されたキーは共通初期値を継承する場合がある。

数値の確度を以下で区別する。

- **A: 公開データで直接確認** — Leanny の `1130` JSON に実値がある。
- **B: 共通値・挙動説明** — 解析者のパラメータ説明、Inkipedia で確認。Nintendo の内部実装そのものを取得・実行して検証したものではない。
- **C: CG2 用の近似・調整** — ステージ寸法、独自描画、操作のつながりのために選ぶ値・アルゴリズム。原作の確定仕様とは呼ばない。

Leanny の対象武器ファイルの最終変更コミットは `7280ff9cde8bb1c5dcef46c700c326471584d2e6`（2026-08-20）。将来 `main` が変わっても確認できるよう、固定リンクも残す。[固定版の武器パラメータ](https://github.com/Leanny/splat3/blob/7280ff9cde8bb1c5dcef46c700c326471584d2e6/data/parameter/1130/weapon/WeaponShooterNormal.game__GameParameterTable.json)

## 単位と CG2 への変換

資料の `frame` は **60 Hz のゲームフレーム**として読む。添付動画の録画速度 30 fps とは別である。時間は `frame / 60`、速度は `元の速度 × 60 × S`、加速度は `元の加速度 × 60² × S` で秒ベースへ変換する。`S` は原作の生データ距離 1 に対する CG2 ワールド距離。

今回の CG2 実装は **S = 0.5** を初期基準とする。これは原作に実在する「メートル換算」ではなく、仮キャラクターとテストステージに合わせた縮尺である。移動速度、弾速、射程、ブラシ寸法、衝突半径を同じ縮尺で扱う。後から見やすさのために変えた項目は個別の調整値として記録する。

Splatoon 1/2 の説明に出る `22 units/frame` や `0.72 units/frame` を、Splatoon 3 生データの `2.266` や `0.072` と直接比較しない。旧作の距離表記は 10 倍スケールの資料がある。現行 Inkipedia の Splatoon 3 節は生データと同じ桁を使用している。[Inkipedia Splattershot](https://splatoonwiki.org/wiki/Splattershot)

## 実装前に採用した主な基準

連射は **6 f = 0.1 秒 = 10 発/秒**、1 発のインク消費 **0.92%**、回復停止 **20 f = 約0.333秒**。通常歩行と遊泳は生データ速度の比 **1 : 2**、射撃歩行は通常歩行の **75%**。地上拡散 **4.86°**、ジャンプ直後 **11.66°**。弾道途中の飛沫と最終着弾を別に生成し、インク残量と自色判定を実際の移動へ結び付ける。[武器 JSON](https://raw.githubusercontent.com/Leanny/splat3/main/data/parameter/1130/weapon/WeaponShooterNormal.game__GameParameterTable.json) / [移動・回復 JSON](https://raw.githubusercontent.com/Leanny/splat3/main/data/parameter/1130/misc/params.json) / [共通パラメータの説明](https://splatoonwiki.org/wiki/Template:Shooter_data_S3)

### 射撃・インク・ダメージ

| 項目 | Splatoon 3 の基準値 | CG2 用換算 / 解釈 | 確度 |
| --- | --- | --- | --- |
| RepeatFrame | 6 f | 0.100000 秒 | B、共通初期値 |
| FireRate | 上記から計算 | 10 発/秒、600 発/分 | 計算 |
| InkConsume | 0.0092 / タンク | 0～100 表示なら 0.92 / 発 | A |
| InkRecoverStop | 20 f | 0.333333 秒 | B、共通初期値 |
| 人型からの初弾 | 3 f | 0.05 秒 | B、観測される動作全体 |
| 遊泳からの初弾 | 12 f | 0.20 秒 | B、変身を含む動作全体 |
| PostShotDelay | 4 f | 0.066667 秒、潜伏などへの移行制限 | B、共通初期値 |
| SquidShotShorteningFrame | 1 f | 生データの短縮項目。上の12 fから重ねて引かない | A |
| ShotGuideFrame | 8 f | 照準位置の基準となる弾の予測時刻 | A、用途はB |
| BaseDamage | 360（内部値） | 通常表示の36 | A |
| MinimumDamage | 180（内部値） | 通常表示の18 | A |
| DamageFalloff | 開始8 f、終了40 f | 0.133333～0.666667秒に36→18 | A |
| ダメージ減衰率 | 上記から計算 | 0.5625 / f、33.75 / 秒 | 計算 |

100% から補給せず射撃できる回数は `floor(100 / 0.92) = 108` 発。連射状態は約10.8秒相当だが、初弾遅延、最後の弾、入力の判定順により表示上の所要時間は変わる。最初の発射を0秒とした「108発目の時刻」は107区間の10.7秒になる。

`PreDelayFrame_HumanShot` / `PreDelayFrame_SquidShot` の共有クラスの数値と、実際に弾が出るまでの3 f / 12 f は同一ではない。アニメーション・変身処理を含む総時間をプロトタイプの `InitialShotDelay` として持つ。キー名だけを見て初弾0 fにしない。[解析者による共通値の説明](https://splatoonwiki.org/wiki/User:XarrotD/paramtable) / [Inkipedia Splattershot](https://splatoonwiki.org/wiki/Splattershot)

### 弾速・衝突・有効射程

| 項目 | 生データ / 公開説明 | S=0.5 の換算 | 確度 |
| --- | --- | --- | --- |
| ProjectileSpeed / SpawnSpeed | 2.266 / f | 67.98 ワールド単位/秒 | A |
| 直進状態 | 4 f | 0.066667秒、直進距離4.532 | A・計算 |
| 減速状態開始時の速度上限 | 1.493 / f | 44.79 ワールド単位/秒 | A |
| BrakeAirResist | 0.36 / f | 1 fごとに速度へ0.64倍を適用 | B |
| BrakeGravity | 0.07 / f² | 126 ワールド単位/秒² | B |
| FreeAirResist | 0.02 / f | 1 fごとに速度へ0.98倍を適用 | B |
| FreeGravity | 0.016 / f² | 28.8 ワールド単位/秒² | A |
| Brake→Free XZ閾値 | 0.2355 / f | 7.065 ワールド単位/秒 | B |
| Brake→Free Y閾値 | -0.15 / f | -4.5 ワールド単位/秒 | B |
| PlayerHitRadius | 初期/最終とも0.285 | 0.1425 | A |
| StageHitRadius | 初期/最終とも0.2 | 0.1 | A |
| EffectiveRange | 約11.56、射角依存 | 約5.78 | B、測定・解釈値 |

弾は **直進 → 強い減速 → 弱い抵抗を受ける落下** の状態を持つ。これを単一の等速直線や一定重力だけで置き換えると、先端の塗りと壁へ届く距離が変わる。減速はフレームの係数なので、可変刻みなら `pow(1 - resist, dt * 60)` のように時間へ対応させる。原作の厳密な更新順・閾値の AND/OR 条件を確認できたわけではないため、プロトタイプの積分器は独自実装とする。[武器 JSON](https://raw.githubusercontent.com/Leanny/splat3/main/data/parameter/1130/weapon/WeaponShooterNormal.game__GameParameterTable.json) / [Shooter データ説明](https://splatoonwiki.org/wiki/Template:Shooter_data_S3)

**有効射程は塗りの最大到達距離ではない。** プレイヤーを狙う高さ・角度での実用上の値であり、下に落ちた弾や高所からの弾はその先の床を塗り得る。`EffectiveRange` を弾の強制消滅距離として使わず、弾の寿命上限と別にする。命中判定は高速弾が薄い壁を抜けないよう、前位置→次位置の掃引を使う。ダメージを使う敵がまだない場合でもパラメータを保持し、「ダメージ戦闘まで再現済み」とはしない。

### 精度

| 項目 | 基準 | 秒換算・実装方針 | 確度 |
| --- | --- | --- | --- |
| GroundSpread | 4.86° | 地上の最大拡散角 | A |
| JumpSpread | 11.66° | ジャンプ直後の最大拡散角 | A |
| Stand_DegBiasMin | 0.01 | 外側へ飛ぶ確率の初期値1% | A、確率の解釈B |
| Stand_DegBiasKf | 0.01 | 連射1発ごとに+1ポイント | A、解釈B |
| Stand_DegBiasMax | 0.25 | 地上連射の上限25% | B、共通初期値 |
| Stand_DegBiasDecrease | 0.015 / f | 回復中は毎秒0.9低下 | A |
| 精度回復開始 | 最後の射撃後6 f | 0.1秒後 | B |
| Jump_DegBiasMax | 0.4 | ジャンプ直後40% | A |
| ジャンプ精度回復開始/終了 | 25 f / 70 f | 0.416667秒 / 1.166667秒 | A |

原作資料が示すのは、中央寄り・外側寄りの確率を別に持つ仕組み。完全な一様円錐乱数を毎回使う方式では初弾の良さが消える。CG2 では中央寄りの分布と外側の分布を混合し、連射履歴とジャンプ経過時間で混合比率・円錐角を変える。**分布の厳密な形、乱数生成器、初弾時の端点処理は独自近似**。射撃を止めたときの精度回復と、ジャンプからの時間回復は独立に管理する。[武器 JSON](https://raw.githubusercontent.com/Leanny/splat3/main/data/parameter/1130/weapon/WeaponShooterNormal.game__GameParameterTable.json) / [Inkipedia Splattershot](https://splatoonwiki.org/wiki/Splattershot)

### 飛行中の飛沫・着弾塗り

| 項目 | 生データ | S=0.5 の寸法 | 確度 |
| --- | --- | --- | --- |
| PaintDropletCount / SpawnNum | 1.5 | 1発あたりの平均的な頻度として扱う | A、解釈B |
| PaintDropletSpacing | 9.2 | 4.6 | A |
| 足元飛沫位置 | 1.2 | 前方0.6 | A |
| SplitNum | 8 | 8パターンの配置基準 | A、用途B |
| ForceSpawnNearestAddNumArray | [4] | 足元が塗れない射撃が続く場合の救済 | A、用途B |
| 通常飛沫 WidthHalf | 1.472 | 0.736 | A |
| 足元飛沫 WidthHalfNearest | 2.0608 | 1.0304 | A |
| 飛沫の低/高落下高さ境界 | 3.0 / 10.0 | 1.5 / 5.0 | A |
| 着弾 WidthHalfNear/Middle | 1.93 / 1.93 | 0.965 / 0.965 | A |
| 着弾 WidthHalfFar | 1.71 | 0.855 | A |
| 着弾 DepthScaleMin/Max | 1.31 / 2.24 | 無次元 | A |
| 落下着弾 DepthScaleMin/MaxBreakFree | 1.12 / 2.24 | 無次元 | A |
| 壁から落ちる塗り 半径 Fall/Ground/Shock | 0.65 / 0.6 / 1.56 | 0.325 / 0.3 / 0.78 | A |

`WidthHalf` はブラシの幅方向半寸法であり、どの角度でも同じ円を描く半径と同一視しない。原作は着弾角度や落下高さで前後方向の伸びが変わる。原作の描画用テクスチャ・Nintendo のコード・モデル・音声はコピーせず、CG2 側で楕円・複数の膨らみ・離れた小粒を持つ独自ブラシを生成する。[武器 JSON](https://raw.githubusercontent.com/Leanny/splat3/main/data/parameter/1130/weapon/WeaponShooterNormal.game__GameParameterTable.json)

`SpawnNum = 1.5` を整数の「必ず1発につき2個」に丸めたり、`SpawnBetweenLength` を「射撃開始位置から最初の飛沫までの距離」と決めつけたりしない。8個の位相・パターンと小さなばらつきを使って、連射の累積で道がつながるように近似する。足元の救済飛沫も別に持つ。主弾から分岐した飛沫は、それぞれ最初に当たる塗装可能面へ塗る。壁の裏や床下へ無条件投影しない。

### 現在の CG2 パラメータとの差分

`project/game/ink/ShooterWeaponParams.h` と `InkSimulation.cpp` の初期実装を照合した。射撃間隔・インク・基準速度・拡散角・回復時間は上表から換算した値を使用する。一方、飛沫・姿勢のつながりは、まず道をつなげて遊べることを優先するため次の独自値を使う。ここを原作の値として読まない。

| CG2 の項目 | 現在の値 | 原作データとの関係 |
| --- | --- | --- |
| 距離縮尺 | 0.5 ワールド単位/生データ単位 | ステージに合わせた独自縮尺 |
| 飛沫の生成上限 | 主弾1つにつき最大8個 | 原作の `SplitNum=8` と同じ意味ではない。生成数の上限 |
| 飛沫間隔 | 1.15、0.86～1.14倍のばらつき | 原作の換算4.6とは異なる。短い経路でも道をつなぐための調整 |
| 元の飛沫数・間隔 | 1.5 / 4.6を別フィールドで保持 | 生成計算へ直接使用していない参考値 |
| 最初の飛沫 | 銃口から進んだ距離0.6 | 原作の足元救済条件全体を再現したものではない |
| 飛沫の横ばらつき | X/Z各方向±0.11 | 独自値 |
| 飛沫の初速・重力 | 下向き1.3 / 下向き32 | 独自値、秒ベース |
| 飛沫の掃引半径・寿命 | 0.035 / 2.5秒 | 独自値 |
| 主弾の寿命上限 | 2秒 | 負荷・画面外の弾の回収用 |
| ブラシの輪郭 | 中央楕円＋飛沫3個/主着弾6個の膨らみ | 独自乱数形状。原作テクスチャ未使用 |
| 着弾の遠距離半径への補間 | 有効射程5.78までの飛距離を基準 | 原作の距離・着弾角・高さ依存の完全な式とは異なる |
| 人型加速度 | 28 | 独自値、ワールド単位/秒² |
| プレイヤー重力・ジャンプ速度 | 20 / 7 | 独自値、秒ベース |
| 壁泳ぎ速度 | 4.32 | 独自値、ワールド単位/秒 |
| 人型/遊泳の移動衝突半径 | 0.28 / 0.16 | 独自値、対人被弾判定とは別 |
| 段差高さ | 0.30 | 独自値 |
| 姿勢の見た目の補間 | 0.12秒 | 状態条件とは別の表示用ブレンド |

原作の `SplitNum` に相当する8組の配置テーブルは現在の実装にはない。現時点では距離ごとの独自飛沫生成と独自ブラシで近似する。`baseDamage`、`playerHitRadius` と減衰時刻は将来の標的用に保持しているが、ダメージを受ける敵はない。

## 人型・遊泳・壁

Leanny の `params.json` は能力カーブの `[High, Mid, Low]`。能力ポイント0では `Low` が適用されることを同じ作者の `ability.html` で確認した。最初の要素を無装備の値と誤認しない。[1130 共通値](https://raw.githubusercontent.com/Leanny/splat3/main/data/parameter/1130/misc/params.json) / [配列の読み方](https://github.com/Leanny/splat3/blob/main/ability.html)

| 項目 | 原作の基準 | S=0.5 の換算 / 採用方針 |
| --- | --- | --- |
| 通常人型移動 | 0.096 / f | 2.88 ワールド単位/秒 |
| スプラシューター射撃移動 | 0.072 / f | 2.16 ワールド単位/秒 |
| 自インクの遊泳 | 0.192 / f | 5.76 ワールド単位/秒 |
| 人型のタンク回復 | 600 fで空→満タン | 10秒、10%/秒 |
| 自インク潜伏中の回復 | 180 fで空→満タン | 3秒、約33.333%/秒 |

上表の回復時間には武器による回復停止時間を足していない。最後の射撃から20 fが過ぎ、回復可能な状態になってから進める。潜伏中の高速回復は **自色のインクへ接していること**を条件にし、キーを押すだけで空中・未塗装面から高速補給できる実装にしない。

任天堂は、自インクを泳いで高速移動できること、塗られた壁を登れること、潜ってタンクを補給できることを説明している。壁塗りは単なる見た目ではなく移動経路を増やす仕組みである。[任天堂 Splatoon 3 製品紹介](https://www.nintendo.com/us/store/products/splatoon-3-switch/) / [任天堂 移動の基本](https://splatoon.nintendo.com/en/news/up-your-game-in-splatoon-3-with-these-quick-tips/)

CG2 の状態は人型・床遊泳・壁遊泳を分け、床と壁のそれぞれの面で自色を照会する。未塗装へ出た場合の人型復帰は今回の依頼に沿うプロトタイプ仕様。原作に存在するインク外の低速なイカ移動まで同一にしたとは扱わない。変身中の当たり判定高さ、床からの吸着距離、壁への到達距離、壁上端の乗り越え、移動加減速、ジャンプ速度・重力は、原作の全数値を確定できなかったため **C の調整項目**とする。

壁面の法線に対して速度を接線方向へ射影し、壁の自インクを失えば吸着を解除する。W/Sを上下、A/Dを左右として操作できる状態をまず作る。壁の上端を超えたときは、天面の有無と接地位置を確認して乗り上げる。天井・凹凸のある任意メッシュへの移行まで同時に一般化しない。イカロール・イカノボリ（Squid Roll/Surge）は原作にあるが、この初期実装の通常壁泳ぎとは別の機能。[任天堂 Splatoon 3 Direct の紹介](https://splatoon.nintendo.com/en/news/catch-up-on-all-the-latest-from-the-splatoon-3-direct/)

遊泳モデルが小さいことと対人被弾判定が小さいことも分ける。任天堂の説明では、旧来のイカ状態の被弾判定は見た目より広く、Ver.11.0.0で人型に近いサイズへ縮小した。今回の「遊泳時に衝突形状を縮小」はテストステージ移動用の独自カプセル調整であり、原作の対人当たり判定の完全再現を意味しない。[任天堂 イカ研究所極秘レポート](https://www.nintendo.com/jp/switch/av5ja/report/index.html)

## 塗装可能面とゲームロジック

各面に `inkable` を明示して、塗れる床・坂・壁と塗れない面を区別する。インクの見た目・自色照会・潜伏可否のすべてに同じ属性を適用する。原作ではガラスなどの非塗装面が存在するが、その素材分類をテストステージへそのままコピーする必要はない。今回は任意素材名ではなく明示的フラグで区別する。[調査した Inkipedia Surface](https://splatoonwiki.org/wiki/Surface) / [Ink](https://splatoonwiki.org/wiki/Ink)

CG2 で必要なのは「描ける」だけでなく、「そこを泳げる」の一致。推奨構成は、各面のローカルUVへブラシを変換して永続的なGPUマスクへ描き、CPUには移動判定に必要な小さな所有者グリッドを保持する方式。描画ごとのGPU→CPU読戻しは不要。GPU描画とCPU判定へ同じスタンプ中心・半径・面ID・チームIDを渡し、境界の誤差はグリッド粒度の範囲へ抑える。これは CG2 用設計であり、任天堂の塗り実装を調査して複製したものではない。

## 添付動画の視覚的な観察

参照: `ローカル参考資料（非同梱）`。メタデータは約16.33秒、1920×1080、30 fps。プロジェクト側で抽出したコンタクトシートを視覚確認した結果を、以下の形状・見せ方の参考として使用した。**録画フレームから発射間隔・弾速・回復時間を計測していない。**

- 主弾だけでなく、その下に落ちる飛沫が射線の下の床を塗る。
- 塗り跡は進行方向へ伸び、輪郭に複数の膨らみがある。周囲に離れた小粒が散る。
- 連射によって個々の塗りが重なり、前進に使える広く連続した道になる。
- 壁に当たると床と異なる向きの塗りが残る。着弾面に沿って形を配置する必要がある。
- カメラの中心からの照準と、手元の銃口から出る弾には視差がある。照準レイで求めた点へ銃口方向を補正する設計が適する。

テンポや追従の印象は調整材料とし、動画だけでは見えない当たり判定・状態遷移・GPU方式を断定しない。

## PC ジャイロの比較

調査時点の一次ドキュメントを確認した。2026年の **GameInput v2 はセンサー取得を持つ**ため、「GameInputではジャイロ不可」という古い説明を採用しない。

| 選択肢 | 取得方法と単位 | CG2 への追加負担 / 判断 |
| --- | --- | --- |
| Microsoft GameInput v2 | `IGameInputReading::GetSensorsState`、`GameInputSensorsState` の `angularVelocityInRadPerSecX/Y/Z` | Windowsとの親和性あり。v2のSDK・ランタイムと実機の対応確認が必要。旧v0/v1とは区別する |
| SDL3 | `SDL_GamepadHasSensor` → `SDL_SetGamepadSensorEnabled` → `SDL_GetGamepadSensorData`、`SDL_SENSOR_GYRO`、rad/s | 入力用ライブラリとして追加可能。Steam非依存で試せる候補。ゲームパッド接続・イベント更新も必要 |
| Steam Input | `ISteamInput::GetMotionData` | SteamクライアントとSteamworks統合が前提。Steam配布・ユーザー設定を重視する段階で有力 |
| Direct HID | HIDレポートから機種ごとに解読 | 単位・較正・接続方式を自前管理する負担が大きい。初期プロトタイプでは優先しない |

Direct HID はWindowsのHIDパーサーで値を取り出せるが、どの報告値を角速度へ対応させるかは別途必要になる。このため上表の追加負担はAPIの役割に基づく実装上の判断である。[Microsoft HIDレポートの解釈](https://learn.microsoft.com/en-us/windows-hardware/drivers/hid/interpreting-hid-reports)

GameInput は利用可能な入力種別を調べ、センサー状態の取得結果を確認する。未対応なら有効な角速度として扱わない。[Microsoft GetSensorsState](https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/interfaces/igameinputreading/methods/igameinputreading_getsensorsstate) / [Microsoft GameInputSensorsState](https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/input/gameinput/structs/gameinputsensorsstate)

SDLのセンサーは初期状態で無効。存在確認と明示的な有効化が必要で、取得APIはSDL 3.2.0以降。pitch/yaw/rollの角速度はrad/sなので、秒単位の経過時間を掛けてカメラ角へ加える。SDLの軸とCG2カメラの軸の符号を確認する。[SDL存在確認](https://wiki.libsdl.org/SDL3/SDL_GamepadHasSensor) / [SDL有効化](https://wiki.libsdl.org/SDL3/SDL_SetGamepadSensorEnabled) / [SDL取得](https://wiki.libsdl.org/SDL3/SDL_GetGamepadSensorData) / [SDL単位と軸](https://wiki.libsdl.org/SDL3/SDL_SensorType)

Steam Inputの値をSDLのrad/sとしてそのまま流し込まない。公式の `InputMotionData_t` は角速度のスケールを別に説明しており、アダプター側で揃える。姿勢クォータニオンにはヨーのドリフトがあり、機器を自然に持つ向きとハードウェア座標も同一とは限らない。[Valve ISteamInput / GetMotionData / InputMotionData_t](https://partner.steamgames.com/doc/api/ISteamInput)

本体の塗り・移動ループを優先し、ジャイロはバックエンドが準備できるまで無効にできる独立した入力経路とする。推奨する設定は `GyroEnabled`、感度、Pitch/Yaw反転、デッドゾーン、平滑化、静止時バイアス較正、視点リセンター。較正は静止している一定時間の角速度平均を引き、平滑化は時間に応じた係数を使う。マウス照準は常に成立させる。実機で角速度が取得できるまで「ジャイロ動作確認済み」とは報告しない。

## 今回は確定していない部分

- 原作と同じ乱数生成器、拡散確率分布の完全な式、飛沫8パターンの正確な配置。
- 内部的なフレーム更新順序、すべての弾の継承初期値、壁飛沫の詳細なアニメーション。
- 壁泳ぎ速度・加速度、変身アニメーションの全区間、坂や角の細かな地形補正。
- 原作のカメラ・照準補正の完全な式、Nintendoが実際に使用するGPU塗り方式。
- ギア、イカフロー、サブ/スペシャル、対人戦、イカロール/イカノボリ、ネットワーク同期。

これらは基本ループを成立させた後の改善対象。数値と見た目の近似を明記し、公開資料がある部分から段階的に精度を上げる。
