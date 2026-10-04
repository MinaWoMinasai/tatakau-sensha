// Production head generation / draw methods run against recording adapters.
// These CPU tests do not create a Device or launch the game.
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace cg2 {
struct Vector3 { float x, y, z; };
struct Vector4 { float x, y, z, w; };
struct Matrix4x4 { float m[4][4]{}; };
Vector3 operator+(Vector3 a, Vector3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vector3 operator-(Vector3 a, Vector3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vector3 operator*(Vector3 a, float b) { return {a.x*b,a.y*b,a.z*b}; }
Vector3 operator/(Vector3 a, float b) { return {a.x/b,a.y/b,a.z/b}; }
float Length(Vector3 a) { return std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z); }
Vector3 Normalize(Vector3 a) { return a/Length(a); }
Vector3 Cross(Vector3 a, Vector3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
struct FakeFence { uint64_t completed=0; uint64_t GetCompletedValue() const { return completed; } };
struct DirectXCommon {
    uint64_t value=0;
    FakeFence fence;
    FakeFence* GetFence() { return &fence; }
    uint64_t GetFenceValue() const { return value; }
    void CompleteFrame() { fence.completed=++value; }
};
class NeonGridRenderer {
public:
    static constexpr uint32_t kMaxVertices=196608;
    struct Call { bool disc; Vector3 a,b; float size; Vector4 color; float soft; };
    static inline NeonGridRenderer* latest=nullptr;
    NeonGridRenderer() { latest=this; }
    void Initialize(DirectXCommon*,const std::string&) {}
    void BeginFrame() { calls.clear(); vertices=0; draws=0; }
    void SetLineStyle(float soft, float) { soft_=soft; }
    void QueueCameraFacingLine(Vector3 a,Vector3 b,float width,Vector4 color,Vector3) {
        calls.push_back({false,a,b,width,color,soft_}); vertices+=18;
    }
    void QueueBillboardDisc(Vector3 center,float radius,Vector4 color,Vector3,Vector3,int segments) {
        assert(segments==12); calls.push_back({true,center,center,radius,color,soft_}); vertices+=segments*3;
    }
    uint32_t GetVertexCount() const { return vertices; }
    void DrawAll(const Matrix4x4&) { ++draws; }
    std::vector<Call> calls;
    uint32_t vertices=0,draws=0;
private:
    float soft_=0;
};
} // namespace cg2

class Bullet {
public:
    enum class SpecialKind { None, Rail, SlashWave, ParryReflection };
    cg2::Vector3 position{2,3,0},velocity{1,0,0};
    cg2::Vector4 color{.28f,1,.37f,1};
    float collisionRadius=.5f;
    uint32_t damage=20;
    bool dead=false;
    SpecialKind special=SpecialKind::None;
    bool IsDead() const { return dead; }
    SpecialKind GetSpecialKind() const { return special; }
    cg2::Vector3 GetWorldPosition() const { return position; }
    cg2::Vector3 GetMove() const { return velocity; }
    cg2::Vector4 GetVisualColor() const { return color; }
};

#include "neon_projectile_production.inc"

template<class F> void Throws(F&& action) {
    bool threw=false;
    try { action(); } catch(const std::logic_error&) { threw=true; }
    assert(threw);
}
bool Near(float a,float b) { return std::abs(a-b)<.00001f; }
void FiniteCalls(const std::vector<cg2::NeonGridRenderer::Call>& calls) {
    for(const auto& call:calls) {
        for(float value:{call.a.x,call.a.y,call.a.z,call.b.x,call.b.y,call.b.z,call.size,
            call.color.x,call.color.y,call.color.z,call.color.w,call.soft}) assert(std::isfinite(value));
        assert(call.size>0&&call.color.w>=0&&call.color.w<=1);
    }
}
void TestHeadAndPhysicsSeparation() {
    cg2::DirectXCommon dx; NeonProjectileRenderer renderer; renderer.Initialize(&dx);
    Bullet bullet;
    renderer.BeginFrame({&bullet},{0,0,1});
    assert(renderer.GetStats().heads==1&&renderer.GetStats().vertices==126);
    const auto first=cg2::NeonGridRenderer::latest->calls;
    assert(first.size()==5&&first[0].size>first[1].size&&first[3].size<first[1].size);
    assert(first[0].color.w<first[1].color.w&&first[0].soft>first[1].soft);
    assert(first[3].color.x/first[3].color.y>first[1].color.x/first[1].color.y); // Pale core.
    assert(bullet.position.x==2&&bullet.velocity.x==1&&bullet.collisionRadius==.5f&&bullet.damage==20);
    renderer.Draw({}); renderer.Draw({});
    assert(renderer.GetStats().drawCalls==1&&cg2::NeonGridRenderer::latest->draws==1);
    dx.CompleteFrame(); bullet.collisionRadius=200;
    renderer.BeginFrame({&bullet},{0,0,1});
    const auto& second=cg2::NeonGridRenderer::latest->calls;
    for(size_t i=0;i<first.size();++i) assert(Near(first[i].size,second[i].size)&&Near(first[i].a.x,second[i].a.x));
    assert(bullet.collisionRadius==200&&bullet.damage==20);
    std::cout<<"PASS: first-frame pale tip / colored band / low-alpha halo; collider dimensions untouched.\n";
}
void TestEligibilityFallbackAndParameters() {
    cg2::DirectXCommon dx; NeonProjectileRenderer renderer; renderer.Initialize(&dx);
    Bullet ordinary,dead,rail,wave,reflection;
    dead.dead=true; rail.special=Bullet::SpecialKind::Rail; wave.special=Bullet::SpecialKind::SlashWave;
    reflection.special=Bullet::SpecialKind::ParryReflection;
    ordinary.velocity={0,0,0};
    renderer.BeginFrame({nullptr,&ordinary,&dead,&rail,&wave,&reflection},{1,0,0});
    assert(renderer.GetStats().heads==2&&renderer.GetStats().skipped==4);
    FiniteCalls(cg2::NeonGridRenderer::latest->calls);
    dx.CompleteFrame();
    const float invalid=std::numeric_limits<float>::quiet_NaN();
    NeonProjectileParams params{};
    params.headLength=invalid;params.headWidth=-4;params.coreIntensity=std::numeric_limits<float>::infinity();
    params.haloAlpha=100;params.haloWidthScale=100;renderer.SetParams(params);
    assert(renderer.GetParams().headLength==NeonProjectileParams{}.headLength);
    assert(renderer.GetParams().headWidth==.03f&&renderer.GetParams().haloAlpha==.4f);
    assert(renderer.GetParams().haloWidthScale==4&&renderer.GetParams().coreIntensity==4.5f);
    ordinary.velocity={invalid,0,0};
    renderer.BeginFrame({&ordinary},{invalid,0,0}); FiniteCalls(cg2::NeonGridRenderer::latest->calls);
    dx.CompleteFrame(); ordinary.position={invalid,0,0};
    renderer.BeginFrame({&ordinary},{0,0,1}); assert(renderer.GetStats().heads==0&&renderer.GetStats().skipped==1);
    renderer.Draw({}); assert(renderer.GetStats().drawCalls==0);
    dx.CompleteFrame();params.enabled=false;renderer.SetParams(params);
    renderer.BeginFrame({&reflection},{0,0,1});renderer.Draw({});assert(renderer.GetStats().vertices==0);
    std::cout<<"PASS: dead / null / Rail / SlashWave filtering, stationary fallback, finite bounded params, disabled mode.\n";
}
void TestFenceAndCompleteHeadCapacity() {
    cg2::DirectXCommon dx; NeonProjectileRenderer renderer;
    Throws([&]{renderer.BeginFrame({},{});});renderer.Initialize(&dx);
    Throws([&]{renderer.Initialize(&dx);});
    Bullet bullet;renderer.BeginFrame({&bullet},{0,0,1});renderer.Draw({});
    const auto immutable=cg2::NeonGridRenderer::latest->calls;
    Throws([&]{renderer.BeginFrame({&bullet},{0,0,1});});
    ++dx.value; Throws([&]{renderer.BeginFrame({&bullet},{0,0,1});}); // Fence incomplete.
    assert(cg2::NeonGridRenderer::latest->calls.size()==immutable.size());
    dx.fence.completed=dx.value;
    std::vector<Bullet*> many(2000,&bullet);renderer.BeginFrame(many,{0,0,1});
    constexpr uint32_t completeHeads=cg2::NeonGridRenderer::kMaxVertices/126;
    assert(renderer.GetStats().heads==completeHeads&&renderer.GetStats().truncated==2000-completeHeads);
    assert(renderer.GetStats().vertices==completeHeads*126&&renderer.GetStats().vertices<=cg2::NeonGridRenderer::kMaxVertices);
    assert(cg2::NeonGridRenderer::latest->calls.size()==completeHeads*5);
    renderer.Draw({}); assert(renderer.GetStats().drawCalls==1);
    std::cout<<"PASS: one upload/draw per frame, in-flight Fence protection, complete-head bounded batch.\n";
}
int main() {
    TestHeadAndPhysicsSeparation();TestEligibilityFallbackAndParameters();TestFenceAndCompleteHeadCapacity();
}
