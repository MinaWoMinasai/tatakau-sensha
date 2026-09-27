#include "Player.h"
#include "Stage.h"
#include "Enemy.h"
#include "game/exp/ExpEnemy.h"
#include "game/exp/EnemyManager.h"

void Player::RefreshAdditiveArmaments()
{
    if(!runModifiers_.enabled||!runModifiers_.expedition||!expeditionCombatStyleSelected_||
        expeditionCombatStyle_!=tankbuild::Style::Shooter||runEvolutionActive_||runStarterConfig_.barrels.empty())return;
    const size_t oldCount=runStarterConfig_.barrels.size();
    const bool oldAlternate=runStarterConfig_.alternateBarrels;
    const float oldAngle=runStarterConfig_.barrels.front().angleDeg;
    const int count=1+(runModifiers_.extraBarrel1?1:0)+(runModifiers_.extraBarrel2?1:0);
    const auto prototype=runStarterConfig_.barrels.front();
    runStarterConfig_.barrels.resize(static_cast<size_t>(count),prototype);
    for(int i=0;i<count;++i) {
        auto& mount=runStarterConfig_.barrels[static_cast<size_t>(i)];
        const float normalized=count==1?0.0f:static_cast<float>(i)*2.0f/static_cast<float>(count-1)-1.0f;
        mount.offset={.72f,normalized*(count==2?.48f:.72f),0};
        mount.angleDeg=runModifiers_.fanMount?normalized*(count==2?8.0f:14.0f):0.0f;
        mount.damageScale=count==1?1.0f:count==2?.65f:.50f;
        mount.reloadScale=mount.projectileSpeedScale=1;
        mount.fires=true;mount.weaponType=WeaponType::Projectile;mount.fireGroup=0;
    }
    runStarterConfig_.alternateBarrels=count>1&&runModifiers_.alternatingFire;
    runStarterConfig_.fireAllBarrels=!runStarterConfig_.alternateBarrels;
    // Alternation distributes one volley over the same period; it adds no DPS.
    runStarterConfig_.reloadScale=runStarterConfig_.alternateBarrels?1.0f/static_cast<float>(count):1.0f;
    if(object_&&(oldCount!=static_cast<size_t>(count)||oldAlternate!=runStarterConfig_.alternateBarrels||
        oldAngle!=runStarterConfig_.barrels.front().angleDeg)) {
        shootBarrelIndex_=shootGroupIndex_=0;weaponGroupCooldowns_.clear();
        InitializeBarrels();UpdateBarrelLayout();
    }
}

void Player::ResetAdditionalAbilities()
{
    empJammerTimer_=0;droneChargeCooldown_=1.5f;droneBombCooldown_=4.0f;recentDashTimer_=0;nextMissionDrone_=0;
    painterLocks_={};targetLockVisuals_.clear();spinCycle_={};finisherSpinReady_=false;
    dashSlashActive_=false;dashSlashTimer_=0;dashSlashTargets_.clear();wallSmashTargets_={};
}

bool Player::TryStartSpinBlade(bool pressed)
{
    if(!IsMeleeBuild()||!runModifiers_.spinBlade)return false;
    if(!spinCycle_.Start(pressed,finisherSpinReady_,stats_.stamina))return false;
    finisherSpinReady_=false;meleeComboStep_=0;bulletCoolTime=.85f;meleeComboTimer_=1.4f;
    if(pendingSpecialCombatEvents_.size()<32)pendingSpecialCombatEvents_.push_back({SpecialEventKind::SpinBlade,GetWorldPosition(),dir_,1});
    return true;
}

std::vector<Player::DroneAbilityVisual> Player::GetDroneAbilityVisuals() const
{
    std::vector<DroneAbilityVisual> result;
    if(!IsDroneBuild())return result;
    for(const auto& drone:drones_) {
        if(!drone||drone->IsDead())continue;
        const auto& mission=drone->GetRunMission();
        if(mission.phase==tankspecial::DronePhase::Escort)continue;
        const float duration=mission.phase==tankspecial::DronePhase::Warning?(mission.bomb?.65f:.30f):
            mission.phase==tankspecial::DronePhase::Rebuilding?5.5f:1.0f;
        result.push_back({drone->GetWorldPosition(),drone->GetRunMissionTarget(),mission.phase,
            (std::clamp)(mission.elapsed/duration,0.0f,1.0f),mission.bomb});
    }
    return result;
}

uint32_t Player::NotifyDroneHit(int drone,Collider* target,uint32_t originalDamage)
{
    if(!IsDroneBuild()||!runModifiers_.targetPainter||!target||drone<0)return originalDamage;
    auto* regular=dynamic_cast<ExpEnemy*>(target);
    auto* boss=dynamic_cast<Enemy*>(target);
    if((!regular&&!boss)||(regular&&(regular->IsDead()||regular->IsRunResource()))||(boss&&boss->IsDead()))return originalDamage;
    const uint64_t id=target->GetCollisionId();
    tankspecial::PainterLock* entry=nullptr,*free=nullptr;
    for(auto& lock:painterLocks_) {
        if(lock.id==id){entry=&lock;break;}
        if(!free&&(lock.id==0||(lock.remaining<=0&&lock.buildup<=0)))free=&lock;
    }
    if(!entry&&free){entry=free;*entry={};entry->id=id;}
    if(!entry)return originalDamage;
    if(entry->Hit(drone)) {
        ++specialCombatStats_.targetLocks;
        if(pendingSpecialCombatEvents_.size()<32)pendingSpecialCombatEvents_.push_back({SpecialEventKind::TargetLock,target->GetWorldPosition(),{},1});
    }
    return static_cast<uint32_t>((std::max)(1.0f,std::round(static_cast<float>(originalDamage)*entry->DamageScale(boss!=nullptr,TankEffectPower(runModifiers_,37)))));
}

float Player::GetDroneTargetDamageScale(const Collider* target,bool boss) const
{
    if(!IsDroneBuild()||!runModifiers_.targetPainter||!target)return 1;
    for(const auto& lock:painterLocks_)if(lock.id==target->GetCollisionId())return lock.DamageScale(boss,TankEffectPower(runModifiers_,37));
    return 1;
}

void Player::ArmWallSmash(ExpEnemy* target,float strength)
{
    if(!IsMeleeBuild()||!runModifiers_.wallSmash||!target||target->IsDead()||target->IsRunResource())return;
    WallSmashTarget* slot=nullptr;
    for(auto& pending:wallSmashTargets_) {
        if(pending.id==target->GetCollisionId()){slot=&pending;break;}
        if(!slot&&pending.seconds<=0)slot=&pending;
    }
    if(slot) {
        // Do not overwrite an unconsumed impact when another slash lands in
        // the same frame between enemy movement and the wall-event pass.
        if(slot->id==target->GetCollisionId()&&slot->seconds>0&&slot->collision!=target->GetWallCollisionCount())return;
        *slot={target->GetCollisionId(),target->GetWallCollisionCount(),1.0f,(std::clamp)(strength,.5f,2.0f)};
    }
}

void Player::UpdateAdditionalAbilities(Stage& stage,BulletManager* bullets,Enemy* boss,EnemyManager* enemies,float dt)
{
    (void)bullets;
    if(!runModifiers_.droneCharge&&!runModifiers_.droneRebuildBomb&&!runModifiers_.targetPainter&&
        !runModifiers_.dashSlash&&!runModifiers_.spinBlade&&!runModifiers_.wallSmash)return;
    auto blocked=[&](const Vector3& from,const Vector3& to) {
        for(const auto& block:stage.GetMergedBlocks())if(tankspecial::SegmentCrossesBox(from.x,from.y,to.x,to.y,
            block.aabb.min.x,block.aabb.min.y,block.aabb.max.x,block.aabb.max.y))return true;
        return false;
    };
    auto emit=[&](SpecialEventKind kind,const Vector3& point,const Vector3& direction,float power=1.0f) {
        if(pendingSpecialCombatEvents_.size()<32)pendingSpecialCombatEvents_.push_back({kind,point,direction,power});
    };
    const auto regulars=enemies?enemies->GetEnemyPtrs():std::vector<ExpEnemy*>{};
    std::vector<Collider*> targets;targets.reserve((std::min)(size_t{128},regulars.size()+1));
    if(boss&&!boss->IsDead())targets.push_back(boss);
    for(auto* enemy:regulars)if(enemy&&!enemy->IsDead()&&!enemy->IsRunResource()&&targets.size()<128)targets.push_back(enemy);
    auto damage=[&](Collider* target,uint32_t amount,const Vector3& source,bool melee,float knockback=0.0f) {
        if(target==boss) {if(boss&&!boss->IsDead()) {boss->TakeDamage(amount);const auto delta=target->GetWorldPosition()-source;
            if(knockback>0&&Length(delta)>.001f)boss->ApplyKnockback(Normalize(delta),knockback);}}
        else if(auto* enemy=dynamic_cast<ExpEnemy*>(target);enemy&&!enemy->IsDead()) {
            enemy->TakeDirectionalDamage(amount,source,melee);
            if(knockback>0) {
                const auto difference=enemy->GetWorldPosition()-source;
                enemy->ApplyKnockback(Length(difference)>.001f?Normalize(difference):Vector3{1,0,0},knockback);
                if(melee)ArmWallSmash(enemy,1);
            }
        }
    };
    for(auto& lock:painterLocks_)lock.Advance(dt);
    targetLockVisuals_.clear();
    if(IsDroneBuild()) {
        for(auto* target:targets)for(const auto& lock:painterLocks_)if(lock.id==target->GetCollisionId()&&(lock.remaining>0||lock.buildup>0))
            targetLockVisuals_.push_back({target->GetWorldPosition(),lock.hits,lock.remaining});
        droneChargeCooldown_=(std::max)(0.0f,droneChargeCooldown_-dt);
        droneBombCooldown_=(std::max)(0.0f,droneBombCooldown_-dt);
        const bool held=!runRoomAwaitInputRelease_&&!upgradeHudMouseCaptured_&&(demoInputEnabled_?demoShoot_:input_->IsPress(input_->GetMouseState().rgbButtons[0]));
        auto launch=[&](bool bomb) {
            if(!held||drones_.empty()||targets.empty())return false;
            Collider* chosen=nullptr;float best=1e30f;
            for(auto* target:targets) {
                if(Length(target->GetWorldPosition()-GetWorldPosition())>26)continue;
                const float score=Length(target->GetWorldPosition()-runAimWorld_);
                if(score<best){best=score;chosen=target;}
            }
            if(!chosen)return false;
            for(size_t n=0;n<drones_.size();++n) {
                const size_t index=(nextMissionDrone_+n)%drones_.size();auto& drone=drones_[index];
                if(!drone||drone->IsDead()||blocked(drone->GetWorldPosition(),chosen->GetWorldPosition()))continue;
                if(drone->StartRunMission(chosen->GetWorldPosition(),bomb)) {
                    nextMissionDrone_=(index+1)%drones_.size();
                    if(!bomb)++specialCombatStats_.droneCharges;
                    emit(SpecialEventKind::DroneCharge,drone->GetWorldPosition(),chosen->GetWorldPosition()-drone->GetWorldPosition(),bomb?1.5f:.5f);
                    return true;
                }
            }
            return false;
        };
        if(runModifiers_.droneRebuildBomb&&droneBombCooldown_<=0&&launch(true))droneBombCooldown_=9.0f;
        if(runModifiers_.droneCharge&&droneChargeCooldown_<=0&&launch(false))droneChargeCooldown_=2.8f;
        const auto tuning=MakeTankDroneTuning(runModifiers_,true);
        const auto* config=GetCurrentClassConfig();
        const float base=stats_.bulletDamage*tuning.damageScale*(config?config->bulletDamageScale:1.0f);
        for(size_t i=0;i<drones_.size();++i) {
            auto& drone=drones_[i];if(!drone||drone->IsDead())continue;
            if(drone->ConsumeRunRebuilt()){++specialCombatStats_.droneRebuilds;emit(SpecialEventKind::DroneRebuild,drone->GetWorldPosition(),{},1);}
            if(!drone->ConsumeRunMissionImpact())continue;
            const bool bomb=drone->GetRunMission().bomb;
            const auto origin=drone->GetWorldPosition();const float radius=bomb?4.5f:2.2f;
            const float scale=bomb?8.0f*TankEffectPower(runModifiers_,36):3.0f*TankEffectPower(runModifiers_,35);
            if(bomb){++specialCombatStats_.droneBombs;emit(SpecialEventKind::DroneBomb,origin,{},radius);}
            for(auto* target:targets) {
                if(Length(target->GetWorldPosition()-origin)>radius+target->GetRadius()||blocked(origin,target->GetWorldPosition()))continue;
                const uint32_t amount=NotifyDroneHit(static_cast<int>(i),target,static_cast<uint32_t>((std::max)(1.0f,std::round(base*scale*(target==boss?.75f:1.0f)))));
                damage(target,amount,origin,false,bomb?.26f:.14f);
                if(!bomb){++specialCombatStats_.droneChargeHits;emit(SpecialEventKind::DroneCharge,target->GetWorldPosition(),{},1);break;}
            }
        }
    }
    if(!IsMeleeBuild())return;
    const auto combo=MakeTankMeleeCombo(0,runModifiers_);
    const auto* config=GetCurrentClassConfig();
    const float baseMelee=stats_.bulletDamage*3.8f*combo.damage*(config?config->bulletDamageScale:1.0f);
    if(dashSlashActive_) {
        dashSlashTimer_-=dt;
        const Vector3 end=GetWorldPosition()+dashSlashDirection_*1.5f;
        for(auto* target:targets) {
            if(std::find(dashSlashTargets_.begin(),dashSlashTargets_.end(),target->GetCollisionId())!=dashSlashTargets_.end())continue;
            const auto p=target->GetWorldPosition();
            if(!tankspecial::SegmentTouches(dashSlashPrevious_.x,dashSlashPrevious_.y,end.x,end.y,p.x,p.y,target->GetRadius()+1.25f)||blocked(GetWorldPosition(),p))continue;
            dashSlashTargets_.push_back(target->GetCollisionId());damage(target,dashSlashDamage_,GetWorldPosition(),true,.28f);
            ++specialCombatStats_.dashSlashHits;emit(SpecialEventKind::DashSlash,p,dashSlashDirection_,.6f);
        }
        dashSlashPrevious_=GetWorldPosition();if(dashSlashTimer_<=0)dashSlashActive_=false;
    }
    if(runModifiers_.spinBlade&&spinCycle_.Step(dt)) {
        MeleeSlashEvent swing{};swing.origin=GetWorldPosition();swing.direction={1,0,0};
        swing.range=GetCombatStyleProfile(tankbuild::Style::Melee).meleeRange*combo.range*.85f;
        swing.arcDeg=360;swing.windupDuration=0;swing.duration=.18f;swing.recoveryDuration=0;swing.comboStep=-1;
        swing.damage=static_cast<uint32_t>((std::max)(1.0f,std::round(baseMelee*.30f*TankEffectPower(runModifiers_,40))));
        swing.knockback=.07f;swing.color={.8f,.3f,1.6f,1};
        pendingMeleeSlashes_.push_back(swing);specialMeleeSwing_=swing;specialMeleeElapsed_=0;
        specialWaveEmitted_=true;specialPerfectFeedback_=true;specialParriedBullets_.clear();
        ++specialCombatStats_.spinTicks;
    }
    if(runModifiers_.wallSmash)for(auto& pending:wallSmashTargets_) {
        if(pending.seconds<=0)continue;pending.seconds-=dt;
        for(auto* target:regulars)if(target&&!target->IsDead()&&target->GetCollisionId()==pending.id) {
            if(target->GetWallCollisionCount()==pending.collision)break;
            pending.seconds=0;const auto origin=target->GetWorldPosition();
            const auto amount=static_cast<uint32_t>(tankspecial::WallSmashDamage(baseMelee,TankEffectPower(runModifiers_,41)));
            target->TakeDirectionalDamage(amount,origin,true);++specialCombatStats_.wallSmashes;
            emit(SpecialEventKind::WallSmash,origin,{},2.8f);
            for(auto* other:targets)if(other!=target&&Length(other->GetWorldPosition()-origin)<=2.8f+other->GetRadius()&&!blocked(origin,other->GetWorldPosition()))
                damage(other,(std::max)(1u,static_cast<uint32_t>(static_cast<float>(amount)*.4f)),origin,false);
            break;
        }
    }
}
