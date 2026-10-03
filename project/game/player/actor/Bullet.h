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

/// @brief 弾と軌跡の色・幅・寿命・補間方法を指定する。有効状態のフラグは保持しない。
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
    /// @brief 射撃強化の有効状態と強さをそのまま保持する。いずれか有効なら遠征ルールも有効にする。
    void ConfigureShooterAbilities(bool chain, bool mark, bool boomerang, bool killBurst, float chainPower = 1, float markPower = 1,
                                   float boomerangPower = 1, float burstPower = 1);
    /// @brief シューター能力を返す。
    const ShooterAbilities& GetShooterAbilities() const
    {
        return shooter_;
    }
    /// @brief 命中元ドローンの添字と自機の借用先を設定する。
    /// @note indexの負値はドローン弾以外を表す。playerはnullptrを許容し、衝突通知で使う間は有効に保つ。
    void ConfigureDroneSource(int index, Player* player)
    {
        sourceDroneIndex_ = index;
        sourcePlayer_ = player;
    }
    /// @brief 命中元ドローンの添字を返す。負値はドローン弾以外を表す。
    int GetSourceDroneIndex() const
    {
        return sourceDroneIndex_;
    }
    /// @brief 命中元の自機の借用ポインターを返す。未設定ならnullptr。
    Player* GetSourcePlayer() const
    {
        return sourcePlayer_;
    }
    /// @brief 帰還中かを返す。
    bool GetIsReturning() const
    {
        return returnFlight_.returning;
    }
    /// @brief ブーメラン能力が有効かを返す。帰還中かとは別の判定。
    bool IsBoomerang() const
    {
        return shooter_.boomerang;
    }
    /// @brief 帰還先のワールド座標をコピーする。対象アクターを保持する処理ではない。
    void SetReturnTarget(const cg2::Vector3& target)
    {
        returnTarget_ = target;
    }
    /// @brief 装甲で反射された弾かを記録する。この関数では速度を反転しない。
    void SetArmorReflected(bool value)
    {
        armorReflected_ = value;
    }
    /// @brief 装甲による反射が既に行われた弾か判定する。
    bool WasArmorReflected() const
    {
        return armorReflected_;
    }
    /// @brief 撃破時の破裂で生まれた子弾かを返す。
    bool IsBurstChild() const
    {
        return burstChild_;
    }
    /// @brief 撃破時の破裂子弾であることを記録する。trueの弾からは破裂を再発動しない。
    void SetBurstChild(bool value)
    {
        burstChild_ = value;
    }
    /// @brief 弾の衝突半径と軌跡幅の倍率を設定する。各入力は0.5～2に制限する。
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
    /// @brief 特殊弾の種類・衝突半径・残り寿命を設定する。
    /// @param radius ワールド座標の半径。0.1～2.5に制限する。
    /// @param lifetime 残り秒数。0.05～kLifeTimeに制限する。
    void ConfigureSpecial(SpecialKind kind, float radius, float lifetime);
    /// @brief 特殊弾の種類を返す。
    SpecialKind GetSpecialKind() const
    {
        return specialKind_;
    }
    /// @brief 直前の移動更新で保存したワールド座標への読み取り専用参照を返す。
    const cg2::Vector3& GetPreviousWorldPosition() const
    {
        return previousPosition_;
    }
    /// @brief 壁反射とアクター貫通の、管理側が未回収の発生件数を保持する。
    struct GrowthEvents {
        uint32_t wallBounces = 0;
        uint32_t actorPierces = 0;
    };

    /// @brief 描画オブジェクトを生成し、位置・速度・所有者・ダメージと戦闘履歴を初期化する。
    /// @param velocity 60FPS相当の基準1フレームの移動量。
    /// @param bulletHp 弾の耐久度。最低0.1に補正する。
    /// @param bulletPenetration 他の弾の耐久度へ与える値。最低0.1に補正する。
    void Initialize(const cg2::Vector3& position, const cg2::Vector3& velocity, uint32_t damage, BulletOwner owner, bool reflectable,
                    float bulletHp = 1.0f, float bulletPenetration = 1.0f);

    /// @brief 前回位置を保存し、移動・帰還・残り寿命・外観・軌跡を更新する。地形衝突と削除は管理側が行う。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Update(float deltaTime);

    /// @brief 互換用の空の描画処理。現在、この関数は描画しない。
    void Draw();
    /// @brief 軌跡管理先から作成した軌跡と設定を借用する。管理先がnullptr、または取付済みなら何もしない。
    /// @note 所有権は移らない。ReleaseTrailまで軌跡を有効に保つ。設定のnullptrは既定の外観を使う。
    /// 設定の借用はReleaseTrail後も残るため、弾の実体を破棄するか、別の設定へ付け替えるまで有効に保つ。
    void AttachTrail(cg2::TrailManager* trailManager, BulletTrailSettings* trailSettings);
    /// @brief 軌跡を非アクティブにし、その借用ポインターを解除する。軌跡の実体や設定は破棄しない。
    void ReleaseTrail();

    /// @brief 死亡であるか判定する。
    bool IsDead() const
    {
        return isDead_;
    }

    /// @brief 成立した接触を受け、耐久度・命中履歴・貫通・分裂予約・死亡状態を更新する。
    /// @note otherはnullptr不可。接触判定と双方への通知はCollisionManagerが行う。
    void OnCollision(Collider* other) override;

    /// @brief 現在のワールド座標を値で返す。
    cg2::Vector3 GetWorldPosition() const override;

    /// @brief 60FPS相当の基準1フレームの移動量を値で返す。
    cg2::Vector3 GetMove() const
    {
        return velocity_;
    }

    /// @brief ワールド座標を設定し、描画オブジェクトへ反映する。移動前の記録は更新しない。
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

    /// @brief 壁反射を許可する設定かを返す。残り反射回数は別に判定する。
    bool IsReflectable() const
    {
        return isReflectable_;
    }
    /// @brief 所有者を返す。
    BulletOwner GetOwner() const
    {
        return owner_;
    }
    /// @brief 弾の現在の耐久度を返す。
    float GetBulletHp() const
    {
        return bulletHp_;
    }
    /// @brief 衝突した他の弾の耐久度へ与える値を返す。
    float GetBulletPenetration() const
    {
        return bulletPenetration_;
    }
    /// @brief 命中時に遠征の資源取得へ算入する設定かを返す。
    bool CanClaimRunResource() const
    {
        return canClaimRunResource_;
    }
    /// @brief 命中時に遠征の資源取得へ算入するかを設定する。
    void SetCanClaimRunResource(bool enabled)
    {
        canClaimRunResource_ = enabled;
    }
    /// @brief 弾耐久度ダメージを現在の状態へ適用する。
    void ApplyBulletDurabilityDamage(float amount);
    /// @brief 壁反射・アクター貫通・分裂の残り回数と分裂威力を補正して設定する。
    /// @param maxWallBounces -1は無制限。-1～32に制限する。
    /// @param actorPierceCount 命中後も飛行を続けられる回数。0～8に制限する。
    /// @param impactSplitCount 1回だけ予約する子弾数。0～2に制限する。
    /// @note 遠征ルールのフラグも上書きする。子弾の実生成はAppendImpactChildrenで行う。
    void ConfigureGrowth(int maxWallBounces, int actorPierceCount, int impactSplitCount, float impactSplitDamageScale = 0.55f);
    /// @brief 遠征用の弾の挙動規則を適用するか判定する。
    bool UsesRunProjectileRules() const
    {
        return usesRunProjectileRules_;
    }
    /// @brief actorがnullptrでなく、現在の命中履歴に衝突IDがなければtrueを返す。状態は変更しない。
    bool CanHitActor(const Collider* actor) const;
    /// @brief 壁衝撃の通知を受けて、このオブジェクトの状態を反映する。
    void OnWallImpact(const cg2::Vector3& safePosition, const cg2::Vector3& normal);
    /// @brief 保留した分裂を消費し、残り枠の範囲で生成した子弾をchildrenへ所有権付きで追加する。
    /// @note 衝突走査後に呼ぶ。枠不足でも予約を消費する。子弾は再分裂せず、親の残り寿命と命中履歴を引き継ぐ。
    void AppendImpactChildren(std::vector<std::unique_ptr<Bullet>>& children, size_t availableSlots);
    /// @brief 残りの壁反射回数を返す。-1は無制限。
    int GetRemainingWallBounces() const
    {
        return remainingWallBounces_;
    }
    /// @brief アクターへ命中後も飛行を続けられる残り回数を返す。
    int GetRemainingActorPierces() const
    {
        return remainingActorPierces_;
    }
    /// @brief 残り寿命（秒）を返す。
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

    /// @brief 死亡状態にして軌跡の借用を解除する。管理配列からの削除は行わない。
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
