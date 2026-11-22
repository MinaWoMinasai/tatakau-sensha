#pragma once
#include "Struct.h"
#include "game/run/SurvivorHeroSkills.h"
#include "game/run/SurvivorHeroes.h"
#include "game/run/SurvivorShop.h"
#include "game/run/SurvivorSpawnConfig.h"
#include "game/run/SurvivorSubWeapons.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

class Collider;

namespace myEngine {
class NeonGridRenderer;
}

namespace gameplay {
struct GameWorld;

/// @brief 機体（ヒーロー）の技を動かす土台（計画書 p5 の H1）。技を使う・進める・敵に当てる・見た目を積む・バリアとスタミナを持つ。
/// @note 育つ戦車の試作（SurvivorRunController）がこれを受け継いで使う。遠征でも同じ処理を使えるよう、モードごとに違う所
/// （機体で遊んでいるか・ボスを狙ってよいか・時計・バリアの設定・ショップの強化の段とパーク）は仮想関数で受ける。
/// 機体を選ぶ・入力を読む・記録を書き出すのは、受け継いだ側が行う。実装は HeroAbilityCore.cpp（技）と HeroBarrier.cpp（バリア）。
class HeroAbilityCore {
public:
    explicit HeroAbilityCore(GameWorld& world) : heroWorld_(world) {}

    virtual ~HeroAbilityCore() = default;
    HeroAbilityCore(const HeroAbilityCore&) = delete;
    HeroAbilityCore& operator=(const HeroAbilityCore&) = delete;

protected:
    // モードごとに違う所。
    /// @brief 機体で遊んでいるか。偽なら技の処理（Q のゲージ・パーク・見た目）は動かさない。
    virtual bool HeroActive() const = 0;
    /// @brief 技がボス（world の enemy_）を狙ってよいか（試作では宿敵がいるか、遠征ではボス戦中か）。
    virtual bool HeroBossTargetActive() const = 0;
    /// @brief 記録に使う時計（秒）。Q のゲージが初めて満タンになった時刻に使う。
    virtual float HeroClock() const = 0;
    /// @brief 右クリックのバリアとスタミナの設定。
    virtual const survivor::SpawnConfig::BarrierConfig& HeroBarrierSettings() const = 0;

    /// @brief ショップの技の強化の段（0〜3。0 主武器、1〜4 技の枠）。強化の無いモードでは0。
    virtual int UpgradeTier(int target) const
    {
        (void)target;
        return 0;
    }

    /// @brief ショップのパークを持っているか。パークの無いモードでは偽。
    virtual bool HasPerk(survivor::PerkId perk) const
    {
        (void)perk;
        return false;
    }

    /// @brief パークの強さ。
    virtual float PerkValue(survivor::PerkId perk) const
    {
        (void)perk;
        return 0.0f;
    }
    /// @brief 機体専用スキルの段（0〜3。計画書 p8 の F2）。スキルの無いモードでは0。
    virtual int HeroSkillTier(survivor::HeroSkill skill) const
    {
        (void)skill;
        return 0;
    }
    /// @brief 機体の技・主武器・副武器の威力に掛ける倍率（試作のレベルアップの埋め草「威力」。計画書 p8 の F1）。
    virtual float HeroPowerScale() const
    {
        return 1.0f;
    }
    /// @brief 自機の移動の速さの土台の倍率（技・パークの倍率はこれに掛ける）。試作では1、遠征では調整ファイルの heroMoveScale。
    virtual float HeroBaseMoveScale() const
    {
        return 1.0f;
    }

    /// @brief 出撃する機体の定義（HeroActive のときだけ呼ぶ）。試作ではショップの技の強化とパークを効かせた後の値。
    const survivor::HeroDefinition& Hero() const
    {
        return heroDef_;
    }

    survivor::HeroDefinition heroDef_{};

    const survivor::AbilityDefinition& Ability(survivor::AbilitySlot slot) const
    {
        return Hero().abilities[static_cast<size_t>(slot)];
    }

    /// @brief 技の待ち時間・走る・Q のゲージを、区画の始め（ステージの始め）の状態にする。
    void ResetHeroState();
    /// @brief 待ち時間と効いている時間を進め、移動の速さと表示を渡す。dt は区画の時間。
    void UpdateHeroAbilities(float dt);
    /// @brief slot の技を使う。待ち時間中・空き・Q のゲージが足りないなら何もしない。使えたら真。
    bool UseAbility(survivor::AbilitySlot slot);
    /// @brief 与えたダメージ（と K2 からは回復した量）で Q のゲージを溜める。
    void AddUltimateCharge(float amount);
    /// @brief 画面の下の一行（技の名前と待ち時間、Q のゲージ）。
    std::string DescribeHeroAbilities() const;
    // 技の枠ごとの、次に使えるまでの残りの秒と、使った回数・見た一番長い待ち時間（自動確認と記録）。
    std::array<float, survivor::kAbilitySlotCount> abilityCooldown_{};
    std::array<int, survivor::kAbilitySlotCount> abilityUses_{};
    std::array<float, survivor::kAbilitySlotCount> abilityCooldownSeen_{};
    // 走るの残りの秒と、走った秒の合計。
    float sprintTimer_ = 0.0f;
    float sprintSeconds_ = 0.0f;
    // Q のゲージ（0〜Hero().ultimateCharge）・初めて満タンになった時計の秒・満タンになった回数・満タンの間の脈の時間。
    float ultimateCharge_ = 0.0f;
    float ultimateFullAt_ = -1.0f;
    int ultimateFills_ = 0;
    float ultimatePulse_ = 0.0f;
    /// @brief 回復の場の輪と、弾幕の間の自機のまわりの輪を積む。
    void QueueHeroVisuals(myEngine::NeonGridRenderer& renderer, const myEngine::Vector3& cameraForward) const;
    /// @brief 弾幕：狙う範囲の中の近い敵を順に選んで、決まった間隔で撃つ。dt は区画の時間。
    void UpdateBarrage(float dt, const survivor::AbilityDefinition& ability);
    /// @brief 回復の場：中にいる間、最大HPの割合ずつ回復する。dt は区画の時間。
    void UpdateHealZone(float dt, const survivor::AbilityDefinition& ability);
    /// @brief kind の技を入れている枠の定義（無ければ nullptr）。
    const survivor::AbilityDefinition* FindAbility(survivor::AbilityKind kind) const;
    // 回復の場：置いた位置・残りの秒・端数の回復。弾幕：残りの秒・次の発射までの秒・狙う順番。
    myEngine::Vector3 healZonePosition_{};
    float healZoneTimer_ = 0.0f;
    float healZoneRemainder_ = 0.0f;
    float healZoneAge_ = 0.0f;
    float barrageTimer_ = 0.0f;
    float barrageShotTimer_ = 0.0f;
    size_t barrageTarget_ = 0;
    // 技の記録（自動確認と遊んだ記録）：ロケットの爆発と倒した数、回復した量と場の中にいた秒、弾幕の発射数と秒。
    uint64_t rocketBlasts_ = 0;
    uint64_t rocketKills_ = 0;
    uint64_t lastBulletBlasts_ = 0;
    uint64_t lastBulletBlastKills_ = 0;
    int healedByZone_ = 0;
    float healZoneInsideSeconds_ = 0.0f;
    int barrageShots_ = 0;
    float barrageSeconds_ = 0.0f;

    // モイラ型（計画書 p4 の K2）：吸い付く光線・吸収ゲージで回復・消える移動・跳ねる球・太い光線。
    /// @brief 主武器が技の部品（吸い付く光線）か。真なら主武器の弾は撃たない。
    bool HeroBeamPrimary() const
    {
        return HeroActive() && Hero().primaryAbility.kind == survivor::AbilityKind::DrainBeam;
    }

    /// @brief 主武器が技の部品（吸い付く光線・拡散連射など）か。真なら主武器の武器の弾は撃たない。
    bool HeroPrimaryLocked() const
    {
        return HeroActive() && Hero().primaryAbility.kind != survivor::AbilityKind::None;
    }

    // 拡散連射（主武器。計画書 p7 の E5）：押している間（自動で撃つなら いつも）、決まった数を撃ってから待つ。
    void UpdateBurstGun(float dt, const survivor::AbilityDefinition& ability);
    // 今の連射の残りの発数・次の弾までの秒・撃ち終えてからの残りの待ち時間・ばらけ方の乱数・撃った数（自動確認）。
    int burstShotsLeft_ = 0;
    float burstShotTimer_ = 0.0f;
    float burstCooldown_ = 0.0f;
    uint32_t burstRandom_ = 0x9E3779B9u;
    int burstShots_ = 0;
    int burstVolleys_ = 0;
    // 専用スキル（計画書 p8 の F2）：狙いすます（落ち着いている秒）と戦場の勘（重なり・残り秒）。
    myEngine::Vector3 skillLastPosition_{};
    bool skillHasLastPosition_ = false;
    float steadyTimer_ = 0.0f;
    int battleStacks_ = 0;
    float battleTimer_ = 0.0f;
    // 自動確認の記録：狙いすますが効いた弾・戦場の勘の一番多い重なり・連射の弾の一番多い数。
    int steadyShots_ = 0;
    int battleStacksMax_ = 0;
    int burstShotsPerVolleyMax_ = 0;

public:
    /// @brief 機体の遊び方で、自機が敵を倒した（戦場の勘が重なる）。
    void OnHeroKill();

protected:

    /// @brief Q（アルティメット）が効いている間か。間は Q のゲージを溜めない。
    bool UltimateActive() const
    {
        return barrageTimer_ > 0.0f || lanceTimer_ > 0.0f || fortressTimer_ > 0.0f;
    }

    /// @brief 主砲1発の威力に倍率を掛けた、機体の技の威力（毎秒の威力にも使う）。
    float HeroDamageUnit(float scale) const;
    /// @brief 機体の技で、敵（ExpEnemy かボス）に直接ダメージを与える。与えた量を返す（倒れていれば0）。
    uint32_t DealHeroDamage(Collider* target, bool rival, uint32_t amount, const myEngine::Vector3& from);
    /// @brief 吸収ゲージを amount 秒ぶん溜める（上限は自分を回復の duration）。
    void AddAbsorb(float amount);
    void UpdateDrainBeam(float dt, const survivor::AbilityDefinition& ability);
    void UpdateSelfHeal(float dt, const survivor::AbilityDefinition& ability);
    void UpdateBounceOrb(float dt, const survivor::AbilityDefinition& ability);
    void UpdateLance(float dt, const survivor::AbilityDefinition& ability);
    // タンク型（計画書 p4 の K7）：突進・衝撃波・大きな盾。
    void UpdateCharge(float dt, const survivor::AbilityDefinition& ability);
    void UpdateFortress(float dt, const survivor::AbilityDefinition& ability);
    /// @brief origin のまわり半径 radius の敵を strength の強さで押し返し、damage を与える。clearBullets なら敵の弾も消す。押し返した数を返す。
    int TriggerShockwave(const myEngine::Vector3& origin, float radius, float damage, float strength, bool clearBullets);
    // 突進：残りの秒・今の突進で当てた敵。衝撃波の見た目：中心・半径・経った秒。大きな盾：残りの秒・止めた弾の数。
    float chargeTimer_ = 0.0f;
    std::vector<uint64_t> chargeHit_;
    myEngine::Vector3 shockCenter_{};
    float shockRadius_ = 0.0f;
    float shockAge_ = 10.0f;
    float fortressTimer_ = 0.0f;
    int fortressBlockedNow_ = 0;
    float fortressAge_ = 0.0f;
    float fortressHealRemainder_ = 0.0f;
    // タンク型の記録：突進・衝撃波で押し返した数、大きな盾で止めた弾と爆発の回数・威力。
    int chargePushes_ = 0;
    int shockwavePushes_ = 0;
    int fortressBlocked_ = 0;
    int fortressBlasts_ = 0;
    uint64_t fortressDamage_ = 0;
    uint64_t shockwaveDamage_ = 0;
    uint64_t chargeDamage_ = 0;
    // 左クリック・右クリックを押しているか（押している間の技。受け継いだ側が入力を読んで入れる）。
    bool primaryHeld_ = false;
    bool secondaryHeld_ = false;
    // 吸収ゲージ（秒）。吸い付く光線：吸い付いている敵の id（0 は無し、ボスは ~0）、次に与えるまでの秒、光線の先の位置。
    float absorb_ = 0.0f;
    uint64_t beamTarget_ = 0;
    float beamTick_ = 0.0f;
    bool beamOn_ = false;
    myEngine::Vector3 beamEnd_{};
    float beamAge_ = 0.0f;
    // 自分を回復：端数の回復と、効いているか（表示）。
    float selfHealRemainder_ = 0.0f;
    bool selfHealOn_ = false;
    // 消える移動の残りの秒。
    float phaseTimer_ = 0.0f;
    // 跳ねる球：残りの秒・位置・速さ（ワールド単位/秒）・次に与えるまでの秒・最後に跳ねた敵の id。
    float orbTimer_ = 0.0f;
    myEngine::Vector3 orbPosition_{};
    myEngine::Vector3 orbVelocity_{};
    float orbTick_ = 0.0f;
    uint64_t orbLastBounce_ = 0;
    // 太い光線：残りの秒・次に与えるまでの秒・効いていた秒。
    float lanceTimer_ = 0.0f;
    float lanceTick_ = 0.0f;
    float lanceAge_ = 0.0f;
    // モイラ型の記録：光線・球・太い光線で与えた量、溜めた吸収ゲージ（秒）、自分を回復した量と押していた秒、球が跳ねた回数、
    // 太い光線が一度に当てた一番多い数、光線が吸い付いていた秒。
    uint64_t drainDamage_ = 0;
    uint64_t orbDamage_ = 0;
    uint64_t lanceDamage_ = 0;
    float absorbGained_ = 0.0f;
    int selfHealed_ = 0;
    float selfHealSeconds_ = 0.0f;
    int orbBounces_ = 0;
    int lanceMaxHits_ = 0;
    float beamLockedSeconds_ = 0.0f;
    // 吸収ビームの3段目：先から跳ねた2体目の位置（無ければ beamChainOn_ が偽）。跳ねる球の3段目：球の大きさの倍率。
    bool beamChainOn_ = false;
    myEngine::Vector3 beamChainEnd_{};
    float orbScale_ = 1.0f;

    // 副武器（計画書 p7 の E4）。HeroSubWeapons.cpp。段が0の副武器は動かない（遠征では全部0のまま）。
    /// @brief 持っている副武器を動かす。UpdateHeroAbilities から呼ぶ。dt は区画の時間。
    void UpdateSubWeapons(float dt);
    /// @brief 衛星の球と置いた地雷を積む。QueueHeroVisuals から呼ぶ。
    void QueueSubWeapons(myEngine::NeonGridRenderer& renderer) const;
    /// @brief 衛星の球の位置（段で数と輪の半径が変わる）。
    size_t OrbitPositions(std::array<myEngine::Vector3, 4>& out) const;
    int SubWeaponTier(survivor::SubWeapon weapon) const
    {
        return subWeaponTiers_[static_cast<size_t>(weapon)];
    }
    std::array<int, survivor::kSubWeaponCount> subWeaponTiers_{};
    // 衛星：回った角度・次に当てるまでの秒。見張りの砲：次に撃つまでの秒。地雷：次に置くまでの秒と、置いた地雷（位置と経った秒）。
    float orbitAngle_ = 0.0f;
    float orbitTick_ = 0.0f;
    float turretTimer_ = 0.0f;
    float mineTimer_ = 0.0f;
    struct SubMine {
        myEngine::Vector3 position{};
        float age = 0.0f;
    };
    std::vector<SubMine> mines_;
    // 副武器の記録（自動確認）：衛星で与えた量と消した弾、砲の発射数、地雷の爆発数。
    uint64_t orbitDamage_ = 0;
    int orbitBulletsCleared_ = 0;
    int turretShots_ = 0;
    int mineBlasts_ = 0;

    // バリアとスタミナ（計画書 p3 の X3）。HeroBarrier.cpp。武器の枠の遊び方（機体なし）でも使う。
    /// @brief スタミナを増減し、wanted で張っている間は敵の弾と敵を止める。dt は区画の時間。
    void UpdateBarrier(float dt, bool wanted);
    /// @brief 位置が（margin の分だけ広げた）扇の中か。
    bool IsInsideBarrier(const myEngine::Vector3& position, float margin) const;
    /// @brief 張っている間、サッカーボールの網目の扇を積む。
    void QueueBarrier(myEngine::NeonGridRenderer& renderer, const myEngine::Vector3& cameraForward) const;
    /// @brief 網目の線の数（自動確認）。
    size_t BarrierNetSegmentCount() const;
    float barrierStamina_ = 0.0f;
    float barrierRegenWait_ = 0.0f;
    float barrierExhausted_ = 0.0f;
    float barrierFlash_ = 0.0f;
    float barrierAge_ = 0.0f;
    bool barrierOn_ = false;
    bool barrierAwaitRelease_ = false;
    // 張った秒・止めた弾・押し返した回数・尽きた回数（自動確認と記録）。
    float barrierSeconds_ = 0.0f;
    int barrierBlocked_ = 0;
    int barrierPushes_ = 0;
    int barrierBreaks_ = 0;

    /// @brief 同じプレイの共有状態（自機・敵・弾・画面の表示）。受け継いだ側の world_ と同じもの。
    GameWorld& heroWorld_;
};
} // namespace gameplay
