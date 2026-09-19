// Graphics-free adapters execute the production method bodies extracted by
// test_tank_collisions.ps1. The collision/damage algorithms are not duplicated.
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <vector>

struct Vector3 { float x=0,y=0,z=0; Vector3& operator+=(Vector3 b) { x+=b.x;y+=b.y;z+=b.z;return *this; } };
struct Vector4 { float x=0,y=0,z=0,w=0; };
Vector3 operator-(Vector3 a,Vector3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vector3 operator*(Vector3 a,float b) { return {a.x*b,a.y*b,a.z*b}; }
float Length(Vector3 a) { return std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z); }
Vector3 Normalize(Vector3 a) { return a*(1.0f/Length(a)); }
struct Segment {};
struct Sphere { Vector3 center; float radius; };
bool IsCollision(Segment,Sphere,float) { return true; }
enum class ColliderShape { Sphere,Capsule };
constexpr uint32_t kCollisionAttributePlayer=1, kCollisionAttributePlayerBullet=2,
    kCollisionAttributePlayerDrone=4,kCollisionAttributeEnemy=8,kCollisionAttributeEnemyBullet=16;

struct Collider {
    virtual ~Collider()=default;
    virtual void OnCollision(Collider*)=0;
    virtual Vector3 GetWorldPosition() const { return {}; }
    float GetRadius() const { return 1; }
    ColliderShape GetShape() const { return ColliderShape::Sphere; }
    Segment GetSegment() const { return {}; }
    float GetCapsuleRadius() const { return 1; }
    uint32_t GetCollisionMask() const { return 0xffffffffu; }
    uint32_t GetCollisionAttribute() const { return attribute; }
    uint32_t GetDamage() const { return damage; }
    float GetHitPower() const { return 1; }
    uint32_t attribute=0,damage=8;
};
struct ParticleManager {
    static ParticleManager* GetInstance() { static ParticleManager instance; return &instance; }
    void EmitNeonImpactEffect(Vector3,Vector3,Vector4,int) {}
    void EmitNeonDeathEffect(Vector3,Vector4,Vector4,float) {}
};
struct Bullet : Collider {
    Bullet(float hp,float penetration,int owner) : owner_(owner),bulletHp_(hp),penetration_(penetration) {
        attribute=owner==0?kCollisionAttributePlayerBullet:kCollisionAttributeEnemyBullet;
    }
    void OnCollision(Collider*) override;
    void ApplyBulletDurabilityDamage(float);
    bool IsDead() const { return isDead_; }
    int GetOwner() const { return owner_; }
    float GetBulletPenetration() const { return penetration_; }
    bool CanClaimRunResource() const { return canClaimRunResource_; }
    Vector4 GetBulletColor() const { return {}; }
    void Die() { isDead_=true; }
    bool isDead_=false;
    bool canClaimRunResource_=true;
    int owner_;
    float bulletHp_,penetration_;
    Vector3 velocity_{1,0,0};
};
struct DummyPlayer { int xp=0; Vector3 position{30,0,0}; void AddExp(int value) { xp+=value; } Vector3 GetWorldPosition() const { return position; } };
enum class ExpEnemyType { Square, Triangle, Pentagon, Shooter };
struct ExpEnemy : Collider {
    void OnCollision(Collider*) override;
    bool IsHostileToBoss() const { return hostileToBoss_; }
    bool IsDead() const { return isDead_; }
    bool IsRunResource() const { return isRunResource_; }
    ExpEnemyType GetType() const { return type_; }
    Vector3 GetWorldPosition() const override { return worldTransform_.translate; }
    bool ApplyDamage(uint32_t,bool,bool);
    bool TakeDamageFromPlayer(uint32_t);
    bool TakeDamageFromEnemy(uint32_t);
    void TriggerDamageFeedback() {}
    bool isDead_=false,isRunResource_=false,hostileToBoss_=true;
    ExpEnemyType type_=ExpEnemyType::Square;
    int hp_=8,expValue_=7;
    struct { Vector3 translate; } worldTransform_;
    Vector3 velocity_;
    float dt_=1.0f/60.0f,invincibleTimer_=0;
    DummyPlayer* player_=nullptr;
    std::function<void(uint32_t)> enemyKillCallback_;
    std::function<void(Vector3)> playerDefeatCallback_;
    std::function<void(bool)> runResourceClaimCallback_;
};
struct CollisionManager { void CheckCollisionPair(Collider*,Collider*); };
struct EnemyManager {
    ExpEnemy* FindNearestEnemy(const Vector3&,float,bool includeShooters=true) const;
    ExpEnemy* FindNearestRunResource(const Vector3&,float) const;
    std::vector<std::unique_ptr<ExpEnemy>> enemies_;
};
struct Enemy {
    void RegisterExpEnemyKill(uint32_t);
    void HealFromFeeding(int);
    void AdvanceFeedingLevel();
    void RegisterRunResourceClaim();
    Vector3 ResolveMoveTargetPosition();
    Vector3 GetWorldPosition() const { return {}; }
    uint32_t GetDamage() const { return damage_; }
    void SetDamage(uint32_t value) { damage_=value; }
    struct { int healOnExpEnemyKill=6,killsPerLevel=4,maxHpGainPerLevel=35; uint32_t damageGainPerLevel=0;
        bool expEnemyHostile=true,levelingModeEnabled=true;
        float levelingEnterPlayerDistance=22,levelingExitPlayerDistance=14,levelingSearchRadius=80;
    } enemyProgressConfig_;
    struct { uint32_t damage=6; } bossAttackConfig_;
    bool prototypeCombatEnabled_=true,prototypeResourceFocus_=false,isDead_=false;
    bool prototypeResourceTargetActive_=false,levelingModeActive_=false;
    DummyPlayer* player_=nullptr;
    EnemyManager* enemyManager_=nullptr;
    int hp_=500,maxHP_=1000,prototypeBaseMaxHp_=1000,prototypeFeedingHealBudget_=500;
    int expEnemyKillCount_=0,enemyLevel_=1;
    uint32_t enemyExp_=0,damage_=6;
};
struct Contact : Collider { void OnCollision(Collider*) override {} };

#include "tank_collision_methods.inc"

int main() {
    CollisionManager collisions;
    for(bool reverse:{false,true}) {
        Bullet player(1,3,0),boss(5,3,1);
        collisions.CheckCollisionPair(reverse?static_cast<Collider*>(&boss):&player,
                                      reverse?static_cast<Collider*>(&player):&boss);
        assert(player.IsDead());
        assert(boss.bulletHp_==2); // Interception survives either callback order.
        Bullet secondPlayer(1,3,0);
        collisions.CheckCollisionPair(&secondPlayer,&boss);
        assert(secondPlayer.IsDead() && boss.IsDead());
    }
    {
        Bullet player(1,1,0),boss(5,3,1);
        collisions.CheckCollisionPair(&player,&boss);
        assert(player.IsDead() && boss.bulletHp_==4); // Neutral interception differs from the upgrade.
    }
    {
        DummyPlayer player;
        ExpEnemy target; target.player_=&player;
        int callbacks=0;
        target.playerDefeatCallback_=[&](Vector3){++callbacks;};
        Bullet first(1,1,0),second(1,1,0);
        collisions.CheckCollisionPair(&first,&target);
        assert(first.IsDead() && target.isDead_ && player.xp==7 && callbacks==1);
        collisions.CheckCollisionPair(&second,&target);
        assert(player.xp==7 && callbacks==1); // Same-frame double hit awards once.
        ExpEnemy another; another.player_=&player;
        collisions.CheckCollisionPair(&first,&another);
        assert(another.hp_==8 && player.xp==7); // Spent projectile cannot damage another target.
    }
    {
        ExpEnemy food;
        int feeds=0;
        food.enemyKillCallback_=[&](uint32_t){++feeds;};
        Bullet first(5,3,1),second(5,3,1);
        collisions.CheckCollisionPair(&first,&food);
        collisions.CheckCollisionPair(&second,&food);
        assert(food.isDead_ && feeds==1);
    }
    for (bool playerFinalHit : {false,true}) {
        DummyPlayer player;
        ExpEnemy core; core.isRunResource_=true; core.hostileToBoss_=false; core.hp_=42; core.player_=&player;
        int claims=0,ordinaryAwards=0;
        bool owner=false;
        core.runResourceClaimCallback_=[&](bool playerOwned) {
            ++claims; owner=playerOwned;
            assert(!core.TakeDamageFromPlayer(999)); // Reentrant claim must be rejected.
        };
        core.playerDefeatCallback_=[&](Vector3) { ++ordinaryAwards; };
        core.enemyKillCallback_=[&](uint32_t) { ++ordinaryAwards; };
        for (auto attribute : {kCollisionAttributePlayer,kCollisionAttributeEnemy,kCollisionAttributePlayerDrone}) {
            Contact body; body.attribute=attribute; body.damage=999;
            core.OnCollision(&body);
        }
        assert(core.hp_==42 && claims==0); // Ramming cannot claim.
        Bullet neutralShooter(5,3,1); neutralShooter.damage=999; neutralShooter.canClaimRunResource_=false;
        collisions.CheckCollisionPair(&neutralShooter,&core);
        assert(core.hp_==42 && claims==0); // A neutral enemy's stray shot cannot award the rival a core.
        assert(!core.TakeDamageFromPlayer(20)); // Beam/mine/slash direct route.
        Bullet boss(5,3,1); boss.damage=6;
        collisions.CheckCollisionPair(&boss,&core);
        assert(core.hp_==16 && claims==0); // Both owners share exactly one HP pool.
        Contact finisher; finisher.damage=16;
        finisher.attribute=playerFinalHit?kCollisionAttributePlayerBullet:kCollisionAttributeEnemyBullet;
        core.OnCollision(&finisher);
        core.OnCollision(&finisher);
        assert(core.isDead_ && claims==1 && owner==playerFinalHit);
        assert(ordinaryAwards==0 && player.xp==0); // Core rewards are only the explicit claim callback.
    }
    for (bool playerOwner : {false,true}) {
        ExpEnemy core; core.isRunResource_=true;
        int claims=0;
        core.runResourceClaimCallback_=[&](bool owner) { assert(owner==playerOwner); ++claims; };
        assert(playerOwner?core.TakeDamageFromPlayer(8):core.TakeDamageFromEnemy(8));
        assert(!core.TakeDamageFromPlayer(8) && !core.TakeDamageFromEnemy(8) && claims==1);
    }
    {
        EnemyManager manager;
        auto add=[&](float distance,bool core,ExpEnemyType type) {
            auto actor=std::make_unique<ExpEnemy>();
            actor->worldTransform_.translate={distance,0,0}; actor->isRunResource_=core; actor->type_=type;
            auto* result=actor.get(); manager.enemies_.push_back(std::move(actor)); return result;
        };
        auto* shooter=add(1,false,ExpEnemyType::Shooter);
        auto* shape=add(2,false,ExpEnemyType::Square);
        auto* core=add(8,true,ExpEnemyType::Pentagon);
        assert(manager.FindNearestEnemy({},20)==shooter);
        assert(manager.FindNearestEnemy({},20,false)==shape);
        assert(manager.FindNearestRunResource({},20)==core);
        DummyPlayer player;
        Enemy rival; rival.player_=&player; rival.enemyManager_=&manager; rival.prototypeResourceFocus_=true;
        assert(rival.ResolveMoveTargetPosition().x==8 && rival.prototypeResourceTargetActive_);
        player.position.x=13; // An approaching player interrupts foraging.
        assert(rival.ResolveMoveTargetPosition().x==13 && !rival.levelingModeActive_);
        player.position.x=18; // Keep targeting the player until the outer hysteresis boundary.
        assert(rival.ResolveMoveTargetPosition().x==18 && !rival.levelingModeActive_);
        player.position.x=30;
        assert(rival.ResolveMoveTargetPosition().x==8 && rival.levelingModeActive_);
        core->isDead_=true;
        assert(manager.FindNearestRunResource({},20)==nullptr);
        assert(rival.ResolveMoveTargetPosition().x==2 && !rival.prototypeResourceTargetActive_);
        rival.enemyProgressConfig_.levelingModeEnabled=false;
        assert(rival.ResolveMoveTargetPosition().x==30 && !rival.levelingModeActive_);
    }
    {
        Enemy rival; rival.prototypeResourceFocus_=true;
        for(int i=0;i<100;++i) rival.RegisterExpEnemyKill(35);
        assert(rival.enemyLevel_==1 && rival.enemyExp_==0 && rival.hp_==600);
        for(int i=0;i<5;++i) rival.RegisterRunResourceClaim();
        assert(rival.enemyLevel_==6 && rival.maxHP_==1175 && rival.hp_==805);
        for(int i=0;i<1000;++i) rival.RegisterRunResourceClaim();
        assert(rival.enemyLevel_==6 && rival.maxHP_==1175 && rival.hp_==1000);
        assert(rival.prototypeFeedingHealBudget_==0); // Includes HP-growth healing.
        rival.isDead_=true; rival.hp_=0;
        rival.RegisterRunResourceClaim(); rival.RegisterExpEnemyKill(999);
        assert(rival.hp_==0); // A late claim cannot revive a defeated rival.
    }
    {
        Enemy rival; rival.prototypeResourceFocus_=true;
        rival.enemyProgressConfig_.maxHpGainPerLevel=400;
        for(int i=0;i<10;++i) rival.RegisterRunResourceClaim();
        assert(rival.maxHP_==1500 && rival.enemyLevel_==6 && rival.prototypeFeedingHealBudget_==0);
        Enemy legacy;
        for(int i=0;i<4;++i) legacy.RegisterExpEnemyKill(7);
        assert(legacy.enemyLevel_==2 && legacy.enemyExp_==28); // Focus disabled retains old feeding.
    }
    std::cout<<"Production collision/resource methods: interception, single kill/claim, shared HP, weapon ownership, target filtering and capped rival growth passed.\n";
}
