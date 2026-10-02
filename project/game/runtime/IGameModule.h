#pragma once

#include <string_view>

class SceneRegistry;

/// @brief ゲームモジュールの識別子とシーン登録の共通契約を定義する。
class IGameModule {
public:
    /// @brief この型の終了処理を行う。所有している資源の寿命を終了させる。
    virtual ~IGameModule() = default;

    /// @brief 識別子を返す。
    virtual std::string_view GetId() const = 0;
    /// @brief 表示名前を返す。
    virtual std::string_view GetDisplayName() const = 0;
    /// @brief このゲームで使用するシーンを、呼び出し側の登録先へ追加する。
    /// @return 必要な登録を完了できた場合true。途中で失敗した登録先は起動へ使用しない。
    virtual bool RegisterScenes(SceneRegistry& registry) const = 0;
};
