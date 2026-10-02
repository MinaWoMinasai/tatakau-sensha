#pragma once

#include "../runtime/IGameModule.h"

#include <string_view>

/// @brief 組み込みゲームの識別子を提供し、利用するシーンを登録する。
class BuiltInGameModule final : public IGameModule {
public:
    static constexpr std::string_view kId = "builtin";
    static constexpr std::string_view kDisplayName = "CG2 Built-in";

    /// @brief 識別子を返す。
    std::string_view GetId() const override;
    /// @brief 表示名前を返す。
    std::string_view GetDisplayName() const override;
    /// @brief Scenesを登録する。
    bool RegisterScenes(SceneRegistry& registry) const override;
};
