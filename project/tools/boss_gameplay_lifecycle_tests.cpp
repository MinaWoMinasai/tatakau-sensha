// Exercise the real Enemy declaration and production gameplay methods with
// adapters for GPU objects, particles and unrelated AI/navigation helpers.
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include "../game/enemy/actor/PrototypeBossCombat.h"
#include "../game/enemy/actor/RivalBossCombat.h"
#include "../game/enemy/actor/NeonDepthCombat.h"
#include "../game/enemy/actor/NeonDepthFloor.h"
#include "../game/collision/CollisionConfig.h"

namespace cg2 {
struct Vector2 { float x = 0, y = 0; };
struct Vector3 {
    float x = 0, y = 0, z = 0;
    Vector3& operator+=(Vector3 b) { x += b.x; y += b.y; z += b.z; return *this; }
};
struct Vector4 { float x = 0, y = 0, z = 0, w = 0; };
Vector3 operator+(Vector3 a, Vector3 b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
Vector3 operator-(Vector3 a, Vector3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
Vector3 operator*(Vector3 a, float b) { return {a.x*b, a.y*b, a.z*b}; }
Vector3 operator*(float a, Vector3 b) { return b*a; }
Vector3 operator/(Vector3 a, float b) { return a*(1.0f/b); }
float Length(Vector3 v) { return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z); }
Vector3 Normalize(Vector3 v) { return Length(v) > 0 ? v/Length(v) : Vector3{}; }
struct Transform { Vector3 scale{1,1,1}, rotate{}, translate{}; };
struct AABB { Vector3 min, max; };
struct Segment { Vector3 origin, diff; };
enum Axis { X, Y };
struct Object3d {
    void SetTransform(Transform t) { transform = t; }
    void Update() { ++updates; }
    void SetColor(Vector4) {}
    Transform transform;
    int updates = 0;
};
struct Sprite {
    Vector2 GetSize() const { return size; }
    void SetSize(Vector2 value) { size = value; }
    void Update() {}
    Vector2 size{};
};
struct ParticleManager {
    static ParticleManager* GetInstance() { static ParticleManager instance; return &instance; }
    void EmitNeonDeathEffect(Vector3, Vector4, Vector4, float) { ++deaths; }
    int deaths = 0;
};
}
using cg2::Vector3;
#include "boss_collider.inc"
class Player : public Collider {
public:
    Vector3 GetWorldPosition() const override { return {5,2,0}; }
    void OnCollision(Collider*) override {}
};
class BulletManager {};
enum class BulletOwner { kEnemy };
struct AttackParam {
    float bulletSpeed = 0, spreadAngleDeg = 0, cooldown = 0, bulletHp = 0, bulletPenetration = 0;
    int bulletCount = 1;
    uint32_t damage = 0;
    bool randomSpread = false, reflect = false, penetrate = false;
};
class AttackController {
public:
    void SetBulletManager(BulletManager*) {}
    void Fire(Vector3, Vector3, AttackParam, BulletOwner) { ++shots; }
    int shots = 0;
};
#include "boss_enemy_declarations.inc"
class Stage {
public:
    void ResolveEnemyCollision(Enemy&, cg2::Axis) { ++resolutions; }
    int resolutions = 0;
};
class ExpEnemy : public Collider {
public:
    Vector3 GetWorldPosition() const override { return {1,0,0}; }
    void OnCollision(Collider*) override {}
    bool IsDead() const { return false; }
    bool IsRunResource() const { return false; }
    bool TakeDamageFromEnemy(uint32_t) { ++hits; return false; }
    uint32_t GetExpValue() const { return 7; }
    int hits = 0;
};
static int aiCalls = 0, prototypeUpdates = 0, rivalUpdates = 0;
Enemy::~Enemy() = default;
void Enemy::UpdateHPBar() {}
void Enemy::ApplyDamageFeedback(float) {}
// Preserve the legacy gameplay fixture; the actual Depth adapter runs in its own runtime suite.
void Enemy::EnableNeonDepthEncounter(bool enabled, const neondepth::Tuning&) { assert(!enabled); neonDepthEnabled_=false; }
void Enemy::AbortNeonDepthEncounter() { assert(!neonDepthEnabled_); }
void Enemy::UpdateNeonDepthCombat(float) { assert(!neonDepthEnabled_); }
void Enemy::UpdateRivalCombat(float) { ++rivalUpdates; }
void Enemy::UpdatePrototypeCombat(float) { ++prototypeUpdates; }
void Enemy::UpdateAIState() { ++aiCalls; }
void Enemy::AIStateMovePower() { ++aiCalls; }
Vector3 Enemy::ResolveMoveTargetPosition() { ++aiCalls; return player_->GetWorldPosition(); }
std::optional<Vector3> Enemy::FindPathDirectionToTarget(const Vector3&) { return std::nullopt; }
Vector3 Enemy::RandomDirection() { return {}; }
Vector3 Enemy::EvadeBullets() { return {}; }
Vector3 Enemy::ApplyHumanLikeSteering(const Vector3& direction, bool, float) { return direction; }
void Enemy::RotateTowardTarget(const Vector3&, float) { ++aiCalls; }
bool Enemy::HasLineOfSightToTarget(const Vector3&) const { return true; }
static Vector3 RotateDirection2D(Vector3 direction, float degrees) {
    const float angle = degrees * 3.1415926535f / 180;
    return {direction.x*std::cos(angle)-direction.y*std::sin(angle),
            direction.x*std::sin(angle)+direction.y*std::cos(angle), direction.z};
}
// Existing Enemy has a deltaTime member alongside deltaTime parameters.
#pragma warning(push)
#pragma warning(disable: 4458)
#include "boss_enemy_methods.inc"
#pragma warning(pop)

struct Contact : Collider {
    Vector3 GetWorldPosition() const override { return {1,0,0}; }
    void OnCollision(Collider*) override {}
};
static void Prepare(Enemy& boss, cg2::Object3d& object, Player& player, Stage& stage) {
    boss.object_ = &object;
    boss.player_ = &player;
    boss.stage_ = &stage;
    boss.sprite = std::make_unique<cg2::Sprite>();
    boss.bossHpRed = std::make_unique<cg2::Sprite>();
    boss.bossHpFont = std::make_unique<cg2::Sprite>();
    boss.worldTransform_.translate = {3,4,0};
    boss.currentMoveTargetPosition_ = player.GetWorldPosition();
}

int main() {
    cg2::Object3d object;
    Player player;
    Stage stage;
    Enemy boss;
    Prepare(boss, object, player, stage);
    boss.hp_ = 10;
    boss.TakeDamage(0);
    assert(boss.GetHp() == 10 && !boss.IsDead());
    boss.TakeDamage(9);
    assert(boss.GetHp() == 1 && !boss.IsDead() && boss.GetDamageFeedbackRatio() > 0);
    boss.Fire();
    boss.ShotgunFire();
    assert(boss.GetShotsFired() == 2 && boss.attackController_.shots == 2);
    boss.velocity_ = boss.impactVelocity_ = boss.dir_ = {0.4f,0.2f,0};
    boss.rivalDashDistance_ = 10;
    boss.levelingModeActive_ = boss.prototypeResourceTargetActive_ = true;
    const int beforeDeaths = cg2::ParticleManager::GetInstance()->deaths;
    boss.TakeDamage(std::numeric_limits<uint32_t>::max());
    assert(boss.GetHp() == 0 && boss.IsDead());
    assert(boss.GetRadius() == 0 && cg2::Length(boss.GetMove()) == 0 && cg2::Length(boss.impactVelocity_) == 0);
    assert(boss.rivalDashDistance_ == 0 && !boss.levelingModeActive_ && !boss.prototypeResourceTargetActive_);
    assert(cg2::ParticleManager::GetInstance()->deaths == beforeDeaths + 1);
    const auto deathPosition = boss.GetWorldPosition();
    const auto deathPhase = boss.GetPrototypeCombatPhase();
    boss.Die();
    boss.TakeDamage(99);
    boss.Fire();
    boss.ShotgunFire();
    boss.RegisterExpEnemyKill(99); // Includes legacy (non-prototype) feeding.
    boss.ApplyKnockback({1,0,0}, 1);
    ExpEnemy food;
    food.SetCollisionAttribute(kCollisionAttributeExpEnemy);
    boss.enemyProgressConfig_.expEnemyHostile = true;
    boss.OnCollision(&food);
    assert(food.hits == 0 && boss.GetHp() == 0 && boss.GetEnemyExp() == 0);
    assert(boss.GetShotsFired() == 2 && boss.attackController_.shots == 2);
    assert(cg2::ParticleManager::GetInstance()->deaths == beforeDeaths + 1);
    for (int frame = 0; frame < 150; ++frame) boss.Update(0.02f);
    boss.Move(0.02f);
    assert(aiCalls == 0 && prototypeUpdates == 0 && rivalUpdates == 0 && stage.resolutions == 0);
    assert(cg2::Length(boss.GetWorldPosition() - deathPosition) == 0);
    assert(boss.GetPrototypeCombatPhase() == deathPhase && !boss.GetRivalCombatStatus().enabled);
    assert(boss.isFinished());
    boss.SetPrototypeMaxHp(999, true);
    assert(boss.IsDead() && boss.GetHp() == 0);
    const auto oldGeneration = boss.GetEncounterGeneration();
    boss.ResetRunEncounter({10,12,0}, 120, 2, false);
    assert(boss.GetEncounterGeneration() == oldGeneration + 1 && !boss.IsDead() && boss.GetHp() == 120);
    assert(boss.GetShotsFired() == 0 && boss.GetRadius() == 2);
    boss.ResetRunEncounter({10,12,0}, 100, 1, false);
    assert(boss.GetEncounterGeneration() == oldGeneration + 2);
    for (auto attribute : {kCollisionAttributePlayerBullet, kCollisionAttributeHostileExpEnemyBullet}) {
        boss.ResetRunEncounter({10,12,0}, 10, 1, false);
        Contact bullet;
        bullet.SetCollisionAttribute(attribute);
        bullet.SetDamage(10);
        boss.OnCollision(&bullet);
        assert(boss.IsDead() && boss.GetHp() == 0); // No subsequent Update needed.
    }
    // HP-zero safety also covers externally established legacy state before AI.
    boss.ResetRunEncounter({10,12,0}, 10, 1, false);
    boss.hp_ = 0;
    boss.Update(0.02f);
    assert(boss.IsDead() && aiCalls == 0 && prototypeUpdates == 0 && rivalUpdates == 0);
    boss.ResetRunEncounter({10,12,0}, 10, 1, false);
    boss.SetRunEncounterEnabled(false);
    boss.TakeDamage(100);
    boss.Fire();
    boss.ShotgunFire();
    boss.OnCollision(&food);
    boss.Update(0.02f);
    assert(boss.GetHp() == 10 && !boss.IsDead() && boss.GetShotsFired() == 0);
    // A living actor retains its update routes and does not inherit the terminal state.
    boss.SetRunEncounterEnabled(true);
    boss.Update(0.02f);
    assert(aiCalls > 0 && prototypeUpdates == 1 && stage.resolutions > 0);
    std::cout << "Boss production lifecycle: immediate lethal collision/direct damage, overflow, AI/attack/feeding stop, retained death presentation, explicit encounter generations passed.\n";
}
