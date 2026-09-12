// Standalone: compile with InkSimulation.cpp; no DirectX, windows.h or game assets.
#include "../game/ink/InkSimulation.h"
#include "../game/ink/InkSpreadPattern.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <queue>

using namespace ink;
namespace {
void Require(bool condition,const char* message) {
    if(!condition) { std::cerr<<"FAIL: "<<message<<'\n'; std::exit(1); }
}
void Advance(Simulation& sim,float seconds,const Controls& controls,float dt=1.0f/120.0f) {
    const int steps=static_cast<int>(std::round(seconds/dt));
    for(int i=0;i<steps;++i) sim.Step(dt,controls);
}
void Fill(Simulation& sim,uint32_t surface,uint32_t team=1) {
    const auto& plane=sim.Surfaces().at(surface);
    sim.Paint({surface,plane.width/2,plane.height/2,100,100,0,team});
}
void MasksAndGeometry() {
    Simulation sim;
    Require(sim.Surfaces().size()==16,"stage fits renderer's sixteen layers");
    const PaintStamp ellipse={0,15,6,1.2f,0.4f,0.6f,1};
    sim.Paint(ellipse);
    Require(sim.GetInkAtWorldPosition({0,0,-8})==1,"paint can be queried at the player's feet");
    Require(sim.GetInkAtWorldPosition({0,1,-8})==0,"ink is local to a surface, not an infinite volume");
    const auto& plane=sim.Surfaces()[0];
    const auto& mask=sim.Mask(0);
    const float c=std::cos(ellipse.angle),s=std::sin(ellipse.angle);
    for(int y=0;y<Simulation::MaskResolution;++y) for(int x=0;x<Simulation::MaskResolution;++x) {
        const float du=(x+0.5f)*plane.width/Simulation::MaskResolution-ellipse.u;
        const float dv=(y+0.5f)*plane.height/Simulation::MaskResolution-ellipse.v;
        const float a=(c*du+s*dv)/ellipse.radiusU,b=(-s*du+c*dv)/ellipse.radiusV;
        Require((mask[y*Simulation::MaskResolution+x]==1)==(a*a+b*b<=1),"CPU mask matches GPU ellipse sample equation");
    }
    Fill(sim,0,2);
    Require(sim.GetInkAtWorldPosition({0,0,-8})==2,"team identity overwrites previous paint");
    Fill(sim,0,0);
    Require(sim.GetInkAtWorldPosition({0,0,-8})==0,"team zero erases paint");
    const auto before=sim.StampCount(); Fill(sim,15);
    Require(sim.StampCount()==before,"metal panel rejects paint");
    Fill(sim,0,3);
    Require(sim.StampCount()==before,"unsupported team IDs are rejected by both CPU and GPU contracts");
    Require(sim.GetInkAtWorldPosition({-2,1,4})==0,"non-inkable panel has no ink");
    Fill(sim,9);
    Require(sim.GetInkAtWorldPosition({0,0,13.8f})==0,"wall ink does not leak onto adjacent dry floor");
    const auto ramp=sim.Raycast({7.5f,6,4},{0,-1,0},10);
    Require(ramp.hit && ramp.surface==4 && std::abs(ramp.position.y-1.5f)<0.001f,"downward ray hits finite inclined ramp");
    Require(!sim.Raycast({7.5f,1,4},{0,-1,0},5).hit,"no hidden floor underneath the ramp");
    const auto wall=sim.Raycast({0,1,10},{0,0,1},10,0.1f);
    Require(wall.hit && wall.surface==9 && std::abs(wall.distance-3.9f)<0.001f,"swept radius detects wall before center crosses it");
    auto pending=sim.TakePendingStamps();
    Require(!pending.empty() && sim.TakePendingStamps().empty(),"GPU stamp transfer drains the queue");
}
void ShootingAndInk() {
    Simulation at60,at120;
    Controls fire; fire.fire=true; fire.aimPoint={0,1,14};
    Advance(at60,2,fire,1.0f/60.0f); Advance(at120,2,fire);
    Require(at60.ShotsFired()==20 && at120.ShotsFired()==20,"six-frame cadence emits twenty shots in two seconds at 60 and 120 Hz");
    Require(std::abs(at60.Player().ink-(1-20*0.0092f))<0.0001f,"tank consumes researched amount per shot");
    Require(at120.StampCount()>20,"projectiles paint irregular impact and airborne droplet stamps");
    const auto painted=at120.GetInkAtWorldPosition({0,0,-7});
    Require(painted==1,"flight droplets make a traversable trail near the muzzle");
    Require(at120.GetInkAtWorldPosition({0,0,-8})==1,"sustained fire adds a small rescue drop connecting dry player feet to the trail");
    Advance(at120,10,fire);
    Require(at120.ShotsFired()==108,"empty tank prevents additional projectiles");
    Require(at120.Player().ink>=0 && at120.Player().ink<at120.weapon.inkConsume,"tank stays bounded after empty fire");
    const float empty=at120.Player().ink;
    Controls idle; Advance(at120,0.5f,idle);
    Require(at120.Player().ink>empty,"human ink recovery starts after release and recovery lock");
    Simulation human,swim;
    Advance(human,3,fire); Advance(swim,3,fire);
    Fill(swim,0);
    Advance(human,0.5f,idle); Controls submerged; submerged.swim=true; Advance(swim,0.5f,submerged);
    const float humanStart=human.Player().ink,swimStart=swim.Player().ink;
    Advance(human,0.5f,idle); Advance(swim,0.5f,submerged);
    Require(swim.Player().ink-swimStart>2*(human.Player().ink-humanStart),"own-ink swim regenerates over twice as fast as human form");
    Simulation delayed; Advance(delayed,0.1f,fire); const float afterShot=delayed.Player().ink;
    Advance(delayed,0.2f,idle);
    Require(std::abs(delayed.Player().ink-afterShot)<0.0001f,"recovery lock prevents immediate refill after shooting");
    Simulation postShot; Fill(postShot,0);
    while(postShot.ShotsFired()==0) postShot.Step(1.0f/120.0f,fire);
    postShot.Step(1.0f/120.0f,submerged);
    Require(postShot.Player().state==PlayerState::Human,"postshot delay prevents immediate swim transition");
    Advance(postShot,0.1f,submerged);
    Require(postShot.Player().state==PlayerState::Swim,"swim transition becomes available after postshot delay");
}
void MovementAndWalls() {
    Simulation human,swim;
    Fill(swim,0);
    Controls move; move.moveZ=1;
    Controls dive=move; dive.swim=true;
    Advance(human,1,move); Advance(swim,1,dive);
    Require(human.Player().grounded && swim.Player().state==PlayerState::Swim,"human remains grounded and swimmer enters own ink");
    Require(swim.Player().position.z+8>1.7f*(human.Player().position.z+8),"own-ink swim travels substantially faster");
    Fill(swim,0,0); Advance(swim,0.05f,dive);
    Require(swim.Player().state==PlayerState::Squid && swim.Player().speed>swim.movement.drySquidSpeed && swim.DrySquidCarryRemaining()>0,"leaving ink keeps exposed squid form and briefly carries existing swim momentum");
    Advance(swim,swim.movement.drySquidCarryMaxTime,dive);
    Require(swim.Player().speed<=swim.movement.drySquidSpeed+0.001f && swim.DrySquidCarryRemaining()==0,"dry-ground momentum expires and returns to the crawl speed");
    Simulation side;
    side.SetPlayerPosition({14,0,-8}); Controls right; right.moveX=1; Advance(side,1,right);
    Require(side.Player().position.x<=15-side.movement.humanRadius+0.001f,"human capsule remains inside finite stage walls");
    Simulation ramp; ramp.SetPlayerPosition({7.5f,0,-1}); Advance(ramp,2.5f,move);
    Require(ramp.Player().grounded && ramp.Player().position.y>1.8f,"walking follows ramp height without falling through");
    Require(std::abs(ramp.Player().position.y-ramp.Player().position.z*0.375f)<0.02f,"ramp support follows its real plane equation");
    Simulation rampSide; rampSide.SetPlayerPosition({4,0,4}); Advance(rampSide,1,right);
    Require(rampSide.Player().grounded && rampSide.Player().position.x<=5-rampSide.movement.humanRadius+0.01f && std::abs(rampSide.Player().position.y)<0.001f,"ramp's elevated side blocks entry into the cutout beneath its top");
    Simulation jump; Controls leap; leap.jump=true; jump.Step(1.0f/120.0f,leap);
    Require(!jump.Player().grounded && jump.Player().velocity.y>0,"jump applies upward velocity");
    Require(jump.Player().accuracy>jump.weapon.groundSpread*2,"jump initially increases spread");
    Advance(jump,2,Controls{});
    Require(jump.Player().grounded && std::abs(jump.Player().position.y)<0.001f,"gravity returns player to floor");
    Require(std::abs(jump.Player().accuracy-jump.weapon.groundSpread)<0.01f,"jump accuracy recovers with elapsed time");
    Simulation wall; Fill(wall,9); wall.SetPlayerPosition({0,0,13.7f});
    Advance(wall,0.5f,dive);
    Require(wall.Player().state==PlayerState::WallSwim && wall.Player().position.y>1.5f,"painted wall attaches swimmer and allows upward motion");
    Advance(wall,1.0f,dive);
    Require(wall.Player().position.z>14 && std::abs(wall.Player().position.y-6)<0.01f,"swimmer mantles onto real platform behind wall");
    wall.Reset(); Fill(wall,9); wall.SetPlayerPosition({0,0,13.7f}); Advance(wall,0.3f,dive);
    dive.jump=true; wall.Step(1.0f/120.0f,dive);
    Require(wall.Player().state==PlayerState::Squid && wall.Player().velocity.z<0 && wall.Player().velocity.y>0,"wall jump detaches in squid form and kicks away from wall");
}
void FormAndSurfaceRules() {
    Controls squid; squid.swim=true; squid.moveZ=1;
    Simulation dry;
    Advance(dry,0.5f,squid);
    Require(dry.Player().state==PlayerState::Squid && dry.Player().grounded && !dry.Player().onOwnInk,
        "dry floor allows squid form without submerging");
    Require(std::abs(dry.Player().speed-dry.movement.drySquidSpeed)<0.001f && dry.StampCount()==0,
        "exposed squid crawls and never creates free transformation paint");
    squid.jump=true; dry.Step(1.0f/120.0f,squid); squid.jump=false;
    Advance(dry,0.2f,squid);
    Require(dry.Player().state==PlayerState::Squid && !dry.Player().grounded,"squid persists during a dry-ground jump");
    Advance(dry,1,squid);
    Require(dry.Player().grounded && dry.Player().state==PlayerState::Squid && dry.StampCount()==0,
        "squid landing on unpainted ground neither forces human form nor paints");
    Simulation empty;
    Controls fire; fire.fire=true; fire.aimPoint={0,1,14}; Advance(empty,12,fire);
    Fill(empty,0,0); squid.moveZ=0;
    empty.Step(1.0f/120.0f,squid);
    Require(empty.Player().ink<empty.weapon.inkConsume && empty.Player().state==PlayerState::Squid,
        "tank below one-shot ink cost still allows exposed squid form");
    Simulation zero; zero.weapon.inkConsume=0.5f; Advance(zero,0.2f,fire); Fill(zero,0,0);
    Advance(zero,0.075f,squid);
    Require(zero.Player().ink==0 && zero.Player().state==PlayerState::Squid,
        "exactly zero ink can transform while refill is still locked");
    Fill(zero,0); zero.Step(1.0f/120.0f,squid);
    Require(zero.Player().ink==0 && zero.Player().state==PlayerState::Swim,
        "exactly zero ink can enter own ink without paying a form cost");
    // Drain all remaining flight drops before checking passive form effects.
    Advance(empty,3,squid); Fill(empty,0,0);
    const auto settled=empty.StampCount(); const float normalInk=empty.Player().ink;
    Advance(empty,0.25f,squid);
    Require(empty.StampCount()==settled && empty.Player().ink>normalInk &&
        std::abs(empty.Player().ink-normalInk-0.025f)<0.0002f,"exposed squid uses normal refill without new paint");
    Fill(empty,0); const float submergedInk=empty.Player().ink;
    Advance(empty,0.25f,squid);
    Require(empty.Player().state==PlayerState::Swim &&
        std::abs(empty.Player().ink-submergedInk-0.25f/3)<0.0002f,"own surface alone grants fast submerged refill");
    Simulation run; Fill(run,0); squid.moveZ=1; Advance(run,0.4f,squid);
    squid.jump=true; run.Step(1.0f/120.0f,squid); squid.jump=false;
    Advance(run,0.2f,squid);
    Require(run.Player().state==PlayerState::Squid && !run.Player().onOwnInk &&
        run.Player().speed>run.movement.swimSpeed*0.98f,"swim takeoff keeps horizontal momentum while exposed in air");
    Simulation airborne; airborne.SetPlayerPosition({0,2,-8}); Advance(airborne,0.1f,squid);
    Require(airborne.Player().state==PlayerState::Squid && airborne.Player().accuracy==airborne.weapon.groundSpread,
        "air transformation is allowed and falling without jumping does not add jump spread");
    Simulation enemy; Fill(enemy,0,2); Controls walk; walk.moveZ=1; Advance(enemy,0.5f,walk);
    Require(enemy.Player().onEnemyInk && std::abs(enemy.Player().speed-enemy.movement.enemyInkSpeed)<0.001f,
        "enemy ink applies the source non-shooting movement cap");
    walk.fire=true; walk.aimPoint={0,1,14}; Advance(enemy,0.05f,walk);
    Require(enemy.Player().speed<=enemy.movement.enemyInkShotSpeed+0.001f,"enemy ink applies the source shooting cap");
    walk.jump=true; enemy.Step(1.0f/120.0f,walk);
    Require(!enemy.Player().grounded && enemy.Player().velocity.y<enemy.movement.enemyInkJumpSpeed &&
        enemy.Player().velocity.y>2,"enemy ink jump uses reduced takeoff velocity");
}
void TapFireCadence() {
    for(float dt:{1.0f/60.0f,1.0f/120.0f}) for(bool squid:{false,true}) {
        Simulation sim; Controls form; form.swim=squid; sim.Step(dt,form);
        Controls fire; fire.fire=true; fire.aimPoint={0,1,14};
        const int delaySteps=static_cast<int>(std::round((squid?sim.weapon.swimInitialShotDelay:sim.weapon.initialShotDelay)/dt));
        for(int n=1;n<=delaySteps;++n) {
            sim.Step(dt,fire);
            Require(sim.ShotsFired()==(n==delaySteps?1:0),"human 3F and exposed squid 12F first-shot timing respect source-frame boundaries");
        }
    }
    for(float dt:{1.0f/60.0f,1.0f/120.0f}) for(int held:{3,6,8,12}) {
        Simulation sim; Controls fire; fire.aimPoint={0,1,14};
        int lastShot=-1000; uint64_t shots=0;
        const int frames=static_cast<int>(3/dt);
        for(int n=0;n<frames;++n) {
            fire.fire=n%(held+1)<held;
            sim.Step(dt,fire);
            if(sim.ShotsFired()!=shots) {
                Require((n-lastShot)*dt>=sim.weapon.FireInterval()-0.00001f,
                    "release/repress cannot shorten the six-frame interval");
                lastShot=n; shots=sim.ShotsFired();
            }
        }
        if(held>=6) Require(shots>5,"tap cadence scenario actually fires repeatedly");
    }
}
void SpreadAndEmission() {
    for(float bias:{0.01f,0.25f,0.40f}) {
        const float spread=11.66f;
        Require(std::abs(SampleSignedSpreadDegrees(0.5f,0.75f,spread,bias)-spread*bias)<0.00001f,
            "measured shooter bias controls median horizontal deviation, not outer-ring probability");
        for(float pitch:{-1.5707963f,-1.3f,0.0f,1.3f,1.5707963f}) {
            const Vec3 aim={0,std::sin(pitch),std::cos(pitch)};
            const auto left=PerturbHorizontal(aim,SampleSignedSpreadDegrees(0.8f,0.25f,spread,bias));
            const auto right=PerturbHorizontal(aim,SampleSignedSpreadDegrees(0.8f,0.75f,spread,bias));
            const float degrees=SampleSignedSpreadDegrees(0.8f,0.75f,spread,bias);
            Require(std::abs(Dot(aim,right)-std::cos(degrees*3.14159265f/180))<0.00001f &&
                std::abs(left.x+right.x)<0.00001f && std::abs(left.y-right.y)<0.00001f,
                "left/right angular spread remains symmetric and pitch independent including vertical aim");
        }
    }
    Simulation sim; Fill(sim,0); sim.TakeImpactEvents();
    Controls fire; fire.fire=true; fire.aimPoint={0,1,14};
    bool one=false,two=false;
    for(int n=0;n<16;++n) {
        const auto before=sim.ShotsFired();
        Advance(sim,0.075f,fire); Advance(sim,0.8f,Controls{});
        Require(sim.ShotsFired()==before+1,"emission scenario samples one actual shot at a time");
        int drops=0;
        for(const auto& event:sim.TakeImpactEvents()) if(event.kind==PaintKind::Droplet) ++drops;
        Require(drops<=2,"source fractional budget never grows into eight drops per bullet");
        one|=drops==1; two|=drops==2;
    }
    Require(one && two,"actual colliding drops vary between single shots, with phase continuing across trigger releases");
    Simulation wall; Fill(wall,0); wall.SetPlayerPosition({0,0,13}); Advance(wall,0.1f,Controls{});
    fire.aimPoint={0,1.1f,14}; Advance(wall,0.075f,fire); Advance(wall,0.8f,Controls{});
    for(const auto& event:wall.TakeImpactEvents())
        Require(event.kind!=PaintKind::Droplet,"near wall truncates emission before any planned flight drop");
    Simulation panel; panel.SetPlayerPosition({-2.5f,0,3.72f});
    fire.aimPoint={-2.5f,1.1f,10}; Advance(panel,2,fire);
    for(const auto& event:panel.TakeImpactEvents())
        Require(event.kind!=PaintKind::Foot || event.position.z<4,"foot rescue cannot spawn through a nearby noninkable panel");
    Require(panel.GetInkAtWorldPosition({-2.4f,0,4.2f})==0,"blocked foot placement does not paint behind the metal panel");
}
void IncidenceAndPaintKinds() {
    Simulation sim;
    const auto& plane=sim.Surfaces()[0];
    const Vec3 point={0,0,-3};
    const auto normal=Simulation::ImpactBrush(plane,0,point,{0,-1,0},1,PaintKind::Main,sim.weapon);
    const auto alongX=Simulation::ImpactBrush(plane,0,point,{1,-0.1f,0},1,PaintKind::Main,sim.weapon);
    const auto alongZ=Simulation::ImpactBrush(plane,0,point,{0,-0.1f,1},1,PaintKind::Main,sim.weapon);
    Require(std::abs(normal.radiusU-normal.radiusV)<0.001f,"normal impact has no arbitrary stretched direction");
    Require(alongX.radiusU>alongX.radiusV*2 && alongZ.radiusU>alongZ.radiusV*2,"shallow impact stretches in the surface plane");
    Require(std::abs(alongX.angle)<0.001f && std::abs(alongZ.angle-1.5707963f)<0.001f,"rotating incoming velocity rotates the long paint axis");
    const auto wallNormal=Simulation::ImpactBrush(sim.Surfaces()[9],9,{0,2,14},{0,0,1},1,PaintKind::Main,sim.weapon);
    const auto wallShallow=Simulation::ImpactBrush(sim.Surfaces()[9],9,{0,2,14},{1,0,0.1f},1,PaintKind::Main,sim.weapon);
    Require(std::abs(wallNormal.radiusU-wallNormal.radiusV)<0.001f && wallShallow.radiusU>wallShallow.radiusV*2,
        "wall shape uses its own normal rather than the world-up floor normal");
    const auto rampNormal=Simulation::ImpactBrush(sim.Surfaces()[4],4,{7,1.5f,4},sim.Surfaces()[4].normal*-1,
        1,PaintKind::Main,sim.weapon);
    Require(std::abs(rampNormal.radiusU-rampNormal.radiusV)<0.001f,"ramp normal impact is also round");
    const auto drop=Simulation::ImpactBrush(plane,0,point,{0,-1,0},sim.weapon.paintDropletRadius,PaintKind::Droplet,sim.weapon);
    const auto foot=Simulation::ImpactBrush(plane,0,point,{0,-1,0},sim.weapon.nearestPaintDropletRadius,PaintKind::Foot,sim.weapon);
    const auto scatter=Simulation::ImpactBrush(plane,0,point,{0,-1,0},1,PaintKind::Scatter,sim.weapon);
    Require(normal.radiusV>drop.radiusV*1.5f && scatter.radiusV<drop.radiusV*0.35f && foot.radiusV<normal.radiusV,
        "main impact, trail drops, small satellites and foot rescue have separate size budgets");
    Controls fire; fire.fire=true; fire.aimPoint={0,0,-3}; Advance(sim,2,fire);
    auto pending=sim.TakePendingStamps();
    bool mainSeen=false,dropSeen=false,footSeen=false,scatterSeen=false;
    Simulation replay;
    for(const auto& stamp:pending) {
        mainSeen|=stamp.kind==PaintKind::Main; dropSeen|=stamp.kind==PaintKind::Droplet;
        footSeen|=stamp.kind==PaintKind::Foot; scatterSeen|=stamp.kind==PaintKind::Scatter;
        replay.Paint(stamp);
    }
    Require(mainSeen && dropSeen && footSeen && scatterSeen,"live firing produces all four paint roles");
    for(uint32_t i=0;i<sim.Surfaces().size();++i)
        Require(sim.Mask(i)==replay.Mask(i),"transferred directional ellipses exactly rebuild every CPU owner cell");
    const auto events=sim.TakeImpactEvents();
    Require(!events.empty() && events.size()<=256 && sim.TakeImpactEvents().empty(),"impact events are bounded and drain independently");
    for(size_t i=1;i<events.size();++i) Require(events[i].sequence>events[i-1].sequence,"impact sequence is strictly increasing");
}
void PaintDoesNotChangeBallistics() {
    Simulation sparse,detailed;
    sparse.weapon.paintDropletCount=0;
    detailed.weapon.impactCoreScale=0.4f;
    detailed.weapon.paintDropletSpacing=0.4f;
    Controls fire; fire.fire=true; fire.aimPoint={0,1,14};
    for(int i=0;i<360;++i) {
        sparse.Step(1.0f/120.0f,fire); detailed.Step(1.0f/120.0f,fire);
        Require(sparse.Projectiles().size()==detailed.Projectiles().size(),"cosmetic brush work cannot change main projectile lifetime");
        for(size_t p=0;p<sparse.Projectiles().size();++p) {
            Require(Length(sparse.Projectiles()[p].position-detailed.Projectiles()[p].position)<0.000001f &&
                Length(sparse.Projectiles()[p].velocity-detailed.Projectiles()[p].velocity)<0.000001f,
                "main shot spread and trajectories use a random stream independent of painting");
        }
        sparse.TakePendingStamps(); detailed.TakePendingStamps();
    }
    Require(sparse.StampCount()!=detailed.StampCount(),"RNG independence scenario exercises different amounts of brush work");
    const ShooterWeaponParams w; const MovementParams m;
    Require(w.repeatFrame==6 && w.inkConsume==0.0092f && w.inkRecoverStop==20.0f/60.0f,
        "phase two preserves cadence, ink cost and refill lock source values");
    Require(std::abs(w.projectileSpeed-67.98f)<0.001f && w.straightFlightTime==4.0f/60 &&
        std::abs(w.brakeInitialSpeed-44.79f)<0.001f && w.brakeAirResistance==0.36f &&
        std::abs(w.brakeGravity-126)<0.001f && std::abs(w.projectileGravity-28.8f)<0.001f && w.freeAirResistance==0.02f,
        "phase two preserves researched ballistic parameters");
    Require(w.groundSpread==4.86f && w.jumpSpread==11.66f && std::abs(m.humanSpeed-2.88f)<0.001f &&
        std::abs(m.swimSpeed-5.76f)<0.001f && m.humanInkRecovery==0.1f && m.swimInkRecovery==1.0f/3,
        "phase two preserves accuracy, movement and recovery rates");
}
void ConnectedPaintTrail() {
    // Sample different shot/paint random states and firing directions. Checking
    // a nearby painted point alone misses dry gaps between otherwise valid drops.
    for(int warmupShots:{0,3,7,13,24}) for(float yaw:{-0.9f,-0.6f,0.0f,0.6f,0.9f}) {
        Simulation sim;
        Controls warm; warm.fire=true; warm.aimPoint={-5,3,0};
        Advance(sim,warmupShots*0.1f,warm); Advance(sim,3,Controls{});
        Fill(sim,0,0);
        const Vec3 forward={std::sin(yaw),0,std::cos(yaw)},start={0,0,-8};
        Controls fire; fire.fire=true; fire.yaw=yaw; fire.aimPoint=start+forward*5;
        Advance(sim,1,fire);
        const auto& plane=sim.Surfaces()[0]; const auto& mask=sim.Mask(0);
        constexpr int n=Simulation::MaskResolution;
        const Vec3 local=start-plane.origin;
        const int first=static_cast<int>(Dot(local,plane.v)/plane.height*n)*n+
            static_cast<int>(Dot(local,plane.u)/plane.width*n);
        Require(mask[first]==1,"sustained shooting paints the route's starting foot cell");
        std::queue<int> pending; pending.push(first);
        std::vector<bool> visited(n*n); visited[first]=true;
        float reach=0;
        while(!pending.empty()) {
            const int cell=pending.front(); pending.pop();
            const int x=cell%n,y=cell/n;
            const Vec3 point=plane.origin+plane.u*((x+0.5f)*plane.width/n)+plane.v*((y+0.5f)*plane.height/n);
            reach=std::max(reach,Dot(point-start,forward));
            for(int direction=0;direction<4;++direction) {
                const int nx=x+(direction==0)-(direction==1),ny=y+(direction==2)-(direction==3);
                if(nx<0 || nx>=n || ny<0 || ny>=n) continue;
                const int neighbor=ny*n+nx;
                if(mask[neighbor]==1 && !visited[neighbor]) { visited[neighbor]=true; pending.push(neighbor); }
            }
        }
        if(reach<=4.4f) std::cerr<<"Connectivity: warm="<<warmupShots<<" yaw="<<yaw<<" reach="<<reach<<'\n';
        Require(reach>4.4f,"own-ink cells form a continuous route from the player's feet into the main impact, with no dry gap");
    }
}
void DamageDummy() {
    ShooterWeaponParams source;
    Require(Simulation::CalculateDamage(0,source)==36 && Simulation::CalculateDamage(source.damageFalloffStart,source)==36,
        "dummy damage is 36 up to the existing falloff start");
    Require(Simulation::CalculateDamage(source.damageFalloffEnd,source)==18 && Simulation::CalculateDamage(2,source)==18,
        "dummy damage is clamped to 18 after the existing falloff end");
    Require(std::abs(Simulation::CalculateDamage((source.damageFalloffStart+source.damageFalloffEnd)*0.5f,source)-27)<0.001f,
        "damage interpolates by projectile age between source endpoints");
    Simulation close;
    close.weapon.groundSpread=0; close.weapon.jumpSpread=0;
    Controls fire; fire.fire=true; const auto muzzle=close.Muzzle(fire);
    close.SetDummyPosition(muzzle+Vec3{0,0,3}); fire.aimPoint=close.Dummy().position;
    Advance(close,0.4f,fire);
    Require(close.Dummy().hp==0 && close.Dummy().lastDamage==36 && close.Dummy().lastShotsToKill==3 && close.Dummy().kills==1,
        "near target is defeated by three swept projectile hits for 36 damage each");
    Advance(close,2.1f,Controls{});
    Require(close.Dummy().hp==100 && close.Dummy().hits==0 && close.Dummy().lastShotsToKill==3,
        "defeated dummy resets its health automatically and keeps the completed shot count");
    Simulation late;
    late.weapon.groundSpread=0; late.weapon.jumpSpread=0;
    // Slow, straight diagnostic shots isolate age falloff without changing the
    // production defaults or requiring a target to follow the falling parabola.
    late.weapon.projectileSpeed=2; late.weapon.straightFlightTime=2;
    late.SetDummyPosition(late.Muzzle(fire)+Vec3{0,0,2.2f}); fire.aimPoint=late.Dummy().position;
    Advance(late,1.5f,fire);
    Require(late.Dummy().hp==0 && late.Dummy().lastDamage==18 && late.Dummy().lastShotsToKill==6,
        "late-age swept target hits use minimum damage and need six shots");
    Simulation blocked;
    blocked.weapon.groundSpread=0; blocked.weapon.jumpSpread=0;
    blocked.SetPlayerPosition({-2,0,1}); blocked.SetDummyPosition({-1.72f,1.1f,5}); fire.aimPoint=blocked.Dummy().position;
    Advance(blocked,1,fire);
    Require(blocked.Dummy().hp==100 && blocked.Dummy().hits==0,"nearer metal stage panel occludes target damage");
    Simulation radius;
    radius.weapon.groundSpread=0; radius.weapon.jumpSpread=0;
    const auto start=radius.Muzzle(fire); radius.SetDummyPosition(start+Vec3{0.60f,0,3}); fire.aimPoint=start+Vec3{0,0,10};
    Advance(radius,0.2f,fire);
    Require(radius.Dummy().hits>0,"target sweep includes the researched player hit radius beyond the visible sphere");
}
void SustainedResourceBounds() {
    Simulation sim;
    Fill(sim,0); sim.TakePendingStamps();
    size_t mostProjectiles=0,mostDroplets=0,mostStampsPerStep=0,totalStamps=0;
    constexpr int frames=7200;
    for(int frame=0;frame<frames;++frame) {
        Controls controls;
        controls.yaw=frame*0.0028f;
        controls.moveZ=0.55f;
        controls.moveX=0.25f*std::sin(frame*0.003f);
        controls.fire=frame%1440<960;
        controls.swim=!controls.fire;
        controls.jump=frame%480==0;
        const Vec3 forward={std::sin(controls.yaw),0,std::cos(controls.yaw)};
        controls.aimPoint=sim.Player().position+forward*20+Vec3{0,1+std::sin(frame*0.008f)*6,0};
        sim.Step(1.0f/120.0f,controls);
        const auto& player=sim.Player();
        Require(std::isfinite(player.position.x) && std::isfinite(player.position.y) && std::isfinite(player.position.z) &&
            std::isfinite(player.speed) && std::isfinite(player.ink),"sustained movement and firing keep simulation state finite");
        Require(player.ink>=0 && player.ink<=1,"tank stays bounded throughout stress scenario");
        Require(player.position.x>=-15 && player.position.x<=15 && player.position.z>=-14 && player.position.z<=20,"stress movement stays within finite stage");
        mostProjectiles=std::max(mostProjectiles,sim.Projectiles().size());
        mostDroplets=std::max(mostDroplets,sim.Droplets().size());
        const auto pending=sim.TakePendingStamps();
        mostStampsPerStep=std::max(mostStampsPerStep,pending.size()); totalStamps+=pending.size();
        Require(sim.Projectiles().size()<=24,"live projectiles stay bounded by cadence and lifetime");
        Require(sim.Droplets().size()<=400,"live falling droplets stay bounded by spawn cap and lifetime");
        Require(pending.size()<=4096,"render upload stays within per-frame stamp budget");
        Require(sim.TakePendingStamps().empty(),"consumed render stamps are not retained during long play");
        if(frame%240==0) Require(sim.TakeImpactEvents().size()<=256,"visual impact events remain bounded even when not drained every frame");
    }
    Advance(sim,5,Controls{}); sim.TakePendingStamps();
    Require(sim.Projectiles().empty() && sim.Droplets().empty(),"all projectiles and droplets expire after firing stops");
    Require(sim.ShotsFired()>150 && totalStamps>1000,"stress test exercises multiple tanks and substantial persistent painting");
    for(uint32_t i=0;i<sim.Surfaces().size();++i)
        Require(sim.Mask(i).size()==Simulation::MaskResolution*Simulation::MaskResolution,"persistent mask storage stays fixed in size");
    std::cout<<"Stress: "<<frames<<" frames @120Hz, "<<sim.ShotsFired()<<" shots, peak projectiles="<<mostProjectiles
             <<", droplets="<<mostDroplets<<", stamps/step="<<mostStampsPerStep<<'\n';
}
}
int main() {
    MasksAndGeometry(); ShootingAndInk(); MovementAndWalls(); FormAndSurfaceRules(); TapFireCadence(); SpreadAndEmission(); IncidenceAndPaintKinds(); PaintDoesNotChangeBallistics(); ConnectedPaintTrail(); DamageDummy(); SustainedResourceBounds();
    std::cout<<"PASS: shared ink masks, finite sweeps, 3F/12F start and 6F tap cadence, zero-ink/dry/air squid, surface recovery, enemy ink, swim momentum, pitch-independent spread, variable emission and occlusion, 25 connected trails, ramps/walls, dummy damage, independent RNG, resource bounds\n";
}
