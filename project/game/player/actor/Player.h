#pragma once
#include "game/weapon/CombatTypes.h"
#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <array>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "Calculation.h"
#include "Collider.h"
#include "CollisionConfig.h"
#include "Object3d.h"
#include "Sprite.h"
#include "TextLabel.h"
#include "AttackController.h"
#include "PlayerDrone.h"
#include "Easing.h"
#include "ParticleManager.h"
#include "game/weapon/WeaponMount.h"
#include "game/player/PlayerClassCatalog.h"
#include "game/ui/TankButtonUI.h"
#include "game/ui/NeonProgressBar.h"
#include "game/ui/NeonSegmentedBar.h"
#include "game/ui/NeonTextEffect.h"
#include "game/player/TankRunModifiers.h"
#include "game/player/PlayerDerivedStats.h"
#include "game/player/TankSpecialCombat.h"
#include "game/player/TankCombatStyleBalance.h"
#include "game/player/TankExpeditionLoadout.h"
#include "game/run/TankExpeditionContent.h"
#include "game/run/TankBuildStyle.h"

class PlayerWeapons;
class PlayerProgression;
struct PlayerUiState;
class PlayerHud;
class PlayerEvolution;
class PlayerClassEditor;

class EnemyManager;
class Enemy;
class ExpEnemy;

/// @brief 図鑑に登録する機体の識別子・名前・解放条件・画像パスを表す。
struct TankSeed {
    ClassType type;
    std::string name;
    int requiredRank;
    std::string texturePath;
};
/// @brief 図鑑の機体データと表示用スプライト・文字を保持する。
struct TankData {
    ClassType type;
    std::string classId;
    std::string name;
    int requiredRank;
    std::string texturePath; // 画像パス
    std::unique_ptr<cg2::Sprite> cardSprite;
    std::unique_ptr<cg2::Sprite> sprite; // 各戦車専用のスプライト
    std::unique_ptr<cg2::TextLabel> nameLabel;
    std::unique_ptr<cg2::TextLabel> rankLabel;
};

/// @brief 遠征中に選べる進化先の識別子と説明を表す。
struct RunEvolutionChoice {
    std::string id;
    std::string name;
    std::string description;
};

class Stage;
namespace cg2 {
class ObjectPostEffect;
}

/// @brief 自機の移動・攻撃・装備と成長状態を管理する。機体の制作データはPlayerClassCatalogから取得する。
class Player : public Collider {

public:
    /// @brief 自機の画面状態と表示担当を生成する。
    Player();
    using BodyShape = PlayerBodyShape;

    /// @brief 自機の性能値と現在のスタミナを保持する。基礎値と補正後の実行値の両方に使う。
    /// @note reloadSpeedは発射間隔の60FPS相当の基準フレーム数。bulletSpeed・moveSpeedは基準1フレームの移動量。
    using PlayerStats = TankPlayerStats;

    /// @brief ゲームの性能調整用の数値をまとめる。実行中の状態とは分けて扱う。
    struct BalanceConfig {
        int maxHp = 10000;
        float reloadSpeed = 10.0f;
        float bulletDamage = 1.0f;
        float bulletSpeed = 0.3f;
        float moveSpeed = 0.2f;
        float staminaRecovery = 1.0f;
        float maxStamina = 3.0f;
        uint32_t bodyDamage = 3;
        float healthRegenUpgrade = 0.08f;
        float maxHpUpgradeAmount = 0.10f;
        float bodyDamageUpgrade = 0.10f;
        float bulletSpeedUpgrade = 0.08f;
        float bulletDamageUpgrade = 0.10f;
        float reloadUpgrade = 0.07f;
        float moveSpeedUpgrade = 0.06f;
        float minReloadSpeed = 3.0f;
        bool healToFull = false;
    };
    /// @brief レーザー発射の位置・方向・性能を戦闘側へ渡す。
    /// @note duration・damageIntervalは秒。range・widthはワールド座標の長さ。
    struct LaserShotEvent {
        cg2::Vector3 origin{};
        cg2::Vector3 direction{1.0f, 0.0f, 0.0f};
        float range = 18.0f;
        float width = 0.18f;
        float duration = 0.12f;
        float damageInterval = 0.08f;
        uint32_t damage = 1;
        cg2::Vector4 color{0.25f, 1.0f, 0.95f, 1.0f};
    };
    /// @brief 地雷の配置位置と性能を戦闘側へ渡す。
    /// @note fuseTime・lifeTimeは秒。radiusはワールド座標の半径。
    struct MineDropEvent {
        cg2::Vector3 position{};
        float radius = 3.2f;
        float fuseTime = 0.45f;
        float lifeTime = 5.0f;
        uint32_t damage = 1;
        cg2::Vector4 color{1.0f, 0.25f, 0.95f, 1.0f};
    };
    /// @brief 近接斬撃の範囲と威力を戦闘側へ渡す。
    /// @note windupDurationは予備動作、durationは有効時間、recoveryDurationは硬直時間で、いずれも秒。
    /// arcDegは度。通常のcomboStepは0・1・2で、回転斬撃は-1を使う。
    struct MeleeSlashEvent {
        cg2::Vector3 origin{};
        cg2::Vector3 direction{1.0f, 0.0f, 0.0f};
        float range = 3.4f;
        float arcDeg = 105.0f;
        float width = 0.20f;
        float duration = 0.18f;
        float windupDuration = 0.08f;
        float recoveryDuration = 0.10f;
        int comboStep = 0;
        float knockback = 0.16f;
        uint32_t damage = 1;
        cg2::Vector4 color{0.55f, 1.25f, 1.0f, 1.0f};
    };
    /// @brief ダッシュ衝突の位置・方向と、ボス・強化攻撃の有無を演出へ渡す。
    struct DashImpactEvent {
        cg2::Vector3 origin{}, direction{1.0f, 0.0f, 0.0f};
        bool boss = false;
        bool powered = false;
    };
    /// @brief ドローン間のレーザー接続のワールド座標の端点と、今回の更新での敵への接触有無を表す。
    struct DroneLaserLink {
        cg2::Vector3 start{}, end{};
        bool contact = false; // 地形に遮られず敵に接触したか。ダメージ間隔による許可の有無には依存しない。
    };
    enum class SpecialEventKind {
        RailShot,
        Parry,
        PerfectParry,
        LinkHit,
        DroneCharge,
        DroneBomb,
        DroneRebuild,
        TargetLock,
        DashSlash,
        SpinBlade,
        WallSmash
    };
    /// @brief 特殊戦闘の発動結果をシーン側へ渡す。
    /// @note strengthの意味はkindによる。RailShotは蓄積秒数、DroneBomb・WallSmashは演出の輪の半径の増加に使う値。
    struct SpecialCombatEvent {
        SpecialEventKind kind = SpecialEventKind::RailShot;
        cg2::Vector3 origin{}, direction{1, 0, 0};
        float strength = 1;
    };
    /// @brief 特殊戦闘の発動・命中の累積件数と、分散照準で選んだ対象のビット集合を保持する。
    struct SpecialCombatStats {
        uint32_t railShots = 0, slashWaves = 0, parries = 0, perfectParries = 0, linkTicks = 0;
        // spreadTargetsは対象添字を32で割った余りのビット集合。
        uint32_t droneCharges = 0, droneChargeHits = 0, droneBombs = 0, droneRebuilds = 0, targetLocks = 0, spreadTargets = 0;
        uint32_t dashSlashes = 0, dashSlashHits = 0, spinTicks = 0, wallSmashes = 0;
    };
    /// @brief ドローン能力の状態を表示するための位置・進行度を表す。
    /// @note progressは段階の経過時間を表示用の基準時間で割り、0～1に制限した値。帰還時の基準は突撃の制限時間。
    struct DroneAbilityVisual {
        cg2::Vector3 position{}, target{};
        tankspecial::DronePhase phase = tankspecial::DronePhase::Escort;
        float progress = 0;
        bool bomb = false;
    };
    /// @brief ロック蓄積中・ロック中の対象位置、命中蓄積数、ロック残り時間（秒）を表示へ渡す。
    struct TargetLockVisual {
        cg2::Vector3 position{};
        int stacks = 0;
        float remaining = 0;
    };
    /// @brief 護衛中を除いた、生存ドローンの任務表示情報を値で返す。ドローン装備系統でなければ空。
    std::vector<DroneAbilityVisual> GetDroneAbilityVisuals() const;
    /// @brief 特殊戦闘更新で作成したロック対象の表示情報への読み取り専用参照を返す。
    /// @note 要素は次の特殊戦闘更新や能力リセットで再取得する。
    const std::vector<TargetLockVisual>& GetTargetLockVisuals() const
    {
        return targetLockVisuals_;
    }
    /// @brief 回転斬撃の残り時間を継続時間0.8秒で割った値を返す。
    /// @note 戻り値自体は範囲を制限しない。非負の時間で通常更新した状態では0～1。
    float GetSpinBladeRatio() const
    {
        return spinCycle_.remaining / .8f;
    }
    /// @brief EMP妨害の残り時間を基準2.5秒で割った値を返す。
    /// @note 0～1への制限はなく、3秒の妨害を適用した直後は1.2になる。
    float GetEmpRatio() const
    {
        return empJammerTimer_ / 2.5f;
    }
    /// @brief 遠征補正が有効ならEMP妨害の残り時間を延長する。現在より短い要求では短縮しない。
    /// @param seconds 要求する残り時間（秒）。0～3秒に制限する。
    void ApplyEmpJammer(float seconds = 2.5f)
    {
        if (runModifiers_.enabled)
            empJammerTimer_ = tankspecial::EmpDuration(empJammerTimer_, seconds);
    }
    /// @brief ドローンの命中をロックへ蓄積し、今回適用する補正後のダメージを返す。
    /// @param drone 命中元ドローンの添字。0～31は蓄積に使い、負値は処理対象外。
    /// @param target 借用する命中対象。nullptr、死亡済み、資源、対応外の型は処理対象外。
    /// @param originalDamage ロック補正を掛ける前のダメージ。
    /// @return 能力・装備系統が対象外、または記録枠が確保できなければoriginalDamage。
    /// 対象の記録がある場合はロック倍率を掛けて四捨五入し、最低1としたダメージ。
    /// @note ロック成立時は集計と演出イベントも更新する。添字32以上は蓄積しないが既存ロックの倍率は適用する。
    /// HPへの適用は呼び出し側が行う。衝突側は弾のダメージを一時的に置き換え、衝突処理後に元へ戻す。
    uint32_t NotifyDroneHit(int drone, Collider* target, uint32_t originalDamage);
    /// @brief 対象の現在のロックによるダメージ倍率を返す。対象・能力・装備系統が該当しなければ1。
    /// @param boss ボス用の倍率を使うか。対象の型判定は呼び出し側で行う。
    float GetDroneTargetDamageScale(const Collider* target, bool boss) const;
    /// @brief 近接攻撃で押し出した経験値敵について、1秒以内の壁衝突を追加ダメージの候補として記録する。
    /// @param target 借用する対象。nullptr、死亡済み、資源、能力・装備系統が対象外なら記録しない。
    /// @param strength 0.5～2に制限して記録する強さ。現在の壁衝突ダメージ計算には使わない。
    /// @note 記録枠がない場合は何もしない。同じ対象の未処理の壁衝突があれば上書きしない。
    void ArmWallSmash(ExpEnemy* target, float strength = 1);
    /// @brief レール砲の押下中の蓄積時間（秒）を返す。押下継続状態でなければ0。
    /// @note 最大時間で割る正規化は行わない。現在のkRailMaxChargeSecondsが1秒のため、比率と同じ数値になる。
    float GetRailChargeRatio() const
    {
        return railCharge_.held ? railCharge_.seconds : 0.0f;
    }
    /// @brief チャージ演出用に先頭砲塔の銃口のワールド座標を返す。設定がなければ照準方向の既定位置。
    cg2::Vector3 GetRailChargeMuzzle() const;
    /// @brief 今回の特殊戦闘更新で作成したドローン間レーザー接続への読み取り専用参照を返す。
    /// @note 要素は次のUpdateSpecialCombatや装備変更・部屋リセットで再取得する。
    const std::vector<DroneLaserLink>& GetDroneLaserLinks() const
    {
        return droneLaserLinks_;
    }
    /// @brief 特殊戦闘の累積集計への読み取り専用参照を返す。
    const SpecialCombatStats& GetSpecialCombatStats() const
    {
        return specialCombatStats_;
    }
    /// @brief 特殊戦闘イベントの未処理分を取り出し、内部の保留分を消費済みにする。
    std::vector<SpecialCombatEvent> ConsumeSpecialCombatEvents();
    /// @brief 追加能力・接続レーザー・斬撃波・パリィを更新し、必要なダメージ・弾・イベントを生成する。
    /// @param dt この処理で進める経過時間（秒）。
    /// @note 自機・敵・弾の移動と地形衝突の後、アクター・弾の衝突処理の前に1回呼ぶ。
    /// boss・enemiesのnullptrは対象なし。bulletsのnullptr、遠征補正無効、死亡中、dtが0以下なら
    /// レーザー接続一覧を消去した後に戻る。
    void UpdateSpecialCombat(Stage& stage, BulletManager* bullets, Enemy* boss, EnemyManager* enemies, float dt);
    /// @brief ダッシュ開始直後の敵への接触を、ダメージ・押し出し・演出イベントへ反映する。
    /// @return 通常の接触通知を省略する場合true。同じダッシュで処理済みの対象もtrueを返す。
    /// @note nullptr、死亡中、ダッシュ外、対象外の敵にはfalse。処理済みでなければ受付時間も確認する。
    bool TryDashImpact(Collider* target);
    /// @brief ダッシュ衝撃イベントの未処理分を取り出し、内部の保留分を消費済みにする。
    std::vector<DashImpactEvent> ConsumeDashImpactEvents();
    /// @brief ダッシュを開始した累積回数を返す。
    uint32_t GetDashStartedCount() const
    {
        return dashStartedCount_;
    }
    /// @brief 無敵・カウンターなどで無効化されず、被ダメージ処理へ進んだ累積回数を返す。
    uint32_t GetDamageTakenCount() const
    {
        return damageTakenCount_;
    }
    /// @brief 通常弾・床hazardを既存条件で実際にperfect-dodgeした累計を返す。
    uint64_t GetPerfectDodgeCount() const
    {
        return perfectDodgeCount_;
    }
    /// @brief 有限な現在の被弾無敵残時間（秒）を非負値で返す。時計は進めない。
    float GetInvincibilityRemainingSeconds() const
    {
        return std::isfinite(invincibleTimer_) ? (std::max)(0.0f, invincibleTimer_) : 0.0f;
    }
    /// @brief 既存のjust-evade保護が成立中かを返す。
    bool IsJustEvading() const
    {
        return isJustEvaded_;
    }

    /// @brief 主攻撃を実行した累積回数を返す。弾数ではなく、近接攻撃や遠征ドローンの発射も含む。
    uint32_t GetPrimaryAttackCount() const
    {
        return primaryAttackCount_;
    }
    /// @brief 近接攻撃強化ビルドであるか判定する。
    bool IsMeleeBuild() const
    {
        return runModifiers_.enabled &&
               (expeditionCombatStyleSelected_ ? expeditionCombatStyle_ == tankbuild::Style::Melee : runModifiers_.meleeBlade);
    }
    /// @brief ドローン強化ビルドであるか判定する。
    bool IsDroneBuild() const
    {
        return runModifiers_.enabled && expeditionCombatStyleSelected_ && expeditionCombatStyle_ == tankbuild::Style::Drone;
    }
    /// @brief 遠征の戦闘系統を選び、性能・装備・特殊状態を切り替える。
    /// @return 有効な遠征で切り替えられた場合、または同じ系統が選択済みならtrue。
    /// @note 無効な系統、遠征外、死亡中、必要なBasic設定がない場合はfalse。切り替えでHP・スタミナを補充しない。
    bool SetExpeditionCombatStyle(tankbuild::Style style);
    /// @brief 保持している遠征の戦闘系統を返す。選択済みかはHasExpeditionCombatStyleで確認する。
    tankbuild::Style GetExpeditionCombatStyle() const
    {
        return expeditionCombatStyle_;
    }
    /// @brief 遠征の戦闘系統を選択済みかを返す。
    bool HasExpeditionCombatStyle() const
    {
        return expeditionCombatStyleSelected_;
    }

    /// @brief 所有する装備とUI資源を破棄する。Initializeで借用した本体オブジェクトの所有権は持たない。
    ~Player();

    /// @brief マウスから求めたZ=0平面の位置、またはデモの照準位置へ機体の向きを更新する。
    /// @note 初期化後に呼ぶ。通常入力ではviewProjectionに有効なカメラが必要。
    void RotateToMouse(cg2::Camera* viewProjection);

    /// @brief 自機の戦闘資源・機体設定・初期位置を用意する。
    /// @param objectBullet 自機本体の描画オブジェクト。nullptr不可。所有権は移らず、自機の利用中は有効に保つ。
    /// @param position ワールド座標での初期位置。
    /// @param arenaUi 旧アリーナ用の図鑑・HUD資源も準備するか。遠征は専用UIを持つ。
    /// @note 初回生成に使う。機体設定の再読込にはReloadPlayerClassConfigsを使う。
    void Initialize(cg2::Object3d* objectBullet, const cg2::Vector3& position, bool arenaUi = true);

    /// @brief UI入力を処理してから自機の移動・攻撃・装備を更新する。
    /// @param deltaTime 戦闘用の経過秒。スローなどを反映した時間。
    /// @param uiDeltaTime UI用の経過秒。戦闘のスローと分けて渡す。
    /// @note 進化画面を操作したフレームは戦闘入力へ流さない。
    /// 初期化後、有効なカメラと弾管理先を渡す。経過時間は非負とする。
    void Update(cg2::Camera* viewProjection, Stage& stage, BulletManager* BulletManager, float deltaTime, float uiDeltaTime);

    /// @brief 通常モデルを使うドローンと、指定に応じて自機本体・砲塔を描画する。
    /// @param drawBody 自機本体・砲塔も描画するか。ドローンの描画には影響しない。
    void Draw(bool drawBody = true);
    /// @brief 自機本体・砲塔を描画する。死亡時の溜め演出中は本体だけ描画する。
    void DrawBodyOnly();

    /// @brief 通常スプライト用の描画状態を準備し、強化HUDを描画する。
    void DrawSprite();
    /// @brief 進化画面の背景と発光演出を、シーンのポストエフェクト後に描画する。
    void DrawEvolutionAfterPostEffects();
    /// @brief 強化HUDのゲージの発光を、シーンのポストエフェクト後に描画する。
    void DrawUpgradeHudAfterPostEffects();
    /// @brief ゲーム中のHUDで発光対象にする文字ラベルの借用ポインターをlabelsの末尾へ追加する。
    /// @note 進化画面中は追加しない。所有権は移らず、UIの再構築やPlayerの破棄をまたいで保持しない。
    void AppendGameplayNeonTextLabels(std::vector<cg2::TextLabel*>& labels) const;
    /// @brief 強化HUD文字テクスチャを利用前に準備する。
    void PrepareUpgradeHudTextTextures();

    /// @brief 管理するドローンの借用ポインター一覧を値で返す。死亡済みの要素も含む。
    /// @note 所有権は移らない。次の更新・装備変更・部屋リセットで削除され得るため、都度取得する。
    std::vector<PlayerDrone*> GetDronePtrs() const;

    /// @brief 確定した接触を受け、回避・ノックバック・被ダメージを処理する。接触判定は呼び出し側で行う。
    /// @param other 接触相手。nullptr不可。
    void OnCollision(Collider* other) override;

    // ワールド座標を取得

    // 半径
    static inline const float kRadius = 0.8f;

    /// @brief ワールド座標での位置を返す。
    cg2::Vector3 GetWorldPosition() const override;

    /// @brief 現在の速度を値で返す。60FPS相当の基準1フレームの移動量として保持する。
    cg2::Vector3 GetMove()
    {
        return velocity_;
    }
    /// @brief 現在の速度を設定する。単位は60FPS相当の基準1フレームの移動量。
    void SetVelocity(const cg2::Vector3& v)
    {
        velocity_ = v;
    }

    /// @brief ワールド座標を設定し、本体オブジェクトの変換と行列を更新する。初期化後に呼ぶ。
    void SetWorldPosition(const cg2::Vector3& pos)
    {
        worldTransform_.translate = pos;
        object_->SetTransform(worldTransform_);
        object_->Update();
    }

    /// @brief AABBを返す。
    cg2::AABB GetAABB();

    /// @brief 無敵時間を0.45秒としてTakeDamageへ渡す。既定のダメージ量はダメージブロック用。
    void Damage(uint32_t amount = kDamageBlockDamage);
    /// @brief 受けたダメージをHPなどの戦闘状態へ反映する。
    /// @param invincibleTime 適用後に設定する無敵時間（秒）。
    /// @note 死亡中、amountが0、無敵中、無傷の開発設定中は何もしない。
    /// カウンターが成立すれば被ダメージを省略する。HPが0になれば死亡演出を開始する。
    void TakeDamage(uint32_t amount, float invincibleTime = 0.45f);
    /// @brief Depth専用の傾斜カメラでは、表示中のVPから安全にZ=0の照準を求める。
    void SetNeonDepthAimEnabled(bool enabled)
    {
        neonDepthAimEnabled_ = enabled;
    }
    /// @brief 成立済みの敵対床hazard接触を既存回避・無敵・被弾処理へ渡し、実HP損失を返す。
    /// @param dodgeable trueなら既存perfect-dodgeの弾と同じ受付条件を使う。
    /// @note 方向ゼロを被弾除外にせず、接触shapeは呼出元のGameplay計画を正とする。
    uint32_t ReceiveHostileHazard(uint32_t amount, bool dodgeable = true);
    /// @brief 性能調整設定を現在の状態へ適用する。
    void ApplyBalanceConfig(const BalanceConfig& config);
    /// @brief 戦闘系統ごとの調整値を補正して反映し、性能とドローン設定を更新する。
    /// @note HP・スタミナは新しい上限内に制限し、補充しない。
    void ApplyCombatStyleBalance(const TankCombatStyleBalances& profiles);
    /// @brief 戦闘系統の性能調整値への読み取り専用参照を返す。無効なstyleは先頭の系統として扱う。
    const TankCombatStyleProfile& GetCombatStyleProfile(tankbuild::Style style) const
    {
        return combatStyleBalances_[tankbuild::Valid(style) ? static_cast<size_t>(style) : 0];
    }
    /// @brief 遠征補正をコピーして性能・装備へ反映する。補正の有効化・無効化に伴う状態も初期化する。
    void SetRunModifiers(const TankRunModifiers& modifiers);
    /// @brief 遠征中の自機の戦闘性能を表示・検証用にまとめる。
    struct RunCombatSnapshot {
        int barrels = 0, projectilesPerBarrel = 1, activeDrones = 0;
        tankbuild::Style style = tankbuild::Style::Shooter;
        bool hasStyle = false, melee = false;
        int droneLimit = 0;
        std::string classId;
        float classDamageScale = 1.0f, classReloadScale = 1.0f, classBulletSpeedScale = 1.0f;
        bool classReflects = false, classPenetrates = false, isAuthored = false;
        int classDroneCount = 0;
        int baseDroneCount = 3;
        int maxWallBounces = -1, actorPierceCount = 0, impactSplitCount = 0;
        bool reflects = false, homing = false, dashBurst = false, dashExplosion = false;
        float homingTurnRate = 0;
        uint32_t dashExplosionsEmitted = 0;
        float shotDamage = 0, reloadSeconds = 0;
    };
    /// @brief 遠征戦闘状態の写しを返す。
    RunCombatSnapshot GetRunCombatSnapshot() const;
    /// @brief デモ入力を使用する設定かを返す。
    bool IsDemoInputEnabled() const
    {
        return demoInputEnabled_;
    }
    /// @brief デモ用の移動・ワールド座標の照準・射撃・ダッシュ入力を設定する。
    /// @note 有効にすると進化画面を閉じ、部屋移動後の入力解放待ちを解除する。
    void SetDemoInput(bool enabled, const cg2::Vector2& move, const cg2::Vector3& aimWorld, bool shoot, bool dash)
    {
        demoInputEnabled_ = enabled;
        demoMove_ = move;
        demoAim_ = aimWorld;
        demoShoot_ = shoot;
        demoDash_ = dash;
        if (enabled) {
            isChangeMode = false;
            runRoomAwaitInputRelease_ = false;
        }
    }
    /// @brief 遠征の初期装備を準備し、整備・進化状態を初期化する。遠征補正無効なら何もしない。
    /// @param archetype 初期系統。0: Twin、1: MachineGun、2: Overseer。範囲外は0～2へ制限する。
    /// @note 部屋ごとの進化方式ではBasicから開始し、レベル・強化・ドローンをリセットしてHPを全回復する。
    void ConfigurePrototypeLoadout(int archetype); // 0: Twin, 1: MachineGun, 2: Overseer
    /// @brief 遠征中の自機のHPを回復する。
    /// @note amountが正で生存中の場合、最大HPまで回復する。遠征補正無効なら何もしない。
    void HealRunPlayer(int amount);
    /// @brief 遠征中の自機のHPを費用として消費する。
    /// @return 正のamountを支払ってHPが1以上残る場合true。遠征補正無効・死亡中・支払不可なら変更せずfalse。
    bool SpendRunHealth(int amount);
    /// @brief 部屋ごとの進化・整備方式を使うか設定する。切り替えに伴う状態の初期化は行わない。
    void SetRunCheckpointEvolution(bool enabled)
    {
        runCheckpointEvolution_ = enabled;
    }
    /// @brief 誘導・ドローン照準に使う対象のワールド座標を、先頭48件までコピーする。
    void SetRunHomingTargets(const std::vector<cg2::Vector3>& targets);
    /// @brief 現在選択できる遠征の進化候補を値で返す。方式・装備系統・準備状態に応じて空になる。
    std::vector<RunEvolutionChoice> GetRunEvolutionChoices() const;
    /// @brief 遠征の進化候補を選べる状態へ進め、進化画面と確定・取消イベントを解除する。
    /// @note 部屋ごとの方式では準備フラグを設定する。旧方式では次の機体に必要なランクまでレベルを上げる。
    void PrepareRunEvolution();
    /// @brief 現在の遠征進化候補から指定IDを選択し、装備へ反映して確定イベントを記録する。
    /// @return 選択できればtrue。候補外・遠征補正無効・死亡中などはfalse。
    bool ChooseRunEvolution(const std::string& id);
    /// @brief 部屋ごとの進化方式で、未付与のクリア部屋に対して整備ポイントを1付与する。
    /// @param clearedRoom 部屋番号（1～4）。
    /// @return 付与した場合true。付与済み・範囲外・遠征補正無効・方式外・死亡中はfalse。
    bool AwardRunMaintenancePoint(int clearedRoom);
    /// @brief 未使用の遠征整備ポイント数を返す。
    int GetRunMaintenancePoints() const
    {
        return runMaintenance_.Points();
    }
    /// @brief 遠征整備ランクを返す。
    /// @param stat 0: 機動、1: 装填、2: 装甲。範囲外は0を返す。
    int GetRunMaintenanceRank(int stat) const
    {
        return runMaintenance_.Rank(stat);
    }
    /// @brief 遠征整備の3項目について、現在の段階と支払可否を値で返す。
    std::array<RunMaintenanceChoice, 3> GetRunMaintenanceChoices() const;
    /// @brief 整備ポイントを1消費し、指定項目を1段階強化して性能を再計算する。
    /// @param stat 0: 機動、1: 装填、2: 装甲。
    /// @return 強化した場合true。方式外・死亡中・範囲外・ポイント不足・上限到達ならfalse。
    /// @note この再計算ではHPを回復しない。
    bool SpendRunMaintenancePoint(int stat);
    /// @brief 指定整備を1段階戻してポイントを1返却し、性能を再計算する。
    /// @param stat 0: 機動、1: 装填、2: 装甲。
    /// @return 返却した場合true。方式外・死亡中・範囲外・未強化ならfalse。
    bool RefundRunMaintenancePoint(int stat);
    /// @brief 部屋移動時に一時的な戦闘状態・ドローン・イベントをリセットし、指定ワールド座標へ移す。
    /// @note 遠征補正有効かつ生存中に行う。HP・成長は保ち、スタミナを全回復して短い無敵時間を設定する。
    /// 通常入力はマウスボタンを解放するまで攻撃を待つ。生成済みの弾はここでは消去しない。
    void ResetRunRoomState(const cg2::Vector3& position);
    /// @brief 被ダメージを無効化する開発用設定を切り替える。
    void SetDebugNoDamage(bool enabled)
    {
        debugNoDamage_ = enabled;
    }
    /// @brief 被ダメージを無効化する開発用設定が有効かを返す。
    bool IsDebugNoDamage() const
    {
        return debugNoDamage_;
    }

    /// @brief 死亡状態にして死亡演出を開始する。既に死亡中なら何もしない。
    void Die();

    /// @brief 死亡済みで、死亡演出が終了した場合trueを返す。
    bool isFinished();

    /// @brief 死亡状態かを返す。
    bool IsDead() const
    {
        return isDead_;
    }
    /// @brief 現在のHPを返す。
    int GetHp() const
    {
        return hp_;
    }
    /// @brief 現在の最大HPを整数へ変換して返す。
    int GetMaxHp() const
    {
        return static_cast<int>(stats_.maxHp);
    }
    /// @brief 撃破画面演出を更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateDefeatPresentation(float deltaTime);

    /// @brief 地形衝突処理で設定された接地状態を返す。
    bool IsOnGround() const
    {
        return isOnGround_;
    }
    /// @brief 接地状態を設定する。
    void SetOnGround(bool onGround)
    {
        isOnGround_ = onGround;
    }

    /// @brief XY平面での照準角度（ラジアン）を返す。
    float GetAngle() const
    {
        return angle_;
    }
    /// @brief 照準方向への読み取り専用参照を返す。
    const cg2::Vector3& GetDirection() const
    {
        return dir_;
    }
    /// @brief ダッシュ中かを返す。
    bool IsDashing() const
    {
        return isDashing_;
    }
    /// @brief 移動入力が存在するか判定する。
    bool HasMovementInput() const
    {
        return cg2::Length(inputDir_) > 0.05f;
    }
    /// @brief 主攻撃の実行フラグを返し、保留フラグを解除する。複数回の実行も1つのtrueにまとめる。
    bool ConsumePrimaryAttackPerformedEvent();
    /// @brief ダッシュ開始の有無を返し、保留フラグを解除する。
    bool ConsumeDashStartedEvent();
    /// @brief ネオン描画する砲塔の位置・方向・形状を表す。
    struct NeonBarrelLayout {
        cg2::Vector3 offset{};
        cg2::Vector3 scale{1.25f, 0.24f, 0.24f};
        float angleRad = 0.0f;
        float recoilOffset = 0.0f;
        float muzzleFlashRatio = 0.0f;
        bool isMelee = false;
        BarrelShape shape = BarrelShape::Box;
        int fireGroup = 0;
        cg2::Vector4 barrelColor{0.25f, 1.0f, 0.95f, 1.0f};
        cg2::Vector4 outlineColor{0.80f, 1.0f, 0.95f, 1.0f};
    };
    /// @brief ネオン描画する機体本体の形状・大きさ・色を表す。
    struct NeonBodyLayout {
        BodyShape shape = BodyShape::Circle;
        cg2::Vector2 scale{1.0f, 1.0f};
        cg2::Vector4 fillColor{0.18f, 0.28f, 0.34f, 0.38f};
        cg2::Vector4 outlineColor{0.50f, 1.0f, 0.35f, 1.0f};
    };
    /// @brief ネオン描画用の砲塔・刃の配置を値で返す。ドローン装備系統では空。
    std::vector<NeonBarrelLayout> GetNeonBarrelLayouts() const;
    /// @brief 現在の機体設定から、ネオン描画用の本体情報を値で返す。設定がなければ既定値。
    NeonBodyLayout GetNeonBodyLayout() const;
    /// @brief 被ダメージ演出の残り時間を継続時間で割り、0～1に制限して返す。継続時間が0以下なら0。
    float GetDamageFeedbackRatio() const;

    /// @brief 主攻撃と遠征用に使う弾管理先を借用し、条件が整っていれば護衛ドローンを揃える。
    /// @note 所有権は移らない。攻撃中は有効な管理先を保つ。nullptrでは護衛ドローンを生成しない。
    void SetAttackControllerBulletManager(BulletManager* bulletManager)
    {
        attackController_.SetBulletManager(bulletManager);
        runBulletManager_ = bulletManager;
        EnsureExpeditionDrones();
    }

    /// @brief 入力・装備・発射待ち時間に従って主攻撃を処理し、反動と実行イベントを更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    /// @note 初期化後に有効な弾管理先を渡す。内部でAddが拒否した場合も、攻撃の集計は登録弾数を保証しない。
    void Attack(BulletManager* BulletManager, float deltaTime);

    /// @brief ドローン数と待ち時間の条件を満たせば、ドローンを1機生成して弾管理先を設定する。
    /// @note この関数自体は弾を発射しない。ドローンの射撃は各ドローンの更新で行う。
    void DroneShoot(BulletManager* BulletManager);

    /// @brief Smasherの押下中の突進力を蓄積し、入力を離したときに突進速度を設定する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Smash(float deltaTime);

    /// @brief 現在のワールド座標とkRadiusから、衝突判定用の球を値で返す。
    cg2::Sphere GetSphere() const;

    /// @brief 経験値を加算してレベル・強化ポイントを更新する。通貨方式では獲得通貨として保留する。
    /// @param amount 獲得した経験値。呼び出し側では非負の値を渡す。
    /// @note 通常方式ではレベル上限で加算を止める。通貨方式の保留上限は1000000。
    void AddExp(int amount);
    /// @brief 撃破報酬をレベルへ加算せず、マップ所有の財布へ渡す通貨として保留する方式を切り替える。
    /// @note 方式が変わると保留通貨を消去する。有効化時はレベル・経験値・強化ポイントを初期化し進化画面を閉じる。
    void SetRunCurrencyMode(bool enabled);
    /// @brief 撃破報酬を通貨として保留する方式かを返す。
    bool IsRunCurrencyMode() const
    {
        return runCurrencyMode_;
    }
    /// @brief 未反映の獲得通貨を取り出し、取り出した分を消費済みにする。
    int TakeRunCurrencyEarned()
    {
        const int earned = runCurrencyEarned_;
        runCurrencyEarned_ = 0;
        return earned;
    }
    /// @brief 制作データから遠征で使う機体設定を登録する。
    /// @note Catalogの検証失敗なら変更しない。成功時は登録一覧を置き換え、適合する選択中の制作機体も更新する。
    void InstallRunAuthoredClasses(const tankcontent::Catalog& catalog);
    /// @brief 登録した制作機体から、現在の装備系統に合う未選択の候補を値で返す。遠征補正無効・死亡中は空。
    std::vector<RunEvolutionChoice> GetRunAuthoredEvolutionChoices() const;
    /// @brief 登録した制作機体を選び、装備と性能を更新して進化確定イベントを記録する。
    /// @return 遠征補正有効・生存中でIDと装備系統が適合すればtrue。それ以外は変更せずfalse。
    /// @note HPは新しい上限内に保ち、補充しない。ドローンと一部の特殊戦闘状態を作り直す。
    bool ChooseRunAuthoredClass(const std::string& id);
    /// @brief レベルを返す。
    int GetLevel() const
    {
        return level_;
    }
    /// @brief 経験値を返す。
    int GetExp() const
    {
        return exp_;
    }
    /// @brief 次のレベルアップに必要な経験値を返す。
    int GetNextLevelExpValue() const
    {
        return nextLevelExp_;
    }
    /// @brief 未使用の性能強化ポイント数を返す。
    int GetSkillPoints() const
    {
        return skillPoints_;
    }
    /// @brief 現在のレベルに対応する機体解放ランクを返す。
    int GetCurrentRank() const
    {
        return GetRankFromLevel(level_);
    }
    /// @brief 強化レベルを返す。
    /// @param index 強化項目の添字（0～6）。範囲外なら0。
    int GetUpgradeLevel(int index) const;
    /// @brief 現在の機体の表示名を借用して返す。設定がなければ旧機体名またはUnknown。
    /// @note 設定の編集・再読込・機体切り替えをまたいで保持せず、必要なら文字列をコピーする。
    const char* GetCurrentClassName() const;
    /// @brief 通常方式の強化ポイントを1消費し、指定項目を1段階上げて性能と実行イベントを更新する。
    /// @param index 0: スタミナ回復、1: 最大HP、2: 接触ダメージ、3: 弾速、4: 弾の威力、5: 発射間隔、6: 移動速度。
    /// @return 強化した場合true。遠征補正有効・範囲外・ポイント不足・上限到達なら変更せずfalse。
    bool ApplyStatUpgrade(int index);
    /// @brief 性能強化の実行フラグを返し、保留フラグを解除する。
    bool ConsumeStatUpgradePerformedEvent();
    /// @brief 通常方式の指定強化を1段階戻し、ポイントを1返却して性能を再計算する。
    /// @return 返却した場合true。遠征補正有効・添字の範囲外・未強化なら変更せずfalse。
    bool RefundStatUpgrade(int index);
    /// @brief 補正後の性能と現在のスタミナへの読み取り専用参照を返す。
    const PlayerStats& GetStats() const
    {
        return stats_;
    }
    /// @brief 指定機体のボタン表示情報をoutputへコピーする。
    /// @return 設定が見つかればtrue。見つからなければoutputを変えずfalse。
    bool GetTankButtonVisualData(const std::string& classId, TankButtonVisualData& output) const;
    /// @brief 機体設定を再読込し、現在の機体の砲塔と配置に反映する。
    /// @return 現在の機体を保ったまま再読込できた場合true。
    /// @note HPと成長状態を初期化しない。
    /// 読み込み失敗や現在の機体IDが欠けている場合は既存設定を保持する。成功時は設定の借用ポインターを再取得する。
    bool ReloadPlayerClassConfigs(const std::string& path = "resources/configs/playerClasses.json");

    /// @brief 保留中のスロー要求があれば消費してtrueを返す。要求がなければfalse。
    bool RequestSlow();

    /// @brief 図鑑を初期化する。
    void InitializeEncyclopedia();

    /// @brief 図鑑を更新する。
    /// @param uiDeltaTime UI用の経過秒。戦闘の時間倍率と分けて渡す。
    void UpdateEncyclopedia(float uiDeltaTime);

    /// @brief 図鑑を描画する。
    void DrawEncyclopedia();

    /// @brief 制作用の機体図鑑で機体プレビューと発射デモを表示する。USE_IMGUIが有効な構成で使う。
    void DrawTankCodex();

    /// @brief 機体設定の選択・複製・編集・JSON保存を行う制作画面を表示する。
    /// @note USE_IMGUIが有効な構成で表示する。戦闘の更新と分けて呼ぶ。
    void DrawPlayerClassEditor();
    /// @brief 強化HUDの表示設定を編集する制作画面を表示する。USE_IMGUIが有効な構成で使う。
    void DrawUpgradeHudDebugImGui();
    /// @brief 進化UI外観編集画面を描画する。
    void DrawEvolutionUiStyleEditor();
    /// @brief 自機UIの更新・描画時間（ミリ秒）、描画件数、表示状態を保持する。
    struct UiProfileStats {
        float updateMs = 0.0f;
        float spriteMs = 0.0f;
        float textMs = 0.0f;
        float totalMs = 0.0f;
        int spriteDraws = 0;
        int textDraws = 0;
        bool visible = false;
#if defined(USE_IMGUI) && !defined(NDEBUG)
        bool baseTextRefreshed = false;
        bool listTextRefreshed = false;
        float baseTextRefreshMs = 0.0f;
        float expLabelRefreshMs = 0.0f;
        float levelLabelRefreshMs = 0.0f;
        float baseTextSetStyleMs = 0.0f;
        float baseTextSetTextMs = 0.0f;
        float baseTextSetTextRebuildMs = 0.0f;
        float baseTextRebuildTextureMs = 0.0f;
        float baseTextGetOrCreateTextureMs = 0.0f;
        float baseTextSpriteSetTextureMs = 0.0f;
        int baseTextCacheFileExistedCount = 0;
        int baseTextGeneratedPngCount = 0;
        float listTextRefreshMs = 0.0f;
        float listTextSetStyleMs = 0.0f;
        float listTextSetTextMs = 0.0f;
        float listTextSetTextRebuildMs = 0.0f;
        float listTextRebuildTextureMs = 0.0f;
        float listTextGetOrCreateTextureMs = 0.0f;
        float listTextSpriteSetTextureMs = 0.0f;
        int listTextCacheFileExistedCount = 0;
        int listTextGeneratedPngCount = 0;
#endif
    };
    /// @brief 強化HUDの処理時間と描画件数への読み取り専用参照を返す。
    const UiProfileStats& GetUpgradeHudProfileStats() const;
    /// @brief 進化UIの処理時間と描画件数への読み取り専用参照を返す。
    const UiProfileStats& GetEvolutionUiProfileStats() const;
#if defined(USE_IMGUI) && !defined(NDEBUG)
    /// @brief 強化HUDの状態を開発表示・検証へ渡す。
    struct UpgradeHudDebugSnapshot {
        int playerLevel = 0;
        int exp = 0;
        int skillPoints = 0;
        bool visible = false;
        float listVisibility = 0.0f;
        bool hideListWithoutPoints = false;
        bool drawListPanels = false;
        bool drawListText = false;
        bool drawBottomBars = false;
        bool drawBottomText = false;
        bool useRectBatch = false;
        bool useNeonProgressBars = false;
        bool useSegmentedUpgradeBars = false;
        bool segmentedBarBloomEnabled = true;
        bool listTextBloomEnabled = false;
        bool listActuallyVisible = false;
        bool isChangeMode = false;
        bool playerIsDead = false;
        int maxEnhancePoint = 0;
    };
    /// @brief 強化HUD開発表示状態の写しを返す。
    UpgradeHudDebugSnapshot GetUpgradeHudDebugSnapshot() const;
    /// @brief 主攻撃入力を継続させる開発用設定を切り替える。
    void SetDebugAutoFireEnabled(bool enabled)
    {
        debugAutoFireEnabled_ = enabled;
    }
    /// @brief 主攻撃入力を継続させる開発用設定が有効かを返す。
    bool IsDebugAutoFireEnabled() const
    {
        return debugAutoFireEnabled_;
    }
#endif
    /// @brief レーザー射撃イベントの未処理分を取り出し、内部の保留分を消費済みにする。
    std::vector<LaserShotEvent> ConsumeLaserShotEvents();
    /// @brief 地雷配置イベントの未処理分を取り出し、内部の保留分を消費済みにする。
    std::vector<MineDropEvent> ConsumeMineDropEvents();
    /// @brief 近接攻撃斬撃イベントの未処理分を取り出し、内部の保留分を消費済みにする。
    std::vector<MeleeSlashEvent> ConsumeMeleeSlashEvents();

    /// @brief レベルから解放ランク（1～4）を返す。5・10・15以上でそれぞれランク2・3・4になる。
    int GetRankFromLevel(int level) const;

    /// @brief 進化選択画面を開いているかを返す。
    bool IsChangeMode()
    {
        return isChangeMode;
    }
    /// @brief チュートリアルの進行に合わせて進化画面を閉じる。
    void CloseEvolutionUiForTutorial();
    /// @brief 進化確定の有無を返し、保留フラグを解除する。
    bool ConsumeEvolutionConfirmed();
    /// @brief 進化取消の有無を返し、保留フラグを解除する。
    bool ConsumeEvolutionCancelled();

private:
    /// @brief 既存のダッシュ回避条件とslow/buff/capacitor/overdrive効果を共用する。
    bool TryPerfectDodgeFromHostileContact(bool dodgeable);

    using PlayerClassConfig = ::PlayerClassConfig;

    // ワールド変換データ
    cg2::Transform worldTransform_;

    // モデル
    cg2::Object3d* object_ = nullptr;
    /// @brief 1砲塔の描画オブジェクトを所有し、変換・配置・反動・発光時間を保持する。
    struct BarrelModel {
        std::unique_ptr<cg2::Object3d> object;
        cg2::Transform transform;
        cg2::Vector3 localOffset;
        float recoilOffset = 0.0f;
        float muzzleFlashTimer = 0.0f;
    };
    static constexpr float kMuzzleFlashDuration = 0.06f;
    std::vector<BarrelModel> barrels_;
    PlayerClassCatalog classCatalog_;
    std::unordered_map<std::string, PlayerClassConfig> runAuthoredClasses_;
    std::unordered_map<std::string, tankbuild::Style> runAuthoredStyles_;
    std::vector<RunEvolutionChoice> runAuthoredChoices_;
    bool runCurrencyMode_ = false;
    bool runAuthoredEvolutionActive_ = false;
    int runCurrencyEarned_ = 0;
    // テクスチャハンドル
    uint32_t textureHandle_ = 0u;

    // キーボード入力
    cg2::Input* input_ = nullptr;

    cg2::Vector3 dir_;

    // キャラクターの移動速さ
    float kCharacterSpeed = 0.2f;
    cg2::Vector3 move_;

    const int kBulletTime = 10;
    float bulletCoolTime = 0.0f;

    // キャラクターの当たり判定サイズ
    static inline const float kWidth = 1.6f;
    static inline const float kHeight = 1.6f;

    cg2::Vector3 velocity_{0, 0, 0}; // 現在速度
    cg2::Vector3 inputDir_{0, 0, 0}; // 入力方向

    float maxSpeed_ = 0.15f; // 最高速度
    float accel_ = 2.5f;     // 加速
    float decel_ = 3.5f;     // 減速（ブレーキ）

    float gravity_ = -2.0f;
    float jumpPower_ = 1.0f;

    bool isOnGround_ = false;

    cg2::Transform hpTransform_;

    // HPモデル
    std::vector<std::unique_ptr<cg2::Sprite>> hpSprites_;
    std::unique_ptr<cg2::Sprite> hpFont;
    static constexpr uint32_t kDamageBlockDamage = 75;
    int hp_ = 10000;

    // 無敵時間
    float invincibleTimer_ = 0.0f;
    bool debugNoDamage_ = false;
    float damageFeedbackTimer_ = 0.0f;
    float damageFeedbackDuration_ = 0.18f;
    cg2::Vector4 baseVehicleColor_{1.0f, 1.0f, 1.0f, 1.0f};
    cg2::Vector4 baseBarrelColor_{0.48f, 0.86f, 0.22f, 1.0f};

    bool isDead_ = false;

    bool isExploding_ = false;
    float deathEffectTimer_ = 0.0f;
    float deathChargeTimer_ = 0.0f;
    float deathChargeDuration_ = 0.28f;

    // 攻撃コントローラ
    AttackController attackController_;

    // プレイヤーのドローン
    std::vector<std::unique_ptr<PlayerDrone>> drones_;
    std::vector<LaserShotEvent> pendingLaserShots_;
    std::vector<MineDropEvent> pendingMineDrops_;
    std::vector<MeleeSlashEvent> pendingMeleeSlashes_;
    std::vector<DashImpactEvent> pendingDashImpacts_;
    std::vector<uint64_t> dashImpactTargets_;
    uint32_t dashStartedCount_ = 0, damageTakenCount_ = 0;
    uint32_t primaryAttackCount_ = 0;
    uint64_t perfectDodgeCount_ = 0;
    int meleeComboStep_ = 0;
    float meleeComboTimer_ = 0.0f;
    float saberCounterTimer_ = 0.0f;
    tankspecial::RailCharge railCharge_{};
    tankspecial::LinkDamageClock linkDamageClock_{};
    std::vector<DroneLaserLink> droneLaserLinks_;
    std::vector<SpecialCombatEvent> pendingSpecialCombatEvents_;
    SpecialCombatStats specialCombatStats_{};
    MeleeSlashEvent specialMeleeSwing_{};
    float specialMeleeElapsed_ = -1;
    bool specialWaveEmitted_ = false, specialPerfectFeedback_ = false;
    std::vector<uint64_t> specialParriedBullets_;
    /// @brief レール砲の入力を蓄積し、離したときに弾を追加して待ち時間・反動・集計を更新する。
    /// @param dt この処理で進める経過時間（秒）。
    /// @note 発射待ち中やbulletsがnullptrならチャージをリセットする。Addの登録成功を確認するAPIではない。
    void AttackRailCannon(BulletManager* bullets, bool pressed, float dt);
    /// @brief 遠征の初期射撃装備へ追加砲身・扇状配置・交互射撃の補正を反映し、必要なら描画装備を作り直す。
    void RefreshAdditiveArmaments();
    /// @brief EMP・ドローン任務待ち時間・ロック・回転斬撃・ダッシュ斬撃・壁衝突候補を初期状態へ戻す。
    void ResetAdditionalAbilities();
    /// @brief ドローン任務とロック、ダッシュ斬撃・回転斬撃・壁衝突ダメージを更新する。
    /// @param dt この処理で進める経過時間（秒）。
    /// @note UpdateSpecialCombatから呼ぶ。bulletsは現在未使用で、敵のnullptrは対象なしを表す。
    void UpdateAdditionalAbilities(Stage& stage, BulletManager* bullets, Enemy* boss, EnemyManager* enemies, float dt);
    /// @brief 近接系統のフィニッシュ後、入力継続とスタミナの条件を満たせば回転斬撃を開始する。
    /// @return 開始してスタミナ・攻撃待ち時間・演出イベントを更新した場合true。
    bool TryStartSpinBlade(bool pressed);
    float empJammerTimer_ = 0, droneChargeCooldown_ = 1.5f, droneBombCooldown_ = 4.0f, recentDashTimer_ = 0;
    size_t nextMissionDrone_ = 0;
    std::array<tankspecial::PainterLock, 128> painterLocks_{};
    std::vector<TargetLockVisual> targetLockVisuals_;
    tankspecial::SpinCycle spinCycle_{};
    bool finisherSpinReady_ = false, dashSlashActive_ = false;
    cg2::Vector3 dashSlashPrevious_{}, dashSlashDirection_{1, 0, 0};
    float dashSlashTimer_ = 0;
    uint32_t dashSlashDamage_ = 1;
    std::vector<uint64_t> dashSlashTargets_;
    /// @brief 押し出した経験値敵の衝突ID、登録時の壁衝突回数、受付の残り秒数、記録した強さを保持する。
    struct WallSmashTarget {
        uint64_t id = 0;
        uint32_t collision = 0;
        float seconds = 0, strength = 1;
    };
    std::array<WallSmashTarget, 128> wallSmashTargets_{};

    // プレイヤーの経験値とレベル
    int exp_ = 0;
    int level_ = 1;
    // 次のレベルまでの必要経験値
    int nextLevelExp_ = 0;
    // 最大レベル
    const int kMaxLevel = 15;
    /// @brief 現在のレベルから、次のレベルアップに必要な経験値を計算する。
    int GetNextLevelExp() const;

    PlayerStats stats_;
    PlayerStats baseStats_;
    TankRunModifiers runModifiers_{};
    TankCombatStyleBalances combatStyleBalances_ = DefaultTankCombatStyleBalances();
    /// @brief 発射間隔の基準値を60FPS相当のフレーム数で返す。選択済みの戦闘系統、または基礎値を使う。
    float GetRunBaseReloadFrames() const;
    tankbuild::Style expeditionCombatStyle_ = tankbuild::Style::Shooter;
    bool expeditionCombatStyleSelected_ = false;
    BulletManager* runBulletManager_ = nullptr;
    TankRunGrowth runGrowth_{};
    uint32_t runDashExplosionsEmitted_ = 0;
    std::string runStarterBranch_ = "Twin";
    PlayerClassConfig runStarterConfig_{};
    bool demoInputEnabled_ = false, demoShoot_ = false, demoDash_ = false;
    cg2::Vector2 demoMove_{};
    cg2::Vector3 demoAim_{};
    cg2::Vector3 runAimWorld_{};
    bool neonDepthAimEnabled_ = false;
    TankExpeditionMaintenance runMaintenance_{};
    PlayerClassConfig runEvolutionConfig_{};
    bool runEvolutionActive_ = false;
    bool runEvolutionPrepared_ = false;
    float runSupportDroneTimer_ = 0.0f;
    float runDashAttackTimer_ = 0.0f;
    float runOverdriveTimer_ = 0.0f;
    float runOverdriveCooldown_ = 0.0f;
    bool runDashBurstPending_ = false;
    bool runRoomAwaitInputRelease_ = false;
    bool runCheckpointEvolution_ = false;
    std::vector<cg2::Vector3> runHomingTargets_;
    /// @brief 現在の機体・遠征補正・EMPなどから、ドローンの射撃性能・発射間隔・追従設定を更新する。
    void ConfigureRunDrone(PlayerDrone& drone) const;
    /// @brief ドローン系統の護衛機数を現在の上限に揃える。超過分を削除し、不足分を生成する。
    /// @note 本体・弾管理先の未設定、死亡中、ドローン系統以外なら何もしない。
    void EnsureExpeditionDrones();
    /// @brief 遠征ドローン上限を返す。
    int GetExpeditionDroneLimit() const;
    /// @brief 発射条件へ現在の射撃強化・反射・耐久度・貫通などを反映する。
    /// @param applyFan 互換用の未使用引数。
    /// @note 遠征補正無効でも弾数を1、分裂数を0に設定する。自機の扇状配置は砲身で表す。
    void ApplyRunProjectileRules(AttackParam& param, bool applyFan = true) const;
    /// @brief 遠征の加速効果と突撃系統のダッシュ後の補正を含む、発射間隔に掛ける倍率を返す。
    float GetRunFireIntervalScale() const;
    /// @brief 登録した対象位置と誘導能力に従って、往路の自機弾の向きを更新する。弾の移動は行わない。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateRunProjectiles(BulletManager* bulletManager, float deltaTime);
    float healthRegenUpgradeRate_ = 0.08f;
    float maxHpUpgradeRate_ = 0.10f;
    float bodyDamageUpgradeRate_ = 0.10f;
    float bulletSpeedUpgradeRate_ = 0.08f;
    float bulletDamageUpgradeRate_ = 0.10f;
    float reloadUpgradeRate_ = 0.07f;
    float moveSpeedUpgradeRate_ = 0.06f;
    float minReloadSpeed_ = 3.0f;
    ClassType currentClass_ = ClassType::Basic;
    std::string currentClassId_ = "Basic";

    /// @brief 旧機体種類をIDへ変換し、EvolveByIdへ渡す。
    void Evolve(ClassType newClass);
    /// @brief 指定IDの機体へ進化し、装備と外観を反映する。
    /// @note 設定がないか遠征で使用できない機体なら何もしない。ランク・進化経路の判定は呼び出し側で行う。
    void EvolveById(const std::string& classId);
    /// @brief 進化条件を満たす機体へ切り替え、履歴と確定イベントを更新する。成功ならtrue。
    bool TryConfirmEvolutionById(const std::string& classId);
    /// @brief 指定機体が次ランクの接続先で、使用条件と必要ランクを満たすか判定する。
    bool CanEvolveTo(const std::string& classId) const;
    /// @brief 遠征で使用できるドローン機体または射撃砲塔を持つ機体か判定する。遠征補正無効ならtrue。
    /// @note 遠征ではSmasherを対象外とする。
    bool IsRunCompatibleClass(const PlayerClassConfig& config) const;
    /// @brief 進化画面で機体を表示対象にするか判定する。遠征補正無効、または現在の機体IDならtrue。
    bool IsEvolutionClassVisible(const std::string& classId) const;
    /// @brief 読み込んだ進化経路にfromからtoへの接続があるか判定する。
    bool HasEvolutionEdge(const std::string& from, const std::string& to) const;
    /// @brief 機体設定を一時Catalogへ読み、現在の機体IDを検証してから置き換える。
    /// @return 読み込みと現在の機体の検証に成功した場合true。
    /// @note 失敗時は現在の設定を保つ。実行中のHPや装備の再初期化は行わない。
    bool LoadPlayerClassConfigs(const std::string& path = "resources/configs/playerClasses.json");
    /// @brief 機体Catalogを指定パスのJSONへ保存する。保存の成否は戻り値で通知しない。
    void SavePlayerClassConfigs(const std::string& path = "resources/configs/playerClasses.json") const;
    /// @brief 指定した旧機体種類の既定設定を値で返す。
    PlayerClassConfig CreateDefaultClassConfig(ClassType type) const;
    /// @brief 機体種類からCatalogの設定を借用する。見つからなければnullptr。
    /// @note 読み込み成功・既定値へのリセット・対象の削除後は再取得する。
    const PlayerClassConfig* GetClassConfig(ClassType type) const;
    /// @brief 機体IDからCatalogの設定を借用する。見つからなければnullptr。
    /// @note 読み込み成功・既定値へのリセット・対象の削除後は再取得する。
    const PlayerClassConfig* GetClassConfig(const std::string& classId) const;
    /// @brief 現在使用する機体設定を借用する。遠征の進化設定・初期装備を優先し、なければCatalogを参照する。
    /// @note 設定切り替えや再読込後は再取得する。Catalogに現在IDがなければnullptrになり得る。
    const PlayerClassConfig* GetCurrentClassConfig() const;
    /// @brief 指定IDのCatalog設定を編集用に借用する。見つからなければnullptr。寿命はGetClassConfigと同じ。
    PlayerClassConfig* GetMutableClassConfig(const std::string& classId);
    /// @brief 機体設定に従って弾・レーザー・地雷・近接攻撃を生成または予約し、発射待ち時間を更新する。
    /// @param baseReload 基準の発射間隔（秒）。
    /// @param recoilDir 発射後の反動方向の出力先。
    /// @param recoilPower 発射後の反動の大きさの出力先。ドローン方式では0にする。
    /// @return 攻撃処理を進めた場合true。待ち時間・上限などで処理しない場合false。
    /// @note 有効な弾管理先が必要。trueでも、ドローン上限やAddの拒否などにより生成弾数が増えるとは限らない。
    bool FireConfiguredClass(const PlayerClassConfig& config, BulletManager* bulletManager, float baseReload, cg2::Vector3& recoilDir,
                             float& recoilPower);
    /// @brief 機体設定と再使用待ち時間から特殊行動を選んで開始する。開始した場合true。
    bool TryActivateSpecialAction();
    /// @brief スタミナと移動方向の条件を満たせばダッシュを開始し、trueを返す。
    /// @note ジャスト回避の成立は後の衝突通知で判定する。この関数は回避成功を保証しない。
    bool ActivatePerfectDodge(const PlayerClassConfig& config);
    /// @brief 使用可能な近接装備とスタミナがあれば、剣カウンターの受付時間を設定してtrueを返す。
    bool ActivateSaberCounter(const PlayerClassConfig& config);
    /// @brief 最初の使用可能な近接装備で反撃を予約し、受付時間の解除・無敵時間・スロー要求を設定する。
    void TriggerSaberCounter(const PlayerClassConfig& config);
    /// @brief 方向ベクトルをXY平面でangleDeg度回転し、Z成分を保って値で返す。
    cg2::Vector3 RotateDirection(const cg2::Vector3& direction, float angleDeg) const;
    /// @brief 現在の機体設定から砲塔の描画オブジェクトを作り直す。
    void InitializeBarrels();
    /// @brief 砲塔の反動・発光時間をdt_秒進め、機体の位置と照準に合わせて配置を更新する。
    void UpdateBarrelLayout();
    /// @brief 砲塔を描画する。
    void DrawBarrels();
    /// @brief 被ダメージの色補正を含む本体・砲塔の色に、指定した透明度を設定する。
    void SetVehicleAlpha(float alpha);
    /// @brief ダメージフィードバックを発動させる。
    void TriggerDamageFeedback();

    /// @brief 基礎性能または選択した戦闘系統に、強化・遠征・整備の補正を掛けて性能を再計算する。
    /// @param healToFull HPを新しい最大値まで回復するか。
    /// @note falseでも再計算前が満タンなら新しい最大HPにする。それ以外は新しい上限内に保つ。
    void RecalculateStatsFromBase(bool healToFull);
    /// @brief 射撃時の薬莢用パーティクルを照準方向へ出す。
    void SpawnCasing();
    int shootBarrelIndex_ = 0; // 次に撃つ砲身の番号
    int shootGroupIndex_ = 0;
    std::vector<float> weaponGroupCooldowns_;

    int maxEnhancePoint = 5;
    std::array<int, 7> upgradeLevels_{};
    int skillPoints_ = 0;
    /// @brief ステルスを更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateStealth(float deltaTime);
    /// @brief 旧Summonerの待ち時間を更新する。現在はこの関数からドローンを生成しない。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateSummoner(float deltaTime);

private:
    /// @brief 死亡用パーティクルを発生させ、本体の溜め時間と演出終了タイマーを設定する。
    void SpawnParticles();
    /// @brief 死亡演出のタイマーを進め、終了時に爆発演出中のフラグを解除する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateParticles(float deltaTime = 1.0f / 60.0f);

    /// @brief 移動方向の後方へ煙とネオンの移動演出を発生させる。
    void SpawnAfterimage();
    /// @brief 互換用の空処理。現在は引数を使わず、状態を変更しない。
    /// @param deltaTime 互換用の経過時間（秒）。現在は未使用。
    void UpdateP(float deltaTime);
    /// @brief バフ継続中の煙パーティクルを自機位置へ出す。
    void SpawnBuffParticle();
    /// @brief ワールド座標をカメラの画面座標へ変換する。
    cg2::Vector2 WorldToScreen(const cg2::Vector3& worldPos, cg2::Camera* camera);

    float angle_ = 0.0f;

    // ダッシュ関連
    bool isDashing_ = false;
    bool primaryAttackPerformedEvent_ = false;
    bool dashStartedEvent_ = false;
    float dashTimer_ = 0.0f;    // ダッシュ持続時間
    float dashCooldown_ = 0.0f; // 再使用までの時間
    float movementParticleTimer_ = 0.0f;
    float movementParticleInterval_ = 0.08f;
    const float kDashDuration = 0.3f; // ダッシュしている時間（秒）
    const float kDashCooldown = 0.3f; // クールタイム（秒）
    const float kDashSpeed = 0.3f;    // ダッシュの強さ

    // ジャスト回避ができるかのスタミナ
    float justEvadeStamina_ = 3.0f;

    // ジャスト回避判定用
    const float kJustEvadeWindow = 0.2f; // ダッシュ開始から何秒間が「ジャスト」か

    // ジャスト回避したか
    bool isJustEvaded_ = false;

    // スロー要求
    bool requestSlow_ = false;

    // デルタタイム
    float dt_ = 1.0f / 60.0f;

    float buffTimer_ = 0.0f;          // バフの残り時間
    bool isBuffActive_ = false;       // バフ中かどうかのフラグ
    const float kBuffDuration = 2.0f; // 能力倍率を掛ける前のバフ持続時間（2秒）

    float smashCharge_ = 0.0f; // スマッシュチャージ時間
    float maxCharge_ = 1.0f;
    bool isSmash_ = false; // スマッシュ中かどうか
    cg2::Vector3 smashDir_;

#if defined(USE_IMGUI) && !defined(NDEBUG)

    bool debugAutoFireEnabled_ = false;
#endif

    cg2::Vector2 mousePosition_;

    bool isChangeMode = false;
    bool evolutionConfirmedEvent_ = false;
    bool evolutionCancelledEvent_ = false;
    bool statUpgradePerformedEvent_ = false;
    int evolutionSelectedIndex_ = 0;
    float evolutionUiTimer_ = 0.0f;

    std::unique_ptr<cg2::Sprite> sprite;

    float stealthAlpha_ = 1.0f; // ステルス時の透明度 (1.0:不透明, 0.0:透明)
    bool isStealth_ = false;    // 現在ステルス中か
    float stealthTimer_ = 0.0f; // ステルス移行までの待機時間

    // Summoner用：ドローンリスト
    const int kMaxSummonerDrones = 4;

    float summonTimer_;

    std::unique_ptr<PlayerUiState> ui_;
    std::unique_ptr<PlayerHud> playerHud_;
    std::unique_ptr<PlayerEvolution> playerEvolution_;
    std::unique_ptr<PlayerClassEditor> playerClassEditor_;
    friend class PlayerHud;
    friend class PlayerEvolution;
    friend class PlayerClassEditor;
    std::unique_ptr<PlayerWeapons> playerWeapons_;
    std::unique_ptr<PlayerProgression> playerProgression_;
    friend class PlayerWeapons;
    friend class PlayerProgression;
};
