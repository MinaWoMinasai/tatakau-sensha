#pragma once
#define NOMINMAX
#include "Collider.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include "Object3d.h"
#include "Sprite.h"
#include "AttackController.h"
#include "PrototypeBossCombat.h"
#include "RivalBossCombat.h"

class Player;
class Stage;
class EnemyManager;

/// @brief ボスのHP・移動・射撃・死亡演出を管理する。通常AI、試作戦闘、遠征ライバルの行動を切り替える。
/// @note 機体モデル・Player・Stage・各管理クラスは借用し、利用中は呼び出し側で有効に保つ。
class Enemy : public Collider {

public:
    /// @brief 射撃パターン、弾速（60FPS基準の1フレーム当たり）、拡散角（度）、発射間隔（秒）、威力・耐久度・貫通力の設定。
    struct BossAttackConfig {
        enum class Pattern {
            Spread = 0,
            Ring = 1,
            Sniper = 2,
            Alternating = 3,
        };
        float bulletSpeed = 0.4f;
        int bulletCount = 2;
        float spreadAngleDeg = 30.0f;
        float cooldown = 0.15f;
        uint32_t damage = 10;
        float bulletHp = 0.0f;
        float bulletPenetration = 0.0f;
        bool randomSpread = true;
        Pattern pattern = Pattern::Spread;
    };
    /// @brief 通常敵への敵対・捕食による回復と成長、捕食対象を探す距離、照準の旋回時間（秒）の設定。
    struct EnemyProgressConfig {
        bool expEnemyHostile = false;
        uint32_t expEnemyContactDamage = 12;
        int healOnExpEnemyKill = 30;
        int killsPerLevel = 3;
        int maxHpGainPerLevel = 20;
        uint32_t damageGainPerLevel = 2;
        bool levelingModeEnabled = true;
        float levelingEnterPlayerDistance = 24.0f;
        float levelingExitPlayerDistance = 16.0f;
        float levelingSearchRadius = 80.0f;
        float aimTurnHalfSeconds = 1.0f;
    };

    enum class AIState {
        Attack,       // 攻撃、射撃
        KeepDistance, // 距離をとる
        Evade,        // 回避
        Wander,       // 徘徊
    };

    /// @brief 所有するHP表示用モデル・スプライトを破棄する。借用する機体モデルは破棄しない。
    ~Enemy() override;

    /// @brief 借用する機体モデルと地形を設定し、衝突属性・HP表示・通常AIの利用を準備する。
    /// @note objectは有効なモデルが必要。遭遇戦の再開にはResetRunEncounterを使う。
    void Initialize(cg2::Object3d* object, const cg2::Vector3& position, Stage* stage);

    /// @brief 生存中だけ行動・射撃・地形衝突付きの移動を更新する。死亡後は演出だけを進める。
    /// @note 遭遇戦が無効なら何もしない。Player・Stage・弾管理は先に設定する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Update(float deltaTime);

    /// @brief drawBodyがtrueなら機体を描画する。HP表示は含めない。
    void Draw(bool drawBody = true);
    /// @brief 機体だけを描画する。死亡後は演出中だけ表示し、描画用に変えた色・姿勢・ライティングを復元する。
    void DrawBodyOnly();

    /// @brief スプライト描画の互換用入口。現在は描画しない。
    void DrawSprite();

    /// @brief 生存中の有効な遭遇戦で、Player方向への単発弾を弾管理へ渡す。
    void Fire();

    /// @brief 生存中の有効な遭遇戦で、移動目標への射撃をBossAttackConfigのパターンと性能で生成する。
    void ShotgunFire();

    /// @brief startPosへボスの位置、targetPosへPlayerの位置を代入する。移動方向は計算しない。
    void ApproachToPlayer(cg2::Vector3& startPos, cg2::Vector3& targetPos);

    /// @brief ボスが保持する姿勢への借用参照を返す。
    const cg2::Transform& GetWorldTransform() const
    {
        return worldTransform_;
    }

    /// @brief ワールド座標での位置を返す。
    cg2::Vector3 GetWorldPosition() const override;

    /// @brief Playerを借用する。更新・照準・射撃で参照する間は有効に保つ。
    void SetPlayer(Player* player)
    {
        player_ = player;
    }
    /// @brief 通常敵・資源を探索するEnemyManagerを借用する。nullptrでは捕食対象を探索しない。
    void SetEnemyManager(EnemyManager* enemyManager)
    {
        enemyManager_ = enemyManager;
    }

    /// @brief ワールド座標の位置を設定し、借用モデルの姿勢・行列も更新する。
    void SetWorldPosition(const cg2::Vector3& pos)
    {
        worldTransform_.translate = pos;
        object_->SetTransform(worldTransform_);
        object_->Update();
    }

    //----------------------
    // 定数

    // 発射間隔
    static inline const int32_t kFireInterval = 60;

    /// @brief 成立済みの接触を受け、通常敵へのダメージ、自機・ドローンによる押し返し、弾によるHP減算を行う。
    /// @note otherは有効な相手が必要。遭遇戦が無効または死亡済みなら何もしない。致死ダメージは直ちに死亡を確定する。
    void OnCollision(Collider* other) override;

    /// @brief 半径を返す。
    float GetRadius() const override
    {
        return radius_;
    }

    /// @brief 死亡状態を返す。
    bool GetIsDead() const
    {
        return isDead_;
    }

    /// @brief 死亡であるか判定する。
    bool IsDead()
    {
        return isDead_;
    }

    /// @brief Attack・Wander・Evadeに応じて接近・回避・徘徊の重みを設定する。KeepDistanceでは保持する。
    void AIStateMovePower();

    /// @brief 行動状態・経路・回避から移動速度を更新し、目標へ旋回する。位置はUpdate側で進める。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Move(float deltaTime);
    /// @brief 徘徊用の乱数ベクトルを返す。単位ベクトルではなく、Z成分も含む。
    cg2::Vector3 RandomDirection();

    /// @brief 敵弾属性以外で10ワールド単位未満の弾から離れるベクトルを合算する。接近方向・死亡状態は検査しない。
    cg2::Vector3 EvadeBullets();
    /// @brief 敵弾属性以外の弾との距離を優先し、次にPlayerとの距離でAI状態を選ぶ。
    void UpdateAIState();

    /// @brief AABBを返す。
    cg2::AABB GetAABB();

    /// @brief 直近の移動用方向を返す。ためらい中などは単位長とは限らない。
    cg2::Vector3 GetDir()
    {
        return dir_;
    }
    /// @brief 照準方向を返す。
    cg2::Vector3 GetAimDirection() const
    {
        return {std::cos(worldTransform_.rotate.z), std::sin(worldTransform_.rotate.z), 0.0f};
    }
    /// @brief ダメージフィードバック比率を返す。
    float GetDamageFeedbackRatio() const
    {
        return damageFeedbackDuration_ > 0.0f ? (std::clamp)(damageFeedbackTimer_ / damageFeedbackDuration_, 0.0f, 1.0f) : 0.0f;
    }

    /// @brief 移動方向の線分を作る。方向がほぼ0なら+Xを使う。
    /// @param length 線分の長さ（ワールド単位）。
    cg2::Segment MakeForwardRay(float length) const;

    /// @brief 移動方向へ長さ3の線分を伸ばし、有効な地形AABBとの交差を判定する。
    bool IsBlockNearByRay();

    /// @brief 前方の壁を検出した初回に、壁沿い状態を設定して減速・横向きの補正ベクトルを返す。
    cg2::Vector3 WallAvoidByRay();

    /// @brief 現在位置にdir * 3を加えた候補点からPlayerまでの距離を返す。dirの正規化や壁の検査は行わない。
    float ScoreDir(const cg2::Vector3& dir);

    /// @brief ボス位置からPlayer位置までの線分を返す。
    cg2::Segment MakeRayToPlayer() const;

    /// @brief rayとPlayerの衝突球が接触するか判定する。地形の遮蔽は調べない。
    bool HitPlayerByRay(const cg2::Segment& ray);

    /// @brief Playerへの射線と地形AABBの交差・中心距離で、射撃可能な見通しを近似判定する。
    bool HasLineOfSightToPlayer() const;
    /// @brief 目標への射線と地形AABBの交差・中心距離で、射撃可能な見通しを近似判定する。
    /// @return 地形が未設定、目標がほぼ同位置、または中心が目標より近い遮蔽物を検出した場合false。
    bool HasLineOfSightToTarget(const cg2::Vector3& targetPos) const;

    /// @brief AIの移動速度（60FPS基準の1フレーム当たり）を返す。追加のノックバック速度は含まない。
    cg2::Vector3 GetMove()
    {
        return velocity_;
    }

    /// @brief 有効な遭遇戦で未死亡なら、死亡フラグを立てて半径を0にし、死亡演出を開始する。実体は削除しない。
    void Die();

    /// @brief 死亡済みで死亡演出も終了しているかを返す。状態は消費しない。
    bool isFinished();

    /// @brief 死亡であるか判定する。
    bool IsDead() const
    {
        return isDead_;
    }
    /// @brief 現在のHPを返す。
    int GetHp() const
    {
        return hp_;
    }
    /// @brief 最大HPを返す。
    int GetMaxHp() const
    {
        return maxHP_;
    }
    /// @brief 撃破画面演出を更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateDefeatPresentation(float deltaTime);
    /// @brief 有効な遭遇戦で未死亡かつamountが0でなければ、HPを減らして被弾演出を開始する。
    /// @note 致死ダメージではHPを0にし、その通知中に死亡を確定する。
    void TakeDamage(uint32_t amount);
    /// @brief 有効な遭遇戦で未死亡なら、追加のノックバック速度を加算して長さを0.34以下に制限する。
    /// @param power 正の有限値を使い、0.95を上限に速度へ変換する。
    /// @note 位置はUpdateで進める。ゼロに近いdirectionは無視し、現在の攻撃状態は中断しない。
    void ApplyKnockback(const cg2::Vector3& direction, float power);
    /// @brief 射撃設定をコピーして弾速・発射数・待ち時間などを制限し、通常射撃の間隔も更新する。
    void SetBossAttackConfig(const BossAttackConfig& config);
    /// @brief ボス攻撃設定を返す。
    const BossAttackConfig& GetBossAttackConfig() const
    {
        return bossAttackConfig_;
    }
    /// @brief 捕食・成長設定をコピーして距離・時間などを制限し、通常敵への衝突マスクも切り替える。
    void SetEnemyProgressConfig(const EnemyProgressConfig& config);
    /// @brief 敵進行度設定を返す。
    const EnemyProgressConfig& GetEnemyProgressConfig() const
    {
        return enemyProgressConfig_;
    }
    /// @brief レベルを返す。
    int GetLevel() const
    {
        return enemyLevel_;
    }
    /// @brief 敵経験値を返す。
    uint32_t GetEnemyExp() const
    {
        return enemyExp_;
    }
    /// @brief 現在、Playerとの距離に応じた捕食対象の探索モードに入っているかを返す。
    bool IsLevelingModeActive() const
    {
        return levelingModeActive_;
    }
    /// @brief 通常敵の撃破数を増やして捕食回復・成長を反映する。
    /// @note 資源争奪中は通常敵からの回復を最大1にし、経験値・レベルは増やさない。遭遇戦無効、または死亡/HP0以下なら何もしない。
    void RegisterExpEnemyKill(uint32_t expValue);

    using PrototypeAttackType = PrototypeBossCombat::AttackType;
    /// @brief 予備動作で示す攻撃方向・範囲・進行を保持する。
    struct PrototypeTelegraph {
        bool active = false;
        PrototypeAttackType attackType = PrototypeAttackType::AimedSpread;
        cg2::Vector3 direction{1.0f, 0.0f, 0.0f};
        float progress = 0.0f;
        // GapRingではdirectionを中心とする安全な開口幅、それ以外では危険な扇形の幅（度）。
        float spreadAngleDeg = 44.0f;
    };
    /// @brief 試作戦闘の有効状態が変わると攻撃時計をリセットする。有効化時は捕食回復の予算も設定する。
    void EnablePrototypeCombat(bool enabled);
    /// @brief 試作戦闘有効であるか判定する。
    bool IsPrototypeCombatEnabled() const
    {
        return prototypeCombatEnabled_;
    }
    /// @brief 試作戦闘の攻撃圧力を0～4に制限して設定する。
    void SetPrototypePressure(int pressure)
    {
        prototypePressure_ = (std::clamp)(pressure, 0, 4);
    }
    /// @brief 最大HPを1～1000000に制限し、捕食回復予算と現在HPを設定する。
    /// @note healToFullがfalseなら現在HPを新しい最大HP内に制限する。死亡済みの機体は復活させない。
    void SetPrototypeMaxHp(int maxHp, bool healToFull = true);
    /// @brief 試作攻撃調整値を設定する。
    void SetPrototypeAttackTuning(const BossAttackConfig& config)
    {
        SetBossAttackConfig(config);
        prototypeTuningEnabled_ = true;
    }
    /// @brief 試作戦闘で資源を優先探索する設定を切り替える。探索を有効にする他の条件は変更しない。
    void SetPrototypeResourceFocus(bool enabled)
    {
        prototypeResourceFocus_ = enabled;
    }
    /// @brief 生存中の有効な試作戦闘で資源優先が有効なら、資源取得による回復・成長を反映する。
    void RegisterRunResourceClaim();
    /// @brief 試作戦闘または遠征ライバルの現在の攻撃予告を値で返す。状態は消費しない。
    PrototypeTelegraph GetPrototypeTelegraph() const;
    /// @brief ボスの遭遇戦を切り替える。無効化時は移動速度と捕食モード・資源を狙うフラグを解除し、試作攻撃時計をリセットする。
    /// @note HP・死亡フラグ・射撃設定・目標座標・ライバルの行動状態は保持する。再開時の初期化はResetRunEncounterを使う。
    void SetRunEncounterEnabled(bool enabled);
    /// @brief ボスの遭遇戦が有効かを返す。生存状態とは別のフラグ。
    bool IsRunEncounterEnabled() const
    {
        return runEncounterEnabled_;
    }
    /// @brief 同じアクターを再利用する遭遇戦の世代。Visualが過去の死亡状態を引き継がないための識別値。
    uint64_t GetEncounterGeneration() const
    {
        return encounterGeneration_;
    }
    /// @brief 試作ボスの行動時計上の現在段階。描画側は遭遇戦・生存状態と併せて参照する。
    PrototypeBossCombat::Phase GetPrototypeCombatPhase() const
    {
        return prototypeCombat_.GetPhase();
    }
    /// @brief 現在の遭遇戦で発行した射撃要求の回数。単発攻撃が同一更新内でRecoveryへ戻っても観測できる。
    unsigned GetShotsFired() const
    {
        return shotsFired_;
    }
    /// @brief ボスを指定位置・HPの試作遭遇戦として復活させ、移動・死亡演出・成長・通常AI/試作戦闘を初期化する。
    /// @note Initialize後、アクター更新・衝突通知の外で呼ぶ。借用先と射撃設定は保持し、ダメージは初回に保存した基準値へ戻す。
    /// ライバルは無効化し、必要ならEnableExpeditionRivalで別に初期化する。
    void ResetRunEncounter(const cg2::Vector3& position, int hp, int pressure, bool resourceFocus);
    /// @brief ライバルボスの行動状態を表示・検証へ提供する。
    struct RivalCombatStatus {
        bool enabled = false;
        bool phase2 = false;
        RivalBossCombat::Phase phase = RivalBossCombat::Phase::Reposition;
        RivalBossCombat::Pattern pattern = RivalBossCombat::Pattern::AimedBurst;
        float progress = 0.0f;
        int ammo = 0;
        int capacity = 0;
        unsigned shotsFired = 0, dashCount = 0, reloadCount = 0;
        cg2::Vector3 direction{1.0f, 0.0f, 0.0f};
        cg2::Vector3 dashDirection{0.0f, 1.0f, 0.0f};
        float dashDistance = 0.0f;
    };
    /// @brief 遠征ライバルの有効状態を設定し、ライバルの時計・移動を初期化する。
    /// @note ResetRunEncounterの後に呼ぶ。有効化時は通常敵への敵対・捕食を止める。無効化だけでは捕食設定を復元しない。
    void EnableExpeditionRival(bool enabled);
    /// @brief 遠征ライバル有効であるか判定する。
    bool IsExpeditionRivalEnabled() const
    {
        return expeditionRivalEnabled_;
    }
    /// @brief ライバル戦闘状態を返す。
    RivalCombatStatus GetRivalCombatStatus() const;

    /// @brief 射撃と回避判定で使うBulletManagerを借用し、AttackControllerにも渡す。
    void SetAttackControllerBulletManager(BulletManager* bulletManager)
    {
        bulletManager_ = bulletManager;
        attackController_.SetBulletManager(bulletManager);
    }

    /// @brief HPバーを更新する。
    void UpdateHPBar();

    /// @brief 現在のHPをバーとして描画する。
    void HPBarDraw();

private:
    /// @brief マップの行・列の位置を表す。
    struct MapIndex {
        int x = 0;
        int y = 0;
    };

    /// @brief Playerまでの経路探索から、次の通行点への方向を返す。探索不要・失敗ならnullopt。
    std::optional<cg2::Vector3> FindPathDirectionToPlayer();
    /// @brief 直進路が塞がれている場合、セルを探索して次の通行点への方向を返す。
    /// @return 直進可能・範囲外・経路なし・通行点がほぼ同位置ならnullopt。
    std::optional<cg2::Vector3> FindPathDirectionToTarget(const cg2::Vector3& targetPos);
    /// @brief ワールド座標をブロック寸法で割って四捨五入し、Yを上下反転してセル添字へ変換する。範囲外ならnullopt。
    std::optional<MapIndex> WorldToMapIndex(const cg2::Vector3& pos) const;
    /// @brief 列x・行yをワールド座標のセル中心へ変換する。CSV先頭行はワールドYの上側。
    cg2::Vector3 MapIndexToWorld(const MapIndex& index) const;
    /// @brief 地形があれば、マップの外周を除く有効範囲でブロックが非アクティブかを返す。地形未設定ならtrue。
    bool IsPassableCell(int x, int y) const;
    /// @brief 機体半幅に応じた周辺セルを含め、経路探索上で通行可能かを返す。
    bool IsPathPassableCell(int x, int y) const;
    /// @brief baseが通れなければ周囲を半径1～7セルの順に探し、最初に候補がある範囲の最寄りセルを返す。候補なしならbase。
    MapIndex FindNearestPathPassableCell(const MapIndex& base) const;
    /// @brief Playerへの移動経路を、中央と機体幅の左右から伸ばした3本の線分で判定する。
    bool HasClearMoveRouteToPlayer() const;
    /// @brief 目標への中央・左右の3本の線分が、有効な地形AABBに交差しないかを返す。
    /// @note 機体幅を3本で近似する。地形未設定ならfalse、目標がほぼ同位置ならtrue。
    bool HasClearMoveRouteToTarget(const cg2::Vector3& targetPos) const;
    /// @brief 横揺れ・方向の追従・経路上のためらいを更新し、補正した移動方向を返す。
    /// @param deltaTime この処理で進める経過時間（秒）。
    cg2::Vector3 ApplyHumanLikeSteering(const cg2::Vector3& desiredDir, bool usingPath, float deltaTime);
    /// @brief Playerとの距離と捕食設定から移動目標を選び、捕食モード・資源を狙う状態も更新する。
    cg2::Vector3 ResolveMoveTargetPosition();
    /// @brief 目標方向へ向けて姿勢を回転する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void RotateTowardTarget(const cg2::Vector3& targetPos, float deltaTime);
    /// @brief 試作戦闘の時計を進め、予告中の停止・旋回と、発射要求からの弾生成を行う。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdatePrototypeCombat(float deltaTime);
    /// @brief 遠征ライバルの照準・行動時計・速度を更新し、発射要求から弾を生成する。位置はUpdate側で進める。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateRivalCombat(float deltaTime);
    /// @brief 通行判定を通る突進方向と距離を候補から選ぶ。候補なしなら距離を0のまま保つ。
    void SelectRivalDashDirection(const cg2::Vector3& towardPlayer);
    /// @brief 経路探索・左右反転・前後退避で移動方向を選ぶ。経路の再探索時計と横移動の符号も更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    cg2::Vector3 ResolveRivalMove(const cg2::Vector3& desired, const cg2::Vector3& towardPlayer, float deltaTime);
    /// @brief 捕食による回復量をHPへ反映する。
    void HealFromFeeding(int amount);
    /// @brief 捕食レベル・最大HP・接触/射撃ダメージを増やす。試作戦闘ではレベルと最大HPの上限を適用する。
    void AdvanceFeedingLevel();

    // ワールド変換データ
    cg2::Transform worldTransform_;
    // モデル
    cg2::Object3d* object_ = nullptr;

    // テクスチャハンドル
    uint32_t textureHandle_ = 0u;

    // 発射タイマー
    int32_t fireIntervalTimer = 0;

    // 自キャラ
    Player* player_ = nullptr;
    EnemyManager* enemyManager_ = nullptr;

    // 最初の弾までの時間
    uint32_t time_ = 60;

    float radius_ = 2.0f;

    std::string behaviorName_;
    // 弾のクールダウン
    float bulletCooldown_ = 0.5f;

    AIState aiState_ = AIState::Wander;

    float attackPower = 0.0f;
    float evadePower = 0.0f;
    float wanderPower = 0.0f;

    const float kPower = 0.1f;

    float wanderChangeTimer = 1.0f;
    cg2::Vector3 evadeVec = {0.0f, 0.0f, 0.0f};
    cg2::Vector3 wanderVec = {0.0f, 0.0f, 0.0f};

    // キャラクターの当たり判定サイズ
    static inline const float kWidth = 3.2f;
    static inline const float kHeight = 3.2f;

    cg2::Vector3 dir_;

    Stage* stage_ = nullptr;

    bool isWallFollowing_ = false;
    float wallFollowTimer_ = 0.0f;
    cg2::Vector3 wallFollowDir_;

    // 通常射撃の待ち時間（秒）
    float kFireTimerMax_ = 0.15f;
    float fireTimer_ = 0.0f;
    BossAttackConfig bossAttackConfig_{};
    EnemyProgressConfig enemyProgressConfig_{};
    bool prototypeCombatEnabled_ = false;
    bool prototypeTuningEnabled_ = false;
    bool runEncounterEnabled_ = true;
    uint64_t encounterGeneration_ = 0;
    unsigned shotsFired_ = 0;
    bool runEncounterBaselineCaptured_ = false;
    uint32_t runEncounterBaseContactDamage_ = 0;
    uint32_t runEncounterBaseBulletDamage_ = 0;
    bool prototypeResourceFocus_ = false;
    bool prototypeResourceTargetActive_ = false;
    PrototypeBossCombat prototypeCombat_{};
    bool expeditionRivalEnabled_ = false;
    RivalBossCombat rivalCombat_{};
    cg2::Vector3 rivalDashDirection_{0.0f, 1.0f, 0.0f};
    cg2::Vector3 rivalPathDirection_{};
    float rivalPathTimer_ = 0.0f;
    float rivalDashDistance_ = 0.0f;
    float rivalStrafeSign_ = 1.0f;
    int prototypePressure_ = 0;
    int prototypeBaseMaxHp_ = 200;
    int prototypeFeedingHealBudget_ = 100;
    int enemyLevel_ = 1;
    int expEnemyKillCount_ = 0;
    uint32_t enemyExp_ = 0;
    int alternatingShotIndex_ = 0;
    bool levelingModeActive_ = false;
    cg2::Vector3 currentMoveTargetPosition_{0.0f, 0.0f, 0.0f};

    cg2::Vector3 velocity_{0, 0, 0};
    cg2::Vector3 impactVelocity_{};

    float maxSpeed_ = 0.06f;
    float accel_ = 0.008f;
    float friction_ = 0.90f;
    cg2::Vector3 steeringDir_{1.0f, 0.0f, 0.0f};
    cg2::Vector3 steeringNoise_{0.0f, 0.0f, 0.0f};
    float steeringNoiseTimer_ = 0.0f;
    float hesitationTimer_ = 0.0f;
    float hesitationCooldown_ = 1.0f;

    std::unique_ptr<cg2::Sprite> bossHpFont;
    std::unique_ptr<cg2::Sprite> sprite;
    std::unique_ptr<cg2::Sprite> bossHpRed;

    int hp_ = 200;
    int maxHP_ = 200;
    cg2::Vector3 baseScale_{1.0f, 1.0f, 1.0f};
    cg2::Vector4 baseColor_{0.0f, 0.0f, 0.0f, 1.0f};
    float damageFeedbackTimer_ = 0.0f;
    float damageFeedbackDuration_ = 0.10f;

    bool isDead_ = false;

    bool isExploding_ = false;
    float deathEffectTimer_ = 0.0f;
    float deathChargeTimer_ = 0.0f;
    float deathChargeDuration_ = 0.28f;

    // 攻撃コントローラ
    AttackController attackController_;

    BulletManager* bulletManager_ = nullptr;

    // HPバーモデル
    cg2::Transform hpBarFillTransform_;
    cg2::Transform hpBarBGTransform_;

    std::unique_ptr<cg2::Object3d> hpBarFill_;
    std::unique_ptr<cg2::Object3d> hpBarBG_;

private:
    /// @brief ダメージフィードバックを発動させる。
    void TriggerDamageFeedback();
    /// @brief ダメージフィードバックを現在の状態へ適用する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void ApplyDamageFeedback(float deltaTime);
    /// @brief 死亡粒子を生成し、機体の膨張と消失を表示する時計を開始する。
    void SpawnParticles();
    /// @brief 死亡演出の時計を進め、時間切れでisExploding_を解除する。粒子自体はParticleManagerが更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateParticles(float deltaTime = 1.0f / 60.0f);
    const float deltaTime = 1.0f / 60.0f;
};
