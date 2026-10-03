#pragma once
#include "Collider.h"
#include "Object3d.h"
#include "AttackController.h"
#include "ExpEnemyCombatCycle.h"
#include "ExpEnemyMagazineCycle.h"
#include "ExpGuardCombat.h"
#include <functional>
#include "game/run/TankExpeditionContent.h"

class Player;
class Stage;
class Enemy;
namespace cg2 {
class NeonGridRenderer;
}

enum class ExpEnemyType {
    Square,
    Triangle,
    Pentagon,
    Shooter,
    Charger,
    Sniper,
    Skirmisher,
    Flanker,
    Suppressor,
    ShieldGuard,
    BladeGuard,
    SummonerCommander,
    EMPJammer,
    ReflectArmor,
};

/// @brief 経験値を与える図形・戦闘敵と遠征資源の1体分のHP・移動・攻撃・表示を管理し、描画オブジェクトを所有する。
class ExpEnemy : public Collider {
public:
    /// @brief 接触ダメージと射撃・索敵の共通設定を保持する。間隔は秒、弾速は60FPS基準の1フレーム当たりの距離。
    struct BalanceConfig {
        uint32_t contactDamage = 12;
        uint32_t shooterContactDamage = 20;
        uint32_t shooterBulletDamage = 15;
        float shooterDetectionRadius = 18.0f;
        float shooterTurnSpeed = 5.5f;
        float shooterFireInterval = 1.25f;
        float shooterBulletSpeed = 0.20f;
    };
    /// @brief 経験値敵がボスを敵対対象に含めるかを指定する。
    struct EnemyInteractionConfig {
        bool hostileToBoss = false;
    };

    /// @brief 全ExpEnemyで共有する性能設定を更新し、ダメージ・範囲・速度・間隔に実装上の下限を適用する。
    static void SetBalanceConfig(const BalanceConfig& config);
    /// @brief 全ExpEnemyのボスへの敵対設定を更新する。既存個体の衝突マスク更新は別途必要。
    static void SetEnemyInteractionConfig(const EnemyInteractionConfig& config);
    /// @brief 通常の経験値敵が敵側に撃破されたとき、経験値量を渡す共有コールバックを設定する。
    /// @note 空の関数で解除できる。TakeDamageFromEnemy経由の撃破通知は呼び出し側が担当する。
    static void SetEnemyKillCallback(std::function<void(uint32_t)> callback);
    /// @brief 自機側が通常の経験値敵を撃破したとき、その敵の位置を渡す共有コールバックを設定する。
    /// @note 自機自身の死亡通知ではない。資源・召喚個体は対象外で、空の関数で解除できる。
    static void SetPlayerDefeatCallback(std::function<void(const cg2::Vector3&)> callback);
    /// @brief 図形のネオン表示を切り替え、有効なら描画方式1、無効なら方式0にする。
    static void SetShapeNeonBillboardEnabled(bool enabled);
    /// @brief 図形のネオン描画方式を0〜3に制限して設定する。0以外では通常のモデル描画を省く。
    static void SetShapeNeonRenderMode(int mode);
    /// @brief 経験値敵がボスへ敵対する共有設定を返す。
    static bool IsHostileToBoss()
    {
        return enemyInteractionConfig_.hostileToBoss;
    }

    /// @brief 指定位置・種類で描画オブジェクト、HP、攻撃周期、衝突属性を初期化する。
    /// @param player 照準と経験値付与に使う借用先。nullptrなら自機を参照する処理を省く。
    /// @note playerはこの個体が参照する間有効に保つ。すべての既存タイマーや速度を消去する処理ではない。
    void Initialize(const cg2::Vector3& position, Player* player, ExpEnemyType type = ExpEnemyType::Square);
    /// @brief 制作定義のHP・ダメージ・移動/射撃倍率・報酬・色を制限して適用する。種類と位置は変更しない。
    void ApplyAuthoredDefinition(const tankcontent::Enemy& definition);
    /// @brief 制作定義または召喚個体用の性能が適用されているかを返す。
    bool HasAuthoredDefinition() const
    {
        return hasAuthoredDefinition_;
    }
    /// @brief 寿命・演出を進め、資源、移動戦闘敵、図形/Shooterごとの移動と攻撃を更新する。
    /// @note 生存個体を呼び出す前提。死亡済み個体の除外・実体の削除はEnemyManagerが担当する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Update(Stage& stage, float deltaTime);
    /// @brief drawBodyがtrueならDrawBodyOnlyを呼ぶ。ネオンの線表示はシーン側の別描画で扱う。
    void Draw(bool drawBody = true);
    /// @brief 通常モデルを描画する。ネオン対象では描画方式が0以外なら省く。
    void DrawBodyOnly();
    /// @brief 描画方式3では、色・照明・環境光係数を一時変更して黒いモデルを描画し、設定を復元する。
    void DrawNeonFillBodyOnly();

    /// @brief 成立した接触から速度・HP・撃破通知を更新する。死亡済み個体と資源の接触対象外経路は早期終了する。
    /// @param other 成立した衝突相手。nullptr不可で、通知中は有効な借用先。
    void OnCollision(Collider* other) override;

    /// @brief 現在のワールド位置を値で返す。
    cg2::Vector3 GetWorldPosition() const override
    {
        return worldTransform_.translate;
    }
    /// @brief 球の衝突半径を返す。資源は1.2、それ以外は0.8ワールド単位。
    float GetRadius() const override
    {
        return isRunResource_ ? 1.2f : 0.8f;
    }

    /// @brief 射撃で使うBulletManagerを借用する。発射時には有効な管理先が必要。
    void SetAttackControllerBulletManager(BulletManager* bulletManager)
    {
        attackController_.SetBulletManager(bulletManager);
    }
    /// @brief Shooterが敵対設定に従って照準するボスを借用する。nullptrで対象を外す。
    void SetBossTarget(Enemy* boss)
    {
        boss_ = boss;
    }

    /// @brief ワールド位置を設定し、描画オブジェクトの変換も更新する。初期化後に呼ぶ。
    void SetWorldPosition(const cg2::Vector3& pos)
    {
        worldTransform_.translate = pos;
        object_->SetTransform(worldTransform_);
        object_->Update();
    }

    /// @brief 中心から各軸±0.8のAABBを返す。資源の球半径や表示倍率による拡大は含めない。
    cg2::AABB GetAABB();

    /// @brief 死亡または召喚解除のフラグを返す。実体の削除はEnemyManager::UpdateまたはClearが行う。
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
        return maxHp_;
    }
    /// @brief 生成用に現在HPと最大HPを同じ値へ設定する。1未満は1にし、死亡フラグや報酬は処理しない。
    void SetHp(int hp)
    {
        hp_ = maxHp_ = (std::max)(1, hp);
    }
    /// @brief 個体を固定位置の遠征資源にし、移動速度・接触ダメージを0へ、表示と衝突マスクを資源用へ変更する。
    /// @param onClaim 撃破時に取得側を渡すコールバック。trueは自機側、falseは敵側。空でもよい。
    /// @note HPと死亡フラグは変更しない。
    void SetRunResource(std::function<void(bool playerOwned)> onClaim);
    /// @brief 遠征の取得用資源として設定されているかを返す。
    bool IsRunResource() const
    {
        return isRunResource_;
    }
    /// @brief 生存中の資源以外のShooterまたは移動戦闘役であるかを返す。
    bool IsCombatThreat() const
    {
        return !isDead_ && !isRunResource_ && (type_ == ExpEnemyType::Shooter || IsExpeditionCombatRole());
    }
    /// @brief 種類がCharger〜ReflectArmorの移動戦闘役に含まれるかを返す。
    bool IsExpeditionCombatRole() const
    {
        return type_ >= ExpEnemyType::Charger && type_ <= ExpEnemyType::ReflectArmor;
    }
    /// @brief 召喚指揮官またはEMP妨害役であるかを返す。
    bool IsSupportRole() const
    {
        return type_ == ExpEnemyType::SummonerCommander || type_ == ExpEnemyType::EMPJammer;
    }
    /// @brief 種類に対応する攻撃周期の段階を返す。支援役では予告中がLocked、それ以外はCooldown。
    ExpEnemyCombatPhase GetCombatPhase() const
    {
        return IsSupportRole() ? (supportPulse_.IsWarning() ? ExpEnemyCombatPhase::Locked : ExpEnemyCombatPhase::Cooldown)
               : type_ == ExpEnemyType::BladeGuard ? bladeCycle_.GetPhase()
               : type_ == ExpEnemyType::Charger    ? combatCycle_.GetPhase()
                                                   : magazineCycle_.GetPhase();
    }
    /// @brief 種類に対応する攻撃予告の0〜1の進行度を返す。
    float GetAttackTelegraphRatio() const
    {
        return IsSupportRole()                     ? supportPulse_.WarningRatio()
               : type_ == ExpEnemyType::BladeGuard ? bladeCycle_.WarningRatio()
               : type_ == ExpEnemyType::Charger    ? combatCycle_.GetWarningRatio()
                                                   : magazineCycle_.GetWarningRatio();
    }
    /// @brief 種類に対応する周期が照準を固定する段階かを返す。
    bool IsAttackAimLocked() const
    {
        return IsSupportRole()                     ? supportPulse_.IsWarning()
               : type_ == ExpEnemyType::BladeGuard ? bladeCycle_.IsCommitted()
               : type_ == ExpEnemyType::Charger    ? combatCycle_.IsAimLocked()
                                                   : magazineCycle_.IsAimLocked();
    }
    /// @brief 弾倉を使う移動戦闘役が再装填中かを返す。
    bool IsReloading() const
    {
        return IsExpeditionCombatRole() && !IsSupportRole() && type_ != ExpEnemyType::Charger && type_ != ExpEnemyType::BladeGuard &&
               magazineCycle_.IsReloading();
    }
    /// @brief 弾倉の残り斉射回数を返す。支援役・Charger・BladeGuardでは0。
    int GetAmmoRemaining() const
    {
        return IsSupportRole() || type_ == ExpEnemyType::Charger || type_ == ExpEnemyType::BladeGuard ? 0 : magazineCycle_.GetAmmo();
    }
    /// @brief 短い移動ダッシュ中、またはChargerの攻撃実行中かを返す。
    bool IsDashing() const
    {
        return dashTimer_ > 0 || (type_ == ExpEnemyType::Charger && combatCycle_.GetPhase() == ExpEnemyCombatPhase::Active);
    }
    /// @brief 移動戦闘役の射撃を呼び出した回数を返す。通常のShooterと弾の登録成否は集計しない。
    uint32_t GetCombatShotsFired() const
    {
        return combatShotsFired_;
    }
    /// @brief 移動戦闘役の突進・短いダッシュを開始した回数を返す。
    uint32_t GetCombatDashCount() const
    {
        return combatDashCount_;
    }
    /// @brief 弾倉の再装填を開始した回数を返す。ResetMagazineで0になる。
    uint32_t GetCombatReloadCount() const
    {
        return magazineCycle_.GetReloadCount();
    }
    /// @brief 正面盾の減衰または反射装甲の受付が成立した回数を返す。地形との接触回数ではない。
    uint32_t GetShieldBlockCount() const
    {
        return shieldBlockCount_;
    }
    /// @brief BladeGuardの斬撃を開始した回数を返す。命中回数とは異なる。
    uint32_t GetBladeSwingCount() const
    {
        return bladeCycle_.SwingCount();
    }
    /// @brief 壁衝撃の受付時間内に、外部衝撃速度を持って地形へ当たった回数を返す。単なる全接触回数ではない。
    uint32_t GetWallCollisionCount() const
    {
        return wallCollisionCount_;
    }
    /// @brief 生存中のReflectArmorへの正面入射を判定し、成立時に受付回数と装甲の演出を更新する。
    /// @param attackSource 攻撃が来た側のワールド位置。
    /// @return 正面の反射受付が成立した場合true。弾の生成・追加予約は呼び出し側が行う。
    bool TryReflectProjectile(const cg2::Vector3& attackSource);
    /// @brief 反射装甲の受付が成立した回数を返す。反射弾の登録成功数ではない。
    uint32_t GetReflectionCount() const
    {
        return reflectionCount_;
    }
    /// @brief EMPパルスを発動した回数を返す。範囲外・遮蔽で自機に適用されない発動も含む。
    uint32_t GetEmpPulseCount() const
    {
        return empPulseCount_;
    }
    /// @brief 召喚要求の有無を返して保留フラグを消去する。複数の要求を件数として蓄積しない。
    bool ConsumeSummonRequest()
    {
        const bool request = summonRequested_;
        summonRequested_ = false;
        return request;
    }
    /// @brief この指揮官から生成した召喚個体の累計を返す。死亡後も減らさない。
    int GetSummonTotal() const
    {
        return summonTotal_;
    }
    /// @brief 召喚個体の生成累計を1増やす。個体の生成・登録はEnemyManagerが行う。
    void RecordSummonedUnit()
    {
        ++summonTotal_;
    }
    /// @brief 指揮官の衝突IDを持つ期限付き召喚個体へ設定し、HP・性能・表示を変更して撃破報酬を0にする。
    /// @param commanderId 召喚元の非0の衝突ID。0ではIsSummonedUnitがfalseになる。
    void ConfigureSummonedUnit(uint64_t commanderId);
    /// @brief 召喚元の衝突IDを返す。0は召喚個体ではないことを表す。
    uint64_t GetSummonerId() const
    {
        return summonerId_;
    }
    /// @brief 非0の召喚元IDを持つ個体かを返す。
    bool IsSummonedUnit() const
    {
        return summonerId_ != 0;
    }
    /// @brief 召喚個体ならHPを0にして死亡フラグを立てる。撃破演出・報酬通知や実体の削除は行わない。
    void DismissSummonedUnit()
    {
        if (IsSummonedUnit()) {
            isDead_ = true;
            hp_ = 0;
        }
    }
    /// @brief 照準方向を返す。
    const cg2::Vector3& GetAimDirection() const
    {
        return aimDirection_;
    }
    /// @brief 地形で切り詰めた攻撃予告の終点を借用参照で返す。位置は更新時に変わる。
    const cg2::Vector3& GetTelegraphEnd() const
    {
        return telegraphEnd_;
    }
    /// @brief 生存中の移動戦闘役の輪郭・攻撃予告・残弾などを、シーンが使うネオン描画先へ予約する。
    void QueueCombatVisuals(cg2::NeonGridRenderer& renderer, const cg2::Vector3& cameraRight, const cg2::Vector3& cameraUp,
                            const cg2::Vector3& cameraForward, float lineWidth) const;
    /// @brief 借用する自機の位置を攻撃元として、自機側ダメージを適用する。自機がなければ中心からの攻撃として扱う。
    /// @return この呼び出しで撃破した場合true。falseでもHPや被弾演出が変わる場合がある。
    bool TakeDamageFromPlayer(uint32_t amount);
    /// @brief 攻撃元とmeleeに応じた正面盾の減衰後、自機側ダメージをHPへ適用する。
    /// @param attackSource 攻撃が来た側のワールド位置。
    /// @param melee trueなら盾正面で近接用の減衰率を使う。
    /// @return この呼び出しで撃破した場合true。死亡済み・0ダメージ・生存継続ならfalse。
    /// @note OnCollisionの自機接触用の無敵タイマーは、この経路では判定しない。
    bool TakeDirectionalDamage(uint32_t amount, const cg2::Vector3& attackSource, bool melee = false);
    /// @brief 方向を正規化して外部衝撃速度へ加算する。位置の移動はUpdateが行う。
    /// @param power 60FPS基準の速度への加算量。正の有限値のみ受け付け、加算量と合成速度に上限0.95を設ける。
    /// @note 死亡・資源・ほぼ0の方向は対象外。0.16以上で壁衝撃を1秒受け付け、0.30以上でダッシュを中断する。
    void ApplyKnockback(const cg2::Vector3& direction, float power);
    /// @brief 敵側ダメージをHPへ適用し、新たな撃破ならtrueを返す。falseでも非致死ダメージは適用される。
    /// @note 資源の取得通知は行うが、通常敵の撃破通知は呼び出し側が担当する。
    bool TakeDamageFromEnemy(uint32_t amount);
    /// @brief 衝突判定マスクを最新の内容へ更新する。
    void RefreshCollisionMask();

    /// @brief 種類または制作定義に基づく撃破時の経験値量を返す。召喚個体は0。
    uint32_t GetExpValue() const
    {
        return expValue_;
    }
    /// @brief 種類を返す。
    ExpEnemyType GetType() const
    {
        return type_;
    }
    /// @brief 表示色を返す。
    const cg2::Vector4& GetVisualColor() const
    {
        return visualColor_;
    }
    /// @brief 表示用のZ軸回転角（ラジアン）を返す。
    float GetVisualRotation() const
    {
        return worldTransform_.rotate.z;
    }
    /// @brief 表示用の各軸回転角（ラジアン）を借用参照で返す。
    const cg2::Vector3& GetVisualRotate() const
    {
        return worldTransform_.rotate;
    }
    /// @brief 表示倍率を返す。
    const cg2::Vector3& GetVisualScale() const
    {
        return worldTransform_.scale;
    }
    /// @brief シューター予告比率を返す。
    float GetShooterWarningRatio() const
    {
        return type_ == ExpEnemyType::Shooter ? (std::clamp)(shooterWarningRatio_, 0.0f, 1.0f) : 0.0f;
    }
    /// @brief シューター銃口フラッシュ比率を返す。
    float GetShooterMuzzleFlashRatio() const
    {
        return type_ == ExpEnemyType::Shooter && kShooterMuzzleFlashDuration > 0.0f
                   ? (std::clamp)(shooterMuzzleFlashTimer_ / kShooterMuzzleFlashDuration, 0.0f, 1.0f)
                   : 0.0f;
    }
    /// @brief 種類が図形/Shooterまたは移動戦闘役かを返す。描画方式や死亡状態は判定しない。
    bool IsShapeNeonBillboardTarget() const;
    /// @brief 種類がネオン表示対象かを返す。描画方式の有効化は別途確認する。
    bool IsShapeNeonRenderTarget() const
    {
        return IsShapeNeonBillboardTarget();
    }

private:
    /// @brief HPを0まで減らし、致死なら死亡フラグ・演出・取得側に応じた報酬を1回処理する。
    /// @return 新たに撃破した場合true。死亡済み・0ダメージ・生存継続ではfalse。
    bool ApplyDamage(uint32_t amount, bool playerOwned, bool reportOrdinaryEnemyKill);
    /// @brief 種類ごとのモデル・色・HP・経験値・接触ダメージを設定する。
    void ApplyTypeParams();
    /// @brief 被弾表示の残り時間を設定する。HPや無敵時間は変更しない。
    void TriggerDamageFeedback();
    /// @brief 被弾表示の残り秒数を進め、その割合の二乗に応じた拡大と白色への補間を適用する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void ApplyDamageFeedback(float deltaTime);
    /// @brief 移動戦闘役の照準・攻撃周期・自律移動・外部衝撃を更新する。進める秒数は0〜0.20へ制限する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateExpeditionCombat(Stage& stage, float deltaTime);
    /// @brief 種類に応じた弾倉を満タンへ戻し、周期と再装填回数を初期化する。正の引数だけ既定値へ上書きする。
    void ResetMagazine(int rounds = 0, float reloadSeconds = 0.0f);
    /// @brief 変位を細分化し、各回のX、Yの順で位置設定と地形補正を行う。
    /// @return どちらかの軸で0.005を超える補正が生じた場合true。
    /// @note Z成分の移動は行わない。壁衝撃の受付条件を満たした接触では回数も更新する。
    bool MoveCombatActor(Stage& stage, const cg2::Vector3& displacement);
    /// @brief 障害物を0.85広げた2単位の格子で次の経由セルを探し、ワールド位置へ戻す。経路なしなら現在位置。
    cg2::Vector3 FindCombatWaypoint(Stage& stage, const cg2::Vector3& target) const;
    /// @brief origin + direction * distanceまでのXY射線を、最初に触れる有効ブロックのAABBへ切り詰める。
    /// @note directionが単位ベクトルならdistanceはワールド距離。Z軸の交差は検査しない。
    cg2::Vector3 ClipCombatRay(Stage& stage, const cg2::Vector3& origin, const cg2::Vector3& direction, float distance) const;
    /// @brief ShieldGuardの正面攻撃を減衰し、減衰が成立したら受付回数と装甲演出を更新してダメージ値を返す。HPは変更しない。
    uint32_t ResolveShieldDamage(uint32_t amount, const cg2::Vector3& attackSource, bool melee);

    static BalanceConfig balanceConfig_;
    static EnemyInteractionConfig enemyInteractionConfig_;
    static std::function<void(uint32_t)> enemyKillCallback_;
    static std::function<void(const cg2::Vector3&)> playerDefeatCallback_;
    static bool shapeNeonBillboardEnabled_;
    static int shapeNeonRenderMode_;

    cg2::Transform worldTransform_;
    cg2::Vector3 baseScale_{1.0f, 1.0f, 1.0f};
    cg2::Vector4 baseColor_{1.0f, 1.0f, 1.0f, 1.0f};
    cg2::Vector4 visualColor_{1.0f, 1.0f, 1.0f, 1.0f};
    std::unique_ptr<cg2::Object3d> object_;

    // 攻撃コントローラ
    AttackController attackController_;

    Player* player_ = nullptr;
    Enemy* boss_ = nullptr;
    cg2::Vector3 aimDirection_{0.0f, -1.0f, 0.0f};

    int hp_ = 10;
    int maxHp_ = 10;
    float bulletCoolTime = 0.0f;

    // キャラクターの当たり判定サイズ
    static inline const float kWidth = 1.6f;
    static inline const float kHeight = 1.6f;

    bool isDead_ = false;
    bool isRunResource_ = false;
    std::function<void(bool playerOwned)> runResourceClaimCallback_;

    uint32_t expValue_ = 10;
    ExpEnemyType type_ = ExpEnemyType::Square;
    bool hasAuthoredDefinition_ = false;
    uint32_t authoredContactDamage_ = 0, authoredBulletDamage_ = 1;
    float authoredMoveSpeedScale_ = 1, authoredFireIntervalScale_ = 1;

    cg2::Vector3 velocity_;
    cg2::Vector3 combatMoveVelocity_{};
    bool combatWasDashing_ = false;

    float decel_ = 3.5f; // 現在の減衰処理では参照していない設定。

    // 最後のUpdateの秒数。接触ノックバックの60FPS基準への換算にも使う。
    float dt_ = 1.0f / 60.0f;

    // 自機本体との接触ダメージだけを抑える残り秒数。弾・直接ダメージには適用しない。
    float invincibleTimer_ = 0.0f;
    float shootInterval_ = 0.0f;
    float shooterWarningRatio_ = 0.0f;
    float shooterMuzzleFlashTimer_ = 0.0f;
    bool shooterHadVisibleTarget_ = false;
    static constexpr float kShooterWarningDuration = 0.30f;
    static constexpr float kShooterMuzzleFlashDuration = 0.08f;
    float damageFeedbackTimer_ = 0.0f;
    float damageFeedbackDuration_ = 0.09f;
    ExpEnemyCombatCycle combatCycle_{};
    ExpEnemyMagazineCycle magazineCycle_{};
    expguard::BladeCycle bladeCycle_{};
    float shieldFlashTimer_ = 0.0f;
    uint32_t shieldBlockCount_ = 0;
    expguard::PulseCycle supportPulse_{};
    bool summonRequested_ = false;
    int summonTotal_ = 0;
    uint64_t summonerId_ = 0;
    float summonLifetime_ = 0, supportFlashTimer_ = 0, wallImpactArmedTimer_ = 0;
    uint32_t reflectionCount_ = 0, empPulseCount_ = 0, wallCollisionCount_ = 0;
    float combatStagger_ = 0.0f, orbitSign_ = 1.0f;
    float dashCooldown_ = 0.0f, dashWarningTimer_ = 0.0f, dashTimer_ = 0.0f;
    cg2::Vector3 dashDirection_{};
    uint32_t combatShotsFired_ = 0, combatDashCount_ = 0;
    cg2::Vector3 telegraphEnd_{};
    cg2::Vector3 combatWaypoint_{};
    float combatRepathTimer_ = 0.0f;
};
