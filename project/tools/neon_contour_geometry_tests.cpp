// Runs the actual NeonGridRenderer CPU geometry on a mapped-memory adapter.
// Checks geometry coverage rather than only recording calls to a fake renderer.
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
#include "../DirectX/engine/particle/NeonParticlePresentation.h"

namespace Microsoft::WRL {template<class T> struct ComPtr {};}
struct ID3D12Resource {};
struct D3D12_VERTEX_BUFFER_VIEW {};
namespace cg2 {
struct Vector2 {float x, y;};
struct Vector3 {float x, y, z;};
struct Vector4 {float x, y, z, w;};
struct Matrix4x4 {};
struct Material {};
struct DirectXCommon {};
struct TrailVertex {Vector3 pos; Vector4 color; Vector2 uv;};
Vector3 operator+(Vector3 a, Vector3 b) {return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vector3 operator-(Vector3 a, Vector3 b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vector3 operator*(Vector3 a, float b) {return {a.x*b,a.y*b,a.z*b};}
Vector3 operator/(Vector3 a, float b) {return {a.x/b,a.y/b,a.z/b};}
Vector3& operator+=(Vector3& a, Vector3 b) {a=a+b;return a;}
float Dot(Vector3 a, Vector3 b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
float Length(Vector3 a) {return std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);}
Vector3 Normalize(Vector3 a) {return a/Length(a);}
Vector3 Cross(Vector3 a, Vector3 b) {return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
}
#define private public
#include "neon_contour_production.inc"
#undef private

using cg2::Vector3;
using cg2::Vector4;
constexpr Vector4 kGreen{.15f,1.0f,.35f,.8f};
constexpr Vector3 kForward{0,0,1};
bool Near(float a,float b) {return std::abs(a-b)<.00001f;}
bool Same(cg2::TrailVertex a,cg2::TrailVertex b) {
    return Near(a.pos.x,b.pos.x)&&Near(a.pos.y,b.pos.y)&&Near(a.pos.z,b.pos.z)&&
        Near(a.color.x,b.color.x)&&Near(a.color.y,b.color.y)&&Near(a.color.z,b.color.z)&&Near(a.color.w,b.color.w)&&
        Near(a.uv.x,b.uv.x)&&Near(a.uv.y,b.uv.y);
}
struct Fixture {
    std::vector<cg2::TrailVertex> vertices=std::vector<cg2::TrailVertex>(cg2::NeonGridRenderer::kMaxVertices+6);
    cg2::NeonGridRenderer renderer;
    Fixture() {renderer.vertexData_=vertices.data();}
    void Finite() const {
        assert(renderer.vertexCount_<=cg2::NeonGridRenderer::kMaxVertices&&renderer.vertexCount_%3==0);
        for(uint32_t i=0;i<renderer.vertexCount_;++i) {
            const auto& vertex=vertices[i];
            for(float value:{vertex.pos.x,vertex.pos.y,vertex.pos.z,vertex.color.x,vertex.color.y,vertex.color.z,
                vertex.color.w,vertex.uv.x,vertex.uv.y}) assert(std::isfinite(value));
            assert(vertex.color.w>=0&&vertex.color.w<=1);
        }
    }
    // Points away from a shared edge should belong to precisely one triangle.
    // Multiple hits expose the bright corner / cap overlaps of independent strips.
    int Coverage(float x,float y) const {
        int hits=0;
        for(uint32_t i=0;i+2<renderer.vertexCount_;i+=3) {
            const auto a=vertices[i].pos,b=vertices[i+1].pos,c=vertices[i+2].pos;
            const double determinant=(static_cast<double>(b.y)-c.y)*(a.x-c.x)+(static_cast<double>(c.x)-b.x)*(a.y-c.y);
            if(std::abs(determinant)<1e-12) continue;
            const double u=((static_cast<double>(b.y)-c.y)*(x-c.x)+(static_cast<double>(c.x)-b.x)*(y-c.y))/determinant;
            const double v=((static_cast<double>(c.y)-a.y)*(x-c.x)+(static_cast<double>(a.x)-c.x)*(y-c.y))/determinant;
            if(u>1e-7&&v>1e-7&&u+v<1.0-1e-7) ++hits;
        }
        return hits;
    }
};

void TestProfileAndCaps() {
    Fixture f;
    f.renderer.QueueContourLine({-1,0,0},{1,0,0},.2f,kGreen,kForward);
    assert(f.renderer.vertexCount_==378);
    f.Finite();
    float maximum=0;
    bool pale=false,shoulder=false,outer=false;
    for(uint32_t i=0;i<f.renderer.vertexCount_;++i) {
        const auto& vertex=f.vertices[i];
        maximum=(std::max)(maximum,std::abs(vertex.pos.y));
        if(vertex.color.y>2.7f) {pale=true;assert(vertex.color.x/vertex.color.y>.6f);}
        if(Near(vertex.color.y,1.8f)) {shoulder=true;assert(Near(vertex.color.x/vertex.color.y,.15f));}
        if(vertex.color.w==0) outer=true;
    }
    assert(pale&&shoulder&&outer&&Near(maximum,.34f));
    assert(f.Coverage(1.06f,.015f)==1&&f.Coverage(-1.045f,.081f)==1&&f.Coverage(.317f,.008f)==1);
    assert(f.Coverage(1.5f,.05f)==0);
    std::cout<<"PASS: pale HDR core, colored shoulder, finite wide halo, connected non-overlapping round caps.\n";
}

void TestClosedJoinsAndCameraPlane() {
    Fixture f;
    f.renderer.QueueContourRectangle({0,0,.02f},{2,2,1},.2f,kGreen);
    assert(f.renderer.vertexCount_>=672&&f.renderer.vertexCount_<=840);
    f.Finite();
    for(auto sample:{cg2::Vector2{.947f,.988f},cg2::Vector2{1.026f,1.013f},cg2::Vector2{1.091f,1.147f},
                     cg2::Vector2{.741f,.692f},cg2::Vector2{-.947f,-.988f},cg2::Vector2{.123f,1.03f}})
        assert(f.Coverage(sample.x,sample.y)==1);
    assert(f.Coverage(0,0)==0&&f.Coverage(1.5f,1.5f)==0);
    // The closing span ends at exactly the first cross-section, including every
    // profile boundary. No separate end cap or duplicate corner disc is needed.
    const uint32_t last=f.renderer.vertexCount_-42;
    for(uint32_t offset=0;offset<7;++offset) {
        const auto a=f.vertices[offset*6],b=f.vertices[last+offset*6+2];
        assert(Same(a,b));
    }
    f.renderer.BeginFrame();
    f.renderer.QueueBillboardContourTriangle({4,5,6},1.0f,.4f,.08f,kGreen,{0,0,1},{0,1,0},{-1,0,0});
    assert(f.renderer.vertexCount_>0);
    f.Finite();
    for(uint32_t i=0;i<f.renderer.vertexCount_;++i) assert(Near(f.vertices[i].pos.x,4));
    f.renderer.BeginFrame();
    f.renderer.QueueBillboardContourRectangle({2,3,4},{2,1},.3f,.09f,kGreen,{1,0,0},{0,1,0},{0,0,0});
    f.Finite();assert(f.renderer.vertexCount_>0);
    std::cout<<"PASS: shared closed seam, round corners without gaps / double emission, rotated billboard plane and forward fallback.\n";
}

void TestStyleIsolationCapacityAndInvalidInput() {
    Fixture f;
    f.renderer.SetLineStyle(.61f,2.1f);
    f.renderer.QueueLine({0,0,0},{1,0,0},.1f,kGreen);
    const auto old=std::vector<cg2::TrailVertex>(f.vertices.begin(),f.vertices.begin()+18);
    assert(f.renderer.vertexCount_==18);
    f.renderer.QueueContourLine({0,2,0},{1,2,0},.1f,kGreen,kForward);
    const uint32_t after=f.renderer.vertexCount_;
    f.renderer.QueueLine({0,0,0},{1,0,0},.1f,kGreen);
    for(uint32_t i=0;i<18;++i) assert(Same(old[i],f.vertices[after+i]));
    assert(Near(f.renderer.lineSoftEdgeRatio_,.61f)&&Near(f.renderer.lineCoreIntensity_,2.1f));
    f.renderer.vertexCount_=cg2::NeonGridRenderer::kMaxVertices-100;
    const uint32_t full=f.renderer.vertexCount_;
    const auto sentinel=f.vertices[full];
    f.renderer.QueueContourLine({0,0,0},{1,0,0},.1f,kGreen,kForward);
    f.renderer.QueueContourRectangle({0,0,0},{2,2,1},.1f,kGreen);
    assert(f.renderer.vertexCount_==full&&Same(sentinel,f.vertices[full]));
    f.renderer.BeginFrame();
    const float nan=std::numeric_limits<float>::quiet_NaN(),infinity=std::numeric_limits<float>::infinity();
    cg2::NeonContourStyle bad{};bad.coreIntensity=nan;bad.haloWidthScale=infinity;bad.coreWhiteMix=-3;bad.haloAlpha=12;
    bad.coreWidthRatio=100;
    cg2::ContourProfile boundedProfile;
    assert(cg2::MakeContourProfile(.1f,kGreen,bad,boundedProfile));
    assert(std::is_sorted(boundedProfile.radii.begin(),boundedProfile.radii.end()));
    f.renderer.QueueContourLine({0,0,0},{1,0,0},.1f,kGreen,{nan,0,0},bad);
    f.Finite();assert(f.renderer.vertexCount_==378);
    f.renderer.BeginFrame();
    const Vector3 duplicate[]{{0,0,0},{1,0,0},{1,0,0}};
    const Vector3 folded[]{{0,0,0},{1,0,0},{2,0,0}};
    f.renderer.QueueContourPolygon(nullptr,3,.1f,kGreen,kForward);
    f.renderer.QueueContourPolygon(duplicate,3,.1f,kGreen,kForward);
    f.renderer.QueueContourPolygon(folded,3,.1f,kGreen,kForward);
    f.renderer.QueueContourPolygon(duplicate,2,.1f,kGreen,kForward);
    f.renderer.QueueContourPolygon(duplicate,97,.1f,kGreen,kForward);
    f.renderer.QueueContourLine({0,0,0},{0,0,1},.1f,kGreen,kForward);
    f.renderer.QueueContourLine({nan,0,0},{1,0,0},.1f,kGreen,kForward);
    f.renderer.QueueContourLine({0,0,0},{1,0,0},infinity,kGreen,kForward);
    f.renderer.QueueContourLine({0,0,0},{1,0,0},.1f,{1,1,1,nan},kForward);
    f.renderer.QueueContourRectangle({0,0,0},{2,2,1},0,kGreen);
    assert(f.renderer.vertexCount_==0);
    // Repeated insertion stops at a complete primitive and leaves guard memory.
    const auto guard=f.vertices[cg2::NeonGridRenderer::kMaxVertices];
    for(uint32_t i=0;i<cg2::NeonGridRenderer::kMaxVertices/378+100;++i)
        f.renderer.QueueContourLine({0,0,0},{1,0,0},.1f,kGreen,kForward);
    assert(f.renderer.vertexCount_==(cg2::NeonGridRenderer::kMaxVertices/378)*378);
    assert(Same(guard,f.vertices[cg2::NeonGridRenderer::kMaxVertices]));f.Finite();
    std::cout<<"PASS: legacy line-style isolation, whole-primitive capacity, finite style bounds, invalid / degenerate input and guards.\n";
}

void TestRoomBudget() {
    // Random spawn paths cap at 45; authored rooms legally allow 128 (tested
    // separately below). Default CSV has at most 115 visible blocks under
    // the 38x24 + 3 margin cull; five near-facing cube faces need at most 20
    // subdivision lines each. Circle+barrel is deliberately heavier than the
    // usual mix of triangular / rectangular / 3D wire enemies.
    auto room=[](int enemies,int blocks,bool localGrids) {
        Fixture f;
        f.renderer.QueueWorldGrid(0,88,0,58,2,.075f,{.15f,.35f,.7f,.2f});
        if(localGrids) {
            f.renderer.QueueLocalGrid({40,30,0},5.4f,1,.1f,kGreen);
            f.renderer.QueueLocalGrid({40,30,0},5.4f*1.15f,1,.1f,kGreen);
            for(int i=0;i<18;++i) f.renderer.QueueLocalGrid({40,30,0},5.4f*.6f,1,.08f,kGreen);
        }
        std::array<Vector3,28> circle{};
        for(uint32_t i=0;i<circle.size();++i) {
            const float angle=static_cast<float>(i)*6.28318530718f/static_cast<float>(circle.size());
            circle[i]={std::cos(angle),std::sin(angle),0};
        }
        const uint32_t backdrop=f.renderer.vertexCount_;
        for(int i=0;i<enemies;++i) {
            const uint32_t before=f.renderer.vertexCount_;
            f.renderer.QueueContourPolygon(circle.data(),static_cast<uint32_t>(circle.size()),.08f,kGreen,kForward);
            f.renderer.QueueContourRectangle({0,0,0},{1,.4f,1},.08f,kGreen);
            assert(f.renderer.vertexCount_-before==1176+672);
        }
        const uint32_t actors=f.renderer.vertexCount_-backdrop;
        f.renderer.QueueContourPolygon(circle.data(),static_cast<uint32_t>(circle.size()),.08f,kGreen,kForward);
        for(int i=0;i<3;++i) {
            f.renderer.QueueContourRectangle({0,0,0},{1,.4f,1},.08f,kGreen);
            f.renderer.QueueContourLine({0,0,0},{1,0,0},.08f,kGreen,kForward);
        }
        const uint32_t beforeBlocks=f.renderer.vertexCount_;
        cg2::NeonContourStyle capless;capless.roundCaps=false;
        uint32_t linesAdded=0;
        for(int block=0;block<blocks;++block) for(int line=0;line<20;++line) {
            const uint32_t before=f.renderer.vertexCount_;
            f.renderer.QueueContourLine({-1,0,0},{1,0,0},.085f,kGreen,kForward,capless);
            if(f.renderer.vertexCount_>before) {assert(f.renderer.vertexCount_-before==42);++linesAdded;}
            else assert(cg2::NeonGridRenderer::kMaxVertices-before<42);
        }
        f.Finite();
        assert(f.renderer.vertexCount_-beforeBlocks==linesAdded*42);
        assert(linesAdded==static_cast<uint32_t>(blocks*20));
        std::cout<<"PASS: room budget enemies="<<enemies<<", blocks="<<blocks<<", localGrids="<<localGrids
            <<", background="<<backdrop<<", actors="<<actors<<", total="<<f.renderer.vertexCount_
            <<"/"<<cg2::NeonGridRenderer::kMaxVertices<<", blockLines="<<linesAdded<<"/"<<blocks*20<<".\n";
    };
    room(20,90,false);
    room(45,115,false);
    room(45,115,true); // Extra legacy local grids still leave every visible block line complete.
}

void TestAuthoredSupportRoomBudget() {
    Fixture f;
    f.renderer.QueueWorldGrid(0,88,0,58,2,.075f,{.15f,.35f,.7f,.2f});
    f.renderer.QueueLocalGrid({40,30,0},5.4f,1,.1f,kGreen);
    f.renderer.QueueLocalGrid({40,30,0},5.4f*1.15f,1,.1f,kGreen);
    for(int i=0;i<18;++i) f.renderer.QueueLocalGrid({40,30,0},5.4f*.6f,1,.08f,kGreen);
    const uint32_t background=f.renderer.vertexCount_;
    constexpr float tau=6.283185307f;
    std::array<Vector3,6> support{};
    for(uint32_t i=0;i<support.size();++i) {
        const float angle=tau*static_cast<float>(i)/static_cast<float>(support.size());
        support[i]={std::cos(angle)*.9f,std::sin(angle)*.9f,0};
    }
    uint32_t supportVertices=0;
    for(int actor=0;actor<128;++actor) {
        const uint32_t before=f.renderer.vertexCount_;
        f.renderer.QueueContourPolygon(support.data(),6,.08f,kGreen,kForward);
        f.renderer.QueueBeveledPolygonFill(support.data(),6,{.006f,.01f,.016f,.96f},kGreen,{-.4472f,.8944f,0});
        for(int spoke=0;spoke<6;++spoke) {
            const float angle=tau*static_cast<float>(spoke)/6.0f;
            const Vector3 radial{std::cos(angle),std::sin(angle),0};
            f.renderer.QueueContourLine(radial*.55f,radial*1.4f,.08f,kGreen,kForward);
        }
        // Summoner warning draws 32 outer + 32 growing legacy ring segments,
        // plus three two-line cross markers. Preserve these telegraph costs.
        for(int line=0;line<70;++line) f.renderer.QueueCameraFacingLine({0,0,0},{1,0,0},.08f,kGreen,kForward);
        const uint32_t added=f.renderer.vertexCount_-before;
        if(actor==0) supportVertices=added;
        assert(added==supportVertices&&supportVertices>6*378+70*18);
    }
    const uint32_t actors=f.renderer.vertexCount_-background;
    // Player silhouette, three barrels and three independent short glyphs.
    std::array<Vector3,28> circle{};
    for(uint32_t i=0;i<circle.size();++i) {
        const float angle=tau*static_cast<float>(i)/static_cast<float>(circle.size());
        circle[i]={std::cos(angle),std::sin(angle),0};
    }
    f.renderer.QueueContourPolygon(circle.data(),28,.08f,kGreen,kForward);
    for(int barrel=0;barrel<3;++barrel) {
        f.renderer.QueueContourRectangle({0,0,0},{1,.4f,1},.08f,kGreen);
        f.renderer.QueueContourLine({0,0,0},{1,0,0},.08f,kGreen,kForward);
    }
    const uint32_t beforeBlocks=f.renderer.vertexCount_;
    cg2::NeonContourStyle capless;capless.roundCaps=false;
    for(int block=0;block<115;++block) for(int line=0;line<20;++line) {
        const uint32_t before=f.renderer.vertexCount_;
        f.renderer.QueueContourLine({-1,0,0},{1,0,0},.085f,kGreen,kForward,capless);
        assert(f.renderer.vertexCount_-before==42);
    }
    assert(f.renderer.vertexCount_-beforeBlocks==115*20*42);
    const uint32_t beforeBurst=f.renderer.vertexCount_;
    uint32_t effectVertices=0;
    for(int effect=0;effect<60;++effect) {
        const uint32_t before=f.renderer.vertexCount_;
        f.renderer.QueueBillboardContourTriangle({0,0,0},.4f,0,.08f,kGreen,{1,0,0},{0,1,0},kForward);
        for(int trail=0;trail<3;++trail)
            f.renderer.QueueBillboardTriangle({0,0,0},.4f,0,.08f,kGreen,{1,0,0},{0,1,0},kForward);
        const uint32_t added=f.renderer.vertexCount_-before;
        if(effect==0) effectVertices=added;
        assert(added==effectVertices&&effectVertices>=630+54*3);
    }
    assert(f.renderer.vertexCount_-beforeBurst==60*effectVertices);
    f.Finite();
    std::cout<<"PASS: authored worst support room 128 actors + six capped spokes + 70 warning lines each: background="<<background
        <<", perSupport="<<supportVertices<<", actors="<<actors<<", blockLines=2300/2300, burst60="<<60*effectVertices
        <<", total="<<f.renderer.vertexCount_<<"/"<<cg2::NeonGridRenderer::kMaxVertices<<"; all primitives complete.\n";
    // Summons share an enemies_.size()<128 global gate and a 24-live-summon
    // gate. Replacing 24 of these high-cost support units with summoned Charger
    // triangles + two capped details only reduces this conservative total.
    Fixture charger;
    const Vector3 chargerBody[]{{1.18f,0,0},{-.72f,.82f,0},{-.72f,-.82f,0}};
    charger.renderer.QueueContourPolygon(chargerBody,3,.08f,kGreen,kForward);
    charger.renderer.QueueContourLine({0,-.42f,0},{.48f,0,0},.08f,kGreen,kForward);
    charger.renderer.QueueContourLine({.48f,0,0},{0,.42f,0},.08f,kGreen,kForward);
    assert(charger.renderer.vertexCount_<supportVertices);
    assert(f.renderer.vertexCount_-24*supportVertices+24*charger.renderer.vertexCount_<cg2::NeonGridRenderer::kMaxVertices);
    std::cout<<"PASS: 104 authored support units + global maximum 24 Charger summons are below the 128-support stress budget.\n";
}

void TestBeveledFaces() {
    Fixture f;
    const Vector3 square[]{{-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0}};
    f.renderer.QueueBeveledPolygonFill(square,4,{.006f,.01f,.016f,.96f},kGreen,{0,1,0});
    assert(f.renderer.vertexCount_==36);
    f.Finite();
    assert(f.Coverage(.23f,.44f)==1&&f.Coverage(.32f,.955f)==1);
    assert(f.Coverage(1.04f,.32f)==0);
    assert(f.vertices[2*9+3].color.y>f.vertices[3].color.y);
    f.renderer.vertexCount_=cg2::NeonGridRenderer::kMaxVertices-30;
    f.renderer.QueueBeveledPolygonFill(square,4,{.006f,.01f,.016f,.96f},kGreen,{0,1,0});
    assert(f.renderer.vertexCount_==cg2::NeonGridRenderer::kMaxVertices-30);
    std::cout<<"PASS: dark face and directional bevel coverage, complete-primitive capacity guard.\n";
}
void TestCombatParticlePresentation() {
    for(uint32_t kind=0;kind<=5;++kind) {
        const auto shape=static_cast<cg2::NeonParticleShape>(kind);
        assert(Near(cg2::NeonParticleFade(0,shape),0)&&Near(cg2::NeonParticleFade(1,shape),1));
        float previous=0;
        for(int i=0;i<=100;++i) {
            const float fade=cg2::NeonParticleFade(i/100.0f,shape);
            assert(fade>=previous&&fade<=1); previous=fade;
        }
    }
    assert(Near(cg2::NeonParticleFade(.20f,cg2::NeonParticleShape::Shard),0));
    assert(cg2::NeonParticleFade(.50f,cg2::NeonParticleShape::Shard)<.25f);
    assert(cg2::NeonParticleDrag(cg2::NeonParticleShape::Spark)==0);
    assert(cg2::NeonParticleDrag(cg2::NeonParticleShape::Shard)>0);
    Fixture f;
    for(auto shape:{cg2::NeonParticleShape::Shard,cg2::NeonParticleShape::Sliver}) {
        std::array<std::array<float,2>,4> local{};
        const auto count=cg2::NeonParticlePoints(shape,local);
        Vector3 points[4]{};
        for(uint32_t i=0;i<count;++i) {
            assert(local[i][0]>=-1.22f+.24f&&local[i][0]<=1.22f-.24f);
            assert(local[i][1]>=-.86f+.24f&&local[i][1]<=1.36f-.24f);
            points[i]={local[i][0],local[i][1],0};
        }
        f.renderer.BeginFrame();
        f.renderer.QueueContourPolygon(points,count,.03f,kGreen,kForward,cg2::ActorNeonContourStyle());
        f.Finite(); assert(f.renderer.vertexCount_>0);
    }
    std::cout<<"PASS: combat silhouette padding, finite rotated shard geometry, monotonic fade with color hold, debris-only drag.\n";
}
int main() {TestProfileAndCaps();TestClosedJoinsAndCameraPlane();TestStyleIsolationCapacityAndInvalidInput();TestRoomBudget();TestAuthoredSupportRoomBudget();TestBeveledFaces();TestCombatParticlePresentation();}
