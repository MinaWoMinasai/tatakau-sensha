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

/// @brief ボスの姿勢・攻撃・HP・描画を管理し、戦闘の状態に応じて行動する。
class Enemy : public Collider {

public:
    /// @brief ボスの攻撃間隔・弾・行動を調整する設定を表す。
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
    /// @brief 敵の進行段階ごとの性能と解放条件を指定する。
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

    /// @brief デストラクタ
    ~Enemy() override;

    /// @brief 初期化
    void Initialize(cg2::Object3d* object, const cg2::Vector3& position, Stage* stage);

    /// @brief 更新
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Update(float deltaTime);

    /// @brief 描画
    void Draw(bool drawBody = true);
    /// @brief 機体専用を描画する。
    void DrawBodyOnly();

    /// @brief スプライト描画
    void DrawSprite();

    /// @brief 弾発射
    void Fire();

    /// @brief 散弾発射
    void ShotgunFire();

    /// @brief
    /// @brief プレイヤーへ近づく移動方向を求める。
    void ApproachToPlayer(cg2::Vector3& startPos, cg2::Vector3& targetPos);

    // 状態クラス用 Getter/Setter
    const cg2::Transform& GetWorldTransform() const
    {
        return worldTransform_;
    }

    /// @brief ワールド座標での位置を返す。
    cg2::Vector3 GetWorldPosition() const override;

    // 自キャラのセッター
    void SetPlayer(Player* player)
    {
        player_ = player;
    }
    /// @brief 敵管理を設定する。
    void SetEnemyManager(EnemyManager* enemyManager)
    {
        enemyManager_ = enemyManager;
    }

    // セッター
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

    /// @brief 衝突判定
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

    /// @brief 移動の傾向
    void AIStateMovePower();

    /// @brief ステートによる移動
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Move(float deltaTime);
    /// @brief 乱数で移動方向を選ぶ。
    cg2::Vector3 RandomDirection();

    /// @brief 接近する弾を避ける移動方向を求める。
    cg2::Vector3 EvadeBullets();
    /// @brief AI状態を更新する。
    void UpdateAIState();

    /// @brief AABBを返す。
    cg2::AABB GetAABB();

    /// @brief Dirを返す。
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

    /// @brief Forward射線を作成して返す。
    cg2::Segment MakeForwardRay(float length) const;

    /// @brief 地形ブロック近距離By射線であるか判定する。
    bool IsBlockNearByRay();

    /// @brief 射線で壁を調べ、壁を避ける方向を求める。
    cg2::Vector3 WallAvoidByRay();

    /// @brief 候補の移動方向の安全性と目的への適合度を評価する。
    float ScoreDir(const cg2::Vector3& dir);

    /// @brief 射線への自機を作成して返す。
    cg2::Segment MakeRayToPlayer() const;

    /// @brief 射線がプレイヤーへ到達するか判定する。
    bool HitPlayerByRay(const cg2::Segment& ray);

    /// @brief 線OfSightへの自機が存在するか判定する。
    bool HasLineOfSightToPlayer() const;
    /// @brief 線OfSightへの対象が存在するか判定する。
    bool HasLineOfSightToTarget(const cg2::Vector3& targetPos) const;

    /// @brief 移動を返す。
    cg2::Vector3 GetMove()
    {
        return velocity_;
    }

    /// @brief 死亡状態にし、以降の攻撃・衝突などの対象から外す。
    void Die(); // ← プレイヤー消滅

    // 演出終了か
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
    /// @brief 最大値HPを返す。
    int GetMaxHp() const
    {
        return maxHP_;
    }
    /// @brief 撃破画面演出を更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateDefeatPresentation(float deltaTime);
    /// @brief 受けたダメージをHPなどの戦闘状態へ反映する。
    void TakeDamage(uint32_t amount);
    /// @brief Knockbackを現在の状態へ適用する。
    void ApplyKnockback(const cg2::Vector3& direction, float power);
    /// @brief ボス攻撃設定を設定する。
    void SetBossAttackConfig(const BossAttackConfig& config);
    /// @brief ボス攻撃設定を返す。
    const BossAttackConfig& GetBossAttackConfig() const
    {
        return bossAttackConfig_;
    }
    /// @brief 敵進行度設定を設定する。
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
    /// @brief Leveling方式有効であるか判定する。
    bool IsLevelingModeActive() const
    {
        return levelingModeActive_;
    }
    /// @brief 経験値敵撃破を登録する。
    void RegisterExpEnemyKill(uint32_t expValue);

    using PrototypeAttackType = PrototypeBossCombat::AttackType;
    /// @brief 予備動作で示す攻撃方向・範囲・進行を保持する。
    struct PrototypeTelegraph {
        bool active = false;
        PrototypeAttackType attackType = PrototypeAttackType::AimedSpread;
        cg2::Vector3 direction{1.0f, 0.0f, 0.0f};
        float progress = 0.0f;
        // GapRing: width of the safe opening around direction. Others: danger cone.
        float spreadAngleDeg = 44.0f;
    };
    /// @brief 試作戦闘を有効にする。
    void EnablePrototypeCombat(bool enabled);
    /// @brief 試作戦闘有効であるか判定する。
    bool IsPrototypeCombatEnabled() const
    {
        return prototypeCombatEnabled_;
    }
    /// @brief 試作Pressureを設定する。
    void SetPrototypePressure(int pressure)
    {
        prototypePressure_ = (std::clamp)(pressure, 0, 4);
    }
    /// @brief 試作最大値HPを設定する。
    void SetPrototypeMaxHp(int maxHp, bool healToFull = true);
    /// @brief 試作攻撃調整値を設定する。
    void SetPrototypeAttackTuning(const BossAttackConfig& config)
    {
        SetBossAttackConfig(config);
        prototypeTuningEnabled_ = true;
    }
    /// @brief 試作リソース注目点を設定する。
    void SetPrototypeResourceFocus(bool enabled)
    {
        prototypeResourceFocus_ = enabled;
    }
    /// @brief 遠征リソース取得を登録する。
    void RegisterRunResourceClaim();
    /// @brief 試作攻撃予告を返す。
    PrototypeTelegraph GetPrototypeTelegraph() const;
    /// @brief 遠征敵出現有効を設定する。
    void SetRunEncounterEnabled(bool enabled);
    /// @brief 遠征敵出現有効であるか判定する。
    bool IsRunEncounterEnabled() const
    {
        return runEncounterEnabled_;
    }
    // Call after Initialize, outside actor updates/collision callbacks.
    /// @brief 遠征敵出現を初期状態へ戻す。
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
    // Opt in after ResetRunEncounter. Arena and the title demo retain their AI.
    /// @brief 遠征ライバルを有効にする。
    void EnableExpeditionRival(bool enabled);
    /// @brief 遠征ライバル有効であるか判定する。
    bool IsExpeditionRivalEnabled() const
    {
        return expeditionRivalEnabled_;
    }
    /// @brief ライバル戦闘状態を返す。
    RivalCombatStatus GetRivalCombatStatus() const;

    /// @brief 攻撃制御弾管理を設定する。
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

    /// @brief パス方向への自機を検索する。
    std::optional<cg2::Vector3> FindPathDirectionToPlayer();
    /// @brief パス方向への対象を検索する。
    std::optional<cg2::Vector3> FindPathDirectionToTarget(const cg2::Vector3& targetPos);
    /// @brief ワールド座標をマップの行・列へ変換する。
    std::optional<MapIndex> WorldToMapIndex(const cg2::Vector3& pos) const;
    /// @brief マップの行・列をワールド座標へ変換する。
    cg2::Vector3 MapIndexToWorld(const MapIndex& index) const;
    /// @brief 通行可能セルであるか判定する。
    bool IsPassableCell(int x, int y) const;
    /// @brief パス通行可能セルであるか判定する。
    bool IsPathPassableCell(int x, int y) const;
    /// @brief 最寄りパス通行可能セルを検索する。
    MapIndex FindNearestPathPassableCell(const MapIndex& base) const;
    /// @brief 消去移動進路への自機が存在するか判定する。
    bool HasClearMoveRouteToPlayer() const;
    /// @brief 消去移動進路への対象が存在するか判定する。
    bool HasClearMoveRouteToTarget(const cg2::Vector3& targetPos) const;
    /// @brief HumanLikeSteeringを現在の状態へ適用する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    cg2::Vector3 ApplyHumanLikeSteering(const cg2::Vector3& desiredDir, bool usingPath, float deltaTime);
    /// @brief 移動対象位置を解決する。
    cg2::Vector3 ResolveMoveTargetPosition();
    /// @brief 目標方向へ向けて姿勢を回転する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void RotateTowardTarget(const cg2::Vector3& targetPos, float deltaTime);
    /// @brief 試作戦闘を更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdatePrototypeCombat(float deltaTime);
    /// @brief ライバル戦闘を更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateRivalCombat(float deltaTime);
    /// @brief ライバルダッシュ方向を選択する。
    void SelectRivalDashDirection(const cg2::Vector3& towardPlayer);
    /// @brief ライバル移動を解決する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    cg2::Vector3 ResolveRivalMove(const cg2::Vector3& desired, const cg2::Vector3& towardPlayer, float deltaTime);
    /// @brief 捕食による回復量をHPへ反映する。
    void HealFromFeeding(int amount);
    /// @brief Feedingレベルを進める。
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

    // 射撃感覚タイマー
    float kFireTimerMax_ = 0.15f;
    float fireTimer_ = 0.0f;
    BossAttackConfig bossAttackConfig_{};
    EnemyProgressConfig enemyProgressConfig_{};
    bool prototypeCombatEnabled_ = false;
    bool prototypeTuningEnabled_ = false;
    bool runEncounterEnabled_ = true;
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
    /// @brief 粒子を出現させる。
    void SpawnParticles();
    /// @brief 粒子を更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateParticles(float deltaTime = 1.0f / 60.0f);
    const float deltaTime = 1.0f / 60.0f;
};
