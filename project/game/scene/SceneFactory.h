#include "AbstractSceneFactory.h"
#include "TitleScene.h"
#include "GameScene.h"
#include "TestScene.h"

class SceneFactory : public AbstractSceneFactory {
public:
    std::unique_ptr<IScene> CreateScene(const std::string& sceneName) override {
        if (sceneName == "TITLE") return std::make_unique<TitleScene>();
        if (sceneName == "TEST")  return std::make_unique<TestScene>();
        if (sceneName == "GAME")  return std::make_unique<GameScene>();
        return nullptr;
    }
};