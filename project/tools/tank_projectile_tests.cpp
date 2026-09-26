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
#include <unordered_map>
#include <vector>
#include "../game/player/TankRunModifiers.h"
#include "../game/player/TankSpecialCombat.h"
#include "../game/player/TankExpeditionLoadout.h"
#include "../game/player/TankCombatStyleBalance.h"
#include "../game/run/TankBuildStyle.h"

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
struct TestInput {
    struct Mouse { unsigned char rgbButtons[3]{}; } mouse;
    const Mouse& GetMouseState() const {return mouse;}
    bool IsPress(unsigned char value) const {return value!=0;}
};
class PlayerDrone : public TestActor {
public:
	bool IsDead() const { return false; }
    void Initialize(Vector3 start,Vector3) {position=start;}
    void SetAttackControllerBulletManager(BulletManager* manager) {runBulletManager_=manager;attackController_.SetBulletManager(manager);}
    void SetRunInput(Vector3 target,bool shoot) {runInputOverride_=true;runWantsAttack_=shoot;dir=Length(target-position)>.001f?Normalize(target-position):Vector3{1,0,0};}
    void ConfigureRunAttack(const AttackParam&,float);
    float followSpeed=.25f,catchupSpeed=.62f,followResponse=5;
    void SetRunFollowTuning(float speed,float catchup,float response) {followSpeed=speed;catchupSpeed=catchup;followResponse=response;}
    void Attack(float);
    bool runAttackEnabled_=false,runRallyShotPending_=false,runInputOverride_=false,runWantsAttack_=false;
    AttackParam runAttackParam_{};
    float runReloadSeconds_=.5f,runShotCooldown_=0;
    BulletManager* runBulletManager_=nullptr;
    AttackController attackController_;
    TestInput input;
    TestInput* input_=&input;
    Vector3 dir{1,0,0};
    int bulletCoolTime=0;
    static constexpr int kBulletTime=30;
};
enum class WeaponType {Projectile};
struct RunEvolutionChoice {std::string id,name,description;};
class Enemy;class EnemyManager;
class Player : public TestActor {
public:
    Player() { position={-100,-100,0}; SetCollisionAttribute(kCollisionAttributePlayer); SetCollisionMask(kCollisionAttributeEnemyBullet); }
    std::vector<PlayerDrone*> GetDronePtrs() { return {}; }
    bool TryDashImpact(Collider*) { return false; } // Body-slam production path covered by collision suite.
    void ApplyRunProjectileRules(AttackParam&,bool=true) const;
    struct Mount {bool fires=true;WeaponType weaponType=WeaponType::Projectile;float angleDeg=0,damageScale=1,reloadScale=1,projectileSpeedScale=1,muzzleForward=1.7f;Vector3 offset;};
    struct PlayerClassConfig {
        bool reflect=false,penetrate=false,usesDrone=false,randomSpread=false;
        int maxDrones=0,bulletCount=1;
        float bulletSpeedScale=1,bulletDamageScale=1,reloadScale=1,spreadAngleDeg=0;
        std::string id="Basic",displayName="Basic";
        std::vector<Mount> barrels{Mount{}};
    } authoredConfig,runStarterConfig_,runEvolutionConfig_;
    const PlayerClassConfig* GetClassConfig(const std::string& id) const {return id=="Basic"?&authoredConfig:nullptr;}
    const PlayerClassConfig* GetCurrentClassConfig() const { return runEvolutionActive_?&runEvolutionConfig_:expeditionCombatStyleSelected_?&runStarterConfig_:&authoredConfig; }
    bool IsDroneBuild() const {return runModifiers_.enabled&&expeditionCombatStyleSelected_&&expeditionCombatStyle_==tankbuild::Style::Drone;}
	bool IsMeleeBuild() const {return runModifiers_.enabled&&expeditionCombatStyleSelected_&&expeditionCombatStyle_==tankbuild::Style::Melee;}
	struct MeleeSlashEvent {
		Vector3 origin{},direction{1,0,0};float range=4.3f,arcDeg=120,duration=.14f,windupDuration=.05f;
		int comboStep=0;uint32_t damage=20;
	};
	struct DroneLaserLink {Vector3 start{},end{};bool contact=false;};
	enum class SpecialEventKind { RailShot, Parry, PerfectParry, LinkHit };
	struct SpecialCombatEvent {SpecialEventKind kind;Vector3 origin,direction;float strength;};
	struct SpecialCombatStats {uint32_t railShots=0,slashWaves=0,parries=0,perfectParries=0,linkTicks=0;};
	struct Barrel {float muzzleFlashTimer=0,recoilOffset=0;};
	std::vector<Barrel> barrels_;
	static constexpr float kMuzzleFlashDuration=.075f;
	Vector3 dir_{1,0,0},velocity_{};
	tankspecial::RailCharge railCharge_;
	tankspecial::LinkDamageClock linkDamageClock_;
	std::vector<DroneLaserLink> droneLaserLinks_;
	std::vector<SpecialCombatEvent> pendingSpecialCombatEvents_;
	SpecialCombatStats specialCombatStats_;
	MeleeSlashEvent specialMeleeSwing_;
	float specialMeleeElapsed_=-1;
	bool specialWaveEmitted_=false,specialPerfectFeedback_=false,primaryAttackPerformedEvent_=false;
	uint32_t primaryAttackCount_=0;
	std::vector<uint64_t> specialParriedBullets_;
	Vector3 RotateDirection(Vector3 dir,float degrees) const {const float a=degrees*.01745329252f;return {dir.x*std::cos(a)-dir.y*std::sin(a),dir.x*std::sin(a)+dir.y*std::cos(a),dir.z};}
	void AttackRailCannon(BulletManager*,bool,float);
	void UpdateSpecialCombat(Stage&,BulletManager*,Enemy*,EnemyManager*,float);
	Vector3 GetRailChargeMuzzle() const;
	std::vector<SpecialCombatEvent> ConsumeSpecialCombatEvents();
    bool SetExpeditionCombatStyle(tankbuild::Style);
    int GetExpeditionDroneLimit() const;
    void EnsureExpeditionDrones();
    void ConfigureRunDrone(PlayerDrone&) const;
    float GetRunFireIntervalScale() const;
    float GetRunBaseReloadFrames() const;
    void ApplyCombatStyleBalance(const TankCombatStyleBalances&);
    void SetRunModifiers(const TankRunModifiers&);
    void SetRunCurrencyMode(bool enabled) {if(!enabled)runCurrencyEarned_=0;}
    float upgradeHudListVisibility_=0,runOverdriveCooldown_=0;
    bool upgradeHudMouseCaptured_=false,runRoomAwaitInputRelease_=false,runDashBurstPending_=false,runCheckpointEvolution_=false;
    std::vector<Vector3> runHomingTargets_;
    TankCombatStyleBalances combatStyleBalances_=DefaultTankCombatStyleBalances();
    const TankCombatStyleProfile& GetCombatStyleProfile(tankbuild::Style style) const {return combatStyleBalances_[static_cast<size_t>(style)];}
    std::vector<RunEvolutionChoice> GetRunAuthoredEvolutionChoices() const;
    bool ChooseRunAuthoredClass(const std::string&);
    std::vector<RunEvolutionChoice> runAuthoredChoices_;
    std::unordered_map<std::string,tankbuild::Style> runAuthoredStyles_;
    std::unordered_map<std::string,PlayerClassConfig> runAuthoredClasses_;
    void RecalculateStatsFromBase(bool);
    int GetMaxHp() const {return static_cast<int>(stats_.maxHp);}
    std::array<int,7> upgradeLevels_{};
    float healthRegenUpgradeRate_=.08f,maxHpUpgradeRate_=.1f,bodyDamageUpgradeRate_=.1f,bulletSpeedUpgradeRate_=.08f;
    float bulletDamageUpgradeRate_=.1f,reloadUpgradeRate_=.07f,moveSpeedUpgradeRate_=.06f,minReloadSpeed_=3;
    TankExpeditionMaintenance runMaintenance_;
    bool isChangeMode=false,evolutionConfirmedEvent_=false;
    void InitializeBarrels() {}
    void UpdateBarrelLayout() {}
    tankbuild::Style expeditionCombatStyle_=tankbuild::Style::Shooter;
    bool expeditionCombatStyleSelected_=false,isDead_=false,runEvolutionActive_=false,runAuthoredEvolutionActive_=false,runEvolutionPrepared_=false;
    int dummyObject=1;
    int* object_=&dummyObject;
    BulletManager* runBulletManager_=nullptr;
    float bulletCoolTime=0,meleeComboTimer_=0,runSupportDroneTimer_=0,runOverdriveTimer_=0,runDashAttackTimer_=0;
    int meleeComboStep_=0,shootBarrelIndex_=0,shootGroupIndex_=0,hp_=73,runCurrencyEarned_=91;
    struct Stats {float bulletDamage=4,bulletSpeed=.5f,reloadSpeed=30,stamina=3,maxStamina=3,staminaRecovery=.9f,maxHp=120,moveSpeed=.23f,bodyDamage=3;} stats_,baseStats_;
    std::vector<float> weaponGroupCooldowns_,pendingMeleeSlashes_,pendingLaserShots_,pendingMineDrops_;
    std::vector<std::unique_ptr<PlayerDrone>> drones_;
    Vector3 runAimWorld_{20,0,0};
    TankRunModifiers runModifiers_;
    TankRunGrowth runGrowth_;
    bool isBuffActive_=false;
};
class Enemy : public TestActor {public:bool IsDead()const{return false;}void TakeDamage(uint32_t d){damageReceived+=d;++hits;}};
class ExpEnemy : public TestActor {public:bool IsDead()const{return false;}bool IsRunResource()const{return false;}void TakeDirectionalDamage(uint32_t d,Vector3){damageReceived+=d;++hits;}};
class EnemyManager { public: std::vector<ExpEnemy*> actors; std::vector<ExpEnemy*> GetEnemyPtrs() { return actors; } };
class Stage {
public:
    struct MergedBlock { AABB aabb; };
    std::vector<MergedBlock> mergedBlocks_;
	const std::vector<MergedBlock>& GetMergedBlocks() const {return mergedBlocks_;}
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
	// Real Player ability methods, with only GPU/actor drawing replaced above.
	{
		Player player;player.position={};BulletManager bullets;
		player.runModifiers_.enabled=player.runModifiers_.expedition=player.runModifiers_.railCannon=true;
		assert(player.SetExpeditionCombatStyle(tankbuild::Style::Shooter));
		for(int i=0;i<30;++i)player.AttackRailCannon(&bullets,true,1.0f/60.0f);
		assert(bullets.GetBulletCount()==0&&Near(player.railCharge_.seconds,.5f));
		for(int i=0;i<90;++i)player.AttackRailCannon(&bullets,true,1.0f/60.0f);
		assert(Near(player.railCharge_.seconds,1)&&bullets.GetBulletCount()==0);
		player.AttackRailCannon(&bullets,false,1.0f/60.0f);
		assert(bullets.GetBulletCount()==1&&player.specialCombatStats_.railShots==1&&player.bulletCoolTime>.1f);
		auto* rail=bullets.GetBulletPtrs().front();assert(rail->GetDamage()==30&&rail->GetRemainingActorPierces()==3);
		const auto muzzle=rail->GetWorldPosition();rail->Update(.1f);
		std::array<ExpEnemy,4> targets;
		for(size_t i=0;i<targets.size();++i) {targets[i].position=muzzle+Vector3{1.0f+static_cast<float>(i),0,0};collisions.CheckCollisionPair(rail,&targets[i]);assert(targets[i].damageReceived==30);}
		assert(rail->IsDead()); // One fast shot reaches four distinct actors, including swept contacts.
		bullets.ClearAll();player.bulletCoolTime=0;
		player.AttackRailCannon(&bullets,true,.016f);player.AttackRailCannon(&bullets,false,.016f);
		assert(bullets.GetBulletCount()==1&&bullets.GetBulletPtrs().front()->GetDamage()<6);
		for(auto style:{tankbuild::Style::Drone,tankbuild::Style::Melee}) {
			bullets.ClearAll();assert(player.SetExpeditionCombatStyle(style));
			player.AttackRailCannon(&bullets,true,1);player.AttackRailCannon(&bullets,false,.016f);assert(bullets.GetBulletCount()==0);
		}
	}
	for(int count:{1,2,3,4,12}) {
		Player player;player.position={};BulletManager bullets;Stage stage;EnemyManager enemies;ExpEnemy target;
		player.runModifiers_.enabled=player.runModifiers_.expedition=player.runModifiers_.droneLaserLink=true;
		player.expeditionCombatStyleSelected_=true;player.expeditionCombatStyle_=tankbuild::Style::Drone;
		player.stats_.bulletDamage=3;
		for(int i=0;i<count;++i) {auto drone=std::make_unique<PlayerDrone>();const float a=6.2831853f*static_cast<float>(i)/static_cast<float>(count);drone->position={std::cos(a)*2,std::sin(a)*2,0};player.drones_.push_back(std::move(drone));}
		target.position=count>1?(player.drones_[0]->position+player.drones_[1]->position)*.5f:Vector3{};enemies.actors.push_back(&target);
		player.UpdateSpecialCombat(stage,&bullets,nullptr,&enemies,.016f);
		assert(player.droneLaserLinks_.size()==static_cast<size_t>(tankspecial::LinkCount(count)));
		assert(target.hits==(count>1?1:0)); // Overlapping neighboring edges share the same target clock.
		for(int i=0;i<10;++i)player.UpdateSpecialCombat(stage,&bullets,nullptr,&enemies,.016f);
		assert(target.hits==(count>1?1:0));
		player.UpdateSpecialCombat(stage,&bullets,nullptr,&enemies,.05f);assert(target.hits==(count>1?2:0));
		const auto hits=target.hits;stage.mergedBlocks_.push_back({{{-3,-3,-1},{3,3,1}}});
		player.UpdateSpecialCombat(stage,&bullets,nullptr,&enemies,.5f);assert(player.droneLaserLinks_.empty()&&target.hits==hits);
		stage.mergedBlocks_.clear();player.expeditionCombatStyle_=tankbuild::Style::Shooter;
		player.UpdateSpecialCombat(stage,&bullets,nullptr,&enemies,.5f);assert(player.droneLaserLinks_.empty()&&target.hits==hits);
	}
	for(int combo:{0,1,2}) {
		Player player;player.position={};BulletManager bullets;Stage stage;
		player.runModifiers_.enabled=player.runModifiers_.slashWave=true;player.expeditionCombatStyleSelected_=true;player.expeditionCombatStyle_=tankbuild::Style::Melee;
		player.specialMeleeElapsed_=0;player.specialMeleeSwing_.comboStep=combo;player.specialMeleeSwing_.damage=40;
		player.UpdateSpecialCombat(stage,&bullets,nullptr,nullptr,.02f);assert(bullets.GetBulletCount()==0);
		player.UpdateSpecialCombat(stage,&bullets,nullptr,nullptr,.04f);assert(bullets.GetBulletCount()==(combo==2?1u:0u));
		if(combo!=2)continue;
		auto* wave=bullets.GetBulletPtrs().front();assert(wave->GetDamage()==22&&wave->GetRemainingActorPierces()==2);
		ExpEnemy target;target.position=wave->GetWorldPosition();collisions.CheckCollisionPair(wave,&target);assert(target.damageReceived==22);
		auto weak=MakeBullet(0,0,0,kEnemy);weak->SetWorldPosition(wave->GetWorldPosition());weak->ApplyBulletDurabilityDamage(6);
		collisions.CheckCollisionPair(wave,weak.get());assert(weak->IsDead());
		auto strong=MakeBullet(0,0,0,kEnemy);strong->SetWorldPosition(wave->GetWorldPosition());
		collisions.CheckCollisionPair(wave,strong.get());assert(!strong->IsDead()&&Near(strong->GetBulletHp(),6.0f));
		const float remaining=strong->GetBulletHp();collisions.CheckCollisionPair(wave,strong.get());assert(Near(strong->GetBulletHp(),remaining));
		player.UpdateSpecialCombat(stage,&bullets,nullptr,nullptr,.03f);assert(player.specialCombatStats_.slashWaves==1);
	}
	for(bool perfect:{false,true})for(float durability:{1.0f,6.0f,12.0f,24.0f}) {
		Player player;player.position={};BulletManager bullets;Stage stage;
		player.runModifiers_.enabled=player.runModifiers_.parryBlade=true;player.expeditionCombatStyleSelected_=true;player.expeditionCombatStyle_=tankbuild::Style::Melee;
		player.specialMeleeElapsed_=perfect?.05f:.18f;
		auto incoming=std::make_unique<Bullet>();incoming->Initialize({2,0,0},{-.2f,0,0},7,kEnemy,false,durability,1);auto* ptr=incoming.get();bullets.Add(std::move(incoming));
		player.UpdateSpecialCombat(stage,&bullets,nullptr,nullptr,.005f);
		assert(ptr->IsDead()==(durability<=6));
		if(durability>6)assert(Near(ptr->GetBulletHp(),perfect?durability-8:durability));
		assert(bullets.GetBulletCount()==(perfect&&durability<=6?2u:1u));
		if(perfect&&durability<=6) {auto* reflected=bullets.GetBulletPtrs().back();assert(reflected->GetOwner()==kPlayer&&reflected->GetMove().x>0&&reflected->GetSpecialKind()==Bullet::SpecialKind::ParryReflection);}
		const auto hp=ptr->GetBulletHp();player.UpdateSpecialCombat(stage,&bullets,nullptr,nullptr,.001f);assert(Near(ptr->GetBulletHp(),hp));
	}
	for(auto style:{tankbuild::Style::Shooter,tankbuild::Style::Drone}) {
		Player player;player.position={};BulletManager bullets;Stage stage;
		player.runModifiers_.enabled=player.runModifiers_.slashWave=player.runModifiers_.parryBlade=true;
		player.expeditionCombatStyleSelected_=true;player.expeditionCombatStyle_=style;
		player.specialMeleeElapsed_=.05f;player.specialMeleeSwing_.comboStep=2;
		auto incoming=std::make_unique<Bullet>();incoming->Initialize({2,0,0},{-.2f,0,0},7,kEnemy,false,6,1);auto* ptr=incoming.get();bullets.Add(std::move(incoming));
		player.UpdateSpecialCombat(stage,&bullets,nullptr,nullptr,.016f);
		assert(bullets.GetBulletCount()==1&&Near(ptr->GetBulletHp(),6)&&player.specialCombatStats_.slashWaves==0&&player.specialCombatStats_.parries==0);
	}
	std::cout << "Production specials PASS: 1s hold/release and tap rail, 5x damage, recovery, swept multi-target pierce; 1/2/3/4/12-drone links, 200ms per-enemy cooldown and walls; third-only wave damage and weak/strong bullet durability; normal/perfect parry and owned reflection.\n";
    {
        Player player;TankRunModifiers mods{};mods.enabled=mods.expedition=true;
        player.SetRunModifiers(mods);player.hp_=player.GetMaxHp();const int hp=player.hp_;
        mods.repair=true;player.SetRunModifiers(mods);
        assert(player.hp_==hp&&player.GetMaxHp()>hp); // Intro editing cannot heal even previously full HP.
        mods.effectPower[5]=2;player.SetRunModifiers(mods);assert(player.hp_==hp);
        assert(player.SetExpeditionCombatStyle(tankbuild::Style::Melee));
        mods.effectPower[5]=5;player.SetRunModifiers(mods);assert(player.hp_==hp&&player.runCurrencyEarned_==91);
        mods.effectPower[11]=3;mods.overdrive=true;player.SetRunModifiers(mods);player.runOverdriveTimer_=1;
        assert(Near(player.GetRunFireIntervalScale(),.25f));
    }
    // Execute real equipment switching and drone firing logic with graphics
    // adapters. This catches healing/reset leaks and nonfunctional base styles.
    {
        Player player;BulletManager manager;player.runBulletManager_=&manager;
        auto& mods=player.runModifiers_;mods.enabled=mods.expedition=true;mods.heavy=mods.rapid=true;
        player.stats_.stamina=1.25f;player.stats_.bulletDamage=5;player.stats_.reloadSpeed=24;
        assert(!player.SetExpeditionCombatStyle(static_cast<tankbuild::Style>(99)));
        for(auto style:{tankbuild::Style::Shooter,tankbuild::Style::Drone,tankbuild::Style::Melee}) {
            assert(player.SetExpeditionCombatStyle(style));
            assert(player.hp_==73&&player.runCurrencyEarned_==91&&Near(player.stats_.stamina,1.25f));
            assert(mods.heavy&&mods.rapid);
            if(style==tankbuild::Style::Drone) {
                assert(player.drones_.size()==3&&player.GetExpeditionDroneLimit()==3);
                assert(!player.GetCurrentClassConfig()->barrels.front().fires);
                auto* first=player.drones_.front().get();
                assert(first->runAttackParam_.damage==4&&Near(first->runAttackParam_.bulletSpeed,.324f));
                assert(Near(first->runReloadSeconds_,.4125f));
                for(auto& drone:player.drones_) {drone->runRallyShotPending_=true;drone->Attack(.5f);}
                assert(manager.GetBulletCount()==0); // Dash cannot fire without left-click.
                for(auto& drone:player.drones_) {drone->SetRunInput({20,0,0},true);drone->Attack(.5f);}
                assert(manager.GetBulletCount()==3); // One shot each, no main-gun volley.
                assert(player.SetExpeditionCombatStyle(style)&&player.drones_.front().get()==first);
                mods.droneFocus=mods.droneGuard=true;
                player.ConfigureRunDrone(*first);
                assert(first->runAttackParam_.damage==5&&Near(first->runReloadSeconds_,.474375f));
                assert(first->runAttackParam_.bulletHp==3&&first->runAttackParam_.bulletPenetration==3&&first->runAttackParam_.bulletCount==1);
                // Evolution authored scaling composes with the owned foundation.
                player.runEvolutionConfig_=player.runStarterConfig_;player.runEvolutionActive_=true;
                player.runEvolutionConfig_.bulletDamageScale=1.4f;player.runEvolutionConfig_.reloadScale=1.2f;
                player.runEvolutionConfig_.maxDrones=2;player.ConfigureRunDrone(*first);player.EnsureExpeditionDrones();
                assert(player.drones_.size()==2&&first->runAttackParam_.damage==7&&Near(first->runReloadSeconds_,.56925f));
                mods.drones=true;player.EnsureExpeditionDrones();assert(player.drones_.size()==4);
                player.runEvolutionConfig_.maxDrones=50;player.EnsureExpeditionDrones();assert(player.drones_.size()==12);
            } else assert(player.drones_.empty());
        }
        mods.enabled=false;
        assert(!player.SetExpeditionCombatStyle(tankbuild::Style::Drone));
    }
    {
        Player player;BulletManager manager;player.runBulletManager_=&manager;
        auto& mods=player.runModifiers_;mods.enabled=mods.expedition=mods.heavy=mods.rapid=true;
        assert(player.SetExpeditionCombatStyle(tankbuild::Style::Drone));
        player.runEvolutionConfig_=player.runStarterConfig_;player.runEvolutionActive_=player.runAuthoredEvolutionActive_=true;
        player.runEvolutionConfig_.id="evolved";player.runEvolutionConfig_.maxDrones=2;
        player.runEvolutionConfig_.bulletDamageScale=1.4f;player.runEvolutionConfig_.reloadScale=1.2f;
        player.stats_.stamina=1.25f;
        auto profiles=DefaultTankCombatStyleBalances();auto& p=profiles[1];
        p.maxHp=175;p.maxStamina=5;p.attackDamage=11;p.attackIntervalSeconds=.8f;p.droneCount=9;
        p.moveSpeed=.4f;p.bulletSpeed=.65f;p.droneFollowSpeed=.8f;p.droneCatchupSpeed=1.5f;p.droneResponse=12;
        player.ApplyCombatStyleBalance(profiles);
        assert(player.hp_==73&&player.runCurrencyEarned_==91&&Near(player.stats_.stamina,1.25f)&&player.GetMaxHp()==175);
        assert(player.runEvolutionConfig_.id=="evolved"&&player.runEvolutionActive_&&mods.heavy&&mods.rapid);
        assert(player.drones_.size()==8&&Near(player.stats_.moveSpeed,.4f));
        const auto& drone=*player.drones_.front();
        assert(drone.runAttackParam_.damage==19&&Near(drone.runReloadSeconds_,.792f)&&Near(drone.runAttackParam_.bulletSpeed,.78f));
        assert(Near(drone.followSpeed,.8f)&&Near(drone.catchupSpeed,1.5f)&&Near(drone.followResponse,12));
        player.ApplyCombatStyleBalance(profiles);
        assert(player.hp_==73&&Near(drone.runReloadSeconds_,.792f)); // Repeated Apply never stacks.
        profiles[1].maxHp=50;profiles[1].maxStamina=.5f;player.ApplyCombatStyleBalance(profiles);
        assert(player.hp_==50&&Near(player.stats_.stamina,.5f));
        // All three families read independent bases, including selected evolutions.
        profiles[0].attackDamage=20;profiles[0].attackIntervalSeconds=.2f;
        profiles[2].attackDamage=38;profiles[2].attackIntervalSeconds=.8f;
        player.ApplyCombatStyleBalance(profiles);
        assert(player.SetExpeditionCombatStyle(tankbuild::Style::Shooter));
        assert(Near(player.stats_.bulletDamage,25)&&Near(player.stats_.reloadSpeed,.2f*60*.825f));
        assert(player.SetExpeditionCombatStyle(tankbuild::Style::Melee));
        assert(Near(player.stats_.bulletDamage*3.8f,47.5f)&&Near(player.GetRunBaseReloadFrames(),48));
    }
    {
        Player player;player.runModifiers_.enabled=player.runModifiers_.expedition=true;
        for(auto style:{tankbuild::Style::Shooter,tankbuild::Style::Drone,tankbuild::Style::Melee}) {
            const std::string id=tankbuild::Id(style);
            player.runAuthoredChoices_.push_back({id,id,id});
            player.runAuthoredStyles_[id]=style;
            player.runAuthoredClasses_[id].id=id;
        }
        for(auto style:{tankbuild::Style::Shooter,tankbuild::Style::Drone,tankbuild::Style::Melee}) {
            assert(player.SetExpeditionCombatStyle(style));
            const auto choices=player.GetRunAuthoredEvolutionChoices();
            assert(choices.size()==1&&choices.front().id==tankbuild::Id(style));
            for(auto other:{tankbuild::Style::Shooter,tankbuild::Style::Drone,tankbuild::Style::Melee})
                if(other!=style)assert(!player.ChooseRunAuthoredClass(tankbuild::Id(other)));
            assert(player.ChooseRunAuthoredClass(tankbuild::Id(style)));
            assert(player.hp_==73&&player.runCurrencyEarned_==91&&player.GetRunAuthoredEvolutionChoices().empty());
            // A live authoring edit changing a queued offer's family is rejected.
            player.runAuthoredStyles_[tankbuild::Id(style)]=static_cast<tankbuild::Style>((static_cast<int>(style)+1)%3);
            assert(!player.ChooseRunAuthoredClass(tankbuild::Id(style)));
            player.runAuthoredStyles_[tankbuild::Id(style)]=style;
        }
    }
    // Production player rules sanitize legacy per-turret counts and retired
    // split cards before the real emitter runs. Separate mounts remain valid.
    for(bool enabled : {false,true}) for(bool expedition : {false,true})
    for(int core=0;core<4;++core) for(int bits=0;bits<8;++bits)
    for(int legacyCount : {0,1,3,5,16}) {
        Player player;
        auto& m=player.runModifiers_;
        m.enabled=enabled;m.expedition=expedition;m.core=static_cast<TankRunCore>(core);
        m.scatterShot=(bits&1)!=0;m.ricochet=(bits&2)!=0;m.pierce=(bits&4)!=0;
        player.authoredConfig.reflect=(bits&2)!=0;player.authoredConfig.penetrate=(bits&4)!=0;
        AttackParam param{};param.bulletCount=legacyCount;param.impactSplitCount=2;
        param.damage=20;param.bulletSpeed=1;
        player.ApplyRunProjectileRules(param);
        assert(param.bulletCount==1 && param.impactSplitCount==0);
        BulletManager manager;AttackController attack;attack.SetBulletManager(&manager);
        attack.FireFromMuzzle({}, {1,0,0},param,kPlayer);
        assert(manager.GetBulletCount()==1);
        attack.FireFromMuzzle({0,1,0}, {1,0,0},param,kPlayer);
        assert(manager.GetBulletCount()==2); // Two authored turrets, one round each.
        for(auto* bullet:manager.GetBulletPtrs()) bullet->OnWallImpact({}, {-1,0,0});
        manager.FlushPendingSplits();
        assert(manager.GetGrowthStats().splitChildrenSpawned==0);
    }
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
    std::cout << "Production projectile growth PASS: style equipment/HP/stamina/wallet preservation, baseline 3-drone firing/input/cap/evolution composition; 640 player one-round-per-mount legacy/core/card combinations, no player split children; 0/1/2/3/4/unlimited bounces; repeated actor/allocator reuse guards; interception; deferred engine forks; owner/resource/lifetime conservation; no recursion; cap; Stage embedded/grazing contacts; AttackParam wiring.\n";
}
