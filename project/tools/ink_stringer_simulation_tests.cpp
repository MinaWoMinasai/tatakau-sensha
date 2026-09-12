// Standalone behavior tests: InkSimulation.cpp + InkSimulation.Stringer.cpp.
// No graphics runtime, catalog parser, or original game assets are required.
#include "../game/ink/InkSimulation.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace ink;
namespace {
constexpr float Dt = 1.0f / 120.0f;
bool Near(float a, float b, float tolerance = .0002f) { return std::abs(a-b) <= tolerance; }
void Check(bool condition, const char* reason) { if (!condition) throw std::runtime_error(reason); }
void Steps(Simulation& sim, int count, const Controls& input, float dt=Dt) {
    for (int n=0;n<count;++n) sim.Step(dt,input);
}
WeaponDefinition Stringer() {
    WeaponDefinition def; def.id="tri-stringer"; def.displayNameJa="test";
    def.type=WeaponClass::Stringer; return def;
}
void Prepare(Simulation& sim, float dt=Dt) {
    sim.Equip(Stringer()); sim.SetDummyPosition({100,100,100});
    sim.Step(dt,{}); // The equip operation requires an actual released trigger.
}
Controls Forward(const Simulation& sim) {
    Controls input; input.aimPoint=sim.Muzzle(input)+Vec3{0,0,30}; return input;
}
void Release(Simulation& sim, Controls input, int heldSteps, float dt=Dt) {
    input.fire=true; Steps(sim,heldSteps,input,dt);
    input.fire=false; sim.Step(dt,input);
}
void TapAndMinimum() {
    Simulation sim; Prepare(sim); auto input=Forward(sim);
    input.fire=true; sim.Step(Dt,input);
    input.fire=false; Steps(sim,16,input);
    Check(sim.ShotsFired()==0,"released click must not fire before the minimum 9 source frames");
    sim.Step(Dt,input);
    Check(sim.ShotsFired()==1 && sim.Projectiles().size()==3,"a quick click must queue exactly one three-arrow volley at 9F");
    Check(Near(sim.Player().ink,.95f),"tap volley costs 5% once, not once per arrow");
    for(const auto& arrow:sim.Projectiles()) {
        Check(arrow.projectileKind==ProjectileKind::StringerArrow && !arrow.explosive,"tap arrows are non-explosive stringer projectiles");
        Check(Near(arrow.directDamage,30),"tap arrow retains 30 damage");
    }
    Steps(sim,240,input); Check(sim.ShotsFired()==1,"a released click never repeats automatically");
}
void HoldAndEndpoints() {
    for(const int held:{18,60,144,240}) {
        Simulation sim; Prepare(sim); auto input=Forward(sim); input.fire=true;
        Steps(sim,held,input);
        Check(sim.ShotsFired()==0 && sim.Projectiles().empty(),"holding any charge level does not automatically fire");
        Check(sim.IsCharging(),"held trigger remains in charging state");
        const auto profile=sim.ChargeProfile();
        input.fire=false; sim.Step(Dt,input);
        Check(sim.ShotsFired()==1 && sim.Projectiles().size()==3,"release emits one volley");
        const float cost=held==18?.05f:(held==60?.06f:.085f);
        Check(Near(sim.Player().ink,1-cost),"charge endpoints use the researched ink costs");
        Check(Near(profile.damage,held==18?30.0f:35.0f),"charge endpoints resolve per-arrow damage");
        Check(profile.level==(held==18?0:(held==60?1:2)),"rings reach min / 30F / 72F states");
        for(const auto& arrow:sim.Projectiles()) Check(arrow.explosive==(held>=60),"explosive arrow threshold is the first ring");
    }
}
void ChargeCadence60And120() {
    for(const int fps:{60,120}) {
        const float dt=1.0f/fps;
        Simulation sim; Prepare(sim,dt); auto input=Forward(sim); input.fire=true;
        Steps(sim,fps/2,input,dt);
        Check(Near(sim.ChargeTime(),.5f) && sim.ChargeProfile().level==1,"first ring takes .5 seconds at 60 and 120Hz");
        Steps(sim,fps*7/10,input,dt);
        Check(Near(sim.ChargeTime(),1.2f) && sim.ChargeProfile().level==2,"full charge takes 1.2 seconds at both rates");
        input.fire=false; sim.Step(dt,input);
        Check(sim.ShotsFired()==1 && Near(sim.Player().ink,.915f),"release and cost agree across update rates");
    }
}
void AirRateAndVerticalFan() {
    Simulation air; Prepare(air); air.SetPlayerPosition({0,12,-8});
    auto input=Forward(air); input.fire=true;
    Steps(air,36,input);
    Check(!air.Player().grounded && Near(air.ChargeTime(),.1f),".3 seconds airborne adds .1 seconds charge");
    input.aimPoint=air.Muzzle(input)+Vec3{0,0,30}; input.fire=false; air.Step(Dt,input);
    Check(air.Projectiles().size()==3,"airborne release emits three arrows");
    float low=1000,high=-1000;
    for(const auto& arrow:air.Projectiles()) {
        Check(std::abs(arrow.velocity.x)<.0001f,"air fan has no horizontal divergence when looking forward");
        low=(std::min)(low,arrow.velocity.y); high=(std::max)(high,arrow.velocity.y);
    }
    Check(high-low>15,"air arrows fan vertically");
    Simulation ground; Prepare(ground); auto horizontal=Forward(ground); Release(ground,horizontal,18);
    for(const auto& arrow:ground.Projectiles()) Check(std::abs(arrow.velocity.y)<.0001f,"ground fan stays horizontal");
}
void CancelAndNoChargeKeep() {
    Simulation sim; Prepare(sim); auto input=Forward(sim); input.fire=true;
    Steps(sim,144,input); input.swim=true; sim.Step(Dt,input);
    Check(!sim.IsCharging() && sim.ChargeTime()==0 && sim.ShotsFired()==0,"swim cancels full charge without firing");
    Check(Near(sim.Player().ink,1),"CG2's documented cancel does not consume ink");
    input.swim=false; Steps(sim,180,input);
    Check(!sim.IsCharging() && sim.ShotsFired()==0,"still-held trigger after cancel cannot restore stored charge");
    input.fire=false; sim.Step(Dt,input); Release(sim,input,18);
    Check(sim.ShotsFired()==1 && Near(sim.Player().ink,.95f),"new click after cancellation starts a fresh tap");
    Simulation queued; Prepare(queued); input=Forward(queued); input.fire=true; queued.Step(Dt,input);
    input.fire=false; queued.Step(Dt,input); input.swim=true; Steps(queued,24,input);
    Check(queued.ShotsFired()==0 && !queued.IsCharging(),"swim cancels a queued early release before the minimum time");
}
void RecoveryAndPostDelay() {
    Simulation sim; Prepare(sim); auto input=Forward(sim); Release(sim,input,60);
    const float ink=sim.Player().ink;
    Check(Near(sim.InkRecoveryDelay(),20.0f/60),"release applies the 20F recovery lock");
    Steps(sim,38,input);
    Check(Near(sim.Player().ink,ink),"ink does not regenerate before recovery lock expires");
    Steps(sim,4,input); Check(sim.Player().ink>ink,"idle regeneration resumes after the recovery lock");
    Simulation hidden; Prepare(hidden); input=Forward(hidden); Release(hidden,input,60);
    input.swim=true; Steps(hidden,18,input);
    Check(hidden.Player().state==PlayerState::Human,"swim waits for the 10F post-shot delay");
    Steps(hidden,3,input); Check(hidden.Player().state!=PlayerState::Human,"swim becomes available after the post-shot delay");
}
void OneArrowDirectDamage() {
    Simulation sim; Prepare(sim);
    const Vec3 target={.28f,1.1f,-3.2f}; sim.SetDummyPosition(target);
    Controls input; input.aimPoint=target; Release(sim,input,60);
    Steps(sim,16,input);
    Check(sim.Dummy().hits==1 && Near(sim.Dummy().hp,65),"one first-ring arrow deals 35 immediately, without instant blast damage");
    Check(Near(sim.Dummy().lastDamage,35),"direct damage does not use the shooter's 36-to-18 age curve");
}
void FullVolleyAndFreeze() {
    Simulation sim; Prepare(sim); Controls input; input.aimPoint={.28f,1.1f,-4};
    sim.SetDummyPosition(input.aimPoint); Release(sim,input,144); Steps(sim,8,input);
    Check(sim.Dummy().hp==0 && sim.Dummy().hits==3 && sim.Dummy().kills==1,"three full-charge direct hits total 105 and splat the 100HP target in one volley");
    Check(sim.EmbeddedArrows().empty(),"arrows absorbed by a direct target never lodge or schedule a ground explosion");
    Simulation freeze; Prepare(freeze); input=Forward(freeze); Release(freeze,input,60);
    input.fire=true; Steps(freeze,29,input);
    Check(!freeze.IsCharging() && freeze.ShotsFired()==1,"first-ring freeze prevents a new charge for 15F");
    freeze.Step(Dt,input);
    Check(freeze.IsCharging() && freeze.ShotsFired()==1,"charge resumes after freeze without automatic firing");
}
void EmptyInkAndSwitchCancel() {
    Simulation empty; Prepare(empty); auto input=Forward(empty);
    for(int n=0;n<20;++n) { Release(empty,input,18); Steps(empty,24,input); }
    Check(empty.ShotsFired()==20 && empty.Player().ink<.0001f,"twenty tap volleys exhaust a full tank without per-arrow overcharging");
    Release(empty,input,18);
    Check(empty.ShotsFired()==20 && empty.Player().ink>=0,"insufficient ink emits no unpaid volley or negative tank");
    Simulation switching; Prepare(switching); input=Forward(switching); input.fire=true;
    Steps(switching,144,input); WeaponDefinition shooter; shooter.id="shooter";
    switching.Equip(shooter); Steps(switching,180,input);
    Check(switching.ShotsFired()==0 && !switching.IsCharging(),"weapon switch cancels charge and suppresses the still-held trigger");
    input.fire=false; switching.Step(Dt,input); input.fire=true; Steps(switching,6,input);
    Check(switching.ShotsFired()==1,"shooter starts normally after a release and new press");
    switching.Equip(Stringer()); input.fire=false; switching.Step(Dt,input);
    input.fire=true; switching.Step(Dt,input);
    Check(switching.ChargeTime()==0,"returning to Stringer cannot restore its previous charge or bypass the shooter's remaining cooldown");
    Steps(switching,14,input);
    Check(switching.IsCharging() && switching.ChargeTime()<.06f,"after the preserved cooldown the Stringer starts a fresh charge");
}
void DelayedExplosion() {
    Simulation sim; Prepare(sim); auto input=Forward(sim); Release(sim,input,144);
    for(int n=0;n<180 && !sim.Projectiles().empty();++n) sim.Step(Dt,input);
    Check(sim.EmbeddedArrows().size()==3,"a full ground volley leaves three visible lodged arrows");
    Check(sim.StampCount()>0,"arrows paint at impact before their delayed blast");
    auto left=*std::min_element(sim.EmbeddedArrows().begin(),sim.EmbeddedArrows().end(),
        [](const EmbeddedArrow& a,const EmbeddedArrow& b){return a.position.x<b.position.x;});
    Check(Near(left.remaining,.75f,Dt+.0001f),"lodged arrows start with a 45F fuse");
    sim.SetDummyPosition(left.position+Vec3{-1.48f,.3f,0});
    const int before=static_cast<int>(std::ceil(left.remaining/Dt))-1;
    Steps(sim,before,input);
    Check(sim.Dummy().hits==0 && !sim.EmbeddedArrows().empty(),"blast damage must wait for the complete fuse");
    sim.Step(Dt,input);
    Check(sim.Dummy().hits==1 && Near(sim.Dummy().hp,70),"one intersecting blast deals flat 30 damage at expiry");
    Check(sim.EmbeddedArrows().empty(),"each lodged arrow expires after a single detonation");
    int blasts=0; for(const auto& event:sim.TakeImpactEvents()) if(event.kind==PaintKind::Explosion) ++blasts;
    Check(blasts==3,"three lodged arrows emit three distinct explosion events");
    Steps(sim,120,input); Check(sim.Dummy().hits==1,"expired arrows do not repeat damage");
}
void WallOcclusion() {
    Simulation sim; Prepare(sim); sim.SetPlayerPosition({-2,0,3.68f}); sim.Step(Dt,{});
    const Vec3 target={-2,1.1f,6}; sim.SetDummyPosition(target); Controls input; input.aimPoint=target;
    Release(sim,input,60); Steps(sim,120,input);
    Check(sim.Dummy().hits==0 && Near(sim.Dummy().hp,100),"nearby solid panel blocks both direct arrows and blasts behind it");
    const auto& mask=sim.Mask(15);
    Check(std::all_of(mask.begin(),mask.end(),[](uint8_t value){return value==0;}),"impact and burst respect the non-inkable panel");
}
void SnapshotAcrossEditAndEquip() {
    Simulation baseline,changed; Prepare(baseline); Prepare(changed);
    auto input=Forward(baseline); Release(baseline,input,144); Release(changed,input,144);
    Check(changed.Projectiles().size()==3,"snapshot test starts with flying arrows");
    const auto recorded=changed.Projectiles().front();
    WeaponDefinition shooter; shooter.id="edited-shooter"; shooter.type=WeaponClass::Shooter;
    shooter.shooter.projectileSpeed=1; shooter.shooter.projectileGravity=1;
    shooter.shooter.impactPaintRadius=.01f; shooter.shooter.impactCoreScale=.1f;
    shooter.stringer.fullDamage=900; shooter.stringer.freeGravity=0;
    shooter.stringer.detonationTime=20; shooter.stringer.explosionDamage=900;
    shooter.stringer.explosionDamageRadius=20; shooter.stringer.explosionPaintRadius=20;
    changed.Equip(shooter);
    Check(Near(changed.Projectiles().front().directDamage,recorded.directDamage),"equipping cannot change an existing arrow's damage");
    for(int n=0;n<240;++n) {
        baseline.Step(Dt,input); changed.Step(Dt,input);
        Check(baseline.Projectiles().size()==changed.Projectiles().size(),"old arrows retain flight and collision lifetime through equip");
        for(size_t i=0;i<baseline.Projectiles().size();++i)
            Check(Length(baseline.Projectiles()[i].position-changed.Projectiles()[i].position)<.0001f,"old arrow flight is independent of newly edited weapon parameters");
        Check(baseline.EmbeddedArrows().size()==changed.EmbeddedArrows().size(),"stuck-arrow fuse also survives weapon changes");
    }
    for(uint32_t s=0;s<baseline.Surfaces().size();++s)
        Check(baseline.Mask(s)==changed.Mask(s),"in-flight and embedded snapshots retain all impact / burst paint after edits and equip");
}
void ResourceBounds() {
    Simulation sim; Prepare(sim); sim.stringer.minInkConsume=sim.stringer.midInkConsume=sim.stringer.fullInkConsume=0;
    auto input=Forward(sim); size_t peakArrows=0,peakDrops=0,peakEmbedded=0;
    for(int n=0;n<2400;++n) {
        input.fire=n%96<60; sim.Step(Dt,input);
        peakArrows=(std::max)(peakArrows,sim.Projectiles().size());
        peakDrops=(std::max)(peakDrops,sim.Droplets().size());
        peakEmbedded=(std::max)(peakEmbedded,sim.EmbeddedArrows().size());
        Check(sim.Player().ink>=0 && sim.Player().ink<=1,"ink resource remains in bounds");
        sim.TakePendingStamps();
    }
    Check(sim.ShotsFired()>=20,"resource test actually fires repeated charged volleys");
    Check(peakArrows<24 && peakDrops<100 && peakEmbedded<=128,"projectile and lodged-arrow populations stay bounded");
    Check(sim.TakeImpactEvents().size()<=256,"undrained visual impact history remains capped");
    input.fire=false; Steps(sim,480,input);
    Check(sim.Projectiles().empty() && sim.Droplets().empty() && sim.EmbeddedArrows().empty(),"all transient weapons resources retire after firing stops");
    std::cout<<"Resource peaks: "<<peakArrows<<" arrows, "<<peakDrops<<" drops, "<<peakEmbedded<<" embedded\n";
}
}
int main() {
    int failed=0;
    auto run=[&](const char* name,void(*test)()) {
        try { test(); std::cout<<"PASS "<<name<<'\n'; }
        catch(const std::exception& error) { ++failed; std::cerr<<"FAIL "<<name<<": "<<error.what()<<'\n'; }
    };
    run("tap and 9F minimum",TapAndMinimum);
    run("hold, charge endpoints and one release",HoldAndEndpoints);
    run("60 / 120Hz charge timing",ChargeCadence60And120);
    run("air charge rate and vertical fan",AirRateAndVerticalFan);
    run("cancel and no charge keep",CancelAndNoChargeKeep);
    run("ink recovery and post-shot lock",RecoveryAndPostDelay);
    run("single-arrow direct damage",OneArrowDirectDamage);
    run("105-damage volley and 15F recharge freeze",FullVolleyAndFreeze);
    run("empty ink and switching cancels charge",EmptyInkAndSwitchCancel);
    run("45F delayed explosion",DelayedExplosion);
    run("near-wall occlusion",WallOcclusion);
    run("immutable flight and paint snapshots",SnapshotAcrossEditAndEquip);
    run("bounded transient resources",ResourceBounds);
    if(failed) std::cerr<<failed<<" Stringer simulation test group(s) failed\n";
    return failed?1:0;
}
