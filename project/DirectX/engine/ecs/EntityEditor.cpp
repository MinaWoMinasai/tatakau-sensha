#include "EntityEditor.h"
#include "EcsComponents.h"
#include "LogWrite.h"
#include "Struct.h"
#include "Utf8Path.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include <cstdio>
#include <cstring>
#include <fstream>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

namespace myEngine {

namespace {

/// @brief エンティティの表示名を返す（無ければ "(名前なし)"）。
std::string DisplayName(flecs::entity entity)
{
    const char* name = entity.doc_name();
    return name && *name ? std::string(name) : std::string("(名前なし)");
}

/// @brief 保存の対象（EntityGuid を持つ）親を返す。無ければ無効なエンティティ。
flecs::entity SceneParent(flecs::entity entity)
{
    const flecs::entity parent = entity.parent();
    return parent && parent.has<EntityGuid>() ? parent : flecs::entity();
}

#ifdef USE_IMGUI

/// @brief リフレクション情報をたどって、値を ImGui の入力欄で編集する。
/// @return 値を書き換えたらtrue。
bool DrawValue(flecs::world& world, flecs::entity_t type, void* value, const char* label)
{
    const flecs::entity_t vector3 = world.component<Vector3>().id();
    const flecs::entity_t vector4 = world.component<Vector4>().id();
    const flecs::entity_t stdString = world.component<std::string>().id();

    if (type == vector3) {
        return ImGui::DragFloat3(label, &static_cast<Vector3*>(value)->x, 0.05f);
    }
    if (type == vector4) {
        // MeshRenderer.color などの Vector4 は色として編集する。
        if (std::strcmp(label, "color") == 0) {
            return ImGui::ColorEdit4(label, &static_cast<Vector4*>(value)->x);
        }
        return ImGui::DragFloat4(label, &static_cast<Vector4*>(value)->x, 0.05f);
    }
    if (type == stdString) {
        auto* text = static_cast<std::string*>(value);
        char buffer[512]{};
        std::snprintf(buffer, sizeof(buffer), "%s", text->c_str());
        if (ImGui::InputText(label, buffer, sizeof(buffer))) {
            *text = buffer;
            return true;
        }
        return false;
    }

    const ecs_world_t* raw = world.c_ptr();
    if (const EcsPrimitive* primitive = ecs_get(raw, type, EcsPrimitive)) {
        switch (primitive->kind) {
        case EcsBool:
            return ImGui::Checkbox(label, static_cast<bool*>(value));
        case EcsF32:
            return ImGui::DragFloat(label, static_cast<float*>(value), 0.05f);
        case EcsF64: {
            float f = static_cast<float>(*static_cast<double*>(value));
            if (ImGui::DragFloat(label, &f, 0.05f)) {
                *static_cast<double*>(value) = f;
                return true;
            }
            return false;
        }
        case EcsI32:
            return ImGui::DragInt(label, static_cast<int*>(value));
        case EcsU32:
            return ImGui::DragScalar(label, ImGuiDataType_U32, value);
        case EcsI64:
            return ImGui::DragScalar(label, ImGuiDataType_S64, value);
        case EcsU64:
            return ImGui::DragScalar(label, ImGuiDataType_U64, value);
        case EcsI16:
            return ImGui::DragScalar(label, ImGuiDataType_S16, value);
        case EcsU16:
            return ImGui::DragScalar(label, ImGuiDataType_U16, value);
        case EcsI8:
            return ImGui::DragScalar(label, ImGuiDataType_S8, value);
        case EcsU8:
        case EcsByte:
        case EcsChar:
            return ImGui::DragScalar(label, ImGuiDataType_U8, value);
        default:
            ImGui::TextDisabled("%s: (この型は編集できません)", label);
            return false;
        }
    }
    if (const EcsStruct* structure = ecs_get(raw, type, EcsStruct)) {
        bool changed = false;
        const int32_t count = ecs_vec_count(&structure->members);
        const ecs_member_t* members = ecs_vec_first_t(&structure->members, ecs_member_t);
        for (int32_t index = 0; index < count; ++index) {
            const ecs_member_t& member = members[index];
            void* memberValue = static_cast<char*>(value) + member.offset;
            ImGui::PushID(index);
            const bool isNestedStruct = ecs_get(raw, member.type, EcsStruct) != nullptr && member.type != vector3 && member.type != vector4;
            if (isNestedStruct) {
                if (ImGui::TreeNode(member.name)) {
                    changed |= DrawValue(world, member.type, memberValue, member.name);
                    ImGui::TreePop();
                }
            } else {
                changed |= DrawValue(world, member.type, memberValue, member.name);
            }
            ImGui::PopID();
        }
        return changed;
    }
    ImGui::TextDisabled("%s: (リフレクションが登録されていません)", label);
    return false;
}

#endif // USE_IMGUI

} // namespace

EntityEditor::EntityEditor(ComponentRegistry& registry, std::filesystem::path sceneFile)
    : registry_(registry), serializer_(registry), sceneFile_(std::move(sceneFile))
{
}

flecs::entity EntityEditor::CreateEntity(const std::string& name, flecs::entity parent)
{
    flecs::world& world = registry_.GetWorld();
    flecs::entity entity = world.entity();
    entity.set<EntityGuid>(EntityGuid::Generate());
    entity.set_doc_name(name.c_str());
    if (parent && parent.is_alive()) {
        entity.child_of(parent);
    }
    // 新しいエンティティには Transform を付けておく（Unity の GameObject と同じ）。
    AddComponent(entity, "Transform");
    selected_ = entity;
    dirty_ = true;
    return entity;
}

void EntityEditor::DeleteEntity(flecs::entity entity)
{
    if (!entity || !entity.is_alive()) {
        return;
    }
    if (selected_ == entity) {
        selected_ = flecs::entity();
    }
    // ChildOf の子も一緒に消える。
    entity.destruct();
    if (selected_ && !selected_.is_alive()) {
        selected_ = flecs::entity();
    }
    dirty_ = true;
}

bool EntityEditor::AddComponent(flecs::entity entity, const std::string& componentName)
{
    const ComponentRegistry::Info* info = registry_.FindByName(componentName);
    if (!info || !entity || entity.has(info->id)) {
        return false;
    }
    info->addDefault(entity);
    dirty_ = true;
    return true;
}

bool EntityEditor::RemoveComponent(flecs::entity entity, const std::string& componentName)
{
    const ComponentRegistry::Info* info = registry_.FindByName(componentName);
    if (!info || !entity || !entity.has(info->id)) {
        return false;
    }
    entity.remove(info->id);
    dirty_ = true;
    return true;
}

bool EntityEditor::Save()
{
    return SaveAs(sceneFile_);
}

bool EntityEditor::SaveAs(const std::filesystem::path& path)
{
    if (playing_) {
        status_ = "Play 中は保存できません。Stop してから保存してください。";
        return false;
    }
    const bool ok = serializer_.SaveToFile(path);
    if (ok && path == sceneFile_) {
        dirty_ = false;
    }
    status_ = ok ? "保存しました: " + PathToUtf8(path) : "保存に失敗しました: " + PathToUtf8(path);
    LogWrite().Log("[EntityEditor] " + status_ + "\n");
    return ok;
}

bool EntityEditor::Reload()
{
    if (playing_) {
        status_ = "Play 中は読み直せません。";
        return false;
    }
    std::ifstream file(sceneFile_, std::ios::binary);
    if (!file) {
        status_ = "シーンファイルを開けません: " + PathToUtf8(sceneFile_);
        return false;
    }
    std::ostringstream text;
    text << file.rdbuf();
    const bool ok = LoadSceneText(text.str());
    dirty_ = !ok;
    status_ = ok ? "シーンファイルから読み直しました。" : "シーンファイルの読み直しに失敗しました。";
    return ok;
}

void EntityEditor::Play()
{
    if (playing_) {
        return;
    }
    playSnapshot_ = serializer_.SaveToString();
    playing_ = true;
    status_ = "Play 中（Stop で Play 前の状態に戻ります）";
}

void EntityEditor::Stop()
{
    if (!playing_) {
        return;
    }
    playing_ = false;
    const bool wasDirty = dirty_;
    LoadSceneText(playSnapshot_);
    dirty_ = wasDirty;
    playSnapshot_.clear();
    status_ = "Stop しました。Play 前の状態に戻しました。";
}

void EntityEditor::ClearSceneEntities()
{
    std::vector<flecs::entity> entities;
    registry_.GetWorld().each([&](flecs::entity entity, const EntityGuid&) {
        entities.push_back(entity);
    });
    for (flecs::entity entity : entities) {
        // 親を先に消すと子も消えるので、まだ残っているものだけを消す。
        if (entity.is_alive()) {
            entity.destruct();
        }
    }
    selected_ = flecs::entity();
}

bool EntityEditor::LoadSceneText(const std::string& text)
{
    // 選んでいたエンティティを、読み直したあとも GUID で選び直す。
    std::optional<EntityGuid> selectedGuid;
    if (selected_ && selected_.is_alive() && selected_.has<EntityGuid>()) {
        selectedGuid = selected_.get<EntityGuid>();
    }
    ClearSceneEntities();
    const SceneLoadResult result = serializer_.LoadFromString(text);
    if (selectedGuid) {
        for (flecs::entity entity : result.entities) {
            if (entity.get<EntityGuid>() == *selectedGuid) {
                selected_ = entity;
            }
        }
    }
    return result.success;
}

void EntityEditor::Draw()
{
#ifdef USE_IMGUI
    if (!ImGui::GetCurrentContext()) {
        return;
    }
    DrawToolbar();
    DrawHierarchy();
    DrawInspector();
    if (pendingDelete_) {
        DeleteEntity(pendingDelete_);
        pendingDelete_ = flecs::entity();
    }
#endif
}

#ifdef USE_IMGUI

void EntityEditor::DrawToolbar()
{
    ImGui::SetNextWindowPos({20.0f, 20.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({460.0f, 110.0f}, ImGuiCond_FirstUseEver);
    const std::string title = std::string("シーン") + (dirty_ ? " *" : "") + "###EntityEditorScene";
    if (ImGui::Begin(title.c_str())) {
        ImGui::TextUnformatted(PathToUtf8(sceneFile_.filename()).c_str());
        if (playing_) {
            if (ImGui::Button("Stop")) {
                Stop();
            }
        } else {
            if (ImGui::Button("Play")) {
                Play();
            }
            ImGui::SameLine();
            if (ImGui::Button("保存")) {
                Save();
            }
            ImGui::SameLine();
            if (ImGui::Button("読み直す")) {
                Reload();
            }
        }
        ImGui::TextWrapped("%s", status_.c_str());
    }
    ImGui::End();
}

void EntityEditor::DrawHierarchy()
{
    ImGui::SetNextWindowPos({20.0f, 140.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({300.0f, 420.0f}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("ヒエラルキー###EntityEditorHierarchy")) {
        if (ImGui::Button("+ エンティティを作る")) {
            CreateEntity("新しいエンティティ");
        }
        ImGui::Separator();
        std::vector<flecs::entity> roots;
        registry_.GetWorld().each([&](flecs::entity entity, const EntityGuid&) {
            if (!SceneParent(entity)) {
                roots.push_back(entity);
            }
        });
        for (flecs::entity root : roots) {
            DrawHierarchyNode(root);
        }
    }
    ImGui::End();
}

void EntityEditor::DrawHierarchyNode(flecs::entity entity)
{
    std::vector<flecs::entity> children;
    entity.children([&](flecs::entity child) {
        if (child.has<EntityGuid>()) {
            children.push_back(child);
        }
    });
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (children.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }
    if (selected_ == entity) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    const std::string label = DisplayName(entity) + "###" + std::to_string(entity.id());
    const bool open = ImGui::TreeNodeEx(label.c_str(), flags);
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        selected_ = entity;
    }
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("子を作る")) {
            CreateEntity("新しいエンティティ", entity);
        }
        if (ImGui::MenuItem("削除")) {
            pendingDelete_ = entity;
        }
        ImGui::EndPopup();
    }
    if (open) {
        for (flecs::entity child : children) {
            DrawHierarchyNode(child);
        }
        ImGui::TreePop();
    }
}

void EntityEditor::DrawInspector()
{
    ImGui::SetNextWindowPos({1100.0f, 20.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({460.0f, 560.0f}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("インスペクター###EntityEditorInspector")) {
        if (!selected_ || !selected_.is_alive()) {
            ImGui::TextDisabled("ヒエラルキーでエンティティを選んでください。");
            ImGui::End();
            return;
        }
        flecs::world& world = registry_.GetWorld();
        char name[256]{};
        std::snprintf(name, sizeof(name), "%s", selected_.doc_name() ? selected_.doc_name() : "");
        if (ImGui::InputText("名前", name, sizeof(name))) {
            selected_.set_doc_name(name);
            dirty_ = true;
        }
        ImGui::TextDisabled("GUID %s", selected_.get<EntityGuid>().ToString().c_str());
        ImGui::Separator();

        std::string removeName;
        for (const ComponentRegistry::Info& info : registry_.GetAll()) {
            if (!info.showInInspector || !selected_.has(info.id)) {
                continue;
            }
            ImGui::PushID(info.name.c_str());
            const bool open = ImGui::CollapsingHeader(info.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 30.0f);
            if (ImGui::SmallButton("x")) {
                removeName = info.name;
            }
            if (open) {
                void* value = ecs_ensure_id(world.c_ptr(), selected_.id(), info.id, info.size);
                if (DrawValue(world, info.id, value, info.name.c_str())) {
                    ecs_modified_id(world.c_ptr(), selected_.id(), info.id);
                    dirty_ = true;
                }
            }
            ImGui::PopID();
        }
        if (!removeName.empty()) {
            RemoveComponent(selected_, removeName);
        }

        ImGui::Separator();
        if (ImGui::BeginCombo("##AddComponent", "コンポーネントを追加")) {
            for (const ComponentRegistry::Info& info : registry_.GetAll()) {
                if (info.showInInspector && !selected_.has(info.id) && ImGui::Selectable(info.name.c_str())) {
                    AddComponent(selected_, info.name);
                }
            }
            ImGui::EndCombo();
        }
    }
    ImGui::End();
}

#endif // USE_IMGUI

} // namespace myEngine
