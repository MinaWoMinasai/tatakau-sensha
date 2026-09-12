#include "InkShooterScene.h"
#include <algorithm>
#include <cmath>

void InkShooterScene::UpdateVisuals(float dt) {
    // Cosmetic state has its own clock and deterministic angular pattern. It
    // never consumes the weapon RNG or feeds movement, hits, or owner queries.
    visualKick_*=std::exp(-24*dt);
    formAge_+=dt;
    for(auto& burst:bursts_) {
        burst.age+=dt;
        if(!burst.ripple) {
            burst.velocity.y-=13*dt;
            burst.position+=burst.velocity*dt;
        }
    }
    std::erase_if(bursts_,[](const VisualBurst& burst) { return burst.age>=burst.lifetime; });
    const auto& p=simulation_.Player();
    const bool submerged=p.state==ink::PlayerState::Swim || p.state==ink::PlayerState::WallSwim;
    const bool wasSubmerged=visualState_==ink::PlayerState::Swim || visualState_==ink::PlayerState::WallSwim;
    auto ring=[&](ink::Vec3 center,ink::Vec3 normal,float radius,float lifetime) {
        bursts_.push_back({center,{},normal,0,lifetime,radius,true});
    };
    if(p.state!=visualState_) formAge_=0;
    // Being a squid does not imply touching ink. Surface ripples are restricted
    // to entering/leaving actual own-ink contact, never dry/air transformations.
    if(submerged!=wasSubmerged && p.onOwnInk && (submerged || p.grounded)) {
        const auto normal=p.state==ink::PlayerState::WallSwim?p.wallNormal:ink::Vec3{0,1,0};
        const auto center=p.position+normal*0.022f;
        ring(center,normal,0.20f,0.42f);
        for(int n=0;n<7;++n) {
            const float angle=n*2.39996f;
            const ink::Vec3 radial={std::sin(angle),0,std::cos(angle)};
            bursts_.push_back({center+radial*0.16f,radial*0.75f+normal*(submerged?1.0f:2.0f),normal,
                0,submerged?0.20f:0.32f,0.047f,false});
        }
    }
    visualState_=p.state;
    if(simulation_.ShotsFired()!=visualShotCount_) visualKick_=1;
    visualShotCount_=simulation_.ShotsFired();
    for(const auto& impact:simulation_.TakeImpactEvents()) {
        const auto normal=impact.normal;
        const auto axis=std::abs(normal.y)<0.9f?ink::Vec3{0,1,0}:ink::Vec3{1,0,0};
        const auto tangent=ink::Normalize(ink::Cross(normal,axis));
        const auto bitangent=ink::Cross(normal,tangent);
        const bool main=impact.kind==ink::PaintKind::Main;
        const bool explosion=impact.kind==ink::PaintKind::Explosion;
        if(explosion) {
            const float size=std::clamp(impact.radius,0.25f,1.5f);
            // The explosion event supplies the attached plane normal. A ring
            // follows that plane on walls as well as floors, while the thicker
            // central liquid lump and spray identify the delayed detonation.
            ring(impact.position+normal*0.030f,normal,size*0.40f,0.34f);
            bursts_.push_back({impact.position+normal*0.045f,normal*0.50f,normal,
                0,0.20f,size*0.24f,false});
        }
        // A separate event-local visual sequence keeps spray varied without
        // advancing either the ballistic or the persistent-paint random stream.
        uint32_t spraySeed=static_cast<uint32_t>(impact.sequence)^0x9e3779b9u;
        auto sprayRandom=[&]() {
            spraySeed^=spraySeed<<13; spraySeed^=spraySeed>>17; spraySeed^=spraySeed<<5;
            return static_cast<float>(spraySeed&0x00ffffffu)/16777216.0f;
        };
        const int count=explosion?10+static_cast<int>(sprayRandom()*6):
            main?3+static_cast<int>(sprayRandom()*5):static_cast<int>(sprayRandom()*3);
        const float phase=sprayRandom()*6.2831853f;
        for(int n=0;n<count;++n) {
            const float angle=phase+n*2.39996f+(sprayRandom()-0.5f)*0.65f;
            const auto radial=tangent*std::sin(angle)+bitangent*std::cos(angle);
            const float strength=(explosion?3.5f:main?1.8f:0.65f)*(0.65f+sprayRandom()*0.70f);
            const float lift=(explosion?1.8f:main?1.1f:0.6f)*(0.70f+sprayRandom()*0.65f);
            const float life=(explosion?0.38f:main?0.27f:0.17f)*(0.80f+sprayRandom()*0.40f);
            const float radius=(explosion?0.085f:main?0.043f:0.028f)*(0.65f+sprayRandom()*0.80f);
            bursts_.push_back({impact.position+normal*0.028f+radial*0.025f,
                radial*strength+normal*lift,normal,0,life,radius,false});
        }
    }
    wakeTime_+=dt;
    if(submerged && p.onOwnInk && p.speed>0.2f && wakeTime_>=0.065f) {
        wakeTime_=0;
        const auto normal=p.state==ink::PlayerState::WallSwim?p.wallNormal:ink::Vec3{0,1,0};
        const auto trail=ink::Normalize(p.velocity)*-0.19f;
        ring(p.position+trail+normal*0.025f,normal,0.13f,0.36f);
    }
    if(bursts_.size()>512) bursts_.erase(bursts_.begin(),bursts_.end()-512);
}

ink::Controls InkShooterScene::ReadValidationControls(float dt) {
    replayTime_+=dt;
    const float t=replayTime_;
    const int section=t<12?0:t<16?1:t<21?2:t<25?3:t<29?4:t<33?5:6;
    if(section!=replaySection_) {
        // Independent, named scenarios reposition/reset at explicit boundaries;
        // movement inside each scenario is the regular 120 Hz simulation.
        if(section!=4 && section!=6) Reset();
        if(section==1) simulation_.SetPlayerPosition({0,0,-8});
        if(section==2) simulation_.SetPlayerPosition({7,0,-2});
        if(section==3) simulation_.SetPlayerPosition({0,0,11});
        if(section==4) simulation_.SetPlayerPosition({0,0.01f,13.68f});
        if(section==5) simulation_.SetPlayerPosition({2.3f,0,-2});
        if(section==6) { simulation_.SetPlayerPosition({0,0,-8}); yaw_=0; }
        cameraReady_=false;
        replaySection_=section;
    }
    ink::Controls c{};
    pitch_=0.19f;
    if(section==0) { c.fire=t<10.25f; c.swim=t>=10.25f; c.moveZ=t>=10.25f?1.0f:0; }
    if(section==1) { c.fire=true; c.moveZ=1; c.jump=t>=13.3f && t-dt<13.3f; }
    if(section==2) { pitch_=-0.16f; c.fire=true; c.moveZ=t<19.9f?1.0f:0; }
    if(section==3) { pitch_=0.25f-(t-21)*0.26f; c.fire=true; }
    if(section==4) { pitch_=-0.22f; c.swim=true; c.moveZ=1; }
    if(section==5) { pitch_=0.08f; c.fire=t<29.35f || t>31.5f; }
    // Paired stationary views alternate the same atlas, camera, and geometry.
    // Skip transition frames when interpreting GPU timings in the CSV.
    if(section==6) wetEdges_=static_cast<int>(t-33)%4>=2;
    auto captureAt=[&](float time,const char* name) { if(t>=time && t-dt<time) RequestCapture(name); };
    captureAt(9.9f,"01_floor_fire"); captureAt(11.1f,"02_swim");
    captureAt(13.5f,"03_jump_fire"); captureAt(20.0f,"04_ramp");
    captureAt(24.8f,"05_wall_paint"); captureAt(26.0f,"06_wall_swim");
    captureAt(29.28f,"07_dummy"); captureAt(33.8f,"08_hard_edge"); captureAt(35.8f,"09_smooth_edge");
    if(t>45) {
        replay_=false; replayLog_.flush(); WriteGpuDiagnostics(); wetEdges_=true;
        debug_=true; SetCaptured(false);
    }
    c.yaw=yaw_; c.pitch=pitch_; c.aimPoint=aimPoint_;
    return c;
}
