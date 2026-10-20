#pragma once
#include "ComponentRegistry.h"

#include <filesystem>
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

    explicit SceneSerializer(const ComponentRegistry& registry) : registry_(registry) {}

    /// @brief world の中で EntityGuid を持つエンティティを、親が先になる順でJSON文字列にする。
    std::string SaveToString() const;
    /// @brief SaveToString の結果をファイルへ書く。パスはUTF-8として扱う。
    bool SaveToFile(const std::filesystem::path& path) const;

    /// @brief JSON文字列からエンティティを作って world へ追加する（既存のエンティティは消さない）。
    SceneLoadResult LoadFromString(std::string_view json) const;
    /// @brief ファイルを読んで LoadFromString する。
    SceneLoadResult LoadFromFile(const std::filesystem::path& path) const;

private:
    const ComponentRegistry& registry_;
};

} // namespace myEngine
