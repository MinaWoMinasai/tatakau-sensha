#include "EditorHistory.h"

#include <utility>

namespace myEngine {

void EditorHistory::Push(EditorCommand command)
{
    if (command.before == command.after) {
        return;
    }
    // 保存した時点がやり直し側にあった場合、別の操作をするとその時点には戻れなくなる。
    if (savedPosition_ && *savedPosition_ > position_) {
        savedPosition_.reset();
    }
    redo_.clear();
    undo_.push_back(std::move(command));
    if (undo_.size() > kMaxCommands) {
        undo_.pop_front();
    }
    ++position_;
}

bool EditorHistory::Undo()
{
    if (undo_.empty()) {
        return false;
    }
    EditorCommand command = std::move(undo_.back());
    undo_.pop_back();
    std::vector<std::string> warnings;
    ApplyStates(serializer_, command.before, warnings);
    redo_.push_back(std::move(command));
    --position_;
    return true;
}

bool EditorHistory::Redo()
{
    if (redo_.empty()) {
        return false;
    }
    EditorCommand command = std::move(redo_.back());
    redo_.pop_back();
    std::vector<std::string> warnings;
    ApplyStates(serializer_, command.after, warnings);
    undo_.push_back(std::move(command));
    ++position_;
    return true;
}

void EditorHistory::Clear()
{
    undo_.clear();
    redo_.clear();
    position_ = 0;
    savedPosition_ = 0;
}

void EditorHistory::ApplyStates(const SceneSerializer& serializer, const EditorCommand::StateList& states,
                                std::vector<std::string>& warnings)
{
    flecs::world& world = serializer.GetWorld();

    // 1. 存在しない状態のエンティティを消す（子は親と一緒に消えるので、残っているものだけ消す）。
    for (const auto& [guid, state] : states) {
        if (!state) {
            if (const flecs::entity entity = serializer.FindByGuid(guid); entity && entity.is_alive()) {
                entity.destruct();
            }
        }
    }

    // 2. 状態のあるエンティティを、無ければ同じ GUID で先に作る（親を探せるように、当てはめる前に全員そろえる）。
    std::vector<std::pair<flecs::entity, const EntityState*>> targets;
    for (const auto& [guid, state] : states) {
        if (!state) {
            continue;
        }
        flecs::entity entity = serializer.FindByGuid(guid);
        if (!entity) {
            entity = world.entity();
            entity.set<EntityGuid>(guid);
        }
        targets.emplace_back(entity, &*state);
    }

    // 3. 名前・親・コンポーネントを状態と同じにする。
    const SceneSerializer::GuidLookup lookup = [&](const EntityGuid& guid) {
        return serializer.FindByGuid(guid);
    };
    for (const auto& [entity, state] : targets) {
        serializer.ApplyEntityState(entity, *state, lookup, warnings);
    }
}

} // namespace myEngine
