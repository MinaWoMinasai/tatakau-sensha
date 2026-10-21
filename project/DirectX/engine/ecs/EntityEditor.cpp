#include "EntityEditor.h"
#include "EcsComponents.h"
#include "LogWrite.h"
#include "RenderSync.h"
#include "Struct.h"
#include "Utf8Path.h"

#include "Camera.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
// ImGuizmo.h は ImGui の型を使うので、imgui.h の後に読む。
#include "externals/imguizmo/ImGuizmo.h"
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

/// @brief ギズモの操作の履歴での名前。
const char* GizmoLabel(GizmoOperation operation)
{
    switch (operation) {
    case GizmoOperation::Rotate:
        return "ギズモで回転";
    case GizmoOperation::Scale:
        return "ギズモで拡大縮小";
    default:
        return "ギズモで移動";
    }
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

bool EntityEditor::BeginTransformEdit(flecs::entity entity)
{
    if (playing_ || transformEditBefore_ || !entity || !entity.is_alive() || !entity.has<EntityGuid>()) {
        return false;
    }
    transformEditBefore_ = serializer_.CaptureEntity(entity);
    return true;
}

bool EntityEditor::ApplyGizmoMatrix(flecs::entity entity, const Matrix4x4& world, GizmoOperation operation)
{
    if (!entity || !entity.is_alive()) {
        return false;
    }
    Transform* transform = entity.try_get_mut<Transform>();
    if (!transform) {
        return false;
    }
    // 親から見たローカル行列 = ワールド行列 × 親のワールド行列の逆行列（行ベクトルの規約）。
    Matrix4x4 local = world;
    if (const flecs::entity parent = entity.parent()) {
        local = Multiply(world, Inverse(ComputeEntityWorldMatrix(parent)));
    }
    switch (operation) {
    case GizmoOperation::Translate:
        transform->translate = {local.m[3][0], local.m[3][1], local.m[3][2]};
        break;
    case GizmoOperation::Rotate: {
        // 回転では拡大縮小は変わらないので、今の拡大縮小（符号を含む）で各行を割って回転だけを取り出す。
        const float scale[3] = {transform->scale.x, transform->scale.y, transform->scale.z};
        Matrix4x4 rotation = MakeIdentity4x4();
        for (int row = 0; row < 3; ++row) {
            if (std::fabs(scale[row]) < 1.0e-6f) {
                return false;
            }
            for (int column = 0; column < 3; ++column) {
                rotation.m[row][column] = local.m[row][column] / scale[row];
            }
        }
        Vector3 unusedScale, rotate, unusedTranslate;
        if (!DecomposeAffineMatrix(rotation, unusedScale, rotate, unusedTranslate)) {
            return false;
        }
        transform->rotate = NearestEulerAngles(rotate, transform->rotate);
        break;
    }
    case GizmoOperation::Scale: {
        // 各行の長さが拡大縮小の大きさ。符号は元の値のまま残す（鏡映の向きを変えない）。
        const auto rowLength = [&](int row) {
            return std::sqrt(local.m[row][0] * local.m[row][0] + local.m[row][1] * local.m[row][1] + local.m[row][2] * local.m[row][2]);
        };
        const auto withSign = [](float length, float original) {
            return original < 0.0f ? -length : length;
        };
        transform->scale = {withSign(rowLength(0), transform->scale.x), withSign(rowLength(1), transform->scale.y),
                            withSign(rowLength(2), transform->scale.z)};
        break;
    }
    }
    entity.modified<Transform>();
    return true;
}

bool EntityEditor::EndTransformEdit(const std::string& label)
{
    if (!transformEditBefore_) {
        return false;
    }
    const EntityState before = *transformEditBefore_;
    transformEditBefore_.reset();
    const flecs::entity edited = serializer_.FindByGuid(before.guid);
    if (!edited) {
        return false;
    }
    const EntityState after = serializer_.CaptureEntity(edited);
    if (after == before) {
        return false;
    }
    Record({label, {{before.guid, before}}, {{after.guid, after}}});
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
    if (playing_ || transformEditBefore_ || !history_.CanUndo()) {
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
    if (playing_ || transformEditBefore_ || !history_.CanRedo()) {
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
    transformEditBefore_.reset();
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
    // ギズモのドラッグの途中なら、そこまでを記録してから Play にする。
    EndTransformEdit(GizmoLabel(gizmoOperation_));
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
    // ギズモを描く全画面のウィンドウを、エディタのウィンドウより先に作る（後ろに置く）。
    ImGuizmo::BeginFrame();
    HandleShortcuts();
    HandleCameraInput();
    DrawToolbar();
    DrawHierarchy();
    DrawInspector();
    DrawGizmo();
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
    if (playing_ || io.WantTextInput) {
        return;
    }
    // W / E / R でギズモの操作を切り替える（Unity と同じ）。右ドラッグ中の W / E はカメラの移動に使う。
    if (!io.KeyCtrl) {
        if (!cameraDragging_ && !ImGui::IsMouseDown(ImGuiMouseButton_Right) && !ImGuizmo::IsUsing()) {
            if (ImGui::IsKeyPressed(ImGuiKey_W, false)) {
                gizmoOperation_ = GizmoOperation::Translate;
            } else if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
                gizmoOperation_ = GizmoOperation::Rotate;
            } else if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
                gizmoOperation_ = GizmoOperation::Scale;
            }
        }
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

void EntityEditor::DrawGizmo()
{
    if (playing_ || !sceneCamera_) {
        return;
    }
    const ImGuiIO& io = ImGui::GetIO();
    // 描画と同じ視野角・クリップ距離で、位置と向きだけエディタ用カメラにしたカメラを作る（このフレームのカメラ操作も反映する）。
    Camera view = *sceneCamera_;
    camera_.ApplyTo(view);
    view.Update();
    Matrix4x4 viewMatrix = view.GetViewMatrix();
    const Matrix4x4& projection = view.GetUnjitteredProjectionMatrix();
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetRect(0.0f, 0.0f, io.DisplaySize.x, io.DisplaySize.y);

    const bool hasTarget = selected_ && selected_.is_alive() && selected_.has<Transform>();
    if (hasTarget) {
        Matrix4x4 world = ComputeEntityWorldMatrix(selected_);
        ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
        float snap[3] = {0.5f, 0.5f, 0.5f};
        if (gizmoOperation_ == GizmoOperation::Rotate) {
            operation = ImGuizmo::ROTATE;
            snap[0] = snap[1] = snap[2] = 15.0f; // 度
        } else if (gizmoOperation_ == GizmoOperation::Scale) {
            operation = ImGuizmo::SCALE;
            snap[0] = snap[1] = snap[2] = 0.1f;
        }
        // 拡大縮小はエンティティの軸でしか意味がないので、常にローカルにする。
        const ImGuizmo::MODE mode = gizmoLocal_ || gizmoOperation_ == GizmoOperation::Scale ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
        const bool changed = ImGuizmo::Manipulate(&viewMatrix.m[0][0], &projection.m[0][0], operation, mode, &world.m[0][0], nullptr,
                                                  io.KeyCtrl ? snap : nullptr);
        if (ImGuizmo::IsUsing()) {
            // ドラッグを始めたフレームに、変更前の状態を覚える（Manipulate はこのフレームで値を変え始めている）。
            BeginTransformEdit(selected_);
            if (changed) {
                ApplyGizmoMatrix(selected_, world, gizmoOperation_);
            }
        }
    }
    // ドラッグを終えたら（または選択が変わったら）、ドラッグ全体を1つの操作として記録する。
    if (transformEditBefore_ && (!hasTarget || !ImGuizmo::IsUsing())) {
        if (EndTransformEdit(GizmoLabel(gizmoOperation_))) {
            status_ = std::string(GizmoLabel(gizmoOperation_)) + "しました。";
        }
    }

    // 画面の右下に視点キューブを出す。クリック・ドラッグでカメラの向きを変える。回転の中心は、選んだものか前方10の点。
    float pivotDistance = 10.0f;
    if (hasTarget) {
        const Matrix4x4 world = ComputeEntityWorldMatrix(selected_);
        const Vector3 toTarget = Vector3{world.m[3][0], world.m[3][1], world.m[3][2]} - camera_.GetTranslate();
        pivotDistance = (std::max)(1.0f, Length(toTarget));
    }
    constexpr float kCubeSize = 128.0f;
    constexpr float kCubeMargin = 16.0f;
    const Matrix4x4 viewBefore = viewMatrix;
    ImGuizmo::ViewManipulate(&viewMatrix.m[0][0], pivotDistance,
                             ImVec2(io.DisplaySize.x - kCubeSize - kCubeMargin, io.DisplaySize.y - kCubeSize - kCubeMargin),
                             ImVec2(kCubeSize, kCubeSize), 0x10101010);
    if (std::memcmp(&viewBefore, &viewMatrix, sizeof(Matrix4x4)) != 0) {
        camera_.SetFromViewMatrix(viewMatrix);
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
            int operation = static_cast<int>(gizmoOperation_);
            ImGui::RadioButton("移動 (W)", &operation, static_cast<int>(GizmoOperation::Translate));
            ImGui::SameLine();
            ImGui::RadioButton("回転 (E)", &operation, static_cast<int>(GizmoOperation::Rotate));
            ImGui::SameLine();
            ImGui::RadioButton("拡大縮小 (R)", &operation, static_cast<int>(GizmoOperation::Scale));
            gizmoOperation_ = static_cast<GizmoOperation>(operation);
            ImGui::SameLine();
            ImGui::Checkbox("ローカル", &gizmoLocal_);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("オンでエンティティの向き、オフでワールドの軸に合わせます。Ctrl でスナップします。");
            }
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
