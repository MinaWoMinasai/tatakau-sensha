// Execute the production TrailInstance/TrailManager classes and method bodies
// against a recording DirectX adapter. No geometry or cache implementation is
// copied into the adapters.
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace cg2 {
struct Vector2 { float x, y; };
struct Vector3 { float x, y, z; };
struct Vector4 { float x, y, z, w; };
Vector3 operator+(Vector3 a, Vector3 b) { return { a.x+b.x, a.y+b.y, a.z+b.z }; }
Vector3 operator-(Vector3 a, Vector3 b) { return { a.x-b.x, a.y-b.y, a.z-b.z }; }
Vector3 operator-(Vector3 a) { return { -a.x, -a.y, -a.z }; }
Vector3 operator*(Vector3 a, float b) { return { a.x*b, a.y*b, a.z*b }; }
Vector4 operator+(Vector4 a, Vector4 b) { return { a.x+b.x, a.y+b.y, a.z+b.z, a.w+b.w }; }
Vector4 operator-(Vector4 a, Vector4 b) { return { a.x-b.x, a.y-b.y, a.z-b.z, a.w-b.w }; }
Vector4 operator*(Vector4 a, float b) { return { a.x*b, a.y*b, a.z*b, a.w*b }; }
struct Matrix4x4 { float m[4][4]{}; };
struct TrailVertex { Vector3 pos; Vector4 color; Vector2 uv; };
struct Material { float shininess = 0; };
Material MakeDefaultMaterial() { return {}; }
} // namespace cg2
using namespace cg2;

using DWORD = uint32_t;
using D3D12_GPU_VIRTUAL_ADDRESS = uint64_t;
struct D3D12_RANGE { size_t Begin, End; };
struct D3D12_VERTEX_BUFFER_VIEW { D3D12_GPU_VIRTUAL_ADDRESS BufferLocation; size_t SizeInBytes, StrideInBytes; };
constexpr uint32_t D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP = 5;
bool testDisableBatching = false;
DWORD GetEnvironmentVariableA(const char*, char* buffer, DWORD) {
    if (!testDisableBatching) return 0;
    buffer[0] = '0'; buffer[1] = 0; return 1;
}
struct ID3D12Resource {
    explicit ID3D12Resource(size_t size) : bytes(size) {}
    std::vector<uint8_t> bytes;
    D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() { return reinterpret_cast<uintptr_t>(this); }
    void Map(uint32_t, const D3D12_RANGE*, void** target) { *target = bytes.data(); }
};
namespace Microsoft::WRL {
template<class T> class ComPtr {
public:
    ComPtr() = default;
    explicit ComPtr(std::shared_ptr<T> value) : value_(std::move(value)) {}
    T* Get() const { return value_.get(); }
    T* operator->() const { return value_.get(); }
    explicit operator bool() const { return static_cast<bool>(value_); }
private:
    std::shared_ptr<T> value_;
};
}
struct FakeHandle { void* Get() { return nullptr; } };
struct FakeRoot { FakeHandle GetSignature() { return {}; } };
struct FakePso { FakeRoot root_; FakeHandle graphicsState_; };
struct FakeFence { uint64_t completed = 0; uint64_t GetCompletedValue() { return completed; } };
struct FakeCommandList {
    struct Draw { uint32_t vertices, first; uint64_t vertexAddress, matrixAddress; };
    std::vector<Draw> draws;
    uint64_t vertexAddress = 0, matrixAddress = 0;
    void SetGraphicsRootSignature(void*) {}
    void SetPipelineState(void*) {}
    void IASetPrimitiveTopology(uint32_t) {}
    void IASetVertexBuffers(uint32_t, uint32_t, const D3D12_VERTEX_BUFFER_VIEW* view) { vertexAddress = view->BufferLocation; }
    void SetGraphicsRootConstantBufferView(uint32_t slot, uint64_t address) { if (slot == 1) matrixAddress = address; }
    void SetGraphicsRootDescriptorTable(uint32_t, uint64_t) {}
    void DrawInstanced(uint32_t vertices, uint32_t, uint32_t first, uint32_t) { draws.push_back({ vertices, first, vertexAddress, matrixAddress }); }
};
namespace cg2 {
struct DirectXCommon {
    FakeFence fence;
    FakeCommandList list;
    FakePso pso;
    uint64_t fenceValue = 0;
    size_t allocations = 0;
    std::vector<std::weak_ptr<ID3D12Resource>> resources;
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t bytes) {
        ++allocations;
        auto resource = std::make_shared<ID3D12Resource>(bytes);
        resources.push_back(resource);
        return Microsoft::WRL::ComPtr<ID3D12Resource>(resource);
    }
    FakeFence* GetFence() { return &fence; }
    uint64_t GetFenceValue() { return fenceValue; }
    FakeCommandList* GetList() { return &list; }
    FakePso& GetPSOTrailForScene() { return pso; }
    void CompleteFrame() { fence.completed = ++fenceValue; list.draws.clear(); }
};
struct Object3dCommon {};
struct TextureManager {
    static TextureManager* GetInstance() { static TextureManager instance; return &instance; }
    uint64_t GetSrvHandleGPU(const std::string&) { return 0; }
};
} // namespace cg2

#include "trail_production_declarations.inc"
#include "trail_production_methods.inc"

const TrailVertex* DrawVertices(const FakeCommandList::Draw& draw) {
    const auto* resource = reinterpret_cast<const ID3D12Resource*>(static_cast<uintptr_t>(draw.vertexAddress));
    return reinterpret_cast<const TrailVertex*>(resource->bytes.data()) + draw.first;
}
const Matrix4x4& DrawMatrix(const FakeCommandList::Draw& draw) {
    const auto* resource = reinterpret_cast<const ID3D12Resource*>(static_cast<uintptr_t>(draw.matrixAddress));
    return *reinterpret_cast<const Matrix4x4*>(resource->bytes.data());
}
bool Equal(Vector3 a, Vector3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
bool Equal(const TrailVertex& a, const TrailVertex& b) {
    return Equal(a.pos,b.pos) && a.color.x==b.color.x && a.color.y==b.color.y &&
        a.color.z==b.color.z && a.color.w==b.color.w && a.uv.x==b.uv.x && a.uv.y==b.uv.y;
}
void Near(float actual, float expected) {
    assert(std::abs(actual-expected) <= 0.00006f * (std::max)(1.0f,std::abs(expected)));
}
void Near(const TrailVertex& a, const TrailVertex& b) {
    Near(a.pos.x,b.pos.x); Near(a.pos.y,b.pos.y); Near(a.pos.z,b.pos.z);
    Near(a.color.x,b.color.x); Near(a.color.y,b.color.y); Near(a.color.z,b.color.z); Near(a.color.w,b.color.w);
    Near(a.uv.x,b.uv.x); Near(a.uv.y,b.uv.y);
}

// Independent polynomial reference matches the original trail's sampling,
// width, color, UV, and end-point conventions, with no cap for this oracle.
std::vector<TrailVertex> ReferenceStrip(const TrailInstance& instance) {
    std::vector<TrailVertex> vertices;
    const auto& p = instance.GetPoints();
    if (p.size() < 4) return vertices;
    const auto& c = instance.GetConfig();
    const size_t steps = (std::max)(1u, c.interpolationSteps);
    for (size_t i = 0; i + 1 < p.size(); ++i) {
        const size_t i0 = i == 0 ? 0 : i-1, i3 = (std::min)(i+2,p.size()-1);
        for (size_t j = 0; j < steps; ++j) {
            const float t = static_cast<float>(j) / static_cast<float>(steps);
            const float ratio = static_cast<float>(i*steps+j) / static_cast<float>((p.size()-1)*steps);
            const float cr = std::pow(ratio,(std::max)(0.05f,c.colorCurvePower));
            const float wr = std::pow(ratio,(std::max)(0.05f,c.widthCurvePower));
            const Vector4 color = c.startColor + (c.endColor-c.startColor)*cr;
            const Vector3 tip = TrailManager::CatmullRom(p[i0].tip,p[i].tip,p[i+1].tip,p[i3].tip,t);
            const Vector3 base = TrailManager::CatmullRom(p[i0].base,p[i].base,p[i+1].base,p[i3].base,t);
            const Vector3 center = (tip+base)*0.5f;
            const Vector3 half = (tip-base)*(0.5f*(c.startWidthScale+(c.endWidthScale-c.startWidthScale)*wr));
            vertices.push_back({center+half,color,{ratio,0}});
            vertices.push_back({center-half,color,{ratio,1}});
        }
    }
    return vertices;
}
using Triangle = std::array<TrailVertex,3>;
std::vector<Triangle> Triangles(const TrailVertex* vertices, size_t count) {
    std::vector<Triangle> result;
    for (size_t i=2; i<count; ++i) {
        Triangle t = i%2==0 ? Triangle{vertices[i-2],vertices[i-1],vertices[i]} : Triangle{vertices[i-1],vertices[i-2],vertices[i]};
        if (Equal(t[0].pos,t[1].pos) || Equal(t[0].pos,t[2].pos) || Equal(t[1].pos,t[2].pos)) continue;
        result.push_back(t);
    }
    return result;
}
void Populate(TrailInstance& trail, int points, int seed, TrailConfig config = {}) {
    config.lifetime = 100;
    config.maxPoints = 100;
    for (int i=0; i<points; ++i) {
        const float x = static_cast<float>(i)*0.7f+static_cast<float>(seed)*3;
        const float y = std::sin(x*0.6f)*2;
        trail.Update(0.01f,{x,y+0.5f,static_cast<float>(seed)},{x,y-0.5f,static_cast<float>(seed)},config);
    }
}

void VerifyGeometryAndBatching() {
    DirectXCommon dxBatch, dxSeparate;
    TrailManager batched, separate;
    testDisableBatching = false; batched.Initialize(&dxBatch,nullptr,"");
    testDisableBatching = true; separate.Initialize(&dxSeparate,nullptr,"");
    testDisableBatching = false;
    std::vector<TrailInstance*> reference;
    for (int i=0; i<128; ++i) {
        TrailConfig config;
        config.interpolationSteps = static_cast<uint32_t>(i%9);
        config.colorCurvePower = (i%3==0) ? 1 : 0.4f+static_cast<float>(i%7)*0.3f;
        config.widthCurvePower = (i%4==0) ? 1 : 0.1f+static_cast<float>(i%5)*0.4f;
        config.startWidthScale = 0.6f+static_cast<float>(i%5)*0.2f;
        config.endWidthScale = 0.1f;
        config.startColor = {0.3f,0.6f,1,0.8f}; config.endColor = {1,0.1f,0.6f,0.1f};
        auto* a = batched.CreateInstance(); auto* b = separate.CreateInstance();
        Populate(*a,i%31,i,config); Populate(*b,i%31,i,config);
        reference.push_back(b);
    }
    batched.DrawAll({}); separate.DrawAll({});
    assert(batched.GetDrawStats().drawCalls==1);
    assert(batched.GetDrawStats().generatedVertices>8192);
    assert(batched.GetDrawStats().truncatedVertices==0);
    assert(batched.GetDrawStats().generatedVertices==separate.GetDrawStats().generatedVertices);
    const auto combined = Triangles(DrawVertices(dxBatch.list.draws.front()),dxBatch.list.draws.front().vertices);
    std::vector<Triangle> original;
    size_t drawIndex = 0;
    for (auto* trail : reference) {
        const auto expected = ReferenceStrip(*trail);
        if (expected.empty()) continue;
        const auto& draw = dxSeparate.list.draws.at(drawIndex++);
        assert(expected.size()==draw.vertices);
        const auto* actual = DrawVertices(draw);
        for (size_t v=0; v<expected.size(); ++v) Near(actual[v],expected[v]);
        const auto triangles = Triangles(actual,draw.vertices);
        original.insert(original.end(),triangles.begin(),triangles.end());
    }
    assert(drawIndex==separate.GetDrawStats().drawCalls);
    assert(combined.size()==original.size());
    for (size_t i=0; i<combined.size(); ++i) for (size_t v=0; v<3; ++v) assert(Equal(combined[i][v],original[i][v]));
    std::cout << "Geometry: " << batched.GetDrawStats().generatedVertices << " unchanged vertices, "
        << separate.GetDrawStats().drawCalls << " draws -> 1, triangle winding/attributes exact.\n";
}

void VerifyCacheAndResourceLifetimes() {
    DirectXCommon dx;
    TrailManager manager;
    manager.Initialize(&dx,nullptr,"");
    auto* trail=manager.CreateInstance(); Populate(*trail,12,1);
    Matrix4x4 firstMatrix{}; firstMatrix.m[0][0]=1;
    manager.DrawAll(firstMatrix);
    assert(manager.GetDrawStats().geometryRebuilt);
    assert(manager.GetDrawStats().uploadedBytes>sizeof(Matrix4x4));
    const auto firstDraw=dx.list.draws.back();
    const auto* firstData=DrawVertices(firstDraw);
    const std::vector<TrailVertex> saved(firstData,firstData+firstDraw.vertices);

    Matrix4x4 secondMatrix{}; secondMatrix.m[0][0]=2;
    manager.DrawAll(secondMatrix);
    assert(!manager.GetDrawStats().geometryRebuilt);
    assert(manager.GetDrawStats().uploadedBytes==sizeof(Matrix4x4));
    assert(dx.list.draws.back().vertexAddress==firstDraw.vertexAddress);
    assert(dx.list.draws.back().matrixAddress!=firstDraw.matrixAddress);
    assert(DrawMatrix(firstDraw).m[0][0]==1);
    assert(DrawMatrix(dx.list.draws.back()).m[0][0]==2);

    // A mutation before submission must keep the already-recorded VB alive.
    trail->Update(0.01f,{9,2,1},{9,1,1},trail->GetConfig());
    manager.DrawAll({});
    assert(manager.GetDrawStats().geometryRebuilt);
    assert(dx.list.draws.back().vertexAddress!=firstDraw.vertexAddress);
    for (size_t i=0; i<saved.size(); ++i) assert(Equal(DrawVertices(firstDraw)[i],saved[i]));
    const auto lastVertexAddress=dx.list.draws.back().vertexAddress;

    dx.CompleteFrame();
    trail->Update(0.01f,{10,2,1},{10,1,1},trail->GetConfig());
    const auto allocations=dx.allocations;
    manager.DrawAll({});
    assert(dx.list.draws.back().vertexAddress==lastVertexAddress);
    assert(dx.allocations==allocations);

    dx.CompleteFrame();
    trail->SetActive(false);
    manager.Update(0.1f);
    manager.DrawAll({});
    assert(!manager.GetDrawStats().geometryRebuilt); // ages alone do not affect mesh
    assert(manager.GetDrawStats().activeInstances==0);
    auto config=trail->GetConfig(); config.endColor.w=0.4f;
    trail->SetConfig(config); manager.DrawAll({});
    assert(manager.GetDrawStats().geometryRebuilt);
    trail->SetConfig(config); manager.DrawAll({});
    assert(!manager.GetDrawStats().geometryRebuilt);

    dx.CompleteFrame();
    trail->SetIsPermanent(true); manager.Update(200);
    manager.DrawAll({});
    assert(manager.GetDrawStats().geometryRebuilt);
    assert(manager.GetDrawStats().drawCalls==0);
    assert(manager.GetDrawStats().generatedVertices==0);
    trail->SetActive(true); Populate(*trail,5,2); manager.DrawAll({});
    assert(manager.GetDrawStats().drawCalls==1);
    trail->Clear(); manager.DrawAll({});
    assert(manager.GetDrawStats().drawCalls==0);

    dx.CompleteFrame();
    manager.ClearInstances();
    auto* replacement=manager.CreateInstance(); Populate(*replacement,5,3);
    manager.DrawAll({});
    assert(manager.GetDrawStats().geometryRebuilt);
    assert(manager.GetDrawStats().totalInstances==1);
    replacement->SetActive(false); manager.Update(200); manager.DrawAll({});
    assert(manager.GetDrawStats().totalInstances==0 && manager.GetDrawStats().drawCalls==0);
    std::cout << "Cache: repeated draw, ages, config, clear/reuse, expiry; immutable VB/CB until fence: passed.\n";
}

void VerifyBoundedGrowth() {
    DirectXCommon dx;
    TrailManager manager;
    manager.Initialize(&dx,nullptr,"");
    TrailConfig config; config.interpolationSteps=1000000;
    for (int i=0; i<4; ++i) Populate(*manager.CreateInstance(),8,i,config);
    manager.DrawAll({});
    const auto& stats=manager.GetDrawStats();
    assert(stats.capacityHit && stats.truncatedVertices>0);
    assert(stats.vertexCapacity==TrailManager::kMaxBatchVertices);
    assert(stats.submittedVertices<=TrailManager::kMaxBatchVertices);
    assert(stats.requestedVertices==stats.generatedVertices+stats.truncatedVertices);
    assert(stats.drawCalls==1);
    manager.ClearInstances(); manager.DrawAll({});
    assert(manager.GetDrawStats().drawCalls==0 && !manager.GetDrawStats().capacityHit);
    std::cout << "Capacity: grows beyond 8192, bounds pathological input and reports truncation: passed.\n";
}

int main() {
    VerifyGeometryAndBatching();
    VerifyCacheAndResourceLifetimes();
    VerifyBoundedGrowth();
    std::cout << "All production trail regression checks passed.\n";
}
