#include "ComponentRegistry.h"
#include "EcsComponents.h"
#include "LogWrite.h"
#include "Struct.h"

#include <utility>

namespace myEngine {

namespace {

/// @brief 登録の失敗を診断ログへ出力する。
void LogRegistryError(const std::string& message)
{
    LogWrite().Log("[ComponentRegistry] " + message + "\n");
}

/// @brief std::string を、flecs のリフレクションでは文字列として扱えるようにする。
void RegisterStdString(flecs::world& world)
{
    world.component<std::string>()
        .opaque(flecs::String)
        .serialize([](const flecs::serializer* serializer, const std::string* data) {
            const char* text = data->c_str();
            return serializer->value(flecs::String, &text);
        })
        .assign_string([](std::string* data, const char* value) {
            *data = value;
        });
}

} // namespace

bool ComponentRegistry::Add(Info info)
{
    if (info.name.empty()) {
        LogRegistryError("Registration rejected because the component name is empty.");
        return false;
    }
    if (indexByName_.contains(info.name)) {
        LogRegistryError("Duplicate component name rejected: '" + info.name + "'.");
        return false;
    }
    if (indexById_.contains(info.id)) {
        LogRegistryError("Component type is already registered under another name: '" + info.name + "'.");
        return false;
    }
    const size_t index = infos_.size();
    indexByName_.emplace(info.name, index);
    indexById_.emplace(info.id, index);
    infos_.push_back(std::move(info));
    return true;
}

const ComponentRegistry::Info* ComponentRegistry::FindByName(std::string_view name) const
{
    const auto found = indexByName_.find(std::string(name));
    return found == indexByName_.end() ? nullptr : &infos_[found->second];
}

const ComponentRegistry::Info* ComponentRegistry::FindById(flecs::entity_t id) const
{
    const auto found = indexById_.find(id);
    return found == indexById_.end() ? nullptr : &infos_[found->second];
}

void RegisterEngineComponents(ComponentRegistry& registry)
{
    flecs::world& world = registry.GetWorld();
    RegisterStdString(world);

    world.component<Vector2>("myEngine::Vector2").member<float>("x").member<float>("y");
    world.component<Vector3>("myEngine::Vector3").member<float>("x").member<float>("y").member<float>("z");
    world.component<Vector4>("myEngine::Vector4").member<float>("x").member<float>("y").member<float>("z").member<float>("w");

    world.component<EntityGuid>("myEngine::EntityGuid").member<uint64_t>("high").member<uint64_t>("low");

    world.component<Transform>("myEngine::Transform").member<Vector3>("scale").member<Vector3>("rotate").member<Vector3>("translate");
    world.component<MeshRenderer>("myEngine::MeshRenderer").member<std::string>("model").member<Vector4>("color");
    world.component<CameraComponent>("myEngine::CameraComponent")
        .member<float>("fovY")
        .member<float>("nearClip")
        .member<float>("farClip")
        .member<bool>("primary");

    // Transform は集成体で既定値を持たないので、拡大率1の値を既定にする。
    registry.Register<Transform>("Transform", Transform{{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}});
    registry.Register<MeshRenderer>("MeshRenderer");
    registry.Register<CameraComponent>("CameraComponent");
}

} // namespace myEngine
