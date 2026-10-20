#pragma once
#include "SceneSerializer.h"

#include <cstddef>
#include <deque>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace myEngine {

/// @brief エディタの1回の操作。操作の前と後の、エンティティごとの状態を持つ。
/// 状態が std::nullopt なら「そのエンティティは存在しない」ことを表す（作成の前、削除の後）。
/// エンティティは GUID で指すので、シーンを読み直して flecs の ID が変わっても使える。
struct EditorCommand {
    using StateList = std::vector<std::pair<EntityGuid, std::optional<EntityState>>>;

    std::string label;
    StateList before;
    StateList after;
};

/// @brief 元に戻す・やり直すの履歴。操作の種類ごとの処理は持たず、状態の差し替えだけで戻す・進める。
class EditorHistory {
public:
    static constexpr size_t kMaxCommands = 256;

    explicit EditorHistory(const SceneSerializer& serializer) : serializer_(serializer) {}

    /// @brief 実行済みの操作を記録する。やり直し用の履歴は消える。前後が同じなら記録しない。
    void Push(EditorCommand command);
    /// @brief 直前の操作を元に戻す。戻せたらtrue。
    bool Undo();
    /// @brief 元に戻した操作をやり直す。やり直せたらtrue。
    bool Redo();
    /// @brief 履歴をすべて消す（シーンを読み直したときなど）。保存時点の位置も今の位置にする。
    void Clear();

    bool CanUndo() const
    {
        return !undo_.empty();
    }

    bool CanRedo() const
    {
        return !redo_.empty();
    }

    /// @brief 次に元に戻す操作の名前。無ければnullptr。
    const std::string* NextUndoLabel() const
    {
        return undo_.empty() ? nullptr : &undo_.back().label;
    }

    /// @brief 次にやり直す操作の名前。無ければnullptr。
    const std::string* NextRedoLabel() const
    {
        return redo_.empty() ? nullptr : &redo_.back().label;
    }

    /// @brief 今の状態を「保存した時点」として覚える。
    void MarkSaved()
    {
        savedPosition_ = position_;
    }

    /// @brief 保存した時点と同じ位置にいるか（変更なしか）。
    bool IsAtSavedPoint() const
    {
        return savedPosition_ && *savedPosition_ == position_;
    }

    /// @brief 状態の一覧を world へ当てはめる。存在しない状態のエンティティは消し、無いエンティティは同じ GUID で作る。
    static void ApplyStates(const SceneSerializer& serializer, const EditorCommand::StateList& states, std::vector<std::string>& warnings);

private:
    const SceneSerializer& serializer_;
    std::deque<EditorCommand> undo_;
    std::vector<EditorCommand> redo_;
    // 履歴上の位置。操作を記録すると増え、元に戻すと減る。古い履歴を捨てても値は変えない。
    long long position_ = 0;
    // 保存した時点の位置。その時点の操作を古い履歴として捨てた後に別の操作をすると、同じ位置には戻れないので無効にする。
    std::optional<long long> savedPosition_ = 0;
};

} // namespace myEngine
