#include "EntityEditor.h"
#include "EcsComponents.h"
#include "LogWrite.h"
#include "RenderSync.h"
#include "Struct.h"
#include "Utf8Path.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include <algorithm>
#include <cmath>
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
    : registry_(registry), serializer_(registry), history_(serializer_), sceneFile_(std::move(sceneFile))
{
    // 最初は、ゲームのカメラと同じところから見る。
    ResetCameraToSceneCamera();
}

bool EntityEditor::ResetCameraToSceneCamera()
{
    bool found = false;
    registry_.GetWorld().each([&](flecs::entity entity, const CameraComponent& component) {
        if (found || !component.primary) {
            return;
        }
        found = true;
        if (const Transform* transform = entity.try_get<Transform>()) {
            camera_.SetPose(transform->translate, transform->rotate);
        }
    });
    return found;
}

bool EntityEditor::FocusSelected()
{
    if (!selected_ || !selected_.is_alive()) {
        return false;
    }
    const Matrix4x4 world = ComputeEntityWorldMatrix(selected_);
    // 大きいものほど離れて見る。拡大縮小はワールド行列の各行の長さから求める。
    float size = 0.0f;
    for (int row = 0; row < 3; ++row) {
        size = (std::max)(size, std::sqrt(world.m[row][0] * world.m[row][0] + world.m[row][1] * world.m[row][1] +
                                          world.m[row][2] * world.m[row][2]));
    }
    camera_.Focus({world.m[3][0], world.m[3][1], world.m[3][2]}, (std::max)(5.0f, size * 4.0f));
    return true;
}

void EntityEditor::Record(EditorCommand command)
{
    if (playing_) {
        return;
    }
    history_.Push(std::move(command));
}

std::vector<flecs::entity> EntityEditor::CollectSubtree(flecs::entity entity) const
{
    std::vector<flecs::entity> result{entity};
    for (size_t index = 0; index < result.size(); ++index) {
        result[index].children([&](flecs::entity child) {
            if (child.has<EntityGuid>()) {
                result.push_back(child);
            }
        });
    }
    return result;
}

flecs::entity EntityEditor::CreateEntity(const std::string& name, flecs::entity parent)
{
    flecs::world& world = registry_.GetWorld();
    flecs::entity entity = world.entity();
    const EntityGuid guid = EntityGuid::Generate();
    entity.set<EntityGuid>(guid);
    entity.set_doc_name(name.c_str());
    if (parent && parent.is_alive()) {
        entity.child_of(parent);
    }
    // 新しいエンティティには Transform を付けておく（Unity の GameObject と同じ）。作成と合わせて1つの操作にする。
    if (const ComponentRegistry::Info* transform = registry_.FindByName("Transform")) {
        transform->addDefault(entity);
    }
    Record({"「" + name + "」を作成", {{guid, std::nullopt}}, {{guid, serializer_.CaptureEntity(entity)}}});
    selected_ = entity;
    return entity;
}

void EntityEditor::DeleteEntity(flecs::entity entity)
{
    if (!entity || !entity.is_alive() || !entity.has<EntityGuid>()) {
        return;
    }
    EditorCommand command;
    command.label = "「" + DisplayName(entity) + "」を削除";
    for (flecs::entity member : CollectSubtree(entity)) {
        const EntityState state = serializer_.CaptureEntity(member);
        command.before.emplace_back(state.guid, state);
        command.after.emplace_back(state.guid, std::nullopt);
    }
    // ChildOf の子も一緒に消える。
    entity.destruct();
    if (selected_ && !selected_.is_alive()) {
        selected_ = flecs::entity();
    }
    Record(std::move(command));
}

bool EntityEditor::ModifyEntity(const std::string& label, flecs::entity entity, const std::function<void(flecs::entity)>& change)
{
    if (!entity || !entity.is_alive() || !entity.has<EntityGuid>()) {
        return false;
    }
    const EntityState before = serializer_.CaptureEntity(entity);
    change(entity);
    const EntityState after = serializer_.CaptureEntity(entity);
    if (before == after) {
        return false;
    }
    Record({label, {{before.guid, before}}, {{after.guid, after}}});
    return true;
}

bool EntityEditor::AddComponent(flecs::entity entity, const std::string& componentName)
{
    const ComponentRegistry::Info* info = registry_.FindByName(componentName);
    if (!info || !entity || entity.has(info->id)) {
        return false;
    }
    return ModifyEntity(componentName + " を追加", entity, [info](flecs::entity target) {
        info->addDefault(target);
    });
}

bool EntityEditor::RemoveComponent(flecs::entity entity, const std::string& componentName)
{
    const ComponentRegistry::Info* info = registry_.FindByName(componentName);
    if (!info || !entity || !entity.has(info->id)) {
        return false;
    }
    return ModifyEntity(componentName + " を削除", entity, [info](flecs::entity target) {
        target.remove(info->id);
    });
}

bool EntityEditor::SetName(flecs::entity entity, const std::string& name)
{
    return ModifyEntity("名前を変更", entity, [&name](flecs::entity target) {
        target.set_doc_name(name.c_str());
    });
}

bool EntityEditor::SetParent(flecs::entity entity, flecs::entity parent)
{
    if (!entity || !entity.is_alive()) {
        return false;
    }
    // 自分や自分の子孫を親にすると循環するので受け付けない。
    for (flecs::entity ancestor = parent; ancestor; ancestor = ancestor.parent()) {
        if (ancestor == entity) {
            return false;
        }
    }
    return ModifyEntity("親を変更", entity, [parent](flecs::entity target) {
        if (parent && parent.is_alive()) {
            target.child_of(parent);
        } else if (SceneParent(target)) {
            target.remove(flecs::ChildOf, flecs::Wildcard);
        }
    });
}

void EntityEditor::ReselectAfterHistory(const std::optional<EntityGuid>& selectedGuid)
{
    selected_ = selectedGuid ? serializer_.FindByGuid(*selectedGuid) : flecs::entity();
    inspectorFrameBefore_.reset();
    inspectorEditBefore_.reset();
}

bool EntityEditor::Undo()
{
    if (playing_ || !history_.CanUndo()) {
        return false;
    }
    const std::string label = *history_.NextUndoLabel();
    const std::optional<EntityGuid> selectedGuid =
        selected_ && selected_.is_alive() ? std::optional<EntityGuid>(selected_.get<EntityGuid>()) : std::nullopt;
    history_.Undo();
    ReselectAfterHistory(selectedGuid);
    status_ = "元に戻しました: " + label;
    return true;
}

bool EntityEditor::Redo()
{
    if (playing_ || !history_.CanRedo()) {
        return false;
    }
    const std::string label = *history_.NextRedoLabel();
    const std::optional<EntityGuid> selectedGuid =
        selected_ && selected_.is_alive() ? std::optional<EntityGuid>(selected_.get<EntityGuid>()) : std::nullopt;
    history_.Redo();
    ReselectAfterHistory(selectedGuid);
    status_ = "やり直しました: " + label;
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
        history_.MarkSaved();
        externalDirty_ = false;
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
    // 読み直した後は、それまでの履歴の状態とつながらないので履歴を消す。
    history_.Clear();
    externalDirty_ = !ok;
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
    inspectorFrameBefore_.reset();
    inspectorEditBefore_.reset();
    status_ = "Play 中（Stop で Play 前の状態に戻ります）";
}

void EntityEditor::Stop()
{
    if (!playing_) {
        return;
    }
    playing_ = false;
    // Play 前の状態へ戻すので、編集中の履歴はそのまま使える（エンティティは GUID で指している）。
    LoadSceneText(playSnapshot_);
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
    ReselectAfterHistory(selectedGuid);
    return result.success;
}

void EntityEditor::Draw()
{
#ifdef USE_IMGUI
    if (!ImGui::GetCurrentContext()) {
        return;
    }
    HandleShortcuts();
    HandleCameraInput();
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

void EntityEditor::HandleShortcuts()
{
    const ImGuiIO& io = ImGui::GetIO();
    // 文字の入力中は、テキストの編集（Ctrl+Z など）を優先する。
    if (playing_ || io.WantTextInput || !io.KeyCtrl) {
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        if (io.KeyShift) {
            Redo();
        } else {
            Undo();
        }
    } else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
        Redo();
    }
}

void EntityEditor::HandleCameraInput()
{
    if (playing_) {
        cameraDragging_ = false;
        return;
    }
    const ImGuiIO& io = ImGui::GetIO();
    const bool right = ImGui::IsMouseDown(ImGuiMouseButton_Right);
    const bool middle = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
    // ImGui のウィンドウの上で押したときはウィンドウの操作なので、カメラは動かさない。
    if ((ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) && !io.WantCaptureMouse) {
        cameraDragging_ = true;
    }
    if (!right && !middle) {
        cameraDragging_ = false;
    }

    EditorCameraInput input;
    input.deltaTime = io.DeltaTime;
    input.fast = io.KeyShift;
    if (cameraDragging_) {
        input.mouseDelta = {io.MouseDelta.x, io.MouseDelta.y};
        input.rotating = right;
        input.panning = middle && !right;
        if (right && !io.WantTextInput) {
            const auto axis = [](ImGuiKey positive, ImGuiKey negative) {
                return (ImGui::IsKeyDown(positive) ? 1.0f : 0.0f) - (ImGui::IsKeyDown(negative) ? 1.0f : 0.0f);
            };
            input.move = {axis(ImGuiKey_D, ImGuiKey_A), axis(ImGuiKey_E, ImGuiKey_Q), axis(ImGuiKey_W, ImGuiKey_S)};
        }
    }
    if (!io.WantCaptureMouse) {
        input.wheel = io.MouseWheel;
    }
    camera_.Apply(input);

    if (!cameraDragging_ && !io.WantTextInput && !io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F, false)) {
        FocusSelected();
    }
}

void EntityEditor::DrawToolbar()
{
    ImGui::SetNextWindowPos({20.0f, 20.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({460.0f, 135.0f}, ImGuiCond_FirstUseEver);
    const std::string title = std::string("シーン") + (IsDirty() ? " *" : "") + "###EntityEditorScene";
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
            ImGui::SameLine();
            ImGui::BeginDisabled(!history_.CanUndo());
            if (ImGui::Button("元に戻す")) {
                Undo();
            }
            if (history_.CanUndo() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("%s (Ctrl+Z)", history_.NextUndoLabel()->c_str());
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(!history_.CanRedo());
            if (ImGui::Button("やり直す")) {
                Redo();
            }
            if (history_.CanRedo() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("%s (Ctrl+Y)", history_.NextRedoLabel()->c_str());
            }
            ImGui::EndDisabled();
        }
        ImGui::TextWrapped("%s", status_.c_str());
        if (!playing_) {
            ImGui::TextDisabled("カメラ: 右ドラッグ+WASD/QE、中ドラッグ、ホイール、F で選択へ");
            ImGui::SameLine();
            if (ImGui::SmallButton("ゲームの視点へ")) {
                ResetCameraToSceneCamera();
            }
        }
    }
    ImGui::End();
}

void EntityEditor::DrawHierarchy()
{
    ImGui::SetNextWindowPos({20.0f, 165.0f}, ImGuiCond_FirstUseEver);
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
        // 連続した変更（ドラッグ・文字入力）を1つの操作にまとめるため、操作が始まる前の状態を毎フレーム覚えておく。
        if (!inspectorEditBefore_) {
            inspectorFrameBefore_ = serializer_.CaptureEntity(selected_);
        }
        bool changed = false;
        char name[256]{};
        std::snprintf(name, sizeof(name), "%s", selected_.doc_name() ? selected_.doc_name() : "");
        if (ImGui::InputText("名前", name, sizeof(name))) {
            selected_.set_doc_name(name);
            changed = true;
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
                    changed = true;
                }
            }
            ImGui::PopID();
        }
        if (changed && !inspectorEditBefore_) {
            inspectorEditBefore_ = inspectorFrameBefore_;
        }
        // 入力欄から手を離した（確定した）ら、変更前と変更後を1つの操作として記録する。
        if (inspectorEditBefore_ && !ImGui::IsAnyItemActive()) {
            const flecs::entity edited = serializer_.FindByGuid(inspectorEditBefore_->guid);
            if (edited) {
                const EntityState after = serializer_.CaptureEntity(edited);
                Record({"インスペクターで変更", {{after.guid, *inspectorEditBefore_}}, {{after.guid, after}}});
            }
            inspectorEditBefore_.reset();
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
