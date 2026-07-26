#pragma once

#include "IScene.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

// シーン生成のための抽象工場
class AbstractSceneFactory {
public:
    virtual ~AbstractSceneFactory() = default;
    // シーン名を指定してシーンを生成する
    virtual std::unique_ptr<IScene> CreateScene(const std::string& sceneName) = 0;
    virtual bool ContainsScene(std::string_view sceneName) const = 0;
    virtual std::vector<std::string> GetRegisteredSceneNames() const = 0;
};
