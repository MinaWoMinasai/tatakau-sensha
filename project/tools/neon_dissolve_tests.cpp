// Pure CPU tests. GPU shader/readback checks and actual-model/controller checks
// live in the Neon pipeline and Preview Animation suites respectively.
#include "../DirectX/engine/3d/neon/NeonDissolve.h"
#include <array>
#include <iostream>
#include <stdexcept>

using namespace cg2;
namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
bool Near(float a, float b, float tolerance = 1.0e-5f) { return std::abs(a - b) <= tolerance; }
float Dot(const Vector3& a, const Vector3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
Matrix4x4 Identity() {
    Matrix4x4 result{};
    for (size_t i=0;i<4;++i) result.m[i][i]=1;
    return result;
}
Vector3 TransformPoint(const Vector3& p, const Matrix4x4& matrix) {
    return {p.x*matrix.m[0][0]+p.y*matrix.m[1][0]+p.z*matrix.m[2][0]+matrix.m[3][0],
            p.x*matrix.m[0][1]+p.y*matrix.m[1][1]+p.z*matrix.m[2][1]+matrix.m[3][1],
            p.x*matrix.m[0][2]+p.y*matrix.m[1][2]+p.z*matrix.m[2][2]+matrix.m[3][2]};
}
void TestSanitation() {
    static_assert(sizeof(NeonDissolveParams)==80);
    static_assert(offsetof(NeonDissolveParams, scanMin)==12 && offsetof(NeonDissolveParams, enabled)==32
        && offsetof(NeonDissolveParams, edgeColor)==48 && offsetof(NeonDissolveParams, edgeWidth)==64);
    NeonDissolveParams value;
    Require(value.enabled==0 && value.progress==0,"Dissolve must remain default disabled");
    value.enabled=value.edgeEnabled=UINT32_MAX;
    value.direction={2,-2,0}; value.scanMin=-4; value.scanMax=7;
    value.progress=2; value.noiseStrength=-1; value.noiseScale=200;
    value.edgeColor={-2,200,std::numeric_limits<float>::quiet_NaN()};
    value.edgeIntensity=200; value.edgeWidth=-1; value.padding=200;
    for(float& x:value.edgePadding) x=200;
    auto safe=SanitizeNeonDissolveParams(value);
    Require(safe.enabled==1 && safe.edgeEnabled==1 && safe.progress==1 && Near(Dot(safe.direction,safe.direction),1)
        && safe.scanMin==-4 && safe.scanMax==7 && safe.noiseStrength==0 && safe.noiseScale==128
        && safe.edgeColor.x==0 && safe.edgeColor.y==100 && safe.edgeColor.z==0
        && safe.edgeIntensity==100 && safe.edgeWidth==0 && safe.padding==0
        && safe.edgePadding[0]==0 && safe.edgePadding[1]==0 && safe.edgePadding[2]==0,
        "Dissolve sanitation must normalize and retain only finite, valid parameters");
    for(int invalid=0;invalid<7;++invalid) {
        auto bad=value;
        if(invalid==0) bad.direction={0,0,0};
        if(invalid==1) bad.direction.x=std::numeric_limits<float>::infinity();
        if(invalid==2) bad.scanMin=std::numeric_limits<float>::quiet_NaN();
        if(invalid==3) bad.scanMax=std::numeric_limits<float>::infinity();
        if(invalid==4) bad.scanMax=bad.scanMin-1;
        if(invalid==5) bad.scanMin=-1000001;
        if(invalid==6) bad.scanMax=1000001;
        safe=SanitizeNeonDissolveParams(bad);
        Require(safe.enabled==0 && safe.scanMin==0 && safe.scanMax==1 && safe.direction.x==1,
            "Invalid scan bounds/direction must disable dissolve safely");
    }
    value.progress=value.noiseScale=value.noiseStrength=value.edgeWidth=value.edgeIntensity=std::numeric_limits<float>::quiet_NaN();
    safe=SanitizeNeonDissolveParams(value);
    Require(safe.progress==0 && safe.noiseScale==0 && safe.noiseStrength==0 && safe.edgeWidth==0 && safe.edgeIntensity==0,
        "Nonfinite dissolve inputs must have finite inactive fallbacks");
    value.direction={(std::numeric_limits<float>::max)(),(std::numeric_limits<float>::max)(),0};
    safe=SanitizeNeonDissolveParams(value);
    Require(safe.enabled==1 && Near(Dot(safe.direction,safe.direction),1),"Direction normalization must not overflow float length");
}
void TestDirectionCovector() {
    const auto identity=Identity();
    Vector3 direction{};
    std::string error;
    Require(MakeNeonDissolveDirection(identity,identity,NeonDissolveDirection::UpperLeftToLowerRight,direction,&error)
        && error.empty() && Near(direction.x,.70710678f) && Near(direction.y,-.70710678f) && direction.z==0,
        "Initial direction must scan camera upper-left to lower-right");
    Matrix4x4 world=identity;
    world.m[0][0]=-2; world.m[0][1]=.4f; world.m[0][2]=.2f;
    world.m[1][0]=.7f; world.m[1][1]=3; world.m[1][2]=-.1f;
    world.m[2][0]=.3f; world.m[2][1]=-.2f; world.m[2][2]=.5f;
    world.m[3][0]=12; world.m[3][1]=-7; world.m[3][2]=3;
    auto camera=identity;
    camera.m[0][0]=0; camera.m[0][2]=-1;
    camera.m[2][0]=1; camera.m[2][2]=0;
    const std::array<NeonDissolveDirection,4> presets={NeonDissolveDirection::UpperLeftToLowerRight,
        NeonDissolveDirection::UpperRightToLowerLeft,NeonDissolveDirection::TopToBottom,NeonDissolveDirection::LeftToRight};
    const std::array<Vector3,4> worldNormals={Vector3{0,-.70710678f,-.70710678f},Vector3{0,-.70710678f,.70710678f},
        Vector3{0,-1,0},Vector3{0,0,-1}};
    for(size_t preset=0;preset<presets.size();++preset) {
        Require(MakeNeonDissolveDirection(world,camera,presets[preset],direction,&error),"Valid affine World rejected");
        Require(Near(Dot(direction,direction),1),"Converted scan direction must be unit length");
        const auto& n=worldNormals[preset];
        const Vector3 raw{world.m[0][0]*n.x+world.m[0][1]*n.y+world.m[0][2]*n.z,
            world.m[1][0]*n.x+world.m[1][1]*n.y+world.m[1][2]*n.z,
            world.m[2][0]*n.x+world.m[2][1]*n.y+world.m[2][2]*n.z};
        const float scale=std::sqrt(Dot(raw,raw));
        const Vector3 origin=TransformPoint({},world);
        for(const Vector3& point: {Vector3{1,2,-3},Vector3{-.25f,.7f,.3f},Vector3{-4,-2,1}}) {
            const Vector3 transformed=TransformPoint(point,world);
            const Vector3 delta{transformed.x-origin.x,transformed.y-origin.y,transformed.z-origin.z};
            Require(Near(Dot(delta,n),Dot(point,direction)*scale,2.0e-5f),
                "Row-vector plane covector must retain World projection under nonuniform scale/shear/mirror");
        }
        auto translated=world; translated.m[3][0]+=100; translated.m[3][1]-=50;
        Vector3 again{};
        Require(MakeNeonDissolveDirection(translated,camera,presets[preset],again)
            && Near(direction.x,again.x) && Near(direction.y,again.y) && Near(direction.z,again.z),
            "World translation must not rotate a fixed scan covector");
    }
    const Vector3 before=direction;
    for(int invalid=0;invalid<5;++invalid) {
        auto badWorld=world, badCamera=camera;
        auto preset=NeonDissolveDirection::UpperLeftToLowerRight;
        if(invalid==0) badWorld.m[0][0]=std::numeric_limits<float>::quiet_NaN();
        if(invalid==1) badWorld.m[0][3]=.1f;
        if(invalid==2) for(size_t i=0;i<3;++i) badWorld.m[1][i]=badWorld.m[0][i];
        if(invalid==3) for(size_t i=0;i<3;++i) badCamera.m[0][i]=0;
        if(invalid==4) preset=static_cast<NeonDissolveDirection>(999);
        Require(!MakeNeonDissolveDirection(badWorld,badCamera,preset,direction,&error) && !error.empty()
            && direction.x==before.x && direction.y==before.y && direction.z==before.z,
            "Rejected transform/direction must report an error and leave prior output unchanged");
    }
}
void TestNoiseAndMonotonicity() {
    size_t samples=0;
    for(const uint32_t seed:{0u,1337u,UINT32_MAX}) {
        NeonDissolveParams params;
        params.enabled=1; params.direction={1,0,0}; params.scanMin=-1; params.scanMax=1;
        params.noiseStrength=.3f; params.noiseScale=8; params.seed=seed;
        for(int ix=-10;ix<=10;++ix) for(int iy=-3;iy<=3;++iy) for(int iz=-2;iz<=2;++iz) {
            const Vector3 point{ix*.1f,iy*.11f,iz*.17f};
            const float noise=NeonDissolveValueNoise(point,params.noiseScale,seed);
            Require(std::isfinite(noise) && noise>=0 && noise<=1 && noise==NeonDissolveValueNoise(point,params.noiseScale,seed),
                "Stable spatial value noise must remain reproducible and bounded for negative coordinates/seeds");
            bool removed=false;
            float previous=std::numeric_limits<float>::infinity();
            for(int step=0;step<=100;++step) {
                params.progress=step*.01f;
                const float distance=EvaluateNeonDissolveSignedDistance(point,params);
                Require(std::isfinite(distance),"Finite point and params must yield finite dissolve distance");
                if(step==0) Require(distance>0,"Progress zero must preserve every original surface point");
                if(step==100) Require(distance<0,"Progress one must remove every surface point and boundary");
                if(step>0 && step<100) {
                    Require(distance<=previous+1.0e-6f,"Interior threshold must increase monotonically");
                    previous=distance;
                }
                Require(!removed || distance<0,"A removed point must never resurrect with increasing progress");
                removed|=distance<0;
            }
            ++samples;
        }
    }
    const Vector3 point{-.999999f,.13f,-.37f}, adjacent{-1.000001f,.13f,-.37f};
    Require(Near(NeonDissolveValueNoise(point,1,7),NeonDissolveValueNoise(adjacent,1,7),1.0e-4f),
        "Noise interpolation must remain continuous across negative lattice boundaries");
    NeonDissolveParams params; params.enabled=1; params.direction={1,0,0}; params.scanMin=-1; params.scanMax=1;
    params.progress=.25f; params.noiseStrength=0;
    Require(Near(EvaluateNeonDissolveSignedDistance({-.75f,0,0},params),-.25f)
        && Near(EvaluateNeonDissolveSignedDistance({.75f,0,0},params),1.25f),"Noise zero must reduce to a deterministic scan plane");
    params.enabled=0; params.progress=1;
    Require(EvaluateNeonDissolveSignedDistance({100,100,100},params)>0,"Disabled dissolve must override progress one");
    params.enabled=1; params.scanMin=params.scanMax=0; params.progress=.5f;
    Require(std::isfinite(EvaluateNeonDissolveSignedDistance({},params)),"Degenerate projection range must avoid zero division");
    Require(NeonDissolveValueNoise({std::numeric_limits<float>::quiet_NaN(),0,0},12,7)==.5f
        && NeonDissolveValueNoise({},std::numeric_limits<float>::infinity(),7)==.5f
        && NeonDissolveValueNoise(point,0,7)==.5f,"Invalid noise input and scale zero must remain finite and neutral");
    params.scanMin=-1; params.scanMax=1; params.noiseStrength=3; params.noiseScale=0;
    Require(Near(EvaluateNeonDissolveSignedDistance({.25f,0,0},params),.25f),
        "Noise scale zero must also remove scan-range noise padding in the CPU contract");
    Require(NeonDissolveValueNoise({.13f,.23f,.33f},12,7)!=NeonDissolveValueNoise({.13f,.23f,.33f},12,8),
        "Different seeds must control genuinely different stable spatial patterns");
    std::cout<<"PASS: "<<samples<<" fixed surface points across 101 progress steps: deterministic bounded noise, monotonic survival, exact endpoints, no-time input.\n";
}
}
int main() {
    try {
        TestSanitation(); TestDirectionCovector(); TestNoiseAndMonotonicity();
        std::cout<<"PASS: 80-byte dissolve layout; opt-in finite sanitation; camera-plane covectors under rotated/nonuniform/mirrored affine World, translation invariance and atomic invalid-transform rejection.\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
