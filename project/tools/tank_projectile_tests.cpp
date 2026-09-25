// Production declarations and method bodies are extracted by the companion
// script. These adapters avoid requiring a graphics device for collision tests.
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <list>
#include <memory>
#include <string>
#include <vector>

struct Vector3 {
    float x=0,y=0,z=0;
    Vector3& operator+=(Vector3 b) { x+=b.x; y+=b.y; z+=b.z; return *this; }
};
struct Vector4 { float x=0,y=0,z=0,w=0; };
Vector3 operator+(Vector3 a,Vector3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vector3 operator-(Vector3 a,Vector3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vector3 operator*(Vector3 a,float b) { return {a.x*b,a.y*b,a.z*b}; }
Vector3 operator*(float a,Vector3 b) { return b*a; }
Vector3 operator/(Vector3 a,float b) { return a*(1.0f/b); }
float Dot(Vector3 a,Vector3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
float Length(Vector3 a) { return std::sqrt(Dot(a,a)); }
Vector3 Normalize(Vector3 a) { return a/Length(a); }
float Rand(float a,float b) { return (a+b)*.5f; }
namespace DirectX { float XMConvertToRadians(float degrees) { return degrees*0.0174532925199433f; } }
struct Matrix4x4 {};
struct Transform { Vector3 scale{1,1,1},rotate{},translate{}; };
Transform InitWorldTransform() { return {}; }
struct Sphere { Vector3 center; float radius; };
struct Segment {};
struct AABB { Vector3 min,max; };
bool IsCollision(Segment,Sphere,float) { return true; }
bool IsCollision(AABB box,Sphere sphere) {
    const Vector3 closest{std::clamp(sphere.center.x,box.min.x,box.max.x),
        std::clamp(sphere.center.y,box.min.y,box.max.y),std::clamp(sphere.center.z,box.min.z,box.max.z)};
    return Length(sphere.center-closest)<=sphere.radius;
}
#include "projectile_types.inc"

struct Object3d {
    void Initialize() {}
    void SetModel(const std::string&) {}
    void SetTransform(Transform) {}
    void SetColor(Vector4) {}
    void Update() {}
};
struct DirectXCommon {};
struct Object3dCommon {};
struct TrailConfig {
    Vector4 startColor,endColor;
    uint32_t interpolationSteps=0,maxPoints=0;
    float lifetime=0,startWidthScale=0,endWidthScale=0,widthCurvePower=0,colorCurvePower=0;
};
struct TrailInstance {
    bool active=false;
    TrailConfig config;
    void SetIsPermanent(bool) {}
    void SetActive(bool value) { active=value; }
    bool IsActive() const { return active; }
    void SetConfig(TrailConfig value) { config=value; }
    void Update(float,Vector3,Vector3,TrailConfig value) { config=value; }
};
struct TrailManager {
    struct DrawStats {};
    std::vector<std::unique_ptr<TrailInstance>> instances;
    void Initialize(DirectXCommon*,Object3dCommon*,const std::string&) {}
    TrailInstance* CreateInstance() { instances.push_back(std::make_unique<TrailInstance>()); return instances.back().get(); }
    void Update(float) {}
    void DrawAll(const Matrix4x4&) {}
    void ClearInstances() { instances.clear(); }
    bool HasDrawableInstances() const { return !instances.empty(); }
    DrawStats GetDrawStats() const { return {}; }
    size_t GetInstanceCount() const { return instances.size(); }
};
struct ParticleManager {
    static ParticleManager* GetInstance() { static ParticleManager instance; return &instance; }
    void EmitNeonImpactEffect(Vector3,Vector3,Vector4,int) { ++impacts; }
    size_t impacts=0;
};
#include "projectile_declarations.inc"

struct TestActor : Collider {
    Vector3 position{};
    uint32_t damageReceived=0;
    int hits=0;
    TestActor() { SetCollisionAttribute(kCollisionAttributeEnemy); SetCollisionMask(kCollisionAttributePlayerBullet); }
    void OnCollision(Collider* other) override { damageReceived+=other->GetDamage(); ++hits; }
    Vector3 GetWorldPosition() const override { return position; }
};
class PlayerDrone : public TestActor {};
class Player : public TestActor {
public:
    Player() { position={-100,-100,0}; SetCollisionAttribute(kCollisionAttributePlayer); SetCollisionMask(kCollisionAttributeEnemyBullet); }
    std::vector<PlayerDrone*> GetDronePtrs() { return {}; }
};
class Enemy : public TestActor {};
class ExpEnemy : public TestActor {};
class EnemyManager { public: std::vector<ExpEnemy*> actors; std::vector<ExpEnemy*> GetEnemyPtrs() { return actors; } };
class Stage {
public:
    struct MergedBlock { AABB aabb; };
    std::vector<MergedBlock> mergedBlocks_;
    void ResolveBulletsCollision(const std::vector<Bullet*>&);
};
#include "projectile_methods.inc"

bool Near(float a,float b) { return std::abs(a-b)<0.0001f; }
std::unique_ptr<Bullet> MakeBullet(int bounces=0,int pierces=0,int splits=0,BulletOwner owner=kPlayer,bool reflect=false) {
    auto bullet=std::make_unique<Bullet>();
    bullet->Initialize({}, {1,0,0},20,owner,reflect,12,3);
    bullet->ConfigureGrowth(bounces,pierces,splits);
    return bullet;
}
std::vector<Bullet*> Living(BulletManager& manager) {
    auto bullets=manager.GetBulletPtrs();
    std::erase_if(bullets,[](const auto* bullet) { return bullet->IsDead(); });
    return bullets;
}
int main() {
    CollisionManager collisions;
    // Growth defaults preserve the arena's unlimited reflect/no fork behavior.
    AttackParam defaults{};
    assert(defaults.maxWallBounces==-1 && defaults.actorPierceCount==0 && defaults.impactSplitCount==0);
    for (int budget : {0,1,2,3,4}) {
        auto bullet=MakeBullet(budget,0,0,kPlayer,true);
        for (int index=0;index<budget;++index) {
            bullet->OnWallImpact({},index%2==0?Vector3{-1,0,0}:Vector3{1,0,0});
            assert(!bullet->IsDead() && bullet->GetRemainingWallBounces()==budget-index-1);
            assert(std::isfinite(Length(bullet->GetMove())) && Near(Length(bullet->GetMove()),1));
        }
        bullet->OnWallImpact({}, {-1,0,0});
        assert(bullet->IsDead());
        assert(bullet->ConsumeGrowthEvents().wallBounces==static_cast<uint32_t>(budget));
        assert(bullet->ConsumeGrowthEvents().wallBounces==0);
    }
    auto legacy=MakeBullet(-1,0,0,kPlayer,true);
    for(int index=0;index<100;++index) legacy->OnWallImpact({}, {-1,0,0});
    assert(!legacy->IsDead() && !legacy->UsesRunProjectileRules() && legacy->GetRemainingWallBounces()==-1);
    assert(legacy->ConsumeGrowthEvents().wallBounces==0);
    auto ordinary=MakeBullet(); ordinary->OnWallImpact({}, {-1,0,0}); assert(ordinary->IsDead());
    for (Vector3 normal : {Vector3{},Vector3{std::numeric_limits<float>::quiet_NaN(),0,0}}) {
        auto bullet=MakeBullet(4,0,2,kPlayer,true); bullet->OnWallImpact({},normal);
        assert(bullet->IsDead() && std::isfinite(Length(bullet->GetMove())));
    }
    // Both collision orders deliver one hit per actor, even during overlap.
    for (bool reverse : {false,true}) {
        auto bullet=MakeBullet(0,1);
        TestActor first,second;
        for(int frame=0;frame<30;++frame) {
            collisions.CheckCollisionPair(reverse?static_cast<Collider*>(&first):bullet.get(),
                reverse?static_cast<Collider*>(bullet.get()):&first);
        }
        assert(first.hits==1 && first.damageReceived==20 && !bullet->IsDead());
        assert(bullet->GetRemainingActorPierces()==0);
        collisions.CheckCollisionPair(&second,bullet.get());
        assert(second.hits==1 && second.damageReceived==20 && bullet->IsDead());
        collisions.CheckCollisionPair(&first,bullet.get()); assert(first.hits==1);
        assert(bullet->ConsumeGrowthEvents().actorPierces==1);
    }
    // A new actor at a reused address is still a different valid target.
    {
        alignas(TestActor) std::byte storage[sizeof(TestActor)];
        auto bullet=MakeBullet(0,2);
        auto* first=std::construct_at(reinterpret_cast<TestActor*>(storage));
        const auto firstId=first->GetCollisionId();
        collisions.CheckCollisionPair(bullet.get(),first); assert(first->hits==1);
        std::destroy_at(first);
        auto* replacement=std::construct_at(reinterpret_cast<TestActor*>(storage));
        assert(replacement->GetCollisionId()!=firstId);
        collisions.CheckCollisionPair(bullet.get(),replacement); assert(replacement->hits==1);
        TestActor copied(*replacement); assert(copied.GetCollisionId()!=replacement->GetCollisionId());
        const auto before=copied.GetCollisionId(); copied=*replacement; assert(copied.GetCollisionId()==before);
        std::destroy_at(replacement);
    }
    // Pierce continues intercepting enemy bullets in either callback order.
    for (bool reverse : {false,true}) {
        auto player=MakeBullet(0,1),enemy=MakeBullet(-1,0,0,kEnemy);
        player->ApplyBulletDurabilityDamage(11); // One durability remains.
        collisions.CheckCollisionPair(reverse?enemy.get():player.get(),reverse?player.get():enemy.get());
        assert(player->IsDead() && Near(enemy->GetBulletHp(),9));
        assert(player->GetRemainingActorPierces()==1); // Interception is not an actor penetration.
    }
    // First impact forks two real lower-damage bullets, conserving expiry,
    // ownership, resource rights and hit guards. Children never fork again.
    for (BulletOwner owner : {kPlayer,kEnemy,kExpEnemyHostile}) {
        BulletManager manager; manager.Initialize(nullptr,nullptr);
        auto parent=MakeBullet(0,0,2,owner); parent->SetCanClaimRunResource(false);
        parent->Update(2.85f); parent->SetWorldPosition({});
        const float expiry=parent->GetRemainingLifetime();
        TestActor target; parent->OnCollision(&target); assert(parent->IsDead());
        manager.Add(std::move(parent)); manager.FlushPendingSplits();
        auto children=Living(manager); assert(children.size()==2);
        assert(manager.GetGrowthStats().impactSplits==(owner==kPlayer?1u:0u) && manager.GetGrowthStats().splitChildrenSpawned==(owner==kPlayer?2u:0u));
        assert(manager.GetTrailInstanceCount()==3);
        for(auto* child:children) {
            assert(child->GetOwner()==owner && !child->CanClaimRunResource());
            assert(child->GetDamage()==11 && Near(child->GetRemainingLifetime(),expiry));
            assert(!child->CanHitActor(&target) && child->UsesRunProjectileRules());
            assert(Near(Length(child->GetMove()),.9f));
            child->OnWallImpact(child->GetWorldPosition(),{-1,0,0});
        }
        manager.FlushPendingSplits();
        assert(manager.GetGrowthStats().splitChildrenSpawned==(owner==kPlayer?2u:0u) && Living(manager).empty());
    }
    {
        BulletManager manager;
        auto parent=MakeBullet(3,1,2,kPlayer,true);
        parent->Update(2.9f); parent->SetWorldPosition({});
        parent->OnWallImpact({}, {-1,0,0});
        manager.Add(std::move(parent)); manager.FlushPendingSplits();
        auto live=Living(manager); assert(live.size()==3);
        for(auto* child:live) {
            assert(child->GetRemainingWallBounces()==2 && child->GetRemainingLifetime()<.101f);
            child->Update(.11f); assert(child->IsDead());
        }
        manager.FlushPendingSplits(); assert(manager.GetGrowthStats().splitChildrenSpawned==2);
        manager.ClearAll(); assert(manager.GetGrowthStats().wallBounces==0 && manager.GetGrowthStats().splitChildrenSpawned==0);
    }
    // End-of-pass spawning never invalidates the collider list or permits a
    // child to damage an extra enemy in its parent's collision iteration.
    {
        BulletManager manager; auto parent=MakeBullet(0,1,2);
        manager.Add(std::move(parent)); Player player; Enemy first;
        EnemyManager actors; ExpEnemy second; actors.actors.push_back(&second);
        collisions.CheckAllCollisions(&player,&first,&manager,&actors);
        assert(first.hits==1 && second.hits==1 && first.damageReceived==20 && second.damageReceived==20);
        assert(manager.GetGrowthStats().actorPierces==1 && manager.GetGrowthStats().splitChildrenSpawned==2);
        collisions.CheckAllCollisions(&player,&first,&manager,&actors);
        assert(first.hits==1 && second.hits==1); // Children inherit both parent targets.
    }
    // Per-owner cap includes all live original and child projectiles. A full
    // budget consumes a pending fork rather than retrying it in a later frame.
    for(size_t count : {size_t{239},size_t{240}}) {
        BulletManager manager;
        auto parent=MakeBullet(3,1,2,kPlayer,true); auto* parentPtr=parent.get();
        manager.Add(std::move(parent));
        for(size_t index=1;index<count;++index) manager.Add(MakeBullet());
        parentPtr->OnWallImpact({}, {-1,0,0}); manager.FlushPendingSplits();
        assert(Living(manager).size()==240 && manager.GetGrowthStats().splitChildrenSpawned==240-count);
        manager.Add(MakeBullet()); assert(Living(manager).size()==240);
        manager.GetBulletPtrs()[1]->Die(); manager.FlushPendingSplits();
        assert(Living(manager).size()==239 && manager.GetGrowthStats().splitChildrenSpawned==240-count);
        manager.Add(MakeBullet(0,0,0,kEnemy)); assert(Living(manager).size()==240);
        manager.Add(MakeBullet(-1)); assert(Living(manager).size()==241); // Arena behavior is not capped.
    }
    // Actual Stage embedded/corner path yields finite outward children; legacy
    // embedded projectiles retain the original immediate-death behavior.
    {
        Stage stage; stage.mergedBlocks_.push_back({{{-1,-1,-1},{1,1,1}}});
        BulletManager manager; manager.Add(MakeBullet(0,0,2));
        manager.Update(stage,0);
        assert(Living(manager).size()==2 && manager.GetGrowthStats().splitChildrenSpawned==2);
        for(auto* child:Living(manager)) {
            assert(child->GetWorldPosition().x<-1.5f && child->GetMove().x<0 && std::isfinite(Length(child->GetMove())));
        }
        auto arena=MakeBullet(-1,0,0,kPlayer,true);
        stage.ResolveBulletsCollision({arena.get()}); assert(arena->IsDead());
        auto grazing=MakeBullet(2,0,2,kPlayer,true);
        grazing->SetVelocity({.01f,1,0}); grazing->OnWallImpact({-1.51f,0,0},{-1,0,0});
        std::vector<std::unique_ptr<Bullet>> children; grazing->AppendImpactChildren(children,2);
        assert(children.size()==2);
        for(const auto& child:children) assert(child->GetMove().x<0 && std::isfinite(Length(child->GetMove())));
    }
    // Firing wires all AttackParam growth fields, including from-muzzle spread.
    {
        BulletManager manager; AttackController attack; attack.SetBulletManager(&manager);
        AttackParam param; param.bulletSpeed=1; param.damage=20; param.reflect=true;
        param.maxWallBounces=4; param.actorPierceCount=1; param.impactSplitCount=2;
        param.impactSplitDamageScale=.4f; param.bulletCount=3; param.spreadAngleDeg=20;
        param.canClaimRunResource=false;
        attack.FireFromMuzzle({}, {1,0,0},param,kPlayer);
        assert(manager.GetBulletCount()==3);
        for(auto* bullet:manager.GetBulletPtrs()) {
            assert(bullet->GetRemainingWallBounces()==4 && bullet->GetRemainingActorPierces()==1 && !bullet->CanClaimRunResource());
            bullet->OnWallImpact({}, {-1,0,0});
        }
        manager.FlushPendingSplits(); assert(Living(manager).size()==9);
        size_t weaker=0;
        for(auto* bullet:Living(manager)) if(bullet->GetDamage()==8) ++weaker;
        assert(weaker==6 && manager.GetGrowthStats().wallBounces==3);
        attack.FireFromMuzzle({}, {},param,kPlayer); assert(manager.GetBulletCount()==9);
        attack.FireFromMuzzle({}, {std::numeric_limits<float>::quiet_NaN(),0,0},param,kPlayer);
        assert(manager.GetBulletCount()==9);
    }
    std::cout << "Production projectile growth PASS: 0/1/2/3/4/unlimited bounces; repeated actor/allocator reuse guards; interception; deferred first-impact forks; owner/resource/lifetime conservation; no recursion; cap; Stage embedded/grazing contacts; AttackParam wiring.\n";
}
