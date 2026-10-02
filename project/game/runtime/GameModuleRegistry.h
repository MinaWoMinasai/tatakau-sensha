#pragma once

#include "IGameModule.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

using GameModuleCreator = std::function<std::unique_ptr<IGameModule>()>;

/// @brief ゲームモジュールを識別子で登録し、プロジェクト設定から選択する。
class GameModuleRegistry {
public:
    /// @brief モジュールIDと生成関数を登録する。既存の登録は上書きしない。
    /// @return 空のID・空の生成関数・重複したIDならfalse。
    bool Register(std::string id, GameModuleCreator creator);

    /// @brief IGameModuleを継承する型をIDで登録する。Tは既定構築可能な型にする。
    template <class T> bool Register(std::string id)
    {
        static_assert(std::is_base_of_v<IGameModule, T>, "Registered game module types must derive from IGameModule.");
        return Register(std::move(id), []() {
            return std::make_unique<T>();
        });
    }

    /// @brief 登録したゲームモジュールを生成し、所有権を呼び出し側へ渡す。
    /// @return 未登録・生成失敗・登録IDとの不一致など、検証に失敗した場合はnullptr。
    std::unique_ptr<IGameModule> Create(std::string_view id) const;
    /// @brief 指定した識別子や値が含まれるか判定する。
    bool Contains(std::string_view id) const;
    /// @brief 登録済みID一覧を返す。
    std::vector<std::string> GetRegisteredIds() const;

private:
    std::unordered_map<std::string, GameModuleCreator> creators_;
};
