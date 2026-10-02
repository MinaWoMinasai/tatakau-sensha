#pragma once

#include "IScene.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

// シーン生成のための抽象工場
/// @brief シーン名からシーンを生成する共通契約を定義する。
class AbstractSceneFactory {
public:
    /// @brief この型の終了処理を行う。所有している資源の寿命を終了させる。
    virtual ~AbstractSceneFactory() = default;
    // シーン名を指定してシーンを生成する
    virtual std::unique_ptr<IScene> CreateScene(const std::string& sceneName) = 0;
    /// @brief 指定したシーン名が登録されているか判定する。
    virtual bool ContainsScene(std::string_view sceneName) const = 0;
    /// @brief 登録済みシーン名前を返す。
    virtual std::vector<std::string> GetRegisteredSceneNames() const = 0;
};
