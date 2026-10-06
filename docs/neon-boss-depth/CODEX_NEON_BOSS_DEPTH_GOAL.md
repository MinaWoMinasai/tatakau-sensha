# Codex実装仕様：Neon Boss Depth Encounter

対象：MinaWoMinasai/tatakau-sensha
目的：見下ろし2Dの遊びを維持し、立体的なボス戦を本編へ実装する。
成果物：遊べるボス戦、回帰検証、実機画像・動画、GitHub Pages制作担当への引継ぎ。

これは実装依頼です。提案だけで終了せず、調査・設計・実装・調整・検証・素材出力まで進めてください。全体を一つのGoalとして扱い、内部では検証可能な段階に分けます。通常の設計判断ごとにユーザーへ確認する必要はありません。以下の範囲で、既存実装を根拠に判断してください。

使用量の消費ではなく、完成条件を満たす成果物を目的にします。完了後の無意味な反復と無関係な機能追加は禁止です。実行できない項目は未完了と明記し、未検証を成功扱いしないでください。

---

## 0. 実現したい体験と採用方針

現在の「2Dボスの中心に3Dモデルを重ねた表示」から、次の体験へ進めます。

> プレイヤーはこれまでどおり床の上で戦車を操作する。その上方・奥側に大型のネオン投影体が浮かび、立体的に身を引き、構え、奥から弾を送り込み、降下して床を叩き、空間から床へビームを走らせる。避ける場所と攻撃する場所は明確で、3Dの迫力と2Dアクションの読みやすさが同居する。

特定の既存ゲームのキャラクター、モデル、音、演出素材、攻撃配置を複製しません。「平面の戦闘空間と、その外側から侵入する立体ボス」という構成上の着想を使います。

### 基本案：大型の浮遊ネオン投影体＋床面の投影コア

- 3D人型を板のように床へ寝かせず、床上に起き上がった浮遊体として表現する。
- 床面のコアを、実際の2D位置と攻撃可能範囲の視覚的基準にする。
- コアと本体を短い光柱・投影線でつなぎ、別の敵に見せない。
- HPは一つ。コアへの攻撃を既存ボスHPへつなぎ、別HPや重複ダメージを作らない。
- 遠距離も近接も、通常操作でコアにダメージを与えられる。
- コアは見えない当たり判定の言い訳ではなく、輪郭・被弾反応・簡潔な案内を持つ明確な攻撃対象にする。
- 本体が奥へ引くときも、コアを場外・到達不能な場所へ置かない。
- 適切に位置取りも変える。常時背景に固定された飾りにしない。
- 高さ・後退・捻りは実3Dワールド変換で示す。画面上の拡大縮小とscreen offsetだけで済ませない。

現行の部屋・投影方式と重大な衝突がある場合は、「床のアンカーと本体を分離した浮遊ボス」を守って最小限の配置変更で解決してください。全ゲームの3D化や横スクロール化はしません。

### 許可するゲームプレイ変更

対象Neonボスについてのみ、専用攻撃3種、予告、硬直、位置取り、段階移行、コアの攻撃可能状態を追加・調整できます。

「2D Gameplayを正とする」は「既存ボスAIを一切変更できない」という意味ではありません。新攻撃の位置・時間・命中・ダメージも2Dゲームプレイ側で決定し、3D表示がそれを読む設計にします。

通常敵・他ボス・プレイヤー能力値・強化・通貨・遠征経済を巻き込む変更は範囲外です。

## 1. 作業の安全性

1. 作業ディレクトリとgit rootを確認する。別のPages用リポジトリへ実装しない。
2. 適用されるAGENTS.mdと既存の実装・コメント基準を読む。
3. branch、HEAD、staged/unstaged/untracked、ツールセット、ビルド構成を記録する。
4. 開始時の変更を保持する。この仕様書自体がuntrackedでも異常としない。
5. branch・HEADを変えない。commit、push、merge、checkout/switch、reset、clean、stashを行わない。
6. 上書き前に内容と差分を確認し、今回の変更と元からある変更を区別する。
7. 他作業のsource変更を検知したら、その領域の編集と検証を止め、衝突を記録する。勝手に取り込んだり巻き戻したりしない。
8. 権限・sandbox・承認設定を維持する。許可回避のために弱めない。
9. 有料API、新クラウドサービス、外部素材・モデル・モーション取得、ソフトウェアの無断導入を行わない。

参照確認時のmasterは`8b9282e5241563bf5bb26b5922aebfe6917bce9d`。これは指定checkout先ではありません。作業時のローカルコードが更新されていれば読み直してください。

真に進められない変更衝突・権限不足・必須環境欠落・素材条件はBLOCKEDとして記録します。好みや調整値を質問しないと決められないだけで止まらず、安全な初期案を採用してください。

## 2. 前回の成果の確認・再利用

少なくとも以下を読み、実装・呼出元・所有権を確認します。パスが変わっていれば実在する対応先を探してください。

- `docs/neon-boss-gameplay-integration.md`
- `docs/repository-engineering-overhaul/README.md`
- 同ディレクトリの`deterministic-scenarios.md`、`final-validation.md`、`visual-validation.md`
- `project/game/enemy/visual/NeonBossVisual.h/.cpp`
- `NeonBossVisualState.h`、`BossVisualBridge.h`
- `project/game/enemy/actor/Enemy.h/.cpp`
- `RivalBossCombat`、`PrototypeBossCombat`
- `project/game/flow/CombatFlowController.h`
- `project/game/scene/GameScene.NeonBoss.cpp`
- `GameScene.GameplayScenario.cpp`、`GameplayScenario`、`GameplayScenarioSession`
- `NeonSkinnedRenderer`、`NeonCharacterStyle`、`NeonDissolve`
- `NeonPreviewAnimations`の実装、Skeleton/Animation/SkinCluster
- Camera、mouse unprojection、照準、wall collision、homing、Drone、Melee
- NeonShowcaseCapture、既存録画・画像保存・Profiler
- 部屋定義、遠征のボス選択、Release packagingとDeveloper除外

参照時に確認された特徴は次のとおり。作業時のコードで再確認してください。

- GameplayはXY平面、Z=0を基準とし、cameraは負Z側から見る。
- `NeonBossVisual::UpdatePlacement`はモデルbounds中心をボス位置に合わせ、モデル高さを半径×2.2にし、0.35radの傾きと平面回転を与えている。
- 行動を値snapshot/bridgeからVisualへ一方向に渡す構造がある。
- AvatarSample_Bには埋込みanimationがなく、共有の生成clipはIdle/Attack中心。GLB由来のモーションと混同しない。
- Developer用13シナリオ、固定seed/dt/input、snapshot、capture、実機wrapperがある。
- 前回の全件63/64 PASSと後続の部分再検証は、最終binaryで全件再実行した記録とは異なる。

万能Scenario Runner、Animation System、Rendererの作り直しではなく、既存機構の拡張を優先してください。

## 3. ベースラインと段階計画

変更前に次を保存してください。

- Development/Releaseのbuild結果。
- 現存する全件検証driverの結果。古い件数を固定せず、現在のテスト一覧を使う。
- 待機・予告・攻撃・移動・死亡の代表画像と短い連続映像。
- seed・入力・解像度・camera・設定付きの代表Gameplay snapshot。
- CPU/GPUの有効な計測値と資源数。未計測は未計測とする。
- 比較binary、関連設定・shader・assetのhash。古いbinaryへ新設定を混ぜない。

生成物は既存ignore対象の`generated/neon-boss-depth/`等に保存し、旧証拠を上書きしません。

`docs/neon-boss-depth/plan.md`に実在する責務境界、変更経路、テスト対応、進捗を記録します。完成条件を削って達成扱いにしないでください。

推奨順序：baseline/座標診断→立体配置/コア/camera→2D attack契約/3攻撃→モーション/登場/死亡→本編/3スタイル→実機調整/全件/性能→最終撮影/Pages引継ぎ。

単一Goalで進めますが、一括変更して最後までビルドしない進め方は避けてください。

## 4. 座標系と本体配置

### 明示的な基底と足元

floor origin、床の2軸、床から離れるnormal、モデルlocal up/forwardを定義し、行列規約・左右手系・掛け順を確認します。

モデルlocal Yが高さでも、world Yへ高さを加えないでください。現在のXY床面に対する高さと、床内の奥行きは別です。カメラ側が負Zであることと意味上のfloor normalを診断表示で確かめます。

bounds中心ではなく足元・ルート付近の安定した点を床面のボス位置へ対応させます。骨格とboundsから基準点を求め、設定で微調整可能にします。動く足先から毎frameアンカーを再算出し、全身をガタつかせないでください。

本体配置は「床アンカー＋floor normal方向の高さ＋意味のある局所offset」。地面の位置、浮遊の高さ、身体の傾きを分けます。

### 向きと大きさ

頭を照準方向へ平面回転させる配置から、体の向き・上体の捻り・攻撃照準を分けます。移動と照準が異なっても破綻させません。

大きさを衝突半径だけで決めず、表示用設定を設けます。ただし大きい本体全体に当たり判定があると誤認させず、床コアとの役割を明示します。

通常戦闘時の本体高さが画面の約20～30%になる案を出発点とし、全身・自機・回避空間を実画像で調整します。固定の正解ではありません。巨大化だけで迫力を作らず、非一様scaleによる引き伸ばしも控えます。

## 5. ボスcameraと2D操作の整合

通常部屋・タイトル・工房は現在のcameraを維持します。対象ボス部屋では浅い斜め見下ろしの安定したprofileを導入できます。

- 真上から15～25度程度傾けた候補を初期比較し、実画像で角度を決める。
- 実際のorthographic/perspectiveを確認し、床・自機・壁が破綻しない方式を使う。
- orthographicなら奥移動だけで小さくなると仮定しない。perspectiveなら投影サイズ・clipping・入力を検証する。
- 別cameraの本体を2D板へ貼る方式を主解決にしない。同じ空間のdepthと接地関係を優先する。
- playerの許可移動範囲・hazard・コア・本体boundsを含むframingを設計する。
- 通常戦闘中は視点を安定させ、大きな演出cameraは導入・撃破に限定する。

必須確認：

1. screen mouseから床へのray intersection/unprojectionを、実際の表示行列と整合させる。
2. 見た目のマウス位置と照準・弾・射程・UI pickingを一致させる。
3. 板ポリが傾斜cameraで消える・薄くなる・壁に埋まる問題を処理する。
4. Drone、Melee範囲、床予告、回収物も同じ床座標で一致させる。
5. 対応済みwindow/aspect/DPIを壊さない。
6. 退出・死亡・再出撃で元cameraへ復帰する。
7. camera shakeも含め、入力と表示の行列関係を説明・検証する。

角度を小さくすることは許可しますが、結局introだけ3Dにして通常戦闘は元の板状表示に戻す解決で完了しないでください。

## 6. 床コア・接地表現・攻撃可能範囲

既存Enemyのposition/HP/damageを正として同じ床座標にコアを置きます。単一targetとして扱い、本体とコアで同一攻撃が二重にdamageを与えないようにします。

- Shooter通常弾・誘導弾、Drone標的選択、Melee判定がコアを認識する。
- 誘導が空中の装飾本体を追って当たらない状態を作らない。
- コアを壁外・非通行領域・ステージ外へ置かない。
- 初回に「床の投影コアを攻撃」など短く案内する。
- 同じdamage eventからコアhit flashと本体の局所反応を示す。
- 無敵状態の有無と理由を見分けられるようにし、長い攻撃不能時間を避ける。
- 浮いた本体の投影へ触れただけで、離れたコアから謎の接触damageを与えない。
- コアの通常接触damageはOFFを初期案とし、攻撃中の明示hazardを使う。既存設計と衝突する場合は安全性を検証し理由を記録する。

近接が常に攻撃不能にならないよう、通常phaseでも到達可能な攻撃機会を確保します。

接地表現は黒い床に黒い影を置くだけでなく、低輝度の接地領域・薄い投影リング・本体への接続線を組み合わせます。高さに応じて濃さ・ぼけ・範囲を滑らかに調整できますが、これは非物理の視覚補助です。物理的に正確な影と称さないでください。

コアの攻撃可能範囲、接地表現、攻撃予告は異なる役割です。同色・同パターンの円で混同させないでください。

## 7. GameplayとPresentationの契約

依存方向：

2D Encounter/Attack state
→ phase・攻撃計画・eventのsnapshot
→ BossVisualBridge / NeonBossVisual / hazard表示 / camera演出
→ renderer

### Gameplay側の所有物

- encounter generation、attack instance ID
- phase、経過時間、duration
- 床上の移動・着地点・目標固定時刻
- 攻撃shape、方向、半径、幅、range、wall clipping結果
- telegraph/active/recoveryの開始・終了
- damage、hit cooldown、vulnerability
- 弾/beam/着地hazardの生成・寿命・取消し
- HP0、player death、結果の優先順位

### Presentation側の所有物

- 高さ・傾き・上体反応・visual root offset
- animation blend、局所発光、接地表現
- 純粋な残像・particle・表示補間
- Gameplayが許可した範囲のcamera演出

守ること：

- animationの再生位置やDraw回数を、damage発生の真実にしない。
- Render OFFでも同じGameplay eventが同順序で発生する。
- 同じsnapshotを複数回読むだけで弾が増えたりclipがrestartしたりしない。
- 予告とhit volumeを別々に手書きせず、同じAttackPlan/shapeを参照する。
- bone socketは発光位置の補助に使えるが、その描画結果を読んで攻撃判定を決定しない。
- 新VFXの乱数は専用局所streamやevent IDから生成し、既存Gameplay乱数の消費順を変えない。
- 既存の全乱数基盤を今回置換しない。

型名は任意。既存bridge等を無視した二重のmanagerを作らないでください。

## 8. 専用攻撃A：奥行き弾幕／Depth Volley

本体が奥へ引き、床へ向かって弾を送り込む攻撃を作ります。

1. 本体が実3D空間で後退・上昇し、上体を引く。
2. 手・胸付近でcharge。床に着弾候補または侵入点を予告。
3. 目標を固定し、最後の回避猶予中に急追尾しない。
4. 複数弾が3Dの弧や斜め軌道で床へ向かう。
5. Gameplay側が持つ床到達時刻に、着弾hazardまたは明示した2D弾へ一度だけ引き継ぐ。
6. 本体が戻り、コアへの反撃時間を作る。

初期案は少数の着弾円。弾数3～5、予告約0.8～1.2秒から調整します。数値は初期調整案であり固定の正解ではありません。

- 空中の見た目だけの弾が、その画面投影位置で無警告の接触damageを出さない。
- 床到達位置・時刻はGameplayが持ち、Visualが軌道を再現する。
- 手元と発射起点が離れて見えないよう調整する。
- 着弾円の予告と実判定を一致させる。
- 壁・床境界への計画は有効位置へ制限し、遮蔽を表示にも反映する。
- 3スタイルの移動で回避余地を検証し、逃げ道のない配置を作らない。

## 9. 専用攻撃B：奥からの降下突撃／Depth Dive Slam

sprite拡大ではなく、本体が奥・上へ引き、そこから床へ侵入して叩きつける大技にします。

1. 本体が屈み・身を引く。コアと接地表現を残す。
2. 本体が高さ・奥行きを使って離れ、目的地点を予告する。
3. 最終予告を固定してから降下する。
4. 着地の瞬間に円形または方向性のあるhazardを発生させる。
5. 着地姿勢、制限付きの局所反動、短い硬直。
6. コアへの反撃機会を明確に作る。

- 飛行中の本体に接触判定を残さない。
- コアを移す場合は床上軌跡もGameplayが決め、到達不能な場所へ飛ばさない。
- 着地だけが危険か、経路も危険かを明示する。初期案は着地だけ。
- 最後の予告固定後、player足元へ瞬間的に追従し直さない。
- ダッシュ無敵・既存の被弾無敵との関係を維持し、無言で貫通させない。
- dt spikeでも着地eventを二重発火せず、予告を飛ばして突然当てない。
- camera zoomで回避場所を画面外へ追い出さない。

## 10. 専用攻撃C：空間から床を薙ぐビーム／Grounded Sweep Beam

上体を捻り、手・胸から床へ接続するビームで、明示した地上範囲を掃射します。

1. 体を横へ捻ってcharge。
2. 床に将来の危険帯と掃射方向を表示。
3. 目標と掃射条件を確定。
4. 本体から床の接続点へ光が伸び、地上危険帯を掃く。
5. beamを消し、上体を戻してrecovery。

- 実判定は床面のcapsule/segment等、既存collisionと整合する小さなshapeで表す。
- 浮いた線の画面投影と実damageが食い違わないよう、床に接続点と危険な長さ・幅を示す。
- 壁で止まる仕様なら、表示・予告・判定が同じclipping結果を使う。
- 予告は危険帯の外周＋弱い塗り。発射前・active・終了を色だけでなく形や動きでも区別する。
- 継続damageは既存無敵時間とtick/cooldownで制御し、描画frameごとに加算しない。
- 安全位置から反撃でき、近接でも常に逃げるだけにならない配置にする。

3攻撃は通常AIで選ばれます。Developerボタンでしか動かない展示機能では未完成です。

## 11. 行動サイクル・難易度・旧ボスの保存

Neon専用encounter profile/policyを設け、旧Rival/Prototypeの非対象経路を保存します。同じactorを使ってもよいですが、旧AIと新AIを二重更新しないでください。

初期サイクルは、3攻撃がrecoveryを挟んで一巡する読みやすい構成。乱数を使う場合も同じ大技の連続上限を設けます。

- Phase 1は動きを学べる構成。
- HP閾値のPhase 2で弾配置や掃射の組合せを少し強める。
- 速度・弾数・予告短縮をすべて同時に強化しない。
- 予告中にHP閾値を越えても、確定済み攻撃を別範囲へ変えない。
- 重複hazardの時間・面積・位置を制限し、回避不能を避ける。
- HP・報酬は既存値から始め、対象ボス内の必要な調整だけ根拠付きで行う。

新旧ボスは意図的に行動が違うため、旧AIとのsnapshot完全一致を新AIへ要求しません。一方、新AIのVisual ON/OFF・演出軽量化ではGameplay state/eventを一致させます。

## 12. 手続きモーションとGPU Skinning

既存生成Idle/Attackを確認し、今回の必要な動作を追加します。外部モーションを勝手に入手しません。

映像で区別できるようにする動き：

- 浮遊待機：小さな呼吸・上下動。
- Charge：胸・腕・肩を引き、攻撃方向へ捻る。
- Volley：押し出しと小さな反動。
- Dive：引き、降下姿勢、着地の屈み、復帰。
- Beam：上体の捻りと戻り。
- Hit：局所反応。毎hitで攻撃を中断しない。
- Defeat：短い崩れ・停止・Dissolveへの移行。

実joint名・bind poseを読み、架空の骨・clipを仮定しないでください。汎用IK/retargeting systemは不要です。

- 生成clipや局所pose合成は既存animation評価の適切な位置で一度だけ適用する。
- 前frameの補正済みposeへ加算し続けてdriftさせない。
- quaternion/角度境界・骨階層の二重変換を検証する。
- 同一phaseでclipを毎frame選択し直さない。
- root motionでGameplay positionを勝手に移動させない。
- 主要骨欠落時はboundedなroot/上体表示にfallbackし、診断を一度記録する。
- Previewの既存Idle/Attack比較は維持し、必要なら本編用profileを分ける。

「専用Walkがないので直立poseを滑らせる」だけで終わらず、浮遊体らしい移動・加速・反動を作ってください。

## 13. 登場・段階移行・撃破

### 登場

約2～3秒を初期案に、床コア起動→投影線→人型形成→構え→戦闘開始。ボス名は既存呼称を尊重し、変える場合は設定値へ分離します。

- 立体であることが分かる斜めの見せ場を作る。
- 操作を止める区間は攻撃・既存弾・接触も安全に止める。
- スキップ可能にし、skip後も同じ初期戦闘状態になる。
- Dissolveを流用する場合、死亡latchを逆再生して復活させず、登場用の生存状態で表示進行だけを使う。
- intro終了/skip時にcamera、time scale、入力lock、露出を確実に復元する。

### 段階移行

短いcharge・局所色変化・姿勢変化でPhase 2を知らせます。長時間の操作停止と危険範囲の突然変更は避けます。

### 撃破

HP0と同じGameplay tickで攻撃・接触・生成待ちhazardを止め、短い崩れ→姿勢固定→既存Directional Dissolve→コア/接続線消灯→資源返却へ進みます。

- 演出のためにHP0からAIを再稼働させない。
- 既存弾・持続hazardを、ボス撃破時の方針に従って停止/無害化/消去する。
- 崩れposeからDissolveする場合、その開始時点の実skinned poseでbounds/directionを求める。
- progress=1でBody/Outline/Halo/Depth/接続線が残らない。
- 消滅後に旧2Dボスへfallbackして復活表示しない。
- loot・clear・resultは一度だけ。CombatFlowの同時死亡優先順位を守る。
- sceneを途中で離れてもcallback・GPU資源を残さない。

ゲームプレイ時計、演出時計、ユーザーpause、Developer freezeを区別してください。pause中に攻撃時計だけ進む、captureのDraw待ちで演出が進む、introのtime scaleが通常戦闘へ残る問題を防ぎます。

## 14. 視認性・ボス部屋・時間方向の描画

優先順位は「自機と即時危険→攻撃可能コア→本体の動作→装飾」。全部のBloomを増やして解決しないでください。

- 対象ボス部屋の壁・床・装飾の発光を整理する。
- 自機の輪郭と位置が本体やbeamに埋もれない。
- 本体に隠れる重要な床予告は、局所透明度・輪郭化・必要最小限の情報overlayで読めるようにする。
- 全VFXの無条件最前面描画でdepth整合を壊さない。
- コア・敵弾・味方弾・予告を形/動きでも区別する。
- 投影装置らしい薄い回路やリングは追加できるが、危険マーカーに似せない。
- 現在の黒・ネオンの作品性を維持し、背景を無関係なテーマへ全面改装しない。
- 髪・腕・胴体を白いBloomの塊ではなく線で読ませる。

Temporal処理がcamera motionのみを扱う場合、新しい本体動作でghostingが出ないか調査します。必要なhistory reset/rejectionや限定対応を行い、巨大な新TAAを作らないでください。camera切替時の履歴も検証します。

画面揺れ・flash・演出強度を抑える設定を設け、軽量化しても予告時間・hitbox・攻撃可能時間を変えません。繰り返す強い全画面点滅を標準にしないでください。

## 15. 実装構成・設定・Developer機能

既存bridge・Visual・flowを使い、GameSceneへ全処理を追加しません。新componentは対象encounterに限定し、万能managerを増やさないでください。

調整可能にする主な値：model scale/足元offset/高さ/向き、boss camera/framing、intro時間、各攻撃のtelegraph/active/recoveryとshape、局所発光/接地/trail/shake、reduced motion。

既存config方式へ統合し、default、finite/range validation、欠落時互換動作を設けます。hot reloadは確定済みattack planを突然変えず、次攻撃または遭遇resetから適用します。

既存F3/F12へ必要な機能だけ追加：本編ボス部屋へ移動、新旧表示/camera比較、各攻撃再現、phase/time/shape/hitbox表示、pause/step/reset、死亡/abort、capture/録画。

素材確認cameraと標準cameraは区別します。Developerの強制phase/無敵/HP操作はReleaseへ露出させず、新ボスの戦闘・モーション・intro・dissolve自体はReleaseに含めます。

## 16. 本編接続・3スタイルの成立

通常のTitle→遠征→対象ボス部屋で新profileが標準動作するようにします。別Developer sceneだけ完成しても未達成です。

既存部屋を優先し、通行範囲とspawnを確認します。背景延長とplayer移動可能範囲を混同しません。

Shooter/Drone/Meleeごとに実機確認：

- 通常入力でコアへdamageが入る。
- 誘導弾/Droneが正しいtargetへ向かう。
- Meleeの通常間合いでコアへ到達し反撃できる。
- 3攻撃すべてに回避場所と猶予がある。
- 本体/演出でplayerが長時間見えなくならない。
- 帰還/再出撃で旧camera/入力/time scaleが残らない。

自動入力の成功と人間による面白さの評価を同一視せず、自動検証とユーザーの手動プレイ確認を区別します。

## 17. 新規テストと既存Scenario Runner拡張

既存13シナリオを維持し、必要なケースを追加します。小さな純粋state/shapeテストと、本編の実actor/collisionを通るテストを両方用意してください。

### A. 配置・camera

- floor/local/world基底、足元anchor、向き、finite
- camera→screen→floorの往復と照準
- 画面端・対応解像度のframing
- camera切替/intro skip/pause/退出後の復元

### B. Gameplay contract

- 3攻撃各々のtelegraph/active/recovery
- target lock後の固定、attack instance一意性
- 予告shapeとhit shapeの共有
- 空中弾の非接触、床到達event一回、着地hit一回
- beamのwall clipping、damage tick、終了後hitなし
- 0/負/非有限dtの安全性
- dt spikeによるevent飛ばし/二重発火/猶予消失の防止
- 同一seed/dt/inputで2回の再現性
- 異なるdtではbit一致でなく合法遷移・猶予・終端を確認

### C. lifecycle

- 全主要phaseでのHP0
- 発射前、着地直前、beam active中の死亡
- playerとボスの同時死亡
- scene abort、title return、retry、encounter generation更新
- 残留hazard/遅延event/入力lock/camera/音の停止
- Dissolve完了/資源返却、旧2D表示への復帰なし

### D. 表示からの独立性

- 同じ新AIでVisual ON/OFFのGameplay snapshot/event一致
- reduced motion ON/OFFのGameplay一致
- Drawを複数回挟んでもsimulationが進まない
- 新VFX乱数によるGameplay乱数のずれなし
- clip/poseの非restart・非累積drift

### E. 3スタイル・Release

- 各スタイルで実damage・実回避・死亡/retry
- 本編の新profile選択
- 無効config/不存在参照の安全な失敗
- Developer機能非露出、既存Release除外契約
- packageの必要asset/shader/config同梱

テスト件数を水増しせず、本編と同じ処理を使います。別の簡易simulationだけPASSさせないでください。

旧ボスfixtureは必要に応じて旧profileを明示し、新ボスの期待結果と混同しません。失敗を消す目的でassertionを広く削ったり、理由なく許容誤差を増やしたりしないでください。

## 18. 実機画像の比較・調整

通常cameraで次を撮影・目視：浮遊Idle/斜め向き/player接近、Volleyのcharge/空中/着弾、Diveの上昇/降下/着地/recovery、Beamの予告/active/終了、Phase 2、被弾、撃破/Dissolve中間/終端、playerが本体背後・左右・画面端にいる状態。

既存表示Aと改善表示Bを可能な限り同じpose/camera/sceneで比較します。cameraも変更した総合比較は別枠にして条件差を記載します。

目視確認：

- 主体が床へ寝た人型に見えないか。
- 輪郭・上体・腕・立体軌道が読めるか。
- 立体感がintro限定ではないか。
- 攻撃場所が分かり、予告より先に当たったように見えないか。
- 自機/床予告が覆われないか。
- 画面外、板ポリ消失、白飛び、黒画面、ghosting、depth残像がないか。

問題があれば原因と比較条件を記録し、修正→再撮影します。問題解消後も理由なく変更し続けません。

「画素が出た」「ビルド成功」「画像保存」だけで画質改善を断定しないでください。目視機能を使えない場合は撮影まで進め、目視を未検証と明記します。

## 19. 性能・資源・測定条件

同じhardware、解像度、configuration、VSync/上限で測定します。次の比較を分けてください。

1. 同一新AI・入力でVisual OFF/ONの追加表示コスト。
2. 新旧ボス戦全体の負荷。攻撃/オブジェクト数の差も記録。
3. 通常描画とcaptureの保存コスト。

既存Profilerでwarmup後の複数frameからCPU/GPUの平均・median・p95を記録します。GPU有効sample数を残し、未取得値を0msにしません。

- 毎frameのmodel/clip/descriptor生成を避ける。
- animation/matrix/bufferの二重更新を避ける。
- 通常描画へ不要なGPU readback/強制waitを入れない。
- GPU資源は実際の使用完了fenceに従って解放する。
- 複数遭遇のload/kill/retry/abortで資源を追い、共有cache warmupとinstance leakを分ける。
- trail/particle/beamに上限を設ける。

性能悪化時は原因を測り装飾密度などから調整します。核心の3D配置/攻撃を無効化して最終成果にしません。

計測後にsource/shader/configを変更した場合、影響する最終版を再測定するか旧版の参考値と明記します。Developmentの局所CPU改善をRelease全体/FPSの改善と呼ばないでください。

## 20. GitHub Pages用画像・動画

別のチャットがPagesを制作しています。今回はサイト本体、別リポジトリ、Pages設定、公開/push/deployを変更しません。受け渡し可能な素材と説明を作ります。

### 静止画

最終版から5～8枚程度を選別します。

- hero：playerと大型3D本体、床との高さ関係。
- depth-volley：本体・空中の弾・床予告の同時表示。
- dive：降下と着地点の関係。
- beam：本体から床への接続と回避領域。
- dissolve：姿勢とネオンラインの消去境界。
- optional comparison：変更前後。条件差も説明。

本来のゲーム解像度で保存します。拡大をnative高解像度と呼ばず、不要なupscaleをしません。原本PNGと、利用可能な既存手段によるWeb用軽量画像/posterを分けます。

### 連続した実描画動画

既存録画、Media Foundation、利用可能encoderを先に調べ、実際の連続frameを記録します。静止画のスライドショーをゲームプレイ動画と呼ばないでください。

目標：

- gameplay：30～45秒程度。標準の本編camera/設定でplayerが操作され、複数攻撃が見える連続映像。
- showcase：必要に応じ別の短い紹介映像。登場/3攻撃/撃破を編集した場合は編集ありと明記。
- loop/poster：短い代表loopとサムネイル。継ぎ目で長いfadeや乱れを作らない。

無敵、HP変更、強制phase、camera固定、自動入力の有無をmetadataへ記録します。Developerで強制した撃破を自然攻略と称しません。公開captionにも必要な区別を記載します。

撮影のための汎用AIプレイヤー新規開発は不要です。既存の入力再現/記録を使い、通常経路の記録と確認用fixtureを分けます。

- HUDを含む通常ゲーム画面を少なくとも一つ残す。
- ImGui/debug hitbox/個人情報/Windows通知/ローカル絶対パスを公開素材へ入れない。
- 通常版にない豪華な撮影専用描画で成果を偽装しない。
- time scale・simulation dt・encoded fpsを記録し、誤った倍速動画にしない。
- MP4等のWeb用形式は利用可能encoderで出力し、再生/duration/解像度/frame countを確認する。
- 利用できる範囲でfast-start等のWeb再生設定を整える。未導入codecを黙ってdownloadしない。
- 動画を出せない場合は理由、連番frameまたは代替形式、再現手順を保存し、動画完成とはしない。
- 容量/frame上限を設け、無制限録画でdiskを埋めない。

### 公開条件と引継ぎ

モデル・派生線素材・音の出典と利用条件を、原文・既存台帳で確認します。モデルファイル再配布と画像/動画掲載を混同しません。未確認素材は公開候補から除外するか要確認と明示します。

公開用動画は無音を初期案にして構いません。音を入れる場合は動画掲載の条件も確認できるものだけを使います。既存モデルを自作、生成clipをモーションキャプチャ等と偽って説明しないでください。

`portfolio-handoff.md`に以下をまとめます。

- ファイル名、相対パス、用途、サイズ、解像度、duration
- 撮影configuration/camera、fixture/強制操作の有無
- 原本と圧縮版の対応、採用/除外候補
- 日本語captionとalt text
- 約100～200字の作品内技術説明
- 2D Gameplay / 3D Presentation / GPU Skinning / 攻撃契約 / Dissolveの説明
- 作者の実装部分と既存・外部素材の区別
- 素材出典、公開に残る確認事項
- Pages担当への推奨配置

必要ならcontrols/poster/preload等を含む単体HTML埋込み例を渡しますが、他担当のサイトを編集しません。未完成の外部URLを捏造しないでください。

## 21. 最終ビルド・全件検証・配布確認

最終source/shader/configをfreezeし、hash/差分を記録してから検証します。

1. Development x64/Release x64をbuildする。
2. 既存＋今回追加した全件検証をfreshな最終版で実行する。
3. 新しい重要runtime scenarioを各2回実行し、event/observableを比較する。
4. 既存13シナリオ、Release除外、異常設定、packagingを再確認する。
5. 配布packageを作り、Title→遠征→対象ボス→結果/帰還→再出撃を実機で確認する。
6. 別cwdの配布copyでもmodel/shader/config/DLLが解決できることを確認する。
7. 最終版の画像・動画・必要な性能記録を保存する。

過去のPASSと修正後の一部PASSを足して、最終全件PASSと表示しないでください。全件実行できない場合は完了数・未実行・失敗・理由を分けます。

検証中にコードを変更した場合は開始版を記録し直し、影響範囲を再検証します。最終全件証拠と変更後状態が違う場合は差を隠さないでください。

環境上実行できないbuild/GPU/録画/目視項目はBLOCKEDと記録します。ゲーム実装と、配布/画質/性能の確認完了を別に報告してください。

## 22. 差分レビューと有限の進め方

仕様書を再読して全差分をreviewします。重点：

- 2D hitboxと3D表示、cameraと照準の一致
- 一方向snapshot、Draw/Animationからのattack発火混入
- damage/event重複、旧AI/新AIの二重更新
- 死亡/scene切替後のhazard/camera/入力lock
- GPU資源解放のfence条件
- 新VFXによるGameplay乱数への影響
- 通常部屋/他ボスへの意図しない変更
- DeveloperコードのRelease混入
- 旧テストを都合よく弱めた差分
- 無関係なformat、生成物追跡、コメント誤り

`git diff --check`等を実行し、修正後に関連検証を再実行します。

複数agentを実際に利用できる環境なら、Gameplay契約、描画/入力、テスト/素材の読取reviewを分担できます。同じファイルへの無制御な並列編集は禁止。GPU実機試験・性能計測は直列化します。別モデルへ相談していないのに相談済みと報告しないでください。

時間・使用量の上限に近づいた場合は、進捗・安全な差分・未完了条件・次の再現コマンドを保存します。無限の改善ループや、全部終わるまで結果を残さない進め方にしません。

## 23. 完成条件

各項目をPASS / FAIL / BLOCKED / NOT RUNで報告します。非該当には理由が必要で、核となる体験を非該当にしてはいけません。

- C01 通常本編経路で対象ボスの新profileが動く。
- C02 本体と足元アンカーが分離され、操作中に高さ・奥行き・上体の動きが分かる。
- C03 拡大sprite/浮いた模型/introだけの演出で終わっていない。
- C04 コアが明確で、Shooter/Drone/Melee全てから攻撃できる。
- C05 HPは一つで、重複damage・見えない接触判定がない。
- C06 Volleyが予告→空中軌道→床着弾→硬直まで動く。
- C07 Diveが後退/上昇→予告固定→降下→着地→反撃時間まで動く。
- C08 Beamが予告/床接続/掃射/実hit/終了まで動く。
- C09 shape/時間/damageはGameplayが正で、Visualが一方向に読む。
- C10 他ボス/通常敵/基本操作/遠征経済を意図せず変更していない。
- C11 camera/mouse aim/床予告/hitが一致する。
- C12 intro skip/pause/death/abort/retryでcamera/入力/時計を復元する。
- C13 モーションで攻撃が見分けられ、driftや毎frame restartがない。
- C14 HP0で攻撃停止し、Dissolve/コア消灯/資源返却/結果が一度だけ完了する。
- C15 自機/危険範囲が本体や発光に長時間埋もれない。
- C16 演出軽量化でGameplayルールが変わらない。
- C17 既存＋新規テストの最終版全件結果が保存されている。
- C18 Development/Release buildと本編配布確認ができている。
- C19 実機画像/連続映像を確認し、撮影条件を保存した。
- C20 最終版に対応する性能/資源測定、または未計測理由がある。
- C21 Pages素材/caption/出典/要確認事項の引継ぎがある。
- C22 無断commit/push/merge/deploy、外部素材取得、ユーザー変更破棄をしていない。

特にC01～C09を満たさず、文書やDeveloperメニューだけ増やしてGoal達成としないでください。

## 24. 最終成果物と回答

原則`docs/neon-boss-depth/`へ保存します。

- `README.md`：目的/結果/完成条件表/最短確認手順
- `plan.md`：開始状態/判断/段階ごとの進捗
- `architecture.md`：座標/camera/2D-3D依存/コア/attack契約
- `attack-design.md`：3攻撃/shape/時間/判定/反撃/3スタイル
- `validation.md`：baseline/最終全件/実機/Release/失敗
- `visual-review.md`：A/B条件/実画像確認/修正理由/主観項目
- `performance.md`：条件/sample/hash/資源/制約
- `portfolio-handoff.md`：Pages素材/説明/公開確認

同じ内容を大量重複させず相互参照で整理します。画像・動画・ログ・binaryは`generated/neon-boss-depth/`等へ置き、実際の保存先一覧を残します。この仕様書は完成条件の原本として残し、都合よく縮小しません。

最終回答に含めるもの：

1. 通常プレイの何がどう変わったか。
2. 足元アンカー/コア/camera/2D attack契約の設計判断。
3. 3攻撃と各スタイルの攻撃・回避方法。
4. 開始branch/HEAD/status、終了status、変更ファイルの役割。
5. 最終build/全件/runtimeの結果と未確認事項。
6. 性能・資源、計測条件と版、未測定項目。
7. 実際の画像/動画/Pages引継ぎファイルのパス。
8. 通常プレイ最短確認とDeveloper再現手順。
9. 残課題、好みの調整、BLOCKEDな完成条件。
10. commit/push/merge/deployをしていないこと。

中心は「3Dが存在すること」ではなく「平面の戦闘場へ立体的なボスが介入し、見て理解できて遊べること」です。小さい表示変更だけで止まらず、本編・検証・素材まで一連の体験としてつなげてください。
