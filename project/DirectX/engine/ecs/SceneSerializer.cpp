#include "SceneSerializer.h"
#include "LogWrite.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace myEngine {

namespace {

using OrderedJson = nlohmann::ordered_json;

/// @brief エンティティの表示名を返す（flecs の doc name。無ければ空）。
std::string GetDisplayName(flecs::entity entity)
{
    const char* name = entity.doc_name();
    return name ? std::string(name) : std::string();
}

/// @brief 保存の対象（EntityGuid を持つ）親を返す。無ければ無効なエンティティ。
flecs::entity GetSavedParent(flecs::entity entity)
{
    const flecs::entity parent = entity.parent();
    return parent && parent.has<EntityGuid>() ? parent : flecs::entity();
}

/// @brief 保存の対象（EntityGuid を持つ）親をたどって、親の数を返す。
int CountSavedAncestors(flecs::entity entity)
{
    int depth = 0;
    for (flecs::entity parent = GetSavedParent(entity); parent; parent = GetSavedParent(parent)) {
        ++depth;
    }
    return depth;
}

/// @brief 警告を一覧とログの両方へ残す。
void Warn(std::vector<std::string>& warnings, const std::string& message)
{
    warnings.push_back(message);
    LogWrite().Log("[SceneSerializer] " + message + "\n");
}

} // namespace

EntityState SceneSerializer::CaptureEntity(flecs::entity entity) const
{
    flecs::world& world = registry_.GetWorld();
    EntityState state;
    state.guid = entity.get<EntityGuid>();
    state.name = GetDisplayName(entity);
    if (const flecs::entity parent = GetSavedParent(entity)) {
        state.parent = parent.get<EntityGuid>();
    }
    for (const ComponentRegistry::Info& info : registry_.GetAll()) {
        if (!info.serializable || !entity.has(info.id)) {
            continue;
        }
        const void* value = ecs_get_id(world.c_ptr(), entity.id(), info.id);
        char* text = ecs_ptr_to_json(world.c_ptr(), info.id, value);
        if (!text) {
            LogWrite().Log("[SceneSerializer] Failed to serialize component '" + info.name + "' of entity " + state.guid.ToString() +
                           ".\n");
            continue;
        }
        state.components[info.name] = OrderedJson::parse(text, nullptr, false);
        ecs_os_free(text);
    }
    return state;
}

OrderedJson SceneSerializer::ToJson(const EntityState& state)
{
    OrderedJson entry = OrderedJson::object();
    entry["guid"] = state.guid.ToString();
    entry["name"] = state.name;
    entry["parent"] = state.parent ? OrderedJson(state.parent->ToString()) : OrderedJson(nullptr);
    entry["components"] = state.components;
    return entry;
}

void SceneSerializer::ApplyEntityState(flecs::entity entity, const EntityState& state, const GuidLookup& findByGuid,
                                       std::vector<std::string>& warnings) const
{
    flecs::world& world = registry_.GetWorld();
    const std::string guidText = state.guid.ToString();

    // 名前。空の名前は、すでに名前が付いているときだけ空にする（新しいエンティティには doc name を付けない）。
    if (!state.name.empty() || entity.doc_name()) {
        entity.set_doc_name(state.name.c_str());
    }

    // 親。見つからない親や自分自身はルートに置く。
    flecs::entity parent;
    if (state.parent) {
        parent = findByGuid(*state.parent);
        if (!parent || parent == entity) {
            Warn(warnings, "Entity " + guidText + " refers to a missing parent " + state.parent->ToString() + ". Kept at the root.");
            parent = flecs::entity();
        }
    }
    if (parent) {
        if (entity.parent() != parent) {
            entity.child_of(parent);
        }
    } else if (GetSavedParent(entity)) {
        entity.remove(flecs::ChildOf, flecs::Wildcard);
    }

    // 保存対象のコンポーネントを state と同じにそろえる。値は既定値を入れてから JSON で上書きする（書かれていない項目は既定値になる）。
    for (const ComponentRegistry::Info& info : registry_.GetAll()) {
        if (info.serializable && entity.has(info.id) && !state.components.contains(info.name)) {
            entity.remove(info.id);
        }
    }
    for (const auto& [key, value] : state.components.items()) {
        const ComponentRegistry::Info* info = registry_.FindByName(key);
        if (!info) {
            Warn(warnings, "Entity " + guidText + " has an unknown component '" + key + "'. Skipped.");
            continue;
        }
        if (!info->serializable) {
            Warn(warnings, "Entity " + guidText + " has a component '" + key + "' that is not saved in scenes. Skipped.");
            continue;
        }
        info->addDefault(entity);
        void* target = ecs_ensure_id(world.c_ptr(), entity.id(), info->id, info->size);
        const std::string valueText = value.dump();
        ecs_from_json_desc_t desc{};
        desc.name = key.c_str();
        desc.expr = valueText.c_str();
        if (!ecs_ptr_from_json(world.c_ptr(), info->id, target, valueText.c_str(), &desc)) {
            Warn(warnings, "Entity " + guidText + " has an invalid value for component '" + key + "'. Default value kept.");
        }
        ecs_modified_id(world.c_ptr(), entity.id(), info->id);
    }
}

flecs::entity SceneSerializer::FindByGuid(const EntityGuid& guid) const
{
    flecs::entity found;
    registry_.GetWorld().each([&](flecs::entity entity, const EntityGuid& value) {
        if (!found && value == guid) {
            found = entity;
        }
    });
    return found;
}

std::string SceneSerializer::SaveToString() const
{
    struct Record {
        EntityState state;
        std::string guidText;
        int depth = 0;
    };

    std::vector<Record> records;
    registry_.GetWorld().each([&](flecs::entity entity, const EntityGuid& guid) {
        records.push_back({CaptureEntity(entity), guid.ToString(), CountSavedAncestors(entity)});
    });
    // 親が先に来るように深さ順、同じ深さは GUID 順にして、保存のたびに同じ並びになるようにする。
    std::sort(records.begin(), records.end(), [](const Record& a, const Record& b) {
        return a.depth != b.depth ? a.depth < b.depth : a.guidText < b.guidText;
    });

    OrderedJson entities = OrderedJson::array();
    for (const Record& record : records) {
        entities.push_back(ToJson(record.state));
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
        Warn(result.warnings, "The scene is not valid JSON.");
        return result;
    }
    if (document.value("format", std::string()) != kFormat) {
        Warn(result.warnings, "Unknown scene format. Expected '" + std::string(kFormat) + "'.");
        return result;
    }
    const int version = document.value("version", 0);
    if (version < 1 || version > kVersion) {
        Warn(result.warnings, "Unsupported scene version " + std::to_string(version) + ".");
        return result;
    }
    const auto entitiesIt = document.find("entities");
    if (entitiesIt == document.end() || !entitiesIt->is_array()) {
        Warn(result.warnings, "The scene has no 'entities' array.");
        return result;
    }
    const OrderedJson& entries = *entitiesIt;

    // 1. すべてのエンティティを先に作り、GUID から引けるようにする（親子は後でつなぐので、書かれた順は問わない）。
    std::unordered_map<std::string, flecs::entity> entityByGuid;
    for (const OrderedJson& entry : entries) {
        const std::string guidText = entry.is_object() ? entry.value("guid", std::string()) : std::string();
        std::optional<EntityGuid> guid = EntityGuid::Parse(guidText);
        if (!guid || !guid->IsValid() || entityByGuid.contains(guid->ToString())) {
            const EntityGuid replacement = EntityGuid::Generate();
            Warn(result.warnings,
                 "Entity has a missing, invalid or duplicate guid '" + guidText + "'. Assigned " + replacement.ToString() + ".");
            guid = replacement;
        }
        flecs::entity entity = world.entity();
        entity.set<EntityGuid>(*guid);
        entityByGuid.emplace(guid->ToString(), entity);
        result.entities.push_back(entity);
    }
    const GuidLookup findInFile = [&](const EntityGuid& guid) {
        const auto found = entityByGuid.find(guid.ToString());
        return found == entityByGuid.end() ? flecs::entity() : found->second;
    };

    // 2. 各エンティティを、ファイルに書かれた状態にする。
    for (size_t index = 0; index < entries.size(); ++index) {
        const OrderedJson& entry = entries[index];
        flecs::entity entity = result.entities[index];
        EntityState state;
        state.guid = entity.get<EntityGuid>();
        if (!entry.is_object()) {
            Warn(result.warnings, "Entity " + state.guid.ToString() + " is not a JSON object.");
            continue;
        }
        state.name = entry.value("name", std::string());
        const auto parentIt = entry.find("parent");
        if (parentIt != entry.end() && parentIt->is_string()) {
            state.parent = EntityGuid::Parse(parentIt->get<std::string>());
            if (!state.parent) {
                Warn(result.warnings, "Entity " + state.guid.ToString() + " refers to a missing parent " + parentIt->get<std::string>() +
                                          ". Kept at the root.");
            }
        }
        const auto componentsIt = entry.find("components");
        if (componentsIt != entry.end() && componentsIt->is_object()) {
            state.components = *componentsIt;
        }
        ApplyEntityState(entity, state, findInFile, result.warnings);
    }

    result.success = true;
    return result;
}

SceneLoadResult SceneSerializer::LoadFromFile(const std::filesystem::path& path) const
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        SceneLoadResult result;
        Warn(result.warnings, "Cannot open the scene file.");
        return result;
    }
    std::ostringstream text;
    text << file.rdbuf();
    return LoadFromString(text.str());
}

} // namespace myEngine
