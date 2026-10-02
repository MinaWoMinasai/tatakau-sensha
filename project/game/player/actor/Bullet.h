#pragma once
#include "game/weapon/CombatTypes.h"
#include <Windows.h>
#include <algorithm>
#include <vector>
#include "Calculation.h"
#include "Collider.h"
#include "CollisionConfig.h"
#include <Object3d.h>
#include "TrailManager.h"
#include "game/player/TankShooterAbilities.h"

class Player;

/// @brief 弾の軌跡の外観と有効状態を指定する。
struct BulletTrailSettings {
    float playerHalfWidth = 0.26f;
    float enemyHalfWidth = 0.22f;
    float lifetime = 0.24f;
    int maxPoints = 22;
    int interpolationSteps = 5;
    float headWidthScale = 1.0f;
    float tailWidthScale = 0.15f;
    float widthCurvePower = 1.0f;
    float colorCurvePower = 1.0f;
    bool useObjectColorForTrail = true;
    float trailHeadIntensity = 1.15f;
    float trailTailIntensity = 0.45f;
    float trailHeadAlpha = 1.0f;
    float trailTailAlpha = 0.0f;
    float playerTrailLifetimeScale = 1.0f;
    float playerTrailAlphaScale = 1.0f;
    cg2::Vector4 playerObjectColor = {1.0f, 0.78f, 0.28f, 1.0f};
    cg2::Vector4 enemyObjectColor = {1.0f, 0.22f, 0.38f, 1.0f};
    cg2::Vector4 reflectableObjectColor = {1.0f, 1.0f, 0.0f, 1.0f};
    cg2::Vector4 startColor = {1.0f, 0.98f, 0.78f, 1.0f};
    cg2::Vector4 playerEndColor = {1.0f, 0.55f, 0.20f, 0.0f};
    cg2::Vector4 enemyEndColor = {1.0f, 0.20f, 0.36f, 0.0f};
    cg2::Vector4 reflectableEndColor = {1.0f, 1.0f, 0.22f, 0.0f};
};

/// @brief 弾1発の移動・寿命・衝突・貫通・反射と軌跡を管理する。
class Bullet : public Collider {

public:
    enum class SpecialKind {
        None,
        Rail,
        SlashWave,
        ParryReflection,
        ArmorReflection
    };
    /// @brief 射撃系の強化効果を弾へ渡すためにまとめる。
    struct ShooterAbilities {
        bool chain = false, mark = false, boomerang = false, killBurst = false;
        float chainPower = 1, markPower = 1, boomerangPower = 1, burstPower = 1;
    };
    /// @brief シューター能力を利用条件を設定する。
    void ConfigureShooterAbilities(bool chain, bool mark, bool boomerang, bool killBurst, float chainPower = 1, float markPower = 1,
                                   float boomerangPower = 1, float burstPower = 1);
    /// @brief シューター能力を返す。
    const ShooterAbilities& GetShooterAbilities() const
    {
        return shooter_;
    }
    /// @brief ドローン元データを利用条件を設定する。
    void ConfigureDroneSource(int index, Player* player)
    {
        sourceDroneIndex_ = index;
        sourcePlayer_ = player;
    }
    /// @brief 元データドローン添字を返す。
    int GetSourceDroneIndex() const
    {
        return sourceDroneIndex_;
    }
    /// @brief 元データ自機を返す。
    Player* GetSourcePlayer() const
    {
        return sourcePlayer_;
    }
    /// @brief 状態帰還中を返す。
    bool GetIsReturning() const
    {
        return returnFlight_.returning;
    }
    /// @brief Boomerangであるか判定する。
    bool IsBoomerang() const
    {
        return shooter_.boomerang;
    }
    /// @brief 帰還対象を設定する。
    void SetReturnTarget(const cg2::Vector3& target)
    {
        returnTarget_ = target;
    }
    /// @brief 装甲Reflectedを設定する。
    void SetArmorReflected(bool value)
    {
        armorReflected_ = value;
    }
    /// @brief 装甲による反射が既に行われた弾か判定する。
    bool WasArmorReflected() const
    {
        return armorReflected_;
    }
    /// @brief 破裂Childであるか判定する。
    bool IsBurstChild() const
    {
        return burstChild_;
    }
    /// @brief 破裂Childを設定する。
    void SetBurstChild(bool value)
    {
        burstChild_ = value;
    }
    /// @brief 表示倍率を利用条件を設定する。
    void ConfigureVisualScale(float size, float trail)
    {
        radius_ = .5f * (std::clamp)(size, .5f, 2.0f);
        visualTrailScale_ = (std::clamp)(trail, .5f, 2.0f);
    }
    /// @brief 弾の特殊な命中によって発生した演出情報を表す。
    struct SpecialImpact {
        SpecialKind kind = SpecialKind::None;
        cg2::Vector3 position{}, direction{};
        bool bulletCut = false;
    };
    /// @brief 特殊命中イベントの未処理分を取り出し、内部の保留分を消費済みにする。
    std::vector<SpecialImpact> ConsumeSpecialImpacts()
    {
        auto events = std::move(specialImpacts_);
        specialImpacts_.clear();
        return events;
    }
    /// @brief 特殊を利用条件を設定する。
    void ConfigureSpecial(SpecialKind kind, float radius, float lifetime);
    /// @brief 特殊種類を返す。
    SpecialKind GetSpecialKind() const
    {
        return specialKind_;
    }
    /// @brief Previousワールド位置を返す。
    const cg2::Vector3& GetPreviousWorldPosition() const
    {
        return previousPosition_;
    }
    /// @brief 弾の反射・貫通・分裂で発生した成長判定用の件数を保持する。
    struct GrowthEvents {
        uint32_t wallBounces = 0;
        uint32_t actorPierces = 0;
    };

    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(const cg2::Vector3& position, const cg2::Vector3& velocity, uint32_t damage, BulletOwner owner, bool reflectable,
                    float bulletHp = 1.0f, float bulletPenetration = 1.0f);

    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Update(float deltaTime);

    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    void Draw();
    /// @brief 軌跡を取り付ける。
    void AttachTrail(cg2::TrailManager* trailManager, BulletTrailSettings* trailSettings);
    /// @brief 軌跡を解放する。
    void ReleaseTrail();

    /// @brief 死亡であるか判定する。
    bool IsDead() const
    {
        return isDead_;
    }

    /// @brief 衝突判定
    void OnCollision(Collider* other) override;

    // ワールド座標を取得
    cg2::Vector3 GetWorldPosition() const override;

    /// @brief 移動を返す。
    cg2::Vector3 GetMove() const
    {
        return velocity_;
    }

    // セッター
    void SetWorldPosition(const cg2::Vector3& pos)
    {
        worldTransform_.translate = pos;
        object_->SetTransform(worldTransform_);
        object_->Update();
    }

    /// @brief 速度を設定する。
    void SetVelocity(const cg2::Vector3& v)
    {
        velocity_ = v;
    }

    /// @brief 半径を返す。
    float GetRadius() const override
    {
        return radius_;
    }

    /// @brief Reflectableであるか判定する。
    bool IsReflectable() const
    {
        return isReflectable_;
    }
    /// @brief 所有者を返す。
    BulletOwner GetOwner() const
    {
        return owner_;
    }
    /// @brief 弾HPを返す。
    float GetBulletHp() const
    {
        return bulletHp_;
    }
    /// @brief 弾Penetrationを返す。
    float GetBulletPenetration() const
    {
        return bulletPenetration_;
    }
    /// @brief 取得遠征リソースが可能か判定する。
    bool CanClaimRunResource() const
    {
        return canClaimRunResource_;
    }
    /// @brief Can取得遠征リソースを設定する。
    void SetCanClaimRunResource(bool enabled)
    {
        canClaimRunResource_ = enabled;
    }
    /// @brief 弾耐久度ダメージを現在の状態へ適用する。
    void ApplyBulletDurabilityDamage(float amount);
    /// @brief 成長を利用条件を設定する。
    void ConfigureGrowth(int maxWallBounces, int actorPierceCount, int impactSplitCount, float impactSplitDamageScale = 0.55f);
    /// @brief 遠征用の弾の挙動規則を適用するか判定する。
    bool UsesRunProjectileRules() const
    {
        return usesRunProjectileRules_;
    }
    /// @brief 命中アクターが可能か判定する。
    bool CanHitActor(const Collider* actor) const;
    /// @brief 壁衝撃の通知を受けて、このオブジェクトの状態を反映する。
    void OnWallImpact(const cg2::Vector3& safePosition, const cg2::Vector3& normal);
    // Drain only outside collision iteration. Children cannot split again and
    // share the parent's expiry time and actor hit history.
    /// @brief 衝撃子要素を末尾へ追加する。
    void AppendImpactChildren(std::vector<std::unique_ptr<Bullet>>& children, size_t availableSlots);
    /// @brief 残り壁Bouncesを返す。
    int GetRemainingWallBounces() const
    {
        return remainingWallBounces_;
    }
    /// @brief 残りアクターPiercesを返す。
    int GetRemainingActorPierces() const
    {
        return remainingActorPierces_;
    }
    /// @brief 残りLifetimeを返す。
    float GetRemainingLifetime() const
    {
        return deathTimer_;
    }
    /// @brief 成長イベントの未処理分を取り出し、内部の保留分を消費済みにする。
    GrowthEvents ConsumeGrowthEvents()
    {
        const auto events = growthEvents_;
        growthEvents_ = {};
        return events;
    }

    /// @brief 死亡状態にし、以降の攻撃・衝突などの対象から外す。
    void Die();

private:
    /// @brief 表示設定を現在の状態へ適用する。
    void ApplyVisualSettings();
    /// @brief 軌跡を更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateTrail(float deltaTime);
    /// @brief 軌跡設定を作成して返す。
    cg2::TrailConfig MakeTrailConfig() const;
    /// @brief 弾色を返す。
    cg2::Vector4 GetBulletColor() const;
    /// @brief 衝撃分裂を後で処理するために予約する。
    void QueueImpactSplit(const cg2::Vector3& direction);
    /// @brief 帰還を開始する。
    void BeginReturn();
    ShooterAbilities shooter_{};
    tankshooter::ReturnFlight returnFlight_{};
    cg2::Vector3 returnTarget_{};
    Player* sourcePlayer_ = nullptr;
    int sourceDroneIndex_ = -1;
    bool armorReflected_ = false, burstChild_ = false;
    float visualTrailScale_ = 1;

    std::unique_ptr<cg2::Object3d> object_;

    // ワールドトランスフォーム
    cg2::Transform worldTransform_;

    // 速度
    cg2::Vector3 velocity_;
    cg2::Vector3 previousPosition_{};
    SpecialKind specialKind_ = SpecialKind::None;
    std::vector<SpecialImpact> specialImpacts_;

    // 寿命
    const float kLifeTime = 3.0f;
    // デスタイマー
    float deathTimer_ = kLifeTime;
    // デスフラグ
    bool isDead_ = false;

    // キャラクターの当たり判定サイズ
    static inline const float kWidth = 1.6f;
    static inline const float kHeight = 1.6f;

    float radius_ = 0.5f;

    // 反射するか
    bool isReflectable_ = false;
    BulletOwner owner_;
    float bulletHp_ = 1.0f;
    float bulletPenetration_ = 1.0f;
    bool canClaimRunResource_ = true;
    bool usesRunProjectileRules_ = false;
    int remainingWallBounces_ = -1;
    int remainingActorPierces_ = 0;
    int impactSplitCount_ = 0;
    int pendingImpactSplitCount_ = 0;
    float impactSplitDamageScale_ = 0.55f;
    cg2::Vector3 pendingImpactDirection_{};
    cg2::Vector3 pendingImpactPosition_{};
    cg2::Vector3 pendingImpactWallNormal_{};
    std::vector<uint64_t> hitActorIds_;
    GrowthEvents growthEvents_{};
    cg2::TrailInstance* trail_ = nullptr;
    BulletTrailSettings* trailSettings_ = nullptr;
};
