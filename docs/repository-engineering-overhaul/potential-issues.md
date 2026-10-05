# 未確証の問題と意図的に残す境界

ここは確認済み不具合一覧ではない。今回の通常Gameplayで再現を確認できていないもの、既存契約を変えないと修正範囲が広がるものを分けて記録する。性能改善の根拠や修正件数を増やす目的で変更しない。

| 項目 | コード上の懸念 | 現在の証拠・判断 |
|---|---|---|
| Actorに渡す任意dt | Enemy::Update／Move、Bullet::Updateなどは引数全体をfinite／nonnegativeで拒否していない。NaNはtransformやfloat→intのsubstep計算へ、負dtは旋回clampの逆転へ到達し得る。 | 現在Sceneは固定の正base dtと制限された時間倍率を渡す。Pure combat／Neon／新Runner境界はinvalid dtを検査する。通常経路の再現なしで全actorの時間 semanticsを変更しない。 |
| animation transition duration | SkinnedModel::TransitionToAnimationのdurationがNaNならblend factorも非有限になり得る。Update dt／checkpoint restore／登録clipは既にfiniteを検証する。 | 現在callerは有限の定数／制限sliderを使う。外部設定からdurationへ流れる再現がないのでAPI robustness候補として残す。 |
| Initializeの再呼び出し | RenderTexture／SkinnedModel／Rendererは同じ実体でteardownせずInitializeするとdescriptorを失い得る。 | runtime ownerは通常新しい実体を構築する。実際にretry経路があったPreviewはC1としてowner側で修正。engine全体へ無条件RAII releaseを入れるとmanager寿命／GPU完了の順序を壊せるため行わない。 |
| 非有限／極端なconfig | JSON上の大きい有限doubleをfloatへ変換するとInfになり得る。std::max／clampだけの項目はすべてfinite保証にはならない。 | PlayerClassCatalogは全読み込みをtransactional try/catchで扱うためwrong-typeは既にatomic failure。通常authoring catalog／room／balanceは検証済み。具体的な未検証項目・runtime到達経路の確認前に主観的なbalance上限を増やさない。 |
| nullable raw pointer | Collider／AttackController／Enemyの低層APIは有効なPlayer／Stage／BulletManager／Objectを前提とする箇所がある。 | 現在SceneはUpdate前にすべて接続する。borrowed pointerのowner／消去境界を記録し、孤立テストではadapterを使う。存在しない通常null経路を理由に大量の早期returnを足さない。 |
| legacy resource missing | 古いmodel／shader／map loaderにはassert中心の経路がある。 | Neon missing modelはpresentation loadのerrorと2D fallback、Stageのrun CSVはatomic reject。Release packaging／resource参照／shader compile検証を実行し、正常な出荷状態を確認した。仮想的なすべての壊れたpackageに新fallbackを作る対象にはしない。 |
| engine lifetime cache | Model／Texture／Particle manager、ShadowMapなどがengine lifetimeで資源を保持する。 | 共有cacheとper-encounter instanceを区別する。Sceneのdescriptor増加だけで意図的cacheをleakと呼ばない。C1検証もwarm-up後の基準値を使う。 |
| engine → scene coupling | Bloom／ShadowがSceneManagerをinclude・直接利用する。 | 確認済みarchitecture couplingで、現在のruntime不具合とは別。純粋stateとScene orchestration抽出を先に行い、描画framework全面置換はしない。 |

Neon Bossの専用Walk／Dash／Death clipがなく、既存Idle／Attack・姿勢保持を使うこと、BLEND alphaを既存Previewと同じcutoutへ近似すること、既存撃破post effectを維持することは既知のpresentation方針である。バグと誤分類して今回の見た目／内容を変更しない。
