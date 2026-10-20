#pragma once
#include "EcsFlecs.h"

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace myEngine {

/// @brief シーンに保存するコンポーネントと、インスペクターに出すコンポーネントの表。
/// 1つの flecs::world ごとに1つ作る（コンポーネントのIDは world ごとに違うため）。
class ComponentRegistry {
public:
    /// @brief 1種類のコンポーネントの登録内容。
    struct Info {
        // シーンファイルでのキー。C++の型名とは分けて、型の改名でファイルが壊れないようにする。
        std::string name;
        flecs::entity_t id = 0;
        size_t size = 0;
        bool serializable = true;
        bool showInInspector = true;
        // 既定値のコンポーネントをエンティティへ付ける。
        std::function<void(flecs::entity)> addDefault;
    };

    /// @brief 登録の追加設定。
    struct Options {
        bool serializable = true;
        bool showInInspector = true;
    };

    explicit ComponentRegistry(flecs::world& world) : world_(world) {}

    /// @brief リフレクション登録済みの型 T を、name というキーで登録する。
    /// @param defaultValue 「コンポーネントを追加」したときと、シーンの読み込みで値を入れる前の初期値。
    /// @return 名前が空か、名前・型が登録済みならfalse。
    template <class T> bool Register(std::string name, T defaultValue = T{}, Options options = {})
    {
        const flecs::entity component = world_.component<T>();
        Info info;
        info.name = std::move(name);
        info.id = component.id();
        info.size = sizeof(T);
        info.serializable = options.serializable;
        info.showInInspector = options.showInInspector;
        info.addDefault = [defaultValue](flecs::entity entity) {
            entity.set<T>(defaultValue);
        };
        return Add(std::move(info));
    }

    /// @brief キーから登録内容を返す。無ければnullptr。
    const Info* FindByName(std::string_view name) const;
    /// @brief flecsのコンポーネントIDから登録内容を返す。無ければnullptr。
    const Info* FindById(flecs::entity_t id) const;

    /// @brief 登録した順の一覧を返す。
    const std::vector<Info>& GetAll() const
    {
        return infos_;
    }

    flecs::world& GetWorld() const
    {
        return world_;
    }

private:
    bool Add(Info info);

    flecs::world& world_;
    std::vector<Info> infos_;
    std::unordered_map<std::string, size_t> indexByName_;
    std::unordered_map<flecs::entity_t, size_t> indexById_;
};

/// @brief エンジンの標準コンポーネント（Transform・MeshRenderer・CameraComponent）と、
/// それらが使う型（Vector2/3/4・std::string）のリフレクションを登録する。
/// EntityGuid と名前・親子関係はシーンの見出しとして扱うので、表には入れない。
void RegisterEngineComponents(ComponentRegistry& registry);

} // namespace myEngine
