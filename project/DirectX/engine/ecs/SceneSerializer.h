#pragma once
#include "ComponentRegistry.h"
#include "EcsComponents.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace myEngine {

/// @brief シーンの読み込み結果。
struct SceneLoadResult {
    // ファイルとして読めたか（形式・版が合っているか）。個々のエンティティやコンポーネントの問題は warnings に入る。
    bool success = false;
    std::vector<std::string> warnings;
    // 作成したエンティティ（ファイルに書かれた順）。
    std::vector<flecs::entity> entities;
};

/// @brief 1つのエンティティの、保存できる状態（シーンファイルの entities の1要素と同じ中身）。
/// 元に戻す・やり直すでは、操作の前後のこの状態を差し替えて使う。
struct EntityState {
    EntityGuid guid;
    std::string name;
    // 保存の対象（EntityGuid を持つ）親。無ければルート。
    std::optional<EntityGuid> parent;
    // 保存対象のコンポーネントだけ（ComponentRegistry のキー名 → 値）。並びは登録順。
    nlohmann::ordered_json components = nlohmann::ordered_json::object();

    bool operator==(const EntityState&) const = default;
};

/// @brief EntityGuid を持つエンティティを、独自のJSON形式（*.scene.json）で保存・読み込みする。
/// エンティティの見出し（guid・name・parent）はこのクラスが書き、コンポーネントの値は
/// ComponentRegistry に登録された保存対象だけを flecs のリフレクションで変換する。
///
/// 形式:
/// { "format": "myengine.scene", "version": 1,
///   "entities": [ { "guid": "...", "name": "...", "parent": "<親のguid>|null",
///                   "components": { "Transform": {...}, ... } } ] }
class SceneSerializer {
public:
    static constexpr const char* kFormat = "myengine.scene";
    static constexpr int kVersion = 1;

    /// @brief GUID からエンティティを探す関数。見つからなければ無効なエンティティを返す。
    using GuidLookup = std::function<flecs::entity(const EntityGuid&)>;

    explicit SceneSerializer(const ComponentRegistry& registry) : registry_(registry) {}

    /// @brief world の中で EntityGuid を持つエンティティを、親が先になる順でJSON文字列にする。
    std::string SaveToString() const;
    /// @brief SaveToString の結果をファイルへ書く。パスはUTF-8として扱う。
    bool SaveToFile(const std::filesystem::path& path) const;

    /// @brief JSON文字列からエンティティを作って world へ追加する（既存のエンティティは消さない）。
    SceneLoadResult LoadFromString(std::string_view json) const;
    /// @brief ファイルを読んで LoadFromString する。
    SceneLoadResult LoadFromFile(const std::filesystem::path& path) const;

    /// @brief EntityGuid を持つエンティティの、現在の保存できる状態を取り出す。
    EntityState CaptureEntity(flecs::entity entity) const;

    /// @brief エンティティを state と同じ状態にする（名前・親・保存対象のコンポーネント）。
    /// 保存対象のコンポーネントは state にあるものだけにそろえ、無いものは取り除く。保存しないコンポーネントには触れない。
    /// entity は state.guid の EntityGuid を持っていること。親は findByGuid で探し、見つからなければルートに置いて警告する。
    void ApplyEntityState(flecs::entity entity, const EntityState& state, const GuidLookup& findByGuid,
                          std::vector<std::string>& warnings) const;

    /// @brief world の中から GUID でエンティティを探す。見つからなければ無効なエンティティ。
    flecs::entity FindByGuid(const EntityGuid& guid) const;

    /// @brief 状態を、シーンファイルの entities の1要素のJSONにする。
    static nlohmann::ordered_json ToJson(const EntityState& state);

private:
    const ComponentRegistry& registry_;
};

} // namespace myEngine
