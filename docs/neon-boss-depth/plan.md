# Neon Boss Depth Encounter — implementation plan

2026-10-05 JST。仕様原本は[CODEX_NEON_BOSS_DEPTH_GOAL.md](CODEX_NEON_BOSS_DEPTH_GOAL.md)。実装・本編調整・検証・Pages向け素材までを一つのGoalとして扱う。途中段階のbuildや表示だけで完成扱いにしない。

## 開始状態と保全

開始branch `feature/neon-boss-depth-encounter`、HEAD `8b9282e5241563bf5bb26b5922aebfe6917bce9d`。staged／unstaged／untrackedは全て0、追跡ファイル960。実作業rootは「自作エンジン2」で、Pages用リポジトリへ触れない。適用されるAGENTS.mdは祖先／repositoryで見つからなかった。

仕様はDownloadsから全文読み、原本SHA256 `F3C3591B58A3C8DC63142D010E0A2D724891222B69573055903B3DF5302D79CA`を保持した。開始情報と749のsource／asset hashは`generated/neon-boss-depth/baseline/start-state.json`、`initial-source-asset-manifest.json`。既存ignoreのgenerated配下へbinary／log／PNG／動画を保存する。

local Visual Studio 18.9.12128.139、v145。Development／Release x64を使用する。既存Python、FFmpeg／FFprobeを利用し、dependency／モデル／モーションを取得・導入しない。branch／HEADの変更、commit／push／merge／checkout／switch／reset／clean／stash／deployを行わない。

sourceに他作業の変更を検知した場合は、その領域の編集・検証を停止して衝突を記録する。agentの編集対象を明示し、同じファイルを無制御に並行編集しない。GPU実機・build・性能・動画encodeはrootが直列に統括する。

## 調査から判明した責務境界

- GameplayはXY床／Z=0、cameraは負Zからのperspective。現本体はbounds中心を既存Enemyへ合わせる配置であり、このまま巨大化しても仕様の浮遊体にならない。
- Enemyが単一のposition／HP／damage targetを所有する。`Enemy::Update`はRival／Prototype／legacyを排他的に選ぶ。新Depth policyもここで一つだけ更新し、本体用の別HPや別actorを作らない。
- 通常の制作boss部屋は`GameScene.ExpeditionMap.cpp`のResetRunEncounter→EnableExpeditionRival経路。旧fixtureはprofileを明示して保全し、通常本編だけDepthを標準として選ぶ。
- 床コアはEnemyの位置／半径／HPを表示する。味方弾／homing／Drone／Melee targetはこのactorを使う。通常接触を無害にするにはcontact damage値だけでなくPlayer／Droneとのcollision pairを確認する。
- 既存BossVisualBridge→NeonBossVisual→NeonSkinnedRendererと、既存GPU Skinning／Directional Dissolveの寿命を維持する。新attack snapshotは値だけで渡し、Visualやanimationからdamage／spawnへ戻さない。
- 実GLBにembedded animationはない。生成Idle／Attackと実在jointのbind poseを使い、本編専用の手続きclip／局所姿勢を非累積で一度だけ評価する。Previewの旧比較は保存する。
- 本編captureは既存NeonShowcaseCaptureのImGui前backbuffer／fenceを利用する。一般captureの32枚制限は維持し、動画用のbounded連続録画を別の明示設定として設ける。

## 内部マイルストーン

| 段階 | 実装・確認の範囲 | Gate | 現状 |
| --- | --- | --- | --- |
| M0 Baseline／座標診断 | 現sourceの両build・全件、旧姿勢／camera／Gameplay／CPU-GPU-resource／連続映像を固定する | fresh全件driver、原本と関連hash、代表capture／動画確認 | 変更前の両buildと58 interface由来64 command casesは全件PASS。source drift0、pristine packageのRelease hash一致。録画専用の両build・旧13・設定probeもPASS。480 native frames／60fps／8秒の連続映像、MP4／loop／修正版posterをencode・全decode・実browser再生と目視で確認済み。詳細はlegacy-recording.md |
| M1 浮遊体／コア／camera | 足元安定anchor、XY基底とnormal、高さ／体のyaw／scale、接地／接続線、bosscamera、mouse床交点 | 配置・projection往復test、両build、15／20／25度候補の実capture、通常roomへの復元 | PASS（検証範囲）：20度camera・VP/client/floor契約、最終原本／45秒映像と実OSマウスによる左右床・core照準、右special移動を確認。Developer無敵入口で900→883、shot別誤差／被弾shakeのnative測定や人間評価は未実施 |
| M2 2D attack契約 | Depthの排他的policy、同じAttackPlanから予告／hit、Volley／Dive／Beam、phase／lock／wall clip／recovery | pure shape/time/lifecycle test＋実Enemy collision、両build、各攻撃capture | PASS：最終Allの41定義×2＝82実行で両phase3攻撃と全style実damage／safe avoidanceを確認。38壁のBeam／wall-skin回帰、移動・射撃・回避を含む45秒連続映像と別Phase2原本を確認 |
| M3 手続き動作／lifecycle | 生成clip／root・上体反応、2–3秒introとskip、phase cue、崩れ→pose固定→Dissolve、core消灯 | nonrestart／非drift、pause／HP0／abort／retry／同時死亡、両build、連続capture | PASS：実生成8clips／WARP契約、最終82実行のskip／pause／56terminal operationsとmodel／FX終端返却を確認。最終binaryのCollapse／Dissolve原本832・854・869も実視。連続gameplay動画のPhase1とは別の強制HP fixture |
| M4 本編／3スタイル | Title→遠征→対象bossの標準選択、通常攻撃でcore damage、実回避・反撃、帰還／再出撃 | Shooter／Drone／Meleeのactual runtime各2回、Release package flow、通常入力／UI／camera | BLOCKED（C18）：全3style damage／dodge／death／実retryを各2回PASS。通常Developmentは18列・46,065更新で自然撃破→結果／Title／再出撃、合法な経済を確認。実OSマウスのDepth照準も確認した。通常Releaseは有限tutorial／retry／Title再入場まで。3連続Goalターンでも操作対象画面を取得できず、通し確認は未実施、人の操作感／面白さも未評価 |
| M5 調整／独立性／性能 | 危険と自機の可読性、Visual・reduced motion OFF/ON、bounds／資源、同条件コスト比較 | actual PNG／連続動画目視、CPU-GPU有効sample、warmupとinstance寿命、Gameplay event比較 | PASS（実証範囲）：ON/OFF/reduced各2,400frame一致・terminal返却、最終性能4run、代表26原本／45秒映像／8WebP／合法下壁のQAを確認。各GPU有効1,199／無効1、Scene SRV＋1後flat・割当元未特定、pure capture cost未計測の限界はperformance.md |
| M6 最終freeze／全件 | 最終source／shader／config hash、現registryの既存＋新規全件、既存13、異常設定、配布実行 | 両build／全件fresh／新runtime各2回／別cwd package、source driftなし | PASS：frozen build_iでall_a_20261005を13:19:51.4373114～14:33:23.4443617 UTC実行、69/69・0FAIL・source drift0。Depth82＋2parity／旧26＋7probe／両build／hardware-WARP／限定ASAN／package自動試験とpostcheck PASS、907/907入力一致。通常Release routeはM4/C18で別判定 |
| M7 Pages素材／完了監査 | native PNG 5–8枚、web軽量版、30–45秒連続gameplay MP4、loop/poster、caption／alt／出典／引継ぎ | duration／fps／frame数／解像度／再生確認、C01～C22個別根拠、whole diff review | PASS（素材引継ぎ）：8PNG／8WebP、45秒MP4／8秒抜粋／clean poster、caption／alt／164字説明／条件／出典要確認と19file manifest-v2を保存・独立照合。元FAILと訂正receiptを保持。現在C01～C22は21PASS／C18 BLOCKED（通し未実施）で、Goal全体の完了とは区別 |

## 初期設計案と変更範囲

floor normalは負Zを候補とし、local +Yをnormalへ向ける基底を確認する。足元はbind骨／boundsから一度求め、動く足の位置で毎frameanchorをずらさない。floor anchor／浮遊高さ／床内offset／body yaw／upper-body twistを分ける。cameraは真上から15–25度を比較し、実際のview-projectionとray-floorで照準を決める。単なるscreen offset／sprite scaleを主解決にしない。

新Neon専用Encounter／AttackPlanは攻撃ID、phase、計画時間、確定target、circle／segment、壁制限、damage cooldown、cancelを持つ小さい値contractとする。Volleyは3–5円の床予告→空中表示→Gameplay床到達event一回、Diveは着地だけ危険＋到達可能なcore、Beamは共有clipped segment／widthで予告・active・終了を扱う。確定計画はHP閾値やhot reloadで書き換えない。大きいdtで予告を飛ばさず、eventを二重発火させない。

隔離prototypeと読取監査は`generated/neon-boss-depth/audit-gameplay/`、`audit-visual/`、`audit-validation/`。配置／projection／生成motion／bounded録画は本番へ採用した。録画専用M0 gateは実行済み、frozen build_iのDevelopment／Releaseは0 warning／0 error、907入力のfreezeを保存。iの通常Development完走（18列／seed226381203）を確認後、最終All all_a_20261005を13:19:51.4373114～14:33:23.4443617 UTCに完了、69/69・0FAIL・drift0とpostcheckを照合した。過去hの部分PASSや失敗は別scopeで保存する。最終性能／Depth連続映像／派生8画像／引継ぎと実OSマウス照準は後続証拠で確認済み。通常ReleaseのDepth勝利を含むC18通し確認と人の操作感／面白さは未完了。camera/input/jitter、Temporal、opaque Body、結果clockによるDissolve中断、Drone contact／perfect dodgeの接続は採用した本番契約と実回帰の範囲をvalidation.mdで区別する。

変更前の比較binaryは`generated/neon-boss-depth/baseline/frozen/`。Development SHA256 `FD1997F1F9BA5C22770F20F89688EFDD624B9052A68351BA540BAD2C5FA285A1`、Release `8FB077797DD7013CCC2FC0658D3297E893CDC067F9956CDD3AA107802F07A6C7`。両隣接build profileのhashも一致を確認した。これらはbaselineであり、Depth実装の成功や最終binaryの検証として扱わない。

旧Rival／Prototype、通常敵、Player stats／強化、通貨・報酬・遠征経済は保存する。対象boss以外のcameraも保存する。新VFXはevent ID／局所streamを使い、既存Gameplay RNGの消費順へ混入させない。

最終判定は[README.md](README.md)のC01～C22に個別の現在証拠を記録する。過去Allの63／64＋部分再検証を今回の最終全件PASSへ流用しない。失敗は原本を残し、原因→小さい再現→修正→関連gate→fresh全件の順に解決する。
