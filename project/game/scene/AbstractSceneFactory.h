#pragma once
#include "IScene.h"
#include <string>
#include <memory>

// シーン生成のための抽象工場
class AbstractSceneFactory {
public:
    virtual ~AbstractSceneFactory() = default;
    // シーン名を指定してシーンを生成する
    virtual std::unique_ptr<IScene> CreateScene(const std::string& sceneName) = 0;
};