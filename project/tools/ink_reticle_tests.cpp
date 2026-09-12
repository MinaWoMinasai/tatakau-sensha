#include "../game/ink/InkReticleMath.h"
#include <cassert>
#include <cstdio>
#include <limits>

int main() {
    using namespace ink::reticle;
    const float ground=ProjectSpreadPixels(4.86f,1.02f,720);
    const float jump=ProjectSpreadPixels(11.66f,1.02f,720);
    assert(ground>54 && ground<55 && jump>132 && jump<134);
    // Projected width is twice the angular half-width. Recovering the angle
    // checks the geometric meaning independently of a copied pixel constant.
    const float focal=360/std::tan(1.02f*0.5f);
    constexpr float DegreesPerRadian=57.2957795131f;
    assert(std::abs(std::atan(ground/focal)*DegreesPerRadian-4.86f)<0.0001f);
    assert(std::abs(std::atan(jump/focal)*DegreesPerRadian-11.66f)<0.0001f);
    assert(std::abs(ProjectSpreadPixels(4.86f,1.02f,1440)-ground*2)<0.001f);
    const float widerFov=ProjectSpreadPixels(4.86f,1.4f,720);
    assert(widerFov<ground);
    assert(std::abs(widerFov/ground-std::tan(0.51f)/std::tan(0.70f))<0.00001f);
    float previous=0;
    for(int n=0;n<800;++n) {
        const float radius=ProjectSpreadPixels(static_cast<float>(n)*0.1f,1.02f,720);
        assert(radius>=previous && std::isfinite(radius));
        previous=radius;
    }
    assert(ProjectSpreadPixels(0,1.02f,720)==0);
    assert(ProjectSpreadPixels(-1,1.02f,720)==0);
    assert(ProjectSpreadPixels(11,0,720)==0);
    assert(ProjectSpreadPixels(11,4,720)==0);
    assert(ProjectSpreadPixels(11,1,0)==0);
    assert(ProjectSpreadPixels(std::numeric_limits<float>::quiet_NaN(),1,720)==0);
    assert(ProjectSpreadPixels(1,std::numeric_limits<float>::infinity(),720)==0);
    assert(ProjectSpreadPixels(1,1,std::numeric_limits<float>::infinity())==0);
    assert(ProjectSpreadPixels(180,1.02f,720)==ProjectSpreadPixels(80,1.02f,720));

    constexpr float FirstCharge=0.416667f;
    auto arcs=SplitCharge(0,FirstCharge);
    assert(arcs.first==0 && arcs.second==0);
    arcs=SplitCharge(FirstCharge*0.5f,FirstCharge);
    assert(std::abs(arcs.first-0.5f)<0.00001f && arcs.second==0);
    arcs=SplitCharge(FirstCharge,FirstCharge);
    assert(arcs.first==1 && arcs.second==0);
    arcs=SplitCharge(FirstCharge+(1-FirstCharge)*0.5f,FirstCharge);
    assert(arcs.first==1 && std::abs(arcs.second-0.5f)<0.00001f);
    arcs=SplitCharge(1,FirstCharge);
    assert(arcs.first==1 && arcs.second==1);
    arcs=SplitCharge(-1,0);
    assert(arcs.first==0 && arcs.second==0);
    arcs=SplitCharge(2,1);
    assert(arcs.first==1 && arcs.second==1);
    arcs=SplitCharge(std::numeric_limits<float>::quiet_NaN(),FirstCharge);
    assert(arcs.first==0 && arcs.second==0);
    const auto fallback=SplitCharge(0.5f,std::numeric_limits<float>::quiet_NaN());
    const auto expected=SplitCharge(0.5f,FirstCharge);
    assert(fallback.first==expected.first && fallback.second==expected.second);
    std::printf("Reticle math PASS: ground %.4f px / jump %.4f px; projection, FOV, viewport, charge stages and finite guards\n",ground,jump);
}
