#include "InkShooterScene.h"
#include <cmath>

ink::Controls InkShooterScene::ReadFeelValidationControls(float dt) {
    replayTime_+=dt;
    const float t=replayTime_,previous=t-dt;
    const int section=t<4?0:t<7?1:t<10?2:t<13?3:t<16?4:t<20?5:t<23?6:7;
    if(section!=replaySection_) {
        if(section<7) {
            EquipCatalogWeapon(section<=2?"splattershot":"tri_stringer");
            Reset();
            if(section<=1) {
                // Own ink ends at z=-6; second island starts at -5.3.
                simulation_.Paint({0,15,6,2,2,0,1});
                if(section==0) simulation_.Paint({0,15,10,2,1.3f,0,1});
            }
            if(section==5) simulation_.SetPlayerPosition({0,0,7});
        }
        replaySection_=section; cameraReady_=false;
    }
    ink::Controls c{}; yaw_=0; pitch_=section==5?-0.06f:0.19f;
    if(section==0) { c.swim=true; c.moveZ=1; }
    if(section==1) { c.swim=true; c.moveZ=t<4.65f?1.0f:t<5.8f?0.0f:-1.0f; }
    if(section==2) c.fire=true;
    if(section==3) c.fire=std::fmod(t-10,0.60f)<0.12f;
    if(section==4) c.fire=std::fmod(t-13,1.20f)<0.65f;
    if(section==5) c.fire=std::fmod(t-16,2.1f)<1.3f;
    if(section==6) { c.fire=t<21.1f; c.swim=t>=20.8f; }
    auto photo=[&](float when,const char* name) { if(t>=when && previous<when) RequestCapture(name); };
    photo(0.53f,"01_ink_edge_carry"); photo(0.82f,"02_small_gap"); photo(1.7f,"03_dry_coast");
    photo(4.88f,"04_release_brake"); photo(17.23f,"05_charge_with_sound"); photo(21.0f,"06_cancel_charge");
    if(section==7) {
        replay_=false; simulation_.CancelCharge(); audio_.Stop(); replayLog_.flush(); WriteGpuDiagnostics();
        debug_=true; SetCaptured(false);
    }
    c.yaw=yaw_; c.pitch=pitch_; c.aimPoint=aimPoint_;
    return c;
}
