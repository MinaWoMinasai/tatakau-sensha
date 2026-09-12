#include "InkSimulation.h"
#include "InkSpreadPattern.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace ink {
namespace {
constexpr float Pi = 3.14159265358979323846f;
float Clamp(float value, float low, float high) { return std::max(low, std::min(high, value)); }
float Approach(float value, float target, float amount) {
    return value < target ? std::min(value + amount, target) : std::max(value - amount, target);
}
bool Inside(const Surface& surface, Vec3 point, float margin=0) {
    const Vec3 delta=point-surface.origin;
    const float u=Dot(delta,surface.u), v=Dot(delta,surface.v);
    return u>=-margin && u<=surface.width+margin && v>=-margin && v<=surface.height+margin;
}
}

Simulation::Simulation() { Reset(); }

void Simulation::BuildStage() {
    surfaces_.clear();
    auto add=[&](Vec3 origin,Vec3 u,Vec3 v,Vec3 normal,float width,float height,
                 bool inkable=true,Vec3 color={0.35f,0.40f,0.45f}) {
        surfaces_.push_back({origin,u,v,normal,width,height,inkable,color});
    };
    // Split the floor around the slope/platform footprint: no coplanar overlaps
    // and no concealed floor that could falsely count as ink beneath the slope.
    add({-15,0,-14},{1,0,0},{0,0,1},{0,1,0},20,28);
    add({10,0,-14},{1,0,0},{0,0,1},{0,1,0},5,28);
    add({5,0,-14},{1,0,0},{0,0,1},{0,1,0},5,14);
    add({5,0,12},{1,0,0},{0,0,1},{0,1,0},5,2);
    const float slopeLength=std::sqrt(73.0f);
    add({5,0,0},{1,0,0},{0,3/slopeLength,8/slopeLength},{0,8/slopeLength,-3/slopeLength},5,slopeLength,true,{0.46f,0.49f,0.49f});
    add({5,3,8},{1,0,0},{0,0,1},{0,1,0},5,4,true,{0.46f,0.49f,0.49f});
    add({5,0,8},{0,0,1},{0,1,0},{-1,0,0},4,3);
    add({10,0,8},{0,0,1},{0,1,0},{1,0,0},4,3);
    add({5,0,12},{1,0,0},{0,1,0},{0,0,1},5,3);
    add({-15,0,14},{1,0,0},{0,1,0},{0,0,-1},30,6,true,{0.42f,0.46f,0.48f});
    add({-15,6,14},{1,0,0},{0,0,1},{0,1,0},30,6,true,{0.46f,0.49f,0.49f});
    add({-15,0,-14},{0,0,1},{0,1,0},{1,0,0},34,8);
    add({15,0,-14},{0,0,1},{0,1,0},{-1,0,0},34,8);
    add({-15,0,-14},{1,0,0},{0,1,0},{0,0,1},30,2,false,{0.21f,0.24f,0.28f});
    add({-15,6,20},{1,0,0},{0,1,0},{0,0,-1},30,2,false,{0.21f,0.24f,0.28f});
    // A contrasting metal panel deliberately rejects all paint.
    add({-4,0,4},{1,0,0},{0,1,0},{0,0,-1},3,2.6f,false,{0.65f,0.37f,0.20f});
}

void Simulation::Reset() {
    BuildStage();
    masks_.assign(surfaces_.size(),std::vector<uint8_t>(MaskResolution*MaskResolution,0));
    pendingStamps_.clear(); projectiles_.clear(); droplets_.clear(); impactEvents_.clear();
    embeddedArrows_.clear(); stringerEmission_.Reset(0x33bc1907u);
    charging_=false; releaseQueued_=false; requireTriggerRelease_=false; chargeTime_=0; chargeAge_=0;
    player_=PlayerStatus{};
    emissionPattern_.Reset();
    dummy_=DummyStatus{};
    stampCount_=0; shotsFired_=0; randomState_=0x7541a349u; paintRandomState_=0x2189a3c7u; impactSequence_=0;
    fireCooldown_=0; recoveryLock_=0; timeSinceShot_=10;
    accuracyBias_=weapon.accuracyBiasMinimum; timeSinceJump_=10; wallReattachLock_=0;
    airMoveSpeed_=movement.humanSpeed;
    drySquidCarryRemaining_=0;
    audioEvents_.clear();
    wasFire_=false; wasJump_=false; wasSwim_=false; wallSurface_=-1;
}

void Simulation::SetPlayerPosition(Vec3 position) {
    player_.position=position; player_.velocity={}; player_.state=PlayerState::Human;
    player_.grounded=false; player_.onOwnInk=false; player_.onEnemyInk=false; wallSurface_=-1;
    airMoveSpeed_=movement.humanSpeed;
    drySquidCarryRemaining_=0;
}

float Simulation::Random() {
    randomState_^=randomState_<<13; randomState_^=randomState_>>17; randomState_^=randomState_<<5;
    return static_cast<float>(randomState_&0x00ffffffu)/16777216.0f;
}

float Simulation::PaintRandom() {
    paintRandomState_^=paintRandomState_<<13; paintRandomState_^=paintRandomState_>>17; paintRandomState_^=paintRandomState_<<5;
    return static_cast<float>(paintRandomState_&0x00ffffffu)/16777216.0f;
}

void Simulation::SetDummyPosition(Vec3 position) {
    dummy_=DummyStatus{}; dummy_.position=position;
}

float Simulation::CalculateDamage(float age,const ShooterWeaponParams& params) {
    const float fraction=Clamp((age-params.damageFalloffStart)/
        std::max(0.0001f,params.damageFalloffEnd-params.damageFalloffStart),0,1);
    return params.baseDamage+(params.minimumDamage-params.baseDamage)*fraction;
}

void Simulation::EmitImpact(Vec3 position,Vec3 normal,Vec3 velocity,float radius,PaintKind kind) {
    // Rendering may be suspended while the simulation continues. Never retain an
    // unbounded particle history; events do not participate in gameplay queries.
    if(impactEvents_.size()>=256) impactEvents_.erase(impactEvents_.begin());
    impactEvents_.push_back({++impactSequence_,position,normal,velocity,radius,kind});
}

std::vector<ImpactEvent> Simulation::TakeImpactEvents() {
    std::vector<ImpactEvent> result; result.swap(impactEvents_); return result;
}

RayHit Simulation::Raycast(Vec3 origin,Vec3 direction,float maxDistance,float radius) const {
    RayHit result; result.distance=std::max(0.0f,maxDistance);
    direction=Normalize(direction); radius=std::max(0.0f,radius);
    if (Length(direction)<0.5f || maxDistance<0) return result;
    for (uint32_t i=0;i<surfaces_.size();++i) {
        const Surface& surface=surfaces_[i];
        Vec3 normal=surface.normal;
        float distance=Dot(origin-surface.origin,normal);
        if (distance<0) { normal=normal*-1; distance=-distance; }
        const float approach=Dot(direction,normal);
        if (approach>=-0.000001f) continue;
        const float t=std::max(0.0f,(radius-distance)/approach);
        if (t>result.distance) continue;
        const Vec3 center=origin+direction*t;
        const Vec3 onPlane=center-normal*Dot(center-surface.origin,normal);
        const float localU=Dot(onPlane-surface.origin,surface.u);
        const float localV=Dot(onPlane-surface.origin,surface.v);
        const Vec3 closest=surface.origin+surface.u*Clamp(localU,0,surface.width)+surface.v*Clamp(localV,0,surface.height);
        if (Length(onPlane-closest)>radius+0.0001f) continue;
        result={true,i,t,closest,normal};
    }
    return result;
}

void Simulation::Paint(const PaintStamp& input) {
    if (input.surface>=surfaces_.size() || !surfaces_[input.surface].inkable ||
        !std::isfinite(input.u) || !std::isfinite(input.v) || !std::isfinite(input.angle) ||
        !std::isfinite(input.radiusU) || !std::isfinite(input.radiusV) || input.radiusU<=0 || input.radiusV<=0) return;
    if(input.team>2) return; // renderer currently supports erase + two team colors
    PaintStamp stamp=input;
    const Surface& surface=surfaces_[stamp.surface];
    const float c=std::cos(stamp.angle),s=std::sin(stamp.angle);
    const float extentU=std::sqrt(stamp.radiusU*stamp.radiusU*c*c+stamp.radiusV*stamp.radiusV*s*s);
    const float extentV=std::sqrt(stamp.radiusU*stamp.radiusU*s*s+stamp.radiusV*stamp.radiusV*c*c);
    if (stamp.u+extentU<0 || stamp.v+extentV<0 || stamp.u-extentU>surface.width || stamp.v-extentV>surface.height) return;
    const int x0=static_cast<int>(Clamp(std::floor((stamp.u-extentU)/surface.width*MaskResolution),0,MaskResolution-1));
    const int x1=static_cast<int>(Clamp(std::floor((stamp.u+extentU)/surface.width*MaskResolution),0,MaskResolution-1));
    const int y0=static_cast<int>(Clamp(std::floor((stamp.v-extentV)/surface.height*MaskResolution),0,MaskResolution-1));
    const int y1=static_cast<int>(Clamp(std::floor((stamp.v+extentV)/surface.height*MaskResolution),0,MaskResolution-1));
    auto& mask=masks_[stamp.surface];
    for (int y=y0;y<=y1;++y) for(int x=x0;x<=x1;++x) {
        const float du=(x+0.5f)*surface.width/MaskResolution-stamp.u;
        const float dv=(y+0.5f)*surface.height/MaskResolution-stamp.v;
        const float a=(c*du+s*dv)/stamp.radiusU;
        const float b=(-s*du+c*dv)/stamp.radiusV;
        if (a*a+b*b<=1) mask[y*MaskResolution+x]=static_cast<uint8_t>(stamp.team);
    }
    // Renderer applies this very same ellipse equation in local WORLD units.
    pendingStamps_.push_back(stamp); ++stampCount_;
}

std::vector<PaintStamp> Simulation::TakePendingStamps() {
    std::vector<PaintStamp> result; result.swap(pendingStamps_); return result;
}

uint32_t Simulation::InkOnSurface(uint32_t index,Vec3 position) const {
    if (index>=surfaces_.size()) return 0;
    const Surface& surface=surfaces_[index];
    if (!surface.inkable || !Inside(surface,position)) return 0;
    const Vec3 delta=position-surface.origin;
    const int x=static_cast<int>(Clamp(Dot(delta,surface.u)/surface.width*MaskResolution,0,MaskResolution-1));
    const int y=static_cast<int>(Clamp(Dot(delta,surface.v)/surface.height*MaskResolution,0,MaskResolution-1));
    return masks_[index][y*MaskResolution+x];
}

uint32_t Simulation::GetInkAtWorldPosition(Vec3 position) const {
    float nearest=0.20f; uint32_t team=0;
    for (uint32_t i=0;i<surfaces_.size();++i) {
        const Surface& surface=surfaces_[i];
        const float distance=std::abs(Dot(position-surface.origin,surface.normal));
        if (distance<=nearest && Inside(surface,position)) {
            nearest=distance; team=InkOnSurface(i,position);
        }
    }
    return team;
}

float Simulation::GroundHeight(Vec3 point,float maxHeight,int* surfaceIndex) const {
    float highest=-1000;
    if (surfaceIndex) *surfaceIndex=-1;
    for (uint32_t i=0;i<surfaces_.size();++i) {
        const Surface& surface=surfaces_[i];
        if (surface.normal.y<0.5f) continue;
        const float height=surface.origin.y-(surface.normal.x*(point.x-surface.origin.x)+surface.normal.z*(point.z-surface.origin.z))/surface.normal.y;
        const Vec3 projected={point.x,height,point.z};
        if (height<=maxHeight+0.0001f && height>highest && Inside(surface,projected,0.001f)) {
            highest=height; if(surfaceIndex) *surfaceIndex=static_cast<int>(i);
        }
    }
    return highest;
}

Vec3 Simulation::Muzzle(const Controls& controls) const {
    const Vec3 forward={std::sin(controls.yaw),0,std::cos(controls.yaw)};
    const Vec3 right={std::cos(controls.yaw),0,-std::sin(controls.yaw)};
    const Vec3 shoulder=player_.position+Vec3{0,1.10f,0};
    const Vec3 intended=shoulder+forward*0.43f+right*0.28f;
    const Vec3 path=intended-shoulder;
    const RayHit obstruction=Raycast(shoulder,path,Length(path),0.04f);
    return obstruction.hit?shoulder+Normalize(path)*std::max(0.0f,obstruction.distance-0.02f):intended;
}

void Simulation::ResolveWalls(Vec3 previous,float radius) {
    const float bodyHeight=player_.state==PlayerState::Human?1.4f:0.48f;
    for (const Surface& surface:surfaces_) {
        if (std::abs(surface.normal.y)>0.25f) continue;
        const float oldDistance=Dot(previous-surface.origin,surface.normal);
        const float sign=oldDistance>=0?1.0f:-1.0f;
        const Vec3 normal=surface.normal*sign;
        const float distance=Dot(player_.position-surface.origin,normal);
        if (distance>=radius) continue;
        if (std::abs(oldDistance)>radius+Length(player_.position-previous)+0.01f) continue;
        // Check the full vertical capsule interval against finite wall height.
        const Vec3 projected=player_.position-normal*distance;
        const float u=Dot(projected-surface.origin,surface.u);
        const float v=Dot(projected-surface.origin,surface.v);
        if (u < -radius || u > surface.width+radius || v+bodyHeight<0.01f || v>=surface.height-0.01f) continue;
        player_.position+=normal*(radius-distance);
        const float toward=Dot(player_.velocity,normal);
        if (toward<0) player_.velocity=player_.velocity-normal*toward;
    }
    player_.position.x=Clamp(player_.position.x,-15+radius,15-radius);
    player_.position.z=Clamp(player_.position.z,-14+radius,20-radius);
}

bool Simulation::TryWallSwim(float dt,const Controls& controls,bool jumpPressed,Vec3 direction) {
    if (!controls.swim || controls.fire || wallReattachLock_>0 || timeSinceShot_<PostShotDelay()) { wallSurface_=-1; return false; }
    int candidate=-1; float closest=0.62f;
    Vec3 wallNormal;
    for(uint32_t i=0;i<surfaces_.size();++i) {
        const Surface& surface=surfaces_[i];
        if (!surface.inkable || std::abs(surface.normal.y)>0.1f) continue;
        float distance=Dot(player_.position-surface.origin,surface.normal);
        const Vec3 normal=surface.normal*(distance>=0?1.0f:-1.0f);
        distance=std::abs(distance);
        const Vec3 sample=player_.position+Vec3{0,0.40f,0}-normal*distance;
        if (distance>closest || !Inside(surface,sample) || InkOnSurface(i,sample)!=1) continue;
        if (player_.state!=PlayerState::WallSwim && Dot(direction,normal)>-0.1f) continue;
        candidate=static_cast<int>(i); closest=distance; wallNormal=normal;
    }
    if(candidate<0) { wallSurface_=-1; return false; }
    const Surface& wall=surfaces_[candidate];
    player_.wallNormal=wallNormal;
    if(jumpPressed) {
        player_.state=PlayerState::Squid; player_.velocity=wallNormal*4.2f+Vec3{0,movement.jumpSpeed,0};
        player_.position+=wallNormal*0.15f; player_.grounded=false;
        player_.onOwnInk=false; player_.onEnemyInk=false; airMoveSpeed_=4.2f;
        wallReattachLock_=0.30f; timeSinceJump_=0; wallSurface_=-1;
        return true;
    }
    Vec3 right=Normalize(Cross(wallNormal,{0,1,0}));
    const Vec3 cameraRight={std::cos(controls.yaw),0,-std::sin(controls.yaw)};
    if(Dot(right,cameraRight)<0) right=right*-1;
    Vec3 climb=right*controls.moveX+Vec3{0,controls.moveZ,0};
    if(Length(climb)>1) climb=Normalize(climb);
    const Vec3 next=player_.position+climb*(movement.wallSwimSpeed*dt);
    const float nextHeight=Dot(next-wall.origin,wall.v);
    if(controls.moveZ>0 && nextHeight+0.45f>=wall.height) {
        // Lift and move the body over the edge only if an actual horizontal
        // supporting surface is present on the far side of this finite wall.
        Vec3 over=next-wallNormal*0.70f;
        const float top=wall.origin.y+wall.v.y*wall.height;
        const float support=GroundHeight(over,top+0.25f);
        if(std::abs(support-top)<0.15f) {
            player_.position={over.x,support,over.z}; player_.velocity={};
            player_.grounded=true; player_.onOwnInk=GetInkAtWorldPosition(player_.position)==1;
            player_.onEnemyInk=GetInkAtWorldPosition(player_.position)==2;
            player_.state=player_.onOwnInk?PlayerState::Swim:PlayerState::Squid;
            wallSurface_=-1; wallReattachLock_=0.15f;
            return true;
        }
    }
    const float distance=Dot(next-wall.origin,wallNormal);
    const Vec3 projected=next-wallNormal*distance;
    const Vec3 sample=projected+Vec3{0,0.40f,0};
    if (!Inside(wall,sample) || InkOnSurface(candidate,sample)!=1) {
        player_.state=PlayerState::Human; wallSurface_=-1; return false;
    }
    player_.position=projected+wallNormal*(movement.swimRadius+0.02f);
    player_.velocity=climb*movement.wallSwimSpeed; player_.grounded=false;
    player_.state=PlayerState::WallSwim; player_.onOwnInk=true; player_.onEnemyInk=false; wallSurface_=candidate;
    const float ground=GroundHeight(player_.position,player_.position.y+0.1f);
    if (player_.position.y<ground) player_.position.y=ground;
    return true;
}

void Simulation::MovePlayer(float dt,const Controls& controls,bool jumpPressed) {
    const bool wasSwimming=IsSubmerged(player_.state);
    const bool wasFloorSwimming=player_.state==PlayerState::Swim && player_.grounded;
    const bool wasGrounded=player_.grounded;
    const bool hadDryCarry=drySquidCarryRemaining_>0;
    const Vec3 forward={std::sin(controls.yaw),0,std::cos(controls.yaw)};
    const Vec3 right={std::cos(controls.yaw),0,-std::sin(controls.yaw)};
    Vec3 direction=forward*controls.moveZ+right*controls.moveX;
    if(Length(direction)>1) direction=Normalize(direction);
    if(TryWallSwim(dt,controls,jumpPressed,direction)) {
        drySquidCarryRemaining_=0;
        player_.speed=Length(player_.velocity); return;
    }
    const uint32_t groundInk=player_.grounded?GetInkAtWorldPosition(player_.position):0;
    player_.onOwnInk=groundInk==1; player_.onEnemyInk=groundInk==2;
    const bool squid=controls.swim && !controls.fire && timeSinceShot_>=PostShotDelay();
    const bool swim=squid && player_.onOwnInk;
    const bool floorSwimAtStart=swim && wasGrounded;
    player_.state=squid?(swim?PlayerState::Swim:PlayerState::Squid):PlayerState::Human;
    const bool shotMovement=controls.fire || charging_ || timeSinceShot_<PostShotDelay();
    float speed=squid?(swim?movement.swimSpeed:movement.drySquidSpeed):movement.humanSpeed;
    if(shotMovement) speed=FiringMoveSpeed();
    // Opponent ink values are exposed human parameters; applying the same cap
    // to exposed squid is CG2 tuning. Tank recovery is not disabled here.
    if(player_.onEnemyInk) speed=std::min(speed,shotMovement?movement.enemyInkShotSpeed:movement.enemyInkSpeed);
    const auto horizontalSpeed=[&]() {
        return std::sqrt(player_.velocity.x*player_.velocity.x+player_.velocity.z*player_.velocity.z);
    };
    const auto beginDryCarry=[&]() {
        if(horizontalSpeed()>movement.drySquidSpeed+0.001f &&
            std::isfinite(movement.drySquidCarryMaxTime) && movement.drySquidCarryMaxTime>0)
            drySquidCarryRemaining_=movement.drySquidCarryMaxTime;
    };
    const bool dryCarryAllowed=squid && player_.grounded && !player_.onOwnInk && !player_.onEnemyInk;
    if(!dryCarryAllowed) drySquidCarryRemaining_=0;
    else if(wasFloorSwimming) beginDryCarry(); // also handles paint erased under a swimmer
    if(player_.grounded) airMoveSpeed_=speed;
    else if(squid) speed=airMoveSpeed_; // retain the takeoff speed while in squid form
    if(player_.grounded && ((wasSwimming && !swim && drySquidCarryRemaining_<=0) || (!squid && hadDryCarry))) {
        const float horizontal=horizontalSpeed();
        if(horizontal>speed) { player_.velocity.x*=speed/horizontal; player_.velocity.z*=speed/horizontal; }
    }
    const float acceleration=movement.acceleration*(player_.grounded?1.0f:0.45f);
    if(drySquidCarryRemaining_>0 && dryCarryAllowed) {
        // Move the actual velocity toward dry-ground input, never reconstruct a
        // stored boost. Thus steering and a wall slide cannot create speed.
        const Vec3 horizontal={player_.velocity.x,0,player_.velocity.z};
        const Vec3 difference=direction*speed-horizontal;
        const float differenceLength=Length(difference);
        float deceleration=std::max(0.0f,movement.drySquidCarryDeceleration);
        if(Length(direction)<0.001f || Dot(direction,horizontal)<0)
            deceleration*=std::max(1.0f,movement.drySquidCarryBrakeMultiplier);
        // The deadline raises braking gently from the first tick when edited
        // deceleration is too weak; there is no final-frame speed snap.
        deceleration=std::max(deceleration,differenceLength/std::max(dt,drySquidCarryRemaining_));
        const Vec3 next=horizontal+(differenceLength>0.000001f?
            difference*(std::min(differenceLength,deceleration*dt)/differenceLength):Vec3{});
        player_.velocity.x=next.x; player_.velocity.z=next.z;
        drySquidCarryRemaining_=std::max(0.0f,drySquidCarryRemaining_-dt);
        if(horizontalSpeed()<=movement.drySquidSpeed+0.001f) drySquidCarryRemaining_=0;
        // Jumping from the dry carry preserves its current momentum through
        // the existing air-control path, without renewing the ground timer.
        airMoveSpeed_=horizontalSpeed();
    } else {
        player_.velocity.x=Approach(player_.velocity.x,direction.x*speed,acceleration*dt);
        player_.velocity.z=Approach(player_.velocity.z,direction.z*speed,acceleration*dt);
    }
    if(jumpPressed && player_.grounded) {
        player_.velocity.y=player_.onEnemyInk?movement.enemyInkJumpSpeed:movement.jumpSpeed;
        player_.grounded=false; timeSinceJump_=0;
        // Retain the swimmer's horizontal momentum through takeoff.
    }
    const Vec3 previous=player_.position;
    if(!player_.grounded) player_.velocity.y-=movement.gravity*dt;
    player_.position+=player_.velocity*dt;
    const float radius=squid?movement.swimRadius:movement.humanRadius;
    ResolveWalls(previous,radius);
    const float allowedHeight=previous.y+(player_.grounded?movement.stepHeight:0.001f);
    // The ramp is rendered as a finite top plane. Treat its elevated side edges
    // as solid so a low capsule cannot enter the cutout beneath that top plane.
    // Tangential motion is kept, letting players slide along an edge naturally.
    for(const Surface& surface:surfaces_) {
        if(surface.normal.y<0.5f) continue;
        const auto projected=[&](Vec3 point) {
            point.y=surface.origin.y-(surface.normal.x*(point.x-surface.origin.x)+surface.normal.z*(point.z-surface.origin.z))/surface.normal.y;
            return point;
        };
        const Vec3 currentTop=projected(player_.position),previousTop=projected(previous);
        if(currentTop.y<=allowedHeight || !Inside(surface,currentTop,radius) || previous.y>=previousTop.y-movement.stepHeight) continue;
        const Vec3 currentLocal=currentTop-surface.origin,previousLocal=previousTop-surface.origin;
        const float u=Dot(currentLocal,surface.u),oldU=Dot(previousLocal,surface.u);
        const float v=Dot(currentLocal,surface.v),oldV=Dot(previousLocal,surface.v);
        auto block=[&](Vec3 axis,float delta) {
            axis.y=0; const float length=Length(axis); if(length<0.01f) return;
            const Vec3 normal=axis/length;
            player_.position+=normal*delta;
            const float speedInto=Dot(player_.velocity,normal);
            if(speedInto*delta<0) player_.velocity=player_.velocity-normal*speedInto;
        };
        if(oldU<0 && u<radius) block(surface.u,-radius-u);
        else if(oldU>surface.width && u>surface.width-radius) block(surface.u,surface.width+radius-u);
        else if(oldV<0 && v<radius) block(surface.v,(-radius-v)*Length(Vec3{surface.v.x,0,surface.v.z}));
        else if(oldV>surface.height && v>surface.height-radius) block(surface.v,(surface.height+radius-v)*Length(Vec3{surface.v.x,0,surface.v.z}));
    }
    int supportIndex=-1;
    const float ground=GroundHeight(player_.position,allowedHeight,&supportIndex);
    const float descendingStep=player_.grounded?movement.stepHeight:0.0f;
    if(player_.velocity.y<=0 && ground>-999 && player_.position.y<=ground+descendingStep) {
        player_.position.y=ground; player_.velocity.y=0; player_.grounded=true;
    } else {
        player_.grounded=false;
    }
    if(player_.position.y < -8) SetPlayerPosition({0,0,-8});
    const uint32_t supportInk=player_.grounded && supportIndex>=0?InkOnSurface(static_cast<uint32_t>(supportIndex),player_.position):0;
    player_.onOwnInk=supportInk==1; player_.onEnemyInk=supportInk==2;
    if(squid) player_.state=player_.onOwnInk?PlayerState::Swim:PlayerState::Squid;
    if(!squid || !player_.grounded || player_.onOwnInk || player_.onEnemyInk)
        drySquidCarryRemaining_=0;
    else if(floorSwimAtStart) beginDryCarry(); // keep velocity on the actual boundary-crossing tick
    // Apply surface restrictions on the actual crossing/landing tick. Becoming
    // exposed never paints the floor, and jumping never forces the human form.
    if(player_.grounded && (player_.onEnemyInk || (squid && !player_.onOwnInk && drySquidCarryRemaining_<=0))) {
        float cap=squid?movement.drySquidSpeed:movement.humanSpeed;
        if(player_.onEnemyInk) cap=std::min(cap,shotMovement?movement.enemyInkShotSpeed:movement.enemyInkSpeed);
        const float horizontal=std::sqrt(player_.velocity.x*player_.velocity.x+player_.velocity.z*player_.velocity.z);
        if(horizontal>cap) { player_.velocity.x*=cap/horizontal; player_.velocity.z*=cap/horizontal; }
    }
    player_.speed=std::sqrt(player_.velocity.x*player_.velocity.x+player_.velocity.z*player_.velocity.z);
}

void Simulation::Fire(const Controls& controls) {
    const Vec3 muzzle=Muzzle(controls);
    Vec3 direction=Normalize(controls.aimPoint-muzzle);
    if(Length(direction)<0.1f) direction={std::sin(controls.yaw),0,std::cos(controls.yaw)};
    const float jumpFactor=1-Clamp((timeSinceJump_-weapon.jumpAccuracyRecoveryStart)/
        std::max(0.01f,weapon.jumpAccuracyRecoveryEnd-weapon.jumpAccuracyRecoveryStart),0,1);
    const float spread=weapon.groundSpread+(weapon.jumpSpread-weapon.groundSpread)*jumpFactor;
    const float bias=Clamp(accuracyBias_+(weapon.jumpAccuracyBiasMaximum-accuracyBias_)*jumpFactor,0,1);
    const float magnitude=Random(),sign=Random();
    direction=PerturbHorizontal(direction,SampleSignedSpreadDegrees(magnitude,sign,spread,bias));
    Projectile projectile;
    projectile.tuning=weapon;
    projectile.position=muzzle; projectile.velocity=direction*weapon.projectileSpeed;
    EmissionSettings emission;
    emission.sourceSpawnNum=weapon.sourcePaintDropletSpawnCount;
    emission.sourceSplitNum=weapon.sourcePaintDropletSplitCount;
    emission.sourceBetweenDistance=weapon.paintDropletSpacing;
    emission.sourceNearestDistance=weapon.firstPaintDropletDistance;
    emission.maxTravelDistance=60; // same cleanup bound; actual stage hits truncate emission
    emission.maxDryShots=weapon.footRescueEveryShots;
    projectile.emission=emissionPattern_.NextShot(emission,player_.grounded && !player_.onOwnInk);
    projectile.emission.count=std::min(projectile.emission.count,std::max(0,weapon.paintDropletCount));
    projectile.nextDroplet=projectile.emission.count?projectile.emission.drops[0].distance:1000;
    projectiles_.push_back(projectile);
    // A separate small rescue drop connects dry feet to the first flight drop.
    // The condition is CG2 tuning: the native ForceSpawnNearest array algorithm
    // is unavailable, so its stored [4] source value is not used as this rule.
    if(projectile.emission.footRescue) {
        Projectile foot;
        foot.tuning=weapon;
        const auto& plan=projectile.emission;
        const Vec3 forward={std::sin(controls.yaw),0,std::cos(controls.yaw)};
        const Vec3 lateral={std::cos(controls.yaw),0,-std::sin(controls.yaw)};
        foot.position=player_.position+forward*plan.footForward+lateral*plan.footLateral+Vec3{0,0.20f,0};
        foot.velocity={0,-1.3f,0}; foot.kind=PaintKind::Foot;
        foot.paintRadius=weapon.nearestPaintDropletRadius*plan.footWidthScale;
        const Vec3 origin=player_.position+Vec3{0,0.20f,0};
        const Vec3 offset=foot.position-origin;
        // A drop placed beyond a nearby wall would bypass the swept main shot.
        // Skip an obstructed rescue; never project free paint through a panel.
        if(!Raycast(origin,offset,Length(offset),0.035f).hit) droplets_.push_back(foot);
    }
    player_.ink=std::max(0.0f,player_.ink-weapon.inkConsume);
    recoveryLock_=weapon.inkRecoverStop; timeSinceShot_=0;
    accuracyBias_=std::min(weapon.accuracyBiasMaximum,accuracyBias_+weapon.accuracyBiasPerShot);
    ++shotsFired_;
    EmitAudio(AudioCue::ShooterShot,muzzle);
}

PaintStamp Simulation::ImpactBrush(const Surface& plane,uint32_t surface,Vec3 point,
    Vec3 incomingVelocity,float radius,PaintKind kind,const ShooterWeaponParams& params) {
    const Vec3 direction=Normalize(incomingVelocity);
    const float incidence=std::asin(Clamp(std::abs(Dot(direction,plane.normal)),0,1))*180.0f/Pi;
    const Vec3 tangent=direction-plane.normal*Dot(direction,plane.normal);
    const float rotation=Length(tangent)>0.0001f?std::atan2(Dot(tangent,plane.v),Dot(tangent,plane.u)):0;
    const float grazing=1-Clamp((incidence-params.sourceDepthAngleMax)/
        std::max(0.01f,params.sourceDepthAngleMin-params.sourceDepthAngleMax),0,1);
    float depth=params.sourceImpactDepthScaleMin+
        (params.sourceImpactDepthScaleMax-params.sourceImpactDepthScaleMin)*grazing;
    // The source thresholds cover 10..35 degrees. Above this, CG2 blends to a
    // round normal impact rather than choosing an arbitrary in-plane heading.
    const float normalBlend=Clamp((incidence-params.sourceDepthAngleMin)/
        std::max(0.01f,90-params.sourceDepthAngleMin),0,1);
    depth+=(params.impactNormalDepthScale-depth)*normalBlend;
    float scale=params.impactCoreScale;
    if(kind==PaintKind::Droplet) { scale=params.dropletCoreScale; depth=1+(depth-1)*0.35f; }
    else if(kind==PaintKind::Foot) { scale=params.footCoreScale; depth=1+(depth-1)*0.15f; }
    else if(kind==PaintKind::Scatter) { scale=params.scatterRadiusScale; depth=1; }
    const Vec3 delta=point-plane.origin;
    const float width=std::max(0.005f,radius*std::max(0.01f,scale));
    return {surface,Dot(delta,plane.u),Dot(delta,plane.v),width*std::max(1.0f,depth),width,rotation,1,kind};
}

void Simulation::Splash(uint32_t surface,Vec3 point,Vec3 incomingVelocity,float radius,PaintKind kind,int scatterCount,const ShooterWeaponParams* tuning) {
    if(surface>=surfaces_.size() || !surfaces_[surface].inkable) return;
    const Surface& plane=surfaces_[surface];
    const auto& brush=tuning?*tuning:weapon;
    const PaintStamp core=ImpactBrush(plane,surface,point,incomingVelocity,radius,kind,brush);
    Paint(core);
    const float c=std::cos(core.angle),s=std::sin(core.angle);
    auto lobe=[&](float along,float across,float ru,float rv,PaintKind lobeKind) {
        Paint({surface,core.u+c*along-s*across,core.v+s*along+c*across,ru,rv,core.angle,1,lobeKind});
    };
    const bool main=kind==PaintKind::Main;
    // Connected, unequal narrow extensions remove the repeated circular scallop
    // silhouette. Detached satellites are much smaller than either paint source.
    const int lobes=main?2+static_cast<int>(PaintRandom()*3):1+static_cast<int>(PaintRandom()*3);
    for(int i=0;i<lobes;++i) {
        const float side=(i%2==0?1.0f:-1.0f);
        const float along=(PaintRandom()-0.45f)*core.radiusU*1.25f;
        const float across=side*core.radiusV*(0.55f+PaintRandom()*0.20f);
        lobe(along,across,core.radiusU*(0.19f+PaintRandom()*0.15f),
            core.radiusV*(0.17f+PaintRandom()*0.17f),kind);
    }
    if(main) {
        if(core.radiusU>core.radiusV*1.2f)
            lobe(core.radiusU*0.93f,core.radiusV*(PaintRandom()-0.5f)*0.22f,
                core.radiusU*0.34f,core.radiusV*0.30f,kind);
        for(int i=0;i<std::clamp(scatterCount,1,5);++i) {
            const float size=radius*brush.scatterRadiusScale*(0.55f+PaintRandom()*0.65f);
            const float side=i%2==0?1.0f:-1.0f;
            lobe(core.radiusU*(PaintRandom()*1.9f-0.7f),
                side*(core.radiusV*(1.20f+PaintRandom()*0.42f)+size),size,size*(0.62f+PaintRandom()*0.35f),PaintKind::Scatter);
        }
    }
    EmitImpact(point,plane.normal,incomingVelocity,core.radiusV,kind);
}

void Simulation::UpdateProjectiles(float dt) {
    for(size_t i=0;i<projectiles_.size();) {
        Projectile& shot=projectiles_[i];
        const auto& shotParams=shot.tuning;
        const float oldAge=shot.age; shot.age+=dt;
        if(shot.projectileKind==ProjectileKind::StringerArrow && shot.age>shotParams.straightFlightTime) {
            const bool braking=shot.age<=shotParams.straightFlightTime+shot.brakeDuration;
            const float drag=std::pow(Clamp(1-(braking?shotParams.brakeAirResistance:shot.freeDrag),0.01f,1),dt*60);
            shot.velocity=shot.velocity*drag;
            shot.velocity.y-=(braking?shotParams.brakeGravity:shot.freeGravity)*dt;
        } else if(shot.projectileKind==ProjectileKind::Shooter && shot.age>shotParams.straightFlightTime) {
            if(oldAge<=shotParams.straightFlightTime) {
                const float speed=Length(shot.velocity);
                if(speed>shotParams.brakeInitialSpeed) shot.velocity=shot.velocity*(shotParams.brakeInitialSpeed/speed);
            }
            const float horizontal=std::sqrt(shot.velocity.x*shot.velocity.x+shot.velocity.z*shot.velocity.z);
            const bool braking=horizontal>shotParams.brakeToFreeSpeedXZ || shot.velocity.y>shotParams.brakeToFreeSpeedY;
            const float drag=std::pow(Clamp(1-(braking?shotParams.brakeAirResistance:shotParams.freeAirResistance),0.01f,1),dt*60);
            shot.velocity.x*=drag; shot.velocity.z*=drag;
            shot.velocity.y*=drag;
            shot.velocity.y-=(braking?shotParams.brakeGravity:shotParams.projectileGravity)*dt;
        }
        const Vec3 displacement=shot.velocity*dt;
        const float distance=Length(displacement);
        const RayHit hit=Raycast(shot.position,displacement,distance,shotParams.stageHitRadius);
        float traveled=hit.hit?hit.distance:distance;
        bool hitDummy=false;
        // Sweep the projectile's target radius, and compare against the nearest
        // stage hit so targets behind a wall cannot absorb or receive shots.
        if(dummy_.hp>0 && distance>0.000001f) {
            const Vec3 ray=displacement/distance,relative=shot.position-dummy_.position;
            const float radius=dummy_.radius+shotParams.playerHitRadius;
            const float b=Dot(relative,ray),c=Dot(relative,relative)-radius*radius;
            const float discriminant=b*b-c;
            if(discriminant>=0) {
                const float t=c<=0?0.0f:-b-std::sqrt(discriminant);
                if(t>=0 && t<traveled) { traveled=t; hitDummy=true; }
            }
        }
        // Spawn at distance crossings along the swept segment, making the paint
        // trail independent of frame rate and avoiding emission beyond a wall.
        while(shot.droplets<shot.emission.count && shot.nextDroplet<=shot.distance+traveled) {
            const float along=Clamp(shot.nextDroplet-shot.distance,0,traveled);
            const auto& planned=shot.emission.drops[shot.droplets];
            Projectile drop;
            drop.tuning=shotParams;
            drop.position=shot.position+Normalize(displacement)*along;
            drop.position+=Normalize(Cross({0,1,0},displacement))*planned.lateralOffset;
            drop.velocity={0,-1.3f,0}; drop.droplets=shot.droplets;
            drop.kind=PaintKind::Droplet;
            drop.paintRadius=shotParams.paintDropletRadius*planned.widthScale;
            droplets_.push_back(drop);
            ++shot.droplets;
            shot.nextDroplet=shot.droplets<shot.emission.count?shot.emission.drops[shot.droplets].distance:1000;
        }
        shot.distance+=traveled;
        shot.position+=Normalize(displacement)*traveled;
        if(hitDummy) {
            const float impactAge=oldAge+dt*Clamp(traveled/std::max(0.000001f,distance),0,1);
            DamageDummy(shot.projectileKind==ProjectileKind::StringerArrow?shot.directDamage:CalculateDamage(impactAge,shotParams),shot.distance);
            const Vec3 normal=Normalize(shot.position-dummy_.position);
            EmitImpact(dummy_.position+normal*dummy_.radius,normal,shot.velocity,0.18f,PaintKind::Main);
        } else if(hit.hit) {
            const float far=Clamp(shot.distance/std::max(1.0f,shotParams.effectiveRange),0,1);
            Splash(hit.surface,hit.position,shot.velocity,
                shotParams.impactPaintRadius+(shotParams.distantImpactPaintRadius-shotParams.impactPaintRadius)*far,PaintKind::Main,shot.emission.scatterCount,&shotParams);
            if(shot.explosive && embeddedArrows_.size()<128) {
                EmitAudio(AudioCue::ArrowStick,hit.position);
                embeddedArrows_.push_back({hit.position,hit.normal,Normalize(shot.velocity),hit.surface,
                    shot.explosionDelay,shot.explosionDelay,shot.explosionDamage,shot.explosionRadius,shot.explosionPaintRadius,shot.explosionOffset,shotParams});
            }
        }
        if(hitDummy || hit.hit || shot.age>shotParams.projectileLifetime || shot.position.y < -10 || shot.distance>60) {
            projectiles_[i]=projectiles_.back(); projectiles_.pop_back();
        } else ++i;
    }
    for(size_t i=0;i<droplets_.size();) {
        Projectile& drop=droplets_[i]; drop.age+=dt; drop.velocity.y-=32.0f*dt;
        const Vec3 displacement=drop.velocity*dt;
        const RayHit hit=Raycast(drop.position,displacement,Length(displacement),0.035f);
        if(hit.hit) Splash(hit.surface,hit.position,drop.velocity,drop.paintRadius,drop.kind,0,&drop.tuning);
        drop.position+=displacement;
        if(hit.hit || drop.age>2.5f || drop.position.y < -10) {
            droplets_[i]=droplets_.back(); droplets_.pop_back();
        } else ++i;
    }
}

void Simulation::Step(float dt,const Controls& input) {
    if(!std::isfinite(dt) || dt<=0) return;
    dt=std::min(dt,0.1f); // scene supplies fixed 1/120 s; guard debugger pauses
    if(dummy_.hp<=0) {
        dummy_.resetRemaining=std::max(0.0f,dummy_.resetRemaining-dt);
        if(dummy_.resetRemaining<=0) { dummy_.hp=100; dummy_.hits=0; }
    }
    recoveryLock_=std::max(0.0f,recoveryLock_-dt);
    wallReattachLock_=std::max(0.0f,wallReattachLock_-dt);
    timeSinceShot_+=dt; timeSinceJump_+=dt;
    if(!input.fire) requireTriggerRelease_=false;
    if(weaponClass_==WeaponClass::Stringer && input.swim) CancelCharge();
    Controls controls=input;
    if(requireTriggerRelease_ || (weaponClass_==WeaponClass::Stringer && controls.swim)) controls.fire=false;
    const bool jumpPressed=controls.jump&&!wasJump_;
    MovePlayer(dt,controls,jumpPressed);
    if(weaponClass_==WeaponClass::Shooter && controls.fire && !wasFire_) fireCooldown_=std::max(fireCooldown_,wasSwim_?weapon.swimInitialShotDelay:weapon.initialShotDelay);
    fireCooldown_-=dt;
    if(weaponClass_==WeaponClass::Stringer) {
        StepStringer(dt,controls);
    } else if(controls.fire && player_.state==PlayerState::Human) {
        // Bound catch-up emissions when an interval is edited live to zero.
        int emitted=0;
        while(fireCooldown_<=0.000001f && player_.ink+0.000001f>=weapon.inkConsume && emitted<12) {
            Fire(controls); fireCooldown_+=std::max(1.0f/120.0f,weapon.FireInterval()); ++emitted;
        }
        if(player_.ink<weapon.inkConsume) fireCooldown_=std::max(0.0f,fireCooldown_);
    } else {
        fireCooldown_=std::max(0.0f,fireCooldown_);
    }
    if(!controls.fire && !charging_ && recoveryLock_<=0) {
        const bool swimming=IsSubmerged(player_.state);
        player_.ink=std::min(1.0f,player_.ink+(swimming?movement.swimInkRecovery:movement.humanInkRecovery)*dt);
    }
    if(timeSinceShot_>=weapon.accuracyRecoveryDelay) accuracyBias_=std::max(weapon.accuracyBiasMinimum,accuracyBias_-weapon.accuracyRecovery*dt);
    const float jumpFactor=1-Clamp((timeSinceJump_-weapon.jumpAccuracyRecoveryStart)/
        std::max(0.01f,weapon.jumpAccuracyRecoveryEnd-weapon.jumpAccuracyRecoveryStart),0,1);
    player_.accuracy=weaponClass_==WeaponClass::Stringer?ChargeProfile().spreadDegrees:
        weapon.groundSpread+(weapon.jumpSpread-weapon.groundSpread)*jumpFactor;
    const float formTarget=player_.state==PlayerState::Human?0.0f:1.0f;
    player_.formBlend=Approach(player_.formBlend,formTarget,dt/std::max(0.01f,movement.formTransitionTime));
    UpdateEmbeddedArrows(dt);
    UpdateProjectiles(dt);
    wasFire_=controls.fire; wasJump_=controls.jump; wasSwim_=player_.state!=PlayerState::Human;
}
} // namespace ink
