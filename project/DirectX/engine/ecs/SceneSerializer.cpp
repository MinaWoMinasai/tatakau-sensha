#include "SceneSerializer.h"
#include "EcsComponents.h"
#include "LogWrite.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace myEngine {

namespace {

using OrderedJson = nlohmann::ordered_json;

/// @brief 保存するエンティティの見出しと、並べ替えに使う情報。
struct EntityRecord {
    flecs::entity entity;
    std::string guid;
    std::string name;
    std::string parentGuid;
    int depth = 0;
};

/// @brief エンティティの表示名を返す（flecs の doc name。無ければ空）。
std::string GetDisplayName(flecs::entity entity)
{
    const char* name = entity.doc_name();
    return name ? std::string(name) : std::string();
}

/// @brief 保存の対象（EntityGuid を持つ）親をたどって、親の数を返す。
int CountSavedAncestors(flecs::entity entity)
{
    int depth = 0;
    for (flecs::entity parent = entity.parent(); parent && parent.has<EntityGuid>(); parent = parent.parent()) {
        ++depth;
    }
    return depth;
}

/// @brief 読み込みの警告を結果とログの両方へ残す。
void Warn(SceneLoadResult& result, const std::string& message)
{
    result.warnings.push_back(message);
    LogWrite().Log("[SceneSerializer] " + message + "\n");
}

} // namespace

std::string SceneSerializer::SaveToString() const
{
    flecs::world& world = registry_.GetWorld();

    std::vector<EntityRecord> records;
    world.each([&](flecs::entity entity, const EntityGuid& guid) {
        EntityRecord record;
        record.entity = entity;
        record.guid = guid.ToString();
        record.name = GetDisplayName(entity);
        const flecs::entity parent = entity.parent();
        if (parent && parent.has<EntityGuid>()) {
            record.parentGuid = parent.get<EntityGuid>().ToString();
        }
        record.depth = CountSavedAncestors(entity);
        records.push_back(std::move(record));
    });
    // 親が先に来るように深さ順、同じ深さは GUID 順にして、保存のたびに同じ並びになるようにする。
    std::sort(records.begin(), records.end(), [](const EntityRecord& a, const EntityRecord& b) {
        return a.depth != b.depth ? a.depth < b.depth : a.guid < b.guid;
    });

    OrderedJson entities = OrderedJson::array();
    for (const EntityRecord& record : records) {
        OrderedJson components = OrderedJson::object();
        for (const ComponentRegistry::Info& info : registry_.GetAll()) {
            if (!info.serializable || !record.entity.has(info.id)) {
                continue;
            }
            const void* value = ecs_get_id(world.c_ptr(), record.entity.id(), info.id);
            char* text = ecs_ptr_to_json(world.c_ptr(), info.id, value);
            if (!text) {
                LogWrite().Log("[SceneSerializer] Failed to serialize component '" + info.name + "' of entity " + record.guid + ".\n");
                continue;
            }
            components[info.name] = OrderedJson::parse(text, nullptr, false);
            ecs_os_free(text);
        }

        OrderedJson entry = OrderedJson::object();
        entry["guid"] = record.guid;
        entry["name"] = record.name;
        entry["parent"] = record.parentGuid.empty() ? OrderedJson(nullptr) : OrderedJson(record.parentGuid);
        entry["components"] = std::move(components);
        entities.push_back(std::move(entry));
    }

    OrderedJson document = OrderedJson::object();
    document["format"] = kFormat;
    document["version"] = kVersion;
    document["entities"] = std::move(entities);
    return document.dump(2) + "\n";
}

bool SceneSerializer::SaveToFile(const std::filesystem::path& path) const
{
    const std::string text = SaveToString();
    std::error_code error;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), error);
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(file);
}

SceneLoadResult SceneSerializer::LoadFromString(std::string_view json) const
{
    SceneLoadResult result;
    flecs::world& world = registry_.GetWorld();

    const OrderedJson document = OrderedJson::parse(json.begin(), json.end(), nullptr, false);
    if (document.is_discarded() || !document.is_object()) {
        Warn(result, "The scene is not valid JSON.");
        return result;
    }
    if (document.value("format", std::string()) != kFormat) {
        Warn(result, "Unknown scene format. Expected '" + std::string(kFormat) + "'.");
        return result;
    }
    const int version = document.value("version", 0);
    if (version < 1 || version > kVersion) {
        Warn(result, "Unsupported scene version " + std::to_string(version) + ".");
        return result;
    }
    const auto entitiesIt = document.find("entities");
    if (entitiesIt == document.end() || !entitiesIt->is_array()) {
        Warn(result, "The scene has no 'entities' array.");
        return result;
    }
    const OrderedJson& entries = *entitiesIt;

    // 1. すべてのエンティティを先に作り、GUID から引けるようにする（親子は後でつなぐ）。
    std::unordered_map<std::string, flecs::entity> entityByGuid;
    for (const OrderedJson& entry : entries) {
        const std::string guidText = entry.is_object() ? entry.value("guid", std::string()) : std::string();
        std::optional<EntityGuid> guid = EntityGuid::Parse(guidText);
        if (!guid || !guid->IsValid() || entityByGuid.contains(guid->ToString())) {
            const EntityGuid replacement = EntityGuid::Generate();
            Warn(result, "Entity has a missing, invalid or duplicate guid '" + guidText + "'. Assigned " + replacement.ToString() + ".");
            guid = replacement;
        }
        flecs::entity entity = world.entity();
        entity.set<EntityGuid>(*guid);
        const std::string name = entry.is_object() ? entry.value("name", std::string()) : std::string();
        if (!name.empty()) {
            entity.set_doc_name(name.c_str());
        }
        entityByGuid.emplace(guid->ToString(), entity);
        result.entities.push_back(entity);
    }

    // 2. 親子関係をつなぎ、コンポーネントの値を入れる。
    for (size_t index = 0; index < entries.size(); ++index) {
        const OrderedJson& entry = entries[index];
        flecs::entity entity = result.entities[index];
        const std::string guidText = entity.get<EntityGuid>().ToString();
        if (!entry.is_object()) {
            Warn(result, "Entity " + guidText + " is not a JSON object.");
            continue;
        }

        const auto parentIt = entry.find("parent");
        if (parentIt != entry.end() && parentIt->is_string()) {
            const auto parent = entityByGuid.find(parentIt->get<std::string>());
            if (parent == entityByGuid.end() || parent->second == entity) {
                Warn(result, "Entity " + guidText + " refers to a missing parent " + parentIt->get<std::string>() + ". Kept at the root.");
            } else {
                entity.child_of(parent->second);
            }
        }

        const auto componentsIt = entry.find("components");
        if (componentsIt == entry.end() || !componentsIt->is_object()) {
            continue;
        }
        for (const auto& [key, value] : componentsIt->items()) {
            const ComponentRegistry::Info* info = registry_.FindByName(key);
            if (!info) {
                Warn(result, "Entity " + guidText + " has an unknown component '" + key + "'. Skipped.");
                continue;
            }
            if (!info->serializable) {
                Warn(result, "Entity " + guidText + " has a component '" + key + "' that is not saved in scenes. Skipped.");
                continue;
            }
            info->addDefault(entity);
            void* target = ecs_ensure_id(world.c_ptr(), entity.id(), info->id, info->size);
            const std::string valueText = value.dump();
            ecs_from_json_desc_t desc{};
            desc.name = key.c_str();
            desc.expr = valueText.c_str();
            if (!ecs_ptr_from_json(world.c_ptr(), info->id, target, valueText.c_str(), &desc)) {
                Warn(result, "Entity " + guidText + " has an invalid value for component '" + key + "'. Default value kept.");
            }
            ecs_modified_id(world.c_ptr(), entity.id(), info->id);
        }
    }

    result.success = true;
    return result;
}

SceneLoadResult SceneSerializer::LoadFromFile(const std::filesystem::path& path) const
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        SceneLoadResult result;
        Warn(result, "Cannot open the scene file.");
        return result;
    }
    std::ostringstream text;
    text << file.rdbuf();
    return LoadFromString(text.str());
}

} // namespace myEngine
