#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include "game/player/TankSpecialCombat.h"
namespace cg2 {
struct Vector3 {float x=0,y=0,z=0; Vector3& operator+=(Vector3 b){x+=b.x;y+=b.y;z+=b.z;return *this;}};
Vector3 operator+(Vector3 a,Vector3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vector3 operator-(Vector3 a,Vector3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vector3 operator*(Vector3 a,float s){return {a.x*s,a.y*s,a.z*s};}
Vector3 operator/(Vector3 a,float s){return {a.x/s,a.y/s,a.z/s};}
float Length(Vector3 a){return std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);}
Vector3 Normalize(Vector3 a){return a/Length(a);}
struct Camera{};
enum Axis {X,Y};
struct Input {struct Mouse {uint8_t rgbButtons[2]{};} mouse; const Mouse& GetMouseState()const{return mouse;} bool IsPress(uint8_t b)const{return b!=0;}};
}
using cg2::Vector3;
constexpr uint32_t kCollisionAttributeEnemyBullet=1,kCollisionAttributeEnemy=2,kCollisionAttributeExpEnemy=4;
struct Collider {virtual ~Collider()=default; uint32_t attribute=kCollisionAttributeEnemyBullet; Vector3 position{3,0,0}; virtual Vector3 GetWorldPosition()const{return position;} uint32_t GetCollisionAttribute()const{return attribute;} float GetHitPower()const{return 1;}};
struct ExpEnemy:Collider {bool resource=false;bool IsRunResource()const{return resource;}};
enum class BulletOwner {kPlayer};
struct AttackParam {float bulletSpeed=0,spreadAngleDeg=0,cooldown=0;int bulletCount=1;bool randomSpread=false,reflect=false,penetrate=false;uint32_t damage=1;};
struct BulletManager {struct Counts {size_t player=0;}counts;size_t shots=0;const Counts& GetBulletCounts()const{return counts;}};
struct AttackController {BulletManager* manager=nullptr;void Fire(Vector3,Vector3,const AttackParam&,BulletOwner){++manager->shots;++manager->counts.player;}};
struct TestObject {template<class T>void SetTransform(T){}void Update(){}};
struct PlayerDrone;
struct Stage {size_t calls=0;void ResolvePlayerDroneCollision(PlayerDrone&,cg2::Axis){++calls;}};
struct MapChip {static constexpr float kBlockWidth=2,kBlockHeight=2;static constexpr int kNumBlockVirtical=32,kNumBlockHorizontal=64;};
struct PlayerDrone:Collider {
void Attack(float);void Update(cg2::Camera*,Stage&,const Vector3&,float);void OnCollision(Collider*);void Damage();void Die();
Vector3 GetWorldPosition()const override{return worldTransform_.translate;}
Vector3 GetMove()const{return velocity_;}void SetWorldPosition(Vector3 p){worldTransform_.translate=p;}void RotateToMouse(cg2::Camera*){}
struct {Vector3 translate{30,29,0};}worldTransform_;
Vector3 velocity_{.2f,0,0},dir{1,0,0},runFollowOffset_{},missionTarget_{};
cg2::Input input;cg2::Input* input_=&input;
TestObject object;TestObject* object_=&object;
int hp_=1;static constexpr int kMaxHp=10,kBulletTime=30;int bulletCoolTime=0;
bool isDead_=false,runAttackEnabled_=true,runInputOverride_=true,runWantsAttack_=true,runRallyShotPending_=false,rebuilt_=false;
float runShotCooldown_=0,runReloadSeconds_=.5f,runExactDamage_=0,runDamageRemainder_=0,invincibleTimer_=0;
float runFollowSpeed_=.25f,runCatchupSpeed_=.62f,runFollowResponse_=5,maxSpeed_=.25f,accel_=.7f,decel_=3.5f;
AttackParam runAttackParam_;BulletManager* runBulletManager_=nullptr;AttackController attackController_;
tankspecial::DroneMission mission_;
};
#include "drone_lifecycle_methods.inc"
void Check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void LethalCollision(bool coincident){
PlayerDrone drone;BulletManager bullets;Stage stage;Collider projectile;drone.runBulletManager_=&bullets;drone.attackController_.manager=&bullets;
if(coincident)projectile.position=drone.GetWorldPosition();
drone.OnCollision(&projectile);const auto atDeath=drone.GetWorldPosition();const auto deathVelocity=drone.velocity_;
Check(drone.hp_==0,"lethal collision HP must be zero");Check(drone.isDead_,"lethal collision must enter death before next update");
drone.OnCollision(&projectile);drone.Damage();drone.Attack(1.0f/60);drone.Update(nullptr,stage,{35,29,0},1.0f/60);
Check(drone.hp_==0,"dead drone damage must be idempotent");Check(bullets.shots==0,"dead drone must never emit another shot");
Check(stage.calls==0&&cg2::Length(drone.GetWorldPosition()-atDeath)==0,"dead drone must not simulate movement/collisions");
Check(cg2::Length(drone.velocity_-deathVelocity)==0,"repeated death collision must not add knockback");
}
void LethalDamage(){PlayerDrone drone;BulletManager bullets;drone.runBulletManager_=&bullets;drone.attackController_.manager=&bullets;
drone.Damage();Check(drone.isDead_&&drone.hp_==0,"Damage must enter death immediately");drone.Damage();drone.Attack(1.0f/60);Check(drone.hp_==0&&bullets.shots==0,"damage death must stop later damage/shot");}
void LivingSemantics(){
PlayerDrone drone;Collider projectile;drone.hp_=5;drone.invincibleTimer_=2;drone.OnCollision(&projectile);
Check(drone.hp_==4&&!drone.isDead_,"living collision retains one HP damage without invincibility test");Check(cg2::Length(drone.velocity_)>0,"living collision retains knockback");
ExpEnemy resource;resource.resource=true;const auto v=drone.velocity_;drone.OnCollision(&resource);
Check(drone.hp_==4&&cg2::Length(drone.velocity_-v)==0,"resource contact remains ignored");
}
void LivingRebuild(){
PlayerDrone drone;BulletManager bullets;Stage stage;Collider projectile;drone.hp_=4;drone.runBulletManager_=&bullets;drone.attackController_.manager=&bullets;
Check(drone.mission_.Start(true),"start bomb mission");drone.mission_.Step(tankspecial::DroneMission::kBombWarningSeconds);drone.mission_.Arrive();
Check(drone.mission_.GetPhase()==tankspecial::DronePhase::Rebuilding,"bomb arrived rebuilding");drone.OnCollision(&projectile);
Check(drone.hp_==4&&!drone.isDead_,"living rebuilding drone ignores collision");
drone.Update(nullptr,stage,{30,29,0},tankspecial::DroneMission::kRebuildSeconds);
Check(drone.hp_==PlayerDrone::kMaxHp&&drone.rebuilt_&&!drone.isDead_,"living rebuilding completion restores HP");
Check(stage.calls==0&&bullets.shots==0,"rebuilding completion frame remains placement only");
}
int main(){try{LethalCollision(false);LethalCollision(true);LethalDamage();LivingSemantics();LivingRebuild();
std::cout<<"PASS: drone immediate death, no postmortem simulation, living damage and rebuild preservation\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<"\n";return 1;}}