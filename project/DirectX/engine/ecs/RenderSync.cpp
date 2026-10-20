#include "RenderSync.h"
#include "Calculation.h"
#include "EcsComponents.h"
#include "Object3d.h"
#include "WinApp.h"

#include <unordered_map>
#include <vector>

namespace myEngine {

RenderHandle::RenderHandle() = default;
RenderHandle::~RenderHandle() = default;
RenderHandle::RenderHandle(RenderHandle&&) noexcept = default;
RenderHandle& RenderHandle::operator=(RenderHandle&&) noexcept = default;

namespace {

/// @brief 親をたどって、エンティティのワールド行列を求める。同じフレーム内の計算結果は cache に残して使い回す。
Matrix4x4 ComputeWorldMatrix(flecs::entity entity, std::unordered_map<flecs::entity_t, Matrix4x4>& cache)
{
    const auto cached = cache.find(entity.id());
    if (cached != cache.end()) {
        return cached->second;
    }
    Matrix4x4 local = MakeIdentity4x4();
    if (const Transform* transform = entity.try_get<Transform>()) {
        local = MakeAffineMatrix(transform->scale, transform->rotate, transform->translate);
    }
    Matrix4x4 world = local;
    if (const flecs::entity parent = entity.parent()) {
        // 行ベクトルの規約なので、子のローカル行列に親のワールド行列を右から掛ける。
        world = Multiply(local, ComputeWorldMatrix(parent, cache));
    }
    cache.emplace(entity.id(), world);
    return world;
}

} // namespace

void RegisterRenderComponents(flecs::world& world)
{
    world.component<RenderHandle>("myEngine::RenderHandle");
}

RenderSync::RenderSync(flecs::world& world) : world_(world)
{
    camera_.SetAspectRatio(float(WinApp::kClientWidth) / float(WinApp::kClientHeight));
    camera_.Update();
}

void RenderSync::UpdateCamera()
{
    // primary の CameraComponent を持つ最初のエンティティを、シーンのカメラにする。
    // 最小版では、カメラの Transform をそのままワールドの値として使う（親の影響は受けない）。
    bool found = false;
    world_.each([&](flecs::entity entity, const CameraComponent& component) {
        if (found || !component.primary) {
            return;
        }
        found = true;
        camera_.SetFovY(component.fovY);
        camera_.SetNearClip(component.nearClip);
        camera_.SetFarClip(component.farClip);
        if (const Transform* transform = entity.try_get<Transform>()) {
            camera_.SetRotate(transform->rotate);
            camera_.SetTranslate(transform->translate);
        }
    });
    camera_.Update();
}

void RenderSync::Update()
{
    UpdateCamera();

    // MeshRenderer が外れたエンティティから描画の状態を取り除く（反復中は変更しないよう、先に集める）。
    std::vector<flecs::entity> orphaned;
    world_.each([&](flecs::entity entity, const RenderHandle&) {
        if (!entity.has<MeshRenderer>()) {
            orphaned.push_back(entity);
        }
    });
    for (flecs::entity entity : orphaned) {
        entity.remove<RenderHandle>();
    }

    // MeshRenderer を持つエンティティに描画の状態を用意する。
    std::vector<flecs::entity> pending;
    world_.each([&](flecs::entity entity, const MeshRenderer&) {
        if (!entity.has<RenderHandle>()) {
            pending.push_back(entity);
        }
    });
    for (flecs::entity entity : pending) {
        entity.set<RenderHandle>(RenderHandle{});
    }

    std::unordered_map<flecs::entity_t, Matrix4x4> worldMatrices;
    world_.each([&](flecs::entity entity, const MeshRenderer& renderer, RenderHandle& handle) {
        if (renderer.model.empty()) {
            handle.object.reset();
            handle.loadedModel.clear();
            return;
        }
        if (!handle.object) {
            handle.object = std::make_unique<Object3d>();
            handle.object->Initialize();
        }
        if (handle.loadedModel != renderer.model) {
            handle.object->SetModel(renderer.model);
            handle.loadedModel = renderer.model;
        }
        handle.object->SetCamera(&camera_);
        handle.object->SetColor(renderer.color);
        handle.object->UpdateWithWorldMatrix(ComputeWorldMatrix(entity, worldMatrices));
    });
}

void RenderSync::Draw()
{
    world_.each([](flecs::entity, const RenderHandle& handle) {
        if (handle.object) {
            handle.object->Draw();
        }
    });
}

} // namespace myEngine
