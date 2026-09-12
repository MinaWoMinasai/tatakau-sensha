#include "InkSimulation.h"
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
    pendingStamps_.clear(); projectiles_.clear(); droplets_.clear();
    player_=PlayerStatus{};
    stampCount_=0; shotsFired_=0; randomState_=0x7541a349u;
    fireCooldown_=0; recoveryLock_=0; timeSinceShot_=10;
    accuracyBias_=weapon.accuracyBiasMinimum; timeSinceJump_=10; wallReattachLock_=0;
    wasFire_=false; wasJump_=false; wasSwim_=false; wallSurface_=-1;
}

void Simulation::SetPlayerPosition(Vec3 position) {
    player_.position=position; player_.velocity={}; player_.state=PlayerState::Human;
    player_.grounded=false; player_.onOwnInk=false; wallSurface_=-1;
}

float Simulation::Random() {
    randomState_^=randomState_<<13; randomState_^=randomState_>>17; randomState_^=randomState_<<5;
    return static_cast<float>(randomState_&0x00ffffffu)/16777216.0f;
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
    if (!controls.swim || controls.fire || wallReattachLock_>0 || timeSinceShot_<weapon.postShotDelay) { wallSurface_=-1; return false; }
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
        player_.state=PlayerState::Human; player_.velocity=wallNormal*4.2f+Vec3{0,movement.jumpSpeed,0};
        player_.position+=wallNormal*0.15f; player_.grounded=false;
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
            player_.state=player_.onOwnInk?PlayerState::Swim:PlayerState::Human;
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
    player_.state=PlayerState::WallSwim; player_.onOwnInk=true; wallSurface_=candidate;
    const float ground=GroundHeight(player_.position,player_.position.y+0.1f);
    if (player_.position.y<ground) player_.position.y=ground;
    return true;
}

void Simulation::MovePlayer(float dt,const Controls& controls,bool jumpPressed) {
    const bool wasSwimming=player_.state==PlayerState::Swim;
    const Vec3 forward={std::sin(controls.yaw),0,std::cos(controls.yaw)};
    const Vec3 right={std::cos(controls.yaw),0,-std::sin(controls.yaw)};
    Vec3 direction=forward*controls.moveZ+right*controls.moveX;
    if(Length(direction)>1) direction=Normalize(direction);
    if(TryWallSwim(dt,controls,jumpPressed,direction)) {
        player_.speed=Length(player_.velocity); return;
    }
    player_.onOwnInk=player_.grounded && GetInkAtWorldPosition(player_.position)==1;
    const bool swim=controls.swim && !controls.fire && player_.onOwnInk && timeSinceShot_>=weapon.postShotDelay;
    player_.state=swim?PlayerState::Swim:PlayerState::Human;
    float speed=swim?movement.swimSpeed:movement.humanSpeed;
    if(controls.fire || timeSinceShot_<weapon.postShotDelay) speed=weapon.moveSpeedWhileFiring;
    if(wasSwimming && !swim && player_.grounded) {
        const float horizontal=std::sqrt(player_.velocity.x*player_.velocity.x+player_.velocity.z*player_.velocity.z);
        if(horizontal>speed) { player_.velocity.x*=speed/horizontal; player_.velocity.z*=speed/horizontal; }
    }
    const float acceleration=movement.acceleration*(player_.grounded?1.0f:0.45f);
    player_.velocity.x=Approach(player_.velocity.x,direction.x*speed,acceleration*dt);
    player_.velocity.z=Approach(player_.velocity.z,direction.z*speed,acceleration*dt);
    if(jumpPressed && player_.grounded) {
        player_.velocity.y=movement.jumpSpeed; player_.grounded=false; timeSinceJump_=0;
        // Retain the swimmer's horizontal momentum through takeoff.
    }
    const Vec3 previous=player_.position;
    if(!player_.grounded) player_.velocity.y-=movement.gravity*dt;
    player_.position+=player_.velocity*dt;
    const float radius=swim?movement.swimRadius:movement.humanRadius;
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
    player_.onOwnInk=player_.grounded && supportIndex>=0 && InkOnSurface(static_cast<uint32_t>(supportIndex),player_.position)==1;
    if(player_.state==PlayerState::Swim && !player_.onOwnInk) {
        player_.state=PlayerState::Human;
        // Exit feedback is immediate: do not carry fast swimming across dry ground.
        if(player_.grounded) {
            const float horizontal=std::sqrt(player_.velocity.x*player_.velocity.x+player_.velocity.z*player_.velocity.z);
            if(horizontal>movement.humanSpeed) {
                player_.velocity.x*=movement.humanSpeed/horizontal; player_.velocity.z*=movement.humanSpeed/horizontal;
            }
        }
    }
    player_.speed=std::sqrt(player_.velocity.x*player_.velocity.x+player_.velocity.z*player_.velocity.z);
}

void Simulation::Fire(const Controls& controls) {
    const Vec3 muzzle=Muzzle(controls);
    Vec3 direction=Normalize(controls.aimPoint-muzzle);
    if(Length(direction)<0.1f) direction={std::sin(controls.yaw),0,std::cos(controls.yaw)};
    const Vec3 right=Normalize(Cross({0,1,0},direction));
    const Vec3 up=Normalize(Cross(direction,right));
    const float jumpFactor=1-Clamp((timeSinceJump_-weapon.jumpAccuracyRecoveryStart)/
        std::max(0.01f,weapon.jumpAccuracyRecoveryEnd-weapon.jumpAccuracyRecoveryStart),0,1);
    const float spread=weapon.groundSpread+(weapon.jumpSpread-weapon.groundSpread)*jumpFactor;
    const float outerProbability=Clamp(accuracyBias_+(weapon.jumpAccuracyBiasMaximum-accuracyBias_)*jumpFactor,0,1);
    // Approximate the documented center-biased/outward-shot mechanism. Most shots
    // cluster tightly; a bounded increasing probability samples the cone's rim.
    const float radial=Random()<outerProbability ? 0.55f+0.45f*std::sqrt(Random()) : 0.28f*Random()*Random();
    const float angle=Random()*Pi*2;
    const float deviation=std::tan(spread*Pi/180.0f)*radial;
    direction=Normalize(direction+right*(std::cos(angle)*deviation)+up*(std::sin(angle)*deviation));
    Projectile projectile;
    projectile.position=muzzle; projectile.velocity=direction*weapon.projectileSpeed;
    projectile.nextDroplet=weapon.firstPaintDropletDistance;
    projectiles_.push_back(projectile);
    player_.ink=std::max(0.0f,player_.ink-weapon.inkConsume);
    recoveryLock_=weapon.inkRecoverStop; timeSinceShot_=0;
    accuracyBias_=std::min(weapon.accuracyBiasMaximum,accuracyBias_+weapon.accuracyBiasPerShot);
    ++shotsFired_;
}

void Simulation::Splash(uint32_t surface,Vec3 point,float radius,bool droplet) {
    if(surface>=surfaces_.size() || !surfaces_[surface].inkable) return;
    const Surface& plane=surfaces_[surface];
    const Vec3 delta=point-plane.origin;
    const float u=Dot(delta,plane.u),v=Dot(delta,plane.v);
    const float rotation=Random()*Pi*2;
    const float major=radius*(0.90f+Random()*0.20f);
    Paint({surface,u,v,major,major*(0.62f+Random()*0.24f),rotation,1});
    const int lobes=droplet?3:6;
    for(int i=0;i<lobes;++i) {
        const float angle=rotation+(i+Random()*0.6f)*2*Pi/lobes;
        const float offset=radius*(0.48f+Random()*0.58f);
        const float size=radius*(0.18f+Random()*0.28f);
        Paint({surface,u+std::cos(angle)*offset,v+std::sin(angle)*offset,size,size*(0.7f+Random()*0.5f),angle,1});
    }
}

void Simulation::UpdateProjectiles(float dt) {
    for(size_t i=0;i<projectiles_.size();) {
        Projectile& shot=projectiles_[i];
        const float oldAge=shot.age; shot.age+=dt;
        if(shot.age>weapon.straightFlightTime) {
            if(oldAge<=weapon.straightFlightTime) {
                const float speed=Length(shot.velocity);
                if(speed>weapon.brakeInitialSpeed) shot.velocity=shot.velocity*(weapon.brakeInitialSpeed/speed);
            }
            const float horizontal=std::sqrt(shot.velocity.x*shot.velocity.x+shot.velocity.z*shot.velocity.z);
            const bool braking=horizontal>weapon.brakeToFreeSpeedXZ || shot.velocity.y>weapon.brakeToFreeSpeedY;
            const float drag=std::pow(Clamp(1-(braking?weapon.brakeAirResistance:weapon.freeAirResistance),0.01f,1),dt*60);
            shot.velocity.x*=drag; shot.velocity.z*=drag;
            shot.velocity.y*=drag;
            shot.velocity.y-=(braking?weapon.brakeGravity:weapon.projectileGravity)*dt;
        }
        const Vec3 displacement=shot.velocity*dt;
        const float distance=Length(displacement);
        const RayHit hit=Raycast(shot.position,displacement,distance,weapon.stageHitRadius);
        const float traveled=hit.hit?hit.distance:distance;
        // Spawn at distance crossings along the swept segment, making the paint
        // trail independent of frame rate and avoiding emission beyond a wall.
        while(shot.droplets<std::max(0,weapon.paintDropletCount) && shot.nextDroplet<=shot.distance+traveled) {
            const float along=Clamp(shot.nextDroplet-shot.distance,0,traveled);
            Projectile drop;
            drop.position=shot.position+Normalize(displacement)*along;
            drop.position.x+=(Random()-0.5f)*0.22f; drop.position.z+=(Random()-0.5f)*0.22f;
            drop.velocity={0,-1.3f,0}; drop.droplets=shot.droplets;
            droplets_.push_back(drop);
            ++shot.droplets;
            shot.nextDroplet+=std::max(0.1f,weapon.paintDropletSpacing)*(0.86f+Random()*0.28f);
        }
        shot.distance+=traveled;
        shot.position+=Normalize(displacement)*traveled;
        if(hit.hit) {
            const float far=Clamp(shot.distance/std::max(1.0f,weapon.effectiveRange),0,1);
            Splash(hit.surface,hit.position,weapon.impactPaintRadius+(weapon.distantImpactPaintRadius-weapon.impactPaintRadius)*far,false);
        }
        if(hit.hit || shot.age>weapon.projectileLifetime || shot.position.y < -10 || shot.distance>60) {
            projectiles_[i]=projectiles_.back(); projectiles_.pop_back();
        } else ++i;
    }
    for(size_t i=0;i<droplets_.size();) {
        Projectile& drop=droplets_[i]; drop.age+=dt; drop.velocity.y-=32.0f*dt;
        const Vec3 displacement=drop.velocity*dt;
        const RayHit hit=Raycast(drop.position,displacement,Length(displacement),0.035f);
        if(hit.hit) Splash(hit.surface,hit.position,
            drop.droplets==0?weapon.nearestPaintDropletRadius:weapon.paintDropletRadius,true);
        drop.position+=displacement;
        if(hit.hit || drop.age>2.5f || drop.position.y < -10) {
            droplets_[i]=droplets_.back(); droplets_.pop_back();
        } else ++i;
    }
}

void Simulation::Step(float dt,const Controls& controls) {
    if(!std::isfinite(dt) || dt<=0) return;
    dt=std::min(dt,0.1f); // scene supplies fixed 1/120 s; guard debugger pauses
    recoveryLock_=std::max(0.0f,recoveryLock_-dt);
    wallReattachLock_=std::max(0.0f,wallReattachLock_-dt);
    timeSinceShot_+=dt; timeSinceJump_+=dt;
    const bool jumpPressed=controls.jump&&!wasJump_;
    MovePlayer(dt,controls,jumpPressed);
    if(controls.fire && !wasFire_) fireCooldown_=wasSwim_?weapon.swimInitialShotDelay:weapon.initialShotDelay;
    fireCooldown_-=dt;
    if(controls.fire && player_.state==PlayerState::Human) {
        // Bound catch-up emissions when an interval is edited live to zero.
        int emitted=0;
        while(fireCooldown_<=0 && player_.ink+0.000001f>=weapon.inkConsume && emitted<12) {
            Fire(controls); fireCooldown_+=std::max(1.0f/120.0f,weapon.FireInterval()); ++emitted;
        }
        if(player_.ink<weapon.inkConsume) fireCooldown_=std::max(0.0f,fireCooldown_);
    } else {
        fireCooldown_=std::max(0.0f,fireCooldown_);
    }
    if(!controls.fire && recoveryLock_<=0) {
        const bool swimming=player_.state==PlayerState::Swim || player_.state==PlayerState::WallSwim;
        player_.ink=std::min(1.0f,player_.ink+(swimming?movement.swimInkRecovery:movement.humanInkRecovery)*dt);
    }
    if(timeSinceShot_>=weapon.accuracyRecoveryDelay) accuracyBias_=std::max(weapon.accuracyBiasMinimum,accuracyBias_-weapon.accuracyRecovery*dt);
    const float jumpFactor=1-Clamp((timeSinceJump_-weapon.jumpAccuracyRecoveryStart)/
        std::max(0.01f,weapon.jumpAccuracyRecoveryEnd-weapon.jumpAccuracyRecoveryStart),0,1);
    player_.accuracy=weapon.groundSpread+(weapon.jumpSpread-weapon.groundSpread)*jumpFactor;
    const float formTarget=player_.state==PlayerState::Human?0.0f:1.0f;
    player_.formBlend=Approach(player_.formBlend,formTarget,dt/std::max(0.01f,movement.formTransitionTime));
    UpdateProjectiles(dt);
    wasFire_=controls.fire; wasJump_=controls.jump; wasSwim_=player_.state!=PlayerState::Human;
}
} // namespace ink
