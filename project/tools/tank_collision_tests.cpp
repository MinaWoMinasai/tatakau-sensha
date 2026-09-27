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
#include <fstream>
#include <sstream>
#include <string>
#include "../game/enemy/actor/PrototypeBossCombat.h"
#include "../game/exp/ExpEnemyCombatCycle.h"
#include "../game/player/TankRunModifiers.h"
#include "../game/player/TankSpecialCombat.h"
#include "../game/player/TankShooterAbilities.h"
#include "../game/exp/ExpGuardCombat.h"

struct Vector3 { float x=0,y=0,z=0; Vector3& operator+=(Vector3 b) { x+=b.x;y+=b.y;z+=b.z;return *this; } };
struct Vector4 { float x=0,y=0,z=0,w=0; };
Vector3 operator-(Vector3 a,Vector3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vector3 operator+(Vector3 a,Vector3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
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
    inline static uint64_t nextCollisionId=0;
    uint64_t collisionId=++nextCollisionId;
    uint64_t GetCollisionId() const { return collisionId; }
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
    void SetDamage(uint32_t value) {damage=value;}
    float GetHitPower() const { return 1; }
    uint32_t attribute=0,damage=8;
};
struct ParticleManager {
    static ParticleManager* GetInstance() { static ParticleManager instance; return &instance; }
    void EmitNeonImpactEffect(Vector3,Vector3,Vector4,int) {}
    void EmitNeonDeathEffect(Vector3,Vector4,Vector4,float) {}
};
struct Player;
struct BulletManager;
constexpr int kPlayer=0;
struct Bullet : Collider {
	enum class SpecialKind {None,Rail,SlashWave,ParryReflection};
	struct SpecialImpact {SpecialKind kind;Vector3 position,direction;bool bulletCut;};
	SpecialKind specialKind_=SpecialKind::None;
	std::vector<SpecialImpact> specialImpacts_;
	SpecialKind GetSpecialKind() const {return specialKind_;}
	Vector3 GetPreviousWorldPosition() const {return {};}
	Vector3 GetMove() const {return velocity_;}
    bool IsBoomerang() const {return shooter_.boomerang;}
    bool WasArmorReflected() const {return false;}
    Player* GetSourcePlayer() const {return nullptr;}
    int GetSourceDroneIndex() const {return -1;}
    struct {bool boomerang=false;} shooter_;
    Bullet(float hp,float penetration,int owner) : owner_(owner),bulletHp_(hp),penetration_(penetration) {
        attribute=owner==0?kCollisionAttributePlayerBullet:kCollisionAttributeEnemyBullet;
    }
    void OnCollision(Collider*) override;
    void ApplyBulletDurabilityDamage(float);
    bool CanHitActor(const Collider*) const;
    // This suite exercises legacy collision/resource rules. Impact children
    // use the complete production Bullet/Manager in test_tank_projectiles.ps1.
    void QueueImpactSplit(const Vector3&) {}
    std::vector<uint64_t> hitActorIds_;
    int remainingActorPierces_=0;
    struct { uint32_t actorPierces=0; } growthEvents_;
    bool IsDead() const { return isDead_; }
    int GetOwner() const { return owner_; }
    float GetBulletPenetration() const { return penetration_; }
    bool CanClaimRunResource() const { return canClaimRunResource_; }
    Vector4 GetBulletColor() const { return {}; }
    void Die() { isDead_=true; }
    void ReleaseTrail() { if (trailReleases) ++*trailReleases; }
    int* trailReleases=nullptr;
    bool isDead_=false;
    bool canClaimRunResource_=true;
    int owner_;
    float bulletHp_,penetration_;
    Vector3 velocity_{1,0,0};
};
struct DummyPlayer { int xp=0; Vector3 position{30,0,0}; void AddExp(int value) { xp+=value; } Vector3 GetWorldPosition() const { return position; } };
enum class ExpEnemyType { Square, Triangle, Pentagon, Shooter, Charger, Sniper, ShieldGuard };
struct ExpEnemy : Collider {
    void OnCollision(Collider*) override;
    bool IsHostileToBoss() const { return hostileToBoss_; }
    bool IsDead() const { return isDead_; }
    bool IsRunResource() const { return isRunResource_; }
    bool IsSummonedUnit() const {return false;}
    bool TryReflectProjectile(const Vector3&) {return false;}
    bool IsCombatThreat() const {
        return !isDead_ && !isRunResource_ && (type_ == ExpEnemyType::Shooter ||
            type_ == ExpEnemyType::Charger || type_ == ExpEnemyType::Sniper);
    }
    ExpEnemyType GetType() const { return type_; }
    Vector3 GetWorldPosition() const override { return worldTransform_.translate; }
    bool ApplyDamage(uint32_t,bool,bool);
    bool TakeDamageFromPlayer(uint32_t);
	bool TakeDirectionalDamage(uint32_t,const Vector3&,bool=false);
	uint32_t ResolveShieldDamage(uint32_t,const Vector3&,bool);
	Vector3 aimDirection_{1,0,0};float shieldFlashTimer_=0;uint32_t shieldBlockCount_=0;
    bool TakeDamageFromEnemy(uint32_t);
    void ApplyKnockback(const Vector3&,float);
    void TriggerDamageFeedback() {}
    bool isDead_=false,isRunResource_=false,hostileToBoss_=true;
    ExpEnemyType type_=ExpEnemyType::Square;
    int hp_=8,expValue_=7;
    struct { Vector3 translate; } worldTransform_;
    Vector3 velocity_;
    ExpEnemyCombatCycle combatCycle_{};
    float dt_=1.0f/60.0f,invincibleTimer_=0;
    float dashTimer_=0,dashWarningTimer_=0,dashCooldown_=0;
    float wallImpactArmedTimer_=0;
    DummyPlayer* player_=nullptr;
    std::function<void(uint32_t)> enemyKillCallback_;
    std::function<void(Vector3)> playerDefeatCallback_;
    std::function<void(bool)> runResourceClaimCallback_;
};
struct CollisionManager { void CheckCollisionPair(Collider*,Collider*); BulletManager* activeBulletManager_=nullptr; };
struct EnemyManager {
    ExpEnemy* FindNearestEnemy(const Vector3&,float,bool includeShooters=true) const;
    ExpEnemy* FindNearestRunResource(const Vector3&,float) const;
    std::vector<std::unique_ptr<ExpEnemy>> enemies_;
    void ClearRunActors();
    void ClearLevelData();
    std::vector<int> spawnAreas_;
    float spawnTimer_=0;
    bool defaultRandomSpawnEnabled_=true;
};
struct TestTransform { Vector3 translate,rotate,scale; };
struct TestObject {
    void SetColor(Vector4) {}
    void SetTransform(TestTransform value) { transform=value; }
    void Update() {}
    TestTransform transform;
};
struct Enemy : Collider {
    void OnCollision(Collider*) override {}
    bool IsDead() const { return isDead_; }
    int GetHp() const {return hp_;}
    bool IsRunEncounterEnabled() const { return runEncounterEnabled_; }
    void TakeDamage(uint32_t amount) { hp_-=static_cast<int>(amount); }
    void ApplyKnockback(const Vector3&,float);
    void RegisterExpEnemyKill(uint32_t);
    void HealFromFeeding(int);
    void AdvanceFeedingLevel();
    void RegisterRunResourceClaim();
    Vector3 ResolveMoveTargetPosition();
    Vector3 GetWorldPosition() const { return {}; }
    uint32_t GetDamage() const { return damage_; }
    void SetDamage(uint32_t value) { damage_=value; }
    struct EnemyProgressConfig { int healOnExpEnemyKill=6,killsPerLevel=4,maxHpGainPerLevel=35; uint32_t damageGainPerLevel=0;
        bool expEnemyHostile=true,levelingModeEnabled=true;
        float levelingEnterPlayerDistance=22,levelingExitPlayerDistance=14,levelingSearchRadius=80;
    } enemyProgressConfig_;
    struct { uint32_t damage=6; } bossAttackConfig_;
    bool prototypeCombatEnabled_=true,prototypeResourceFocus_=false,isDead_=false,expeditionRivalEnabled_=false;
    bool prototypeResourceTargetActive_=false,levelingModeActive_=false;
    DummyPlayer* player_=nullptr;
    EnemyManager* enemyManager_=nullptr;
    int hp_=500,maxHP_=1000,prototypeBaseMaxHp_=1000,prototypeFeedingHealBudget_=500;
    int expEnemyKillCount_=0,enemyLevel_=1;
    uint32_t enemyExp_=0,damage_=6;
    void SetRunEncounterEnabled(bool);
    void ResetRunEncounter(const Vector3&,int,int,bool);
    void SetPrototypeMaxHp(int,bool=true);
    void SetPrototypePressure(int value) { prototypePressure_=std::clamp(value,0,4); }
    void SetEnemyProgressConfig(EnemyProgressConfig value) { enemyProgressConfig_=value; }
    void UpdateHPBar() {}
    bool runEncounterEnabled_=true,runEncounterBaselineCaptured_=false,isExploding_=false,isWallFollowing_=false;
    uint32_t runEncounterBaseContactDamage_=0,runEncounterBaseBulletDamage_=0,time_=0;
    PrototypeBossCombat prototypeCombat_;
    int prototypePressure_=0,fireIntervalTimer=0,alternatingShotIndex_=0;
    float radius_=2,deathChargeTimer_=0,deathEffectTimer_=0,damageFeedbackTimer_=0;
    float attackPower=0,evadePower=0,wanderPower=0,wallFollowTimer_=0,steeringNoiseTimer_=0,hesitationTimer_=0;
    float hesitationCooldown_=0,wanderChangeTimer=0,bulletCooldown_=0,fireTimer_=0,kFireTimerMax_=0.15f;
    Vector3 currentMoveTargetPosition_,velocity_,dir_,evadeVec,wanderVec,wallFollowDir_,steeringDir_,steeringNoise_,baseScale_{1,1,1};
    Vector3 impactVelocity_;
    enum class AIState { Wander,Attack };
    AIState aiState_=AIState::Wander;
    Vector4 baseColor_;
    TestTransform worldTransform_;
    TestObject* object_=nullptr;
};
struct Player : Collider {
    void OnCollision(Collider*) override { ++contactCallbacks; }
    Vector3 GetWorldPosition() const override { return {}; }
    bool TryDashImpact(Collider*);
    void ArmWallSmash(ExpEnemy*,float=1) {}
    uint32_t NotifyDroneHit(int,Collider*,uint32_t amount) {return amount;}
    struct DashImpactEvent { Vector3 origin,direction; bool boss=false,powered=false; };
    std::vector<DashImpactEvent> pendingDashImpacts_;
    std::vector<uint64_t> dashImpactTargets_;
    bool isDead_=false,isDashing_=true;
    float kDashDuration=.3f,kJustEvadeWindow=.2f,dashTimer_=.25f;
    Vector3 velocity_{.6f,0,0};
    TankRunModifiers runModifiers_{};
    struct { float bulletDamage=10,bodyDamage=3; } stats_;
    int contactCallbacks=0;
};
struct Contact : Collider { void OnCollision(Collider*) override {} };
struct TestTrailManager {
    void ClearInstances() { assert(!releaseCounter || *releaseCounter==2); ++clearCalls; }
    int clearCalls=0;
    int* releaseCounter=nullptr;
};
struct BulletManager {
    void ClearAll();
    std::vector<std::unique_ptr<Bullet>> bullets_;
	std::vector<Bullet::SpecialImpact> specialImpacts_;
    std::unique_ptr<TestTrailManager> trailManager_;
    struct { uint64_t wallBounces=0,actorPierces=0,impactSplits=0,splitChildrenSpawned=0; } growthStats_;
    void QueueArmorReflection(const Bullet&,const Vector3&) {}
    void NotifyPlayerHit(Bullet&,Collider&,bool) {}
    std::vector<int> pendingBuildShots_,buildEvents_;
    tankshooter::MarkLedger marks_{};
    struct {uint64_t count=0;} shooterStats_;
    void* combatPlayer_=nullptr;void* combatBoss_=nullptr;void* combatEnemies_=nullptr;void* combatStage_=nullptr;
};
enum class MapChipType { Blank,Wall,Hazard };
struct MapChip {
    static constexpr uint32_t kNumBlockHorizontal=45,kNumBlockVirtical=30;
    struct { std::vector<std::vector<MapChipType>> data; } mapChipData_;
    void ResetMapChipData() { mapChipData_.data.assign(30,std::vector<MapChipType>(45,MapChipType::Blank)); }
};
struct Stage {
    bool LoadRunMap(const std::string&);
    void ClearBlocksForPreview() { ++clears; }
    void GenerateBlocks() { ++generates; }
    std::unique_ptr<MapChip> mapChip_;
    int clears=0,generates=0;
};

#include "tank_collision_methods.inc"

int main() {
    CollisionManager collisions;
    for(bool reverse : {false,true}) {
        Player tank; tank.attribute=kCollisionAttributePlayer;
        ExpEnemy target; target.hp_=200; target.attribute=32;
        for(int frame=0;frame<120;++frame) {
            collisions.CheckCollisionPair(reverse?static_cast<Collider*>(&target):&tank,
                                          reverse?static_cast<Collider*>(&tank):&target);
        }
        assert(target.hp_==172 && tank.pendingDashImpacts_.size()==1 && tank.contactCallbacks==0);
        assert(target.velocity_.x>.61f && target.velocity_.x<.63f);
        tank.dashTimer_=.01f;
        assert(tank.TryDashImpact(&target)); // Remnant overlap stays resolved for this dash.
        ExpEnemy second; second.hp_=200;
        assert(!tank.TryDashImpact(&second)); // Late contact cannot create another slam.
        tank.dashImpactTargets_.clear(); tank.dashTimer_=.25f;
        assert(tank.TryDashImpact(&target) && target.hp_==144);
        Bullet projectile(5,1,1); assert(!tank.TryDashImpact(&projectile));
        ExpEnemy resource;resource.isRunResource_=true;assert(!tank.TryDashImpact(&resource));
        Enemy boss; const int oldHp=boss.hp_;
        assert(tank.TryDashImpact(&boss) && boss.hp_==oldHp-28);
        assert(boss.impactVelocity_.x>.21f && boss.impactVelocity_.x<.23f);
        tank.isDashing_=false;assert(!tank.TryDashImpact(&second));
    }
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
    {
        Enemy rival; TestObject object; rival.object_=&object;
        rival.ResetRunEncounter({26,28,0},300,2,true);
        rival.RegisterRunResourceClaim();
        rival.isDead_=rival.isExploding_=true; rival.radius_=0; rival.deathEffectTimer_=2;
        rival.velocity_={9,8,0}; rival.damageFeedbackTimer_=1;
        for(int i=0;i<15;++i) rival.prototypeCombat_.Step(.1f,true,1,1,2,false);
        rival.ResetRunEncounter({62,30,0},650,9,false);
        assert(!rival.isDead_ && !rival.isExploding_ && rival.radius_==2);
        assert(rival.hp_==650 && rival.maxHP_==650 && rival.enemyLevel_==1 && rival.expEnemyKillCount_==0);
        assert(rival.prototypePressure_==4 && rival.prototypeCombat_.GetPhase()==PrototypeBossCombat::Phase::Recovery);
        assert(!rival.prototypeResourceFocus_ && !rival.enemyProgressConfig_.levelingModeEnabled);
        assert(rival.velocity_.x==0 && rival.damageFeedbackTimer_==0 && rival.deathEffectTimer_==0);
        assert(object.transform.translate.x==62 && object.transform.translate.y==30);
        rival.SetRunEncounterEnabled(false);
        const auto hp=rival.hp_; rival.RegisterExpEnemyKill(100); rival.RegisterRunResourceClaim();
        assert(!rival.runEncounterEnabled_ && !rival.isDead_ && rival.hp_==hp && rival.enemyLevel_==1);
        EnemyManager actors; actors.enemies_.push_back(std::make_unique<ExpEnemy>()); actors.spawnAreas_.push_back(1);
        actors.ClearRunActors(); assert(actors.enemies_.empty() && actors.spawnAreas_.empty() && !actors.defaultRandomSpawnEnabled_);
    }
    {
        BulletManager bullets; int released=0;
        bullets.trailManager_=std::make_unique<TestTrailManager>(); bullets.trailManager_->releaseCounter=&released;
        for(int i=0;i<2;++i) { auto b=std::make_unique<Bullet>(1.0f,1.0f,0); b->trailReleases=&released; bullets.bullets_.push_back(std::move(b)); }
        bullets.ClearAll(); assert(bullets.bullets_.empty() && released==2 && bullets.trailManager_->clearCalls==1);
    }
    {
        Stage stage;
        for(const char* name : {"crossfire","resource_fork","hazard_lane","final_duel"}) {
            assert(stage.LoadRunMap(std::string("../../project/resources/maps/expedition_")+name+".csv"));
            assert(stage.mapChip_->mapChipData_.data.size()==30 && stage.mapChip_->mapChipData_.data[0].size()==45);
        }
        const auto* previous=stage.mapChip_.get();
        assert(stage.mapChip_->mapChipData_.data[10][18]==MapChipType::Blank);
        assert(stage.mapChip_->mapChipData_.data[10][16]==MapChipType::Wall);
        std::ofstream("invalid_room.csv")<<"0,1,2\n";
        assert(!stage.LoadRunMap("invalid_room.csv") && previous==stage.mapChip_.get() && stage.clears==4 && stage.generates==4);
        assert(!stage.LoadRunMap("missing_room.csv") && previous==stage.mapChip_.get());
        std::ostringstream invalid;
        for(int y=0;y<30;++y) {
            for(int x=0;x<45;++x) invalid<<(x?",":"")<<(x==44&&y==29?9:0);
            invalid<<'\n';
        }
        std::ofstream("invalid_room.csv")<<invalid.str();
        assert(!stage.LoadRunMap("invalid_room.csv") && previous==stage.mapChip_.get() && stage.clears==4);
    }
    std::cout<<"Production collision/resource/encounter methods: rewards, ownership, capped growth, reset, cleanup and map replacement passed.\n";
}
