#pragma once

#include "../scene/IScene.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

using SceneCreator = std::function<std::unique_ptr<IScene>()>;

/// @brief シーン名と生成関数を登録し、実行時にシーンを生成する。
class SceneRegistry {
public:
    /// @brief シーン名と生成関数を登録する。既存の登録は上書きしない。
    /// @return 空の名前・空の生成関数・重複した名前ならfalse。
    bool Register(std::string name, SceneCreator creator);

    /// @brief ISceneを継承する型をシーン名で登録する。Tは既定構築可能な型にする。
    template <class T> bool Register(std::string name)
    {
        static_assert(std::is_base_of_v<IScene, T>, "Registered scene types must derive from IScene.");
        return Register(std::move(name), []() {
            return std::make_unique<T>();
        });
    }

    /// @brief 登録したシーンを生成し、所有権を呼び出し側へ渡す。
    /// @return 未登録または生成関数が空を返した場合はnullptr。
    std::unique_ptr<IScene> Create(std::string_view name) const;
    /// @brief 指定した識別子や値が含まれるか判定する。
    bool Contains(std::string_view name) const;
    /// @brief 登録済み名前を返す。
    std::vector<std::string> GetRegisteredNames() const;

private:
    std::unordered_map<std::string, SceneCreator> creators_;
};
