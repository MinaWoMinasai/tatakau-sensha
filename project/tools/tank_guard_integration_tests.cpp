// Compiles the real ExpEnemy declaration, AI update and damage callbacks.
// Only rendering/audio, the player target and unrelated navigation are adapters.
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <unordered_set>
#include "../game/exp/ExpEnemyCombatCycle.h"
#include "../game/exp/ExpEnemyMagazineCycle.h"
#include "../game/exp/ExpGuardCombat.h"
#include "../game/player/TankSpecialCombat.h"

struct Vector3 {
    float x = 0, y = 0, z = 0;
    Vector3& operator+=(Vector3 v) { x += v.x; y += v.y; z += v.z; return *this; }
};
struct Vector4 { float x = 0, y = 0, z = 0, w = 0; };
Vector3 operator+(Vector3 a, Vector3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
Vector3 operator-(Vector3 a, Vector3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
Vector3 operator*(Vector3 a, float b) { return { a.x * b, a.y * b, a.z * b }; }
Vector3 operator/(Vector3 a, float b) { return a * (1.0f / b); }
float Length(Vector3 v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
Vector3 Normalize(Vector3 v) { return v / Length(v); }
Vector4 LerpColor(Vector4 a, Vector4, float) { return a; } // Color is not a damage input.
struct Transform { Vector3 scale{ 1,1,1 }, rotate{}, translate{}; };
Transform InitWorldTransform() { return {}; }
struct AABB { Vector3 min, max; };
struct Segment {};
#include "../game/collision/CollisionConfig.h"
#include "guard_collider.inc"

struct Object3d {
    void Initialize() {}
    void SetTransform(Transform) {}
    void Update() {}
    void SetModel(const std::string&) {}
    void SetColor(Vector4) {}
    void SetLighting(bool) {}
};
struct ParticleManager {
    static ParticleManager* GetInstance() { static ParticleManager instance; return &instance; }
    void EmitNeonImpactEffect(Vector3, Vector3, Vector4, uint32_t) { ++impacts; }
    void EmitNeonDeathEffect(Vector3, Vector4, Vector4, float) { ++deaths; }
    int impacts = 0, deaths = 0;
};
class Bullet : public Collider {
public:
    enum class SpecialKind { None, Rail, SlashWave, ParryReflection };
    SpecialKind special = SpecialKind::None;
    Vector3 position{}, previousPosition{}, movement{ -1,0,0 };
    Bullet() { SetCollisionAttribute(kCollisionAttributePlayerBullet); SetDamage(100); }
    Vector3 GetWorldPosition() const override { return position; }
    void OnCollision(Collider*) override {}
    bool CanClaimRunResource() const { return true; }
    Vector3 GetMove() const { return movement; }
    Vector3 GetPreviousWorldPosition() const { return previousPosition; }
    SpecialKind GetSpecialKind() const { return special; }
};
class Player : public Collider {
public:
    Vector3 position{ 2,0,0 }, velocity{};
    int hp = 1000, damageCalls = 0;
    unsigned exp = 0;
    bool invulnerable = false;
    int empCalls = 0;
    float empSeconds = 0;
    Vector3 GetWorldPosition() const override { return position; }
    void OnCollision(Collider*) override {}
    bool IsDead() const { return hp <= 0; }
    int GetHp() const { return hp; }
    void TakeDamage(uint32_t amount, float) { ++damageCalls; if (!invulnerable) hp -= static_cast<int>(amount); }
    Vector3 GetMove() const { return velocity; }
    void SetVelocity(Vector3 v) { velocity = v; }
    void AddExp(uint32_t value) { exp += value; }
    void ApplyEmpJammer(float seconds) { ++empCalls; empSeconds = (std::max)(empSeconds, seconds); }
};
class BulletManager {};
enum class BulletOwner { kEnemy };
struct AttackParam {
    float bulletSpeed = 0, spreadAngleDeg = 0, bulletHp = 0, bulletPenetration = 0;
    int bulletCount = 1;
    bool randomSpread = false, canClaimRunResource = false;
    uint32_t damage = 0;
};
class AttackController {
public:
    void SetBulletManager(BulletManager*) {}
    void FireFromMuzzle(Vector3, Vector3, AttackParam param, BulletOwner) { lastShot = param; ++shots; }
    AttackParam lastShot;
    int shots = 0;
};
namespace tankcontent { struct Enemy; }
#include "guard_enemy_declarations.inc"

struct Block { bool isActive = true; AABB aabb; };
enum Axis { X, Y };
class Stage {
public:
    std::vector<std::vector<Block>> blocks;
    const auto& GetBlocks() const { return blocks; }
    bool IsCollisionWithAnyBlock(Vector3 p, float radius) {
        for (const auto& row : blocks) for (const auto& block : row) if (block.isActive) {
            const float dx = p.x - (std::clamp)(p.x, block.aabb.min.x, block.aabb.max.x);
            const float dy = p.y - (std::clamp)(p.y, block.aabb.min.y, block.aabb.max.y);
            if (dx * dx + dy * dy < radius * radius) return true;
        }
        return false;
    }
    void ResolveExpEnemyCollision(ExpEnemy& actor, Axis axis) {
        for (const auto& row : blocks) for (const auto& block : row) if (block.isActive) {
            auto p = actor.GetWorldPosition(); const float r = actor.GetRadius();
            if (p.x + r <= block.aabb.min.x || p.x - r >= block.aabb.max.x ||
                p.y + r <= block.aabb.min.y || p.y - r >= block.aabb.max.y) continue;
            if (axis == X) p.x = p.x < (block.aabb.min.x + block.aabb.max.x) * .5f ? block.aabb.min.x - r : block.aabb.max.x + r;
            else p.y = p.y < (block.aabb.min.y + block.aabb.max.y) * .5f ? block.aabb.min.y - r : block.aabb.max.y + r;
            actor.SetWorldPosition(p);
        }
    }
};
ExpEnemy::BalanceConfig ExpEnemy::balanceConfig_{};
ExpEnemy::EnemyInteractionConfig ExpEnemy::enemyInteractionConfig_{};
std::function<void(uint32_t)> ExpEnemy::enemyKillCallback_{};
std::function<void(const Vector3&)> ExpEnemy::playerDefeatCallback_{};
bool ExpEnemy::shapeNeonBillboardEnabled_ = false;
int ExpEnemy::shapeNeonRenderMode_ = 0;
// Ray obstruction uses production ClipCombatRay; walking around a wall is
// covered by the pre-existing navigation suite, not these damage tests.
Vector3 ExpEnemy::FindCombatWaypoint(Stage&, const Vector3& target) const { return target; }
#include "guard_enemy_methods.inc"
class EnemyManager {
public:
    std::vector<std::unique_ptr<ExpEnemy>> enemies_;
    Player* player_ = nullptr;
    Enemy* boss_ = nullptr;
    BulletManager* bulletManager_ = nullptr;
    void DismissOrphanedSummons();
    void UpdateSummonedUnits(Stage& stage);
};
#include "guard_manager_methods.inc"

int main() {
    Stage stage;
    Player player;
    // Read bullet durability from the actual production enemy shooting path,
    // so interception tests cannot silently assume a weaker made-up projectile.
    ExpEnemy ordinaryShooter;
    ordinaryShooter.Initialize({}, &player, ExpEnemyType::Skirmisher);
    ordinaryShooter.authoredMoveSpeedScale_ = 0;
    for (int frame = 0; frame < 300 && !ordinaryShooter.attackController_.shots; ++frame)
        ordinaryShooter.UpdateExpeditionCombat(stage, 0.01f);
    assert(ordinaryShooter.attackController_.shots > 0);
    const float ordinaryHp = ordinaryShooter.attackController_.lastShot.bulletHp;
    assert(ordinaryHp == tankspecial::kOrdinaryEnemyBulletHp);
    assert(tankspecial::ParryDurabilityDamage(ordinaryHp, false) >= ordinaryHp);
    assert(tankspecial::ParryDurabilityDamage(ordinaryHp, true) >= ordinaryHp);
    ExpEnemy armoredShooter;
    armoredShooter.Initialize({}, &player, ExpEnemyType::Sniper);
    armoredShooter.authoredMoveSpeedScale_ = 0;
    for (int frame = 0; frame < 350 && !armoredShooter.attackController_.shots; ++frame)
        armoredShooter.UpdateExpeditionCombat(stage, 0.01f);
    assert(armoredShooter.attackController_.shots > 0);
    const float armoredHp = armoredShooter.attackController_.lastShot.bulletHp;
    assert(armoredHp == tankspecial::kArmoredEnemyBulletHp);
    assert(tankspecial::ParryDurabilityDamage(armoredHp, false) == 0);
    assert(tankspecial::ParryDurabilityDamage(armoredHp, true) < armoredHp);
    ExpEnemy shield;
    shield.Initialize({}, &player, ExpEnemyType::ShieldGuard);
    shield.aimDirection_ = { 1,0,0 };
    shield.SetHp(1000);
    shield.TakeDirectionalDamage(100, { 3,0,0 });
    assert(shield.GetHp() == 985 && shield.GetShieldBlockCount() == 1);
    shield.TakeDirectionalDamage(100, { -3,0,0 });
    assert(shield.GetHp() == 885 && shield.GetShieldBlockCount() == 1);
    shield.TakeDirectionalDamage(100, { 0,3,0 }, true);
    assert(shield.GetHp() == 785);
    shield.TakeDirectionalDamage(100, { 3,0,0 }, true);
    assert(shield.GetHp() == 745 && shield.GetShieldBlockCount() == 2);

    Bullet bullet;
    // Position has already crossed behind the actor, but velocity says it
    // arrived from the front. The callback must use approach direction.
    bullet.position = { -0.3f,0,0 };
    bullet.movement = { -1,0,0 };
    bullet.previousPosition = { 1,0,0 };
    shield.OnCollision(&bullet);
    assert(shield.GetHp() == 730 && shield.GetShieldBlockCount() == 3);
    // A wall ricochet returning from behind is unmitigated.
    bullet.position = { 0.3f,0,0 };
    bullet.movement = { 1,0,0 };
    shield.OnCollision(&bullet);
    assert(shield.GetHp() == 630 && shield.GetShieldBlockCount() == 3);
    bullet.movement = { -1,0,0 };
    bullet.special = Bullet::SpecialKind::SlashWave;
    shield.OnCollision(&bullet);
    assert(shield.GetHp() == 590 && shield.GetShieldBlockCount() == 4);
    shield.TakeDirectionalDamage(10000, { -3,0,0 });
    const unsigned exp = player.exp;
    shield.TakeDirectionalDamage(10000, { -3,0,0 });
    assert(shield.IsDead() && player.exp == exp && exp == shield.GetExpValue());

    ExpEnemy blade;
    player.hp = 1000;
    player.damageCalls = 0;
    blade.Initialize({}, &player, ExpEnemyType::BladeGuard);
    blade.bladeCycle_.Reset(0);
    blade.UpdateExpeditionCombat(stage, 0.01f);
    assert(blade.GetCombatPhase() == ExpEnemyCombatPhase::Locked);
    assert(blade.GetDamage() == 0 && player.GetHp() == 1000);
    // The entire warning remains visible even while a target hugs the body.
    for (int frame = 0; frame < 44; ++frame) {
        blade.UpdateExpeditionCombat(stage, 0.01f);
        assert(blade.GetCombatPhase() == ExpEnemyCombatPhase::Locked);
        assert(player.damageCalls == 0);
    }
    blade.UpdateExpeditionCombat(stage, 0.02f);
    assert(blade.GetCombatPhase() == ExpEnemyCombatPhase::Active);
    assert(player.GetHp() == 973 && player.damageCalls == 1 && Length(player.GetMove()) >= 0.49f);
    for (int frame = 0; frame < 12; ++frame) blade.UpdateExpeditionCombat(stage, 0.01f);
    assert(player.damageCalls == 1); // No HP invulnerability in this adapter.
    blade.UpdateExpeditionCombat(stage, 0.03f);
    assert(blade.GetCombatPhase() == ExpEnemyCombatPhase::Recovery);
    for (int frame = 0; frame < 89; ++frame) {
        blade.UpdateExpeditionCombat(stage, 0.01f);
        assert(blade.GetCombatPhase() == ExpEnemyCombatPhase::Recovery);
        assert(player.damageCalls == 1 && blade.GetDamage() == 0);
    }

    // Actual AI locks its aim; dashing to the rear before the swing is safe.
    player.position = { 2,0,0 };
    blade.Initialize({}, &player, ExpEnemyType::BladeGuard);
    blade.bladeCycle_.Reset(0);
    blade.UpdateExpeditionCombat(stage, 0.01f);
    player.position = { -2,0,0 };
    const int callsBeforeDodge = player.damageCalls;
    for (int frame = 0; frame < 65; ++frame) blade.UpdateExpeditionCombat(stage, 0.01f);
    assert(player.damageCalls == callsBeforeDodge);

    // Cover placed between an already committed swing and the player blocks it.
    player.position = { 2,0,0 };
    blade.Initialize({}, &player, ExpEnemyType::BladeGuard);
    blade.bladeCycle_.Reset(0);
    blade.UpdateExpeditionCombat(stage, 0.01f);
    stage.blocks = { { {true, {{0.8f,-2,-1},{1.2f,2,1}}} } };
    for (int frame = 0; frame < 65; ++frame) blade.UpdateExpeditionCombat(stage, 0.01f);
    assert(player.damageCalls == callsBeforeDodge);

    // Even damage rejection consumes one sweep, preventing late-frame retries.
    stage.blocks.clear();
    player.invulnerable = true;
    player.velocity = {};
    blade.Initialize({}, &player, ExpEnemyType::BladeGuard);
    blade.bladeCycle_.Reset(0);
    for (int frame = 0; frame < 65; ++frame) blade.UpdateExpeditionCombat(stage, 0.01f);
    assert(player.damageCalls == callsBeforeDodge + 1 && Length(player.velocity) == 0);
    ExpEnemy armor;
    armor.Initialize({}, &player, ExpEnemyType::ReflectArmor);
    armor.aimDirection_ = {1,0,0};
    assert(armor.TryReflectProjectile({2,0,0}) && armor.GetReflectionCount() == 1);
    assert(!armor.TryReflectProjectile({-2,0,0}) && !armor.TryReflectProjectile({0,2,0}));
    assert(!shield.TryReflectProjectile({2,0,0}));
    const int armorHp = armor.GetHp();
    armor.TakeDirectionalDamage(10, {2,0,0}, true);
    assert(armor.GetHp() == armorHp - 10); // Reflection never mitigates blade/link/area damage.
    assert(!armor.TryReflectProjectile({std::cos(51.0f * 3.14159265f / 180), std::sin(51.0f * 3.14159265f / 180),0}));

    // The real support AI guarantees the full one-second ring, then checks
    // both range and obstruction exactly when the pulse is emitted.
    ExpEnemy jammer;
    player.position = {6,0,0};
    player.empCalls = 0;
    jammer.Initialize({}, &player, ExpEnemyType::EMPJammer);
    jammer.authoredMoveSpeedScale_ = 0;
    jammer.supportPulse_.Reset(6, 1, 0);
    jammer.UpdateExpeditionCombat(stage, .01f);
    assert(jammer.GetCombatPhase() == ExpEnemyCombatPhase::Locked && player.empCalls == 0);
    for (int frame = 0; frame < 99; ++frame) {
        jammer.UpdateExpeditionCombat(stage, .01f);
        assert(player.empCalls == 0);
    }
    jammer.UpdateExpeditionCombat(stage, .02f);
    assert(player.empCalls == 1 && player.empSeconds == 2.5f && jammer.GetEmpPulseCount() == 1);
    for (int frame = 0; frame < 200; ++frame) jammer.UpdateExpeditionCombat(stage, .01f);
    assert(player.empCalls == 1 && jammer.GetCombatShotsFired() == 0);
    jammer.supportPulse_.Reset(6,1,0);
    jammer.UpdateExpeditionCombat(stage,.01f);
    player.position = {12,0,0};
    for (int frame = 0; frame < 105; ++frame) jammer.UpdateExpeditionCombat(stage,.01f);
    assert(player.empCalls == 1); // Leaving the ring avoids the debuff.
    player.position = {6,0,0};
    jammer.supportPulse_.Reset(6,1,0);
    jammer.UpdateExpeditionCombat(stage,.01f);
    stage.blocks = {{{true,{{2,-2,-1},{3,2,1}}}}};
    for (int frame = 0; frame < 105; ++frame) jammer.UpdateExpeditionCombat(stage,.01f);
    assert(player.empCalls == 1); // Cover blocks the emitted EMP.
    stage.blocks.clear();

    EnemyManager manager;
    manager.player_ = &player;
    player.position = {20,0,0};
    auto commander = std::make_unique<ExpEnemy>();
    commander->Initialize({}, &player, ExpEnemyType::SummonerCommander);
    commander->authoredMoveSpeedScale_ = 0;
    ExpEnemy* owner = commander.get();
    manager.enemies_.push_back(std::move(commander));
    for (int frame = 0; frame < 499; ++frame) owner->UpdateExpeditionCombat(stage,.01f);
    assert(!owner->ConsumeSummonRequest());
    for (int frame = 0; frame < 3; ++frame) owner->UpdateExpeditionCombat(stage,.01f);
    assert(owner->summonRequested_);
    manager.UpdateSummonedUnits(stage);
    assert(manager.enemies_.size() == 4 && owner->GetSummonTotal() == 3);
    owner->summonRequested_ = true;
    manager.UpdateSummonedUnits(stage);
    assert(manager.enemies_.size() == 4); // Alive cap is enforced even for a forced request.
    int rewardCalls = 0;
    ExpEnemy::playerDefeatCallback_ = [&](const Vector3&) { ++rewardCalls; };
    const auto expBeforeMinions = player.exp;
    for (size_t i = 1; i < manager.enemies_.size(); ++i) {
        auto& minion = *manager.enemies_[i];
        assert(minion.IsSummonedUnit() && minion.GetSummonerId() == owner->GetCollisionId());
        assert(minion.GetHp() == 12 && minion.GetExpValue() == 0);
        assert(!stage.IsCollisionWithAnyBlock(minion.GetWorldPosition(), .85f));
        minion.TakeDirectionalDamage(999, player.position);
    }
    assert(rewardCalls == 0 && player.exp == expBeforeMinions);
    owner->summonRequested_ = true;
    manager.UpdateSummonedUnits(stage);
    assert(manager.enemies_.size() == 7 && owner->GetSummonTotal() == 6);
    for (size_t i = 4; i < manager.enemies_.size(); ++i) assert(!manager.enemies_[i]->IsDead());
    owner->TakeDirectionalDamage(999,player.position);
    manager.DismissOrphanedSummons();
    for (size_t i = 1; i < manager.enemies_.size(); ++i) assert(manager.enemies_[i]->IsDead());
    assert(rewardCalls == 1); // Cleanup does not award the minions again.
    ExpEnemy::playerDefeatCallback_ = {};
    owner->isDead_ = false; owner->SetHp(75); owner->summonRequested_ = true;
    manager.UpdateSummonedUnits(stage);
    assert(manager.enemies_.size() == 7); // Six lifetime summons cannot be farmed indefinitely.

    ExpEnemy pushed;
    pushed.Initialize({}, &player, ExpEnemyType::BladeGuard);
    stage.blocks = {{{true,{{1.0f,-4,-1},{1.5f,4,1}}}}};
    pushed.ApplyKnockback({1,0,0},.5f);
    assert(pushed.MoveCombatActor(stage,{1,0,0}));
    assert(pushed.GetWallCollisionCount() == 1);
    pushed.MoveCombatActor(stage,{1,0,0});
    assert(pushed.GetWallCollisionCount() == 1); // One wall-smash event per impulse.
    std::cout << "Enemy production integration: guards, reflection cone, EMP warning/range/cover, finite summons/caps/no rewards/cleanup, and wall impulse passed.\n";
}
