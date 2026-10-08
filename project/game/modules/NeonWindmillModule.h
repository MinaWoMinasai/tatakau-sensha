#pragma once
#include "../runtime/IGameModule.h"

/// @brief ネオン風車デモ専用の起動シーンを登録する。
class NeonWindmillModule final : public IGameModule {
public:
    static constexpr std::string_view kId = "neon_windmill";
    /// @brief プロジェクト選択に使う識別子を返す。
    std::string_view GetId() const override
    {
        return kId;
    }
    /// @brief ログに表示するデモ名を返す。
    std::string_view GetDisplayName() const override
    {
        return "Neon Windmill / procedural HDR study";
    }
    /// @brief 専用のNEON_WINDMILLシーンを登録する。
    bool RegisterScenes(SceneRegistry& registry) const override;
};
