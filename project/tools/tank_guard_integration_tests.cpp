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
    Vector3 position{}, movement{ -1,0,0 };
    Bullet() { SetCollisionAttribute(kCollisionAttributePlayerBullet); SetDamage(100); }
    Vector3 GetWorldPosition() const override { return position; }
    void OnCollision(Collider*) override {}
    bool CanClaimRunResource() const { return true; }
    Vector3 GetMove() const { return movement; }
    SpecialKind GetSpecialKind() const { return special; }
};
class Player : public Collider {
public:
    Vector3 position{ 2,0,0 }, velocity{};
    int hp = 1000, damageCalls = 0;
    unsigned exp = 0;
    bool invulnerable = false;
    Vector3 GetWorldPosition() const override { return position; }
    void OnCollision(Collider*) override {}
    bool IsDead() const { return hp <= 0; }
    int GetHp() const { return hp; }
    void TakeDamage(uint32_t amount, float) { ++damageCalls; if (!invulnerable) hp -= static_cast<int>(amount); }
    Vector3 GetMove() const { return velocity; }
    void SetVelocity(Vector3 v) { velocity = v; }
    void AddExp(uint32_t value) { exp += value; }
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
class Stage {
public:
    std::vector<std::vector<Block>> blocks;
    const auto& GetBlocks() const { return blocks; }
    bool IsCollisionWithAnyBlock(Vector3, float) { return false; }
};
ExpEnemy::BalanceConfig ExpEnemy::balanceConfig_{};
ExpEnemy::EnemyInteractionConfig ExpEnemy::enemyInteractionConfig_{};
std::function<void(uint32_t)> ExpEnemy::enemyKillCallback_{};
std::function<void(const Vector3&)> ExpEnemy::playerDefeatCallback_{};
bool ExpEnemy::shapeNeonBillboardEnabled_ = false;
int ExpEnemy::shapeNeonRenderMode_ = 0;
// Ray obstruction uses production ClipCombatRay; walking around a wall is
// covered by the pre-existing navigation suite, not these damage tests.
bool ExpEnemy::MoveCombatActor(Stage&, const Vector3& move) { worldTransform_.translate += move; return false; }
Vector3 ExpEnemy::FindCombatWaypoint(Stage&, const Vector3& target) const { return target; }
#include "guard_enemy_methods.inc"

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
    std::cout << "Guard production integration: directional projectiles/melee, ricochet, kill idempotency, locked sweep, single hit, rear dodge, wall occlusion and rejected-damage knockback passed.\n";
}
