#include "AbstractSceneFactory.h"
#include "TitleScene.h"
#include "GameScene.h"

class SceneFactory : public AbstractSceneFactory {
public:
    std::unique_ptr<IScene> CreateScene(const std::string& sceneName) override {
        if (sceneName == "TITLE") return std::make_unique<TitleScene>();
        if (sceneName == "GAME")  return std::make_unique<GameScene>();
        return nullptr;
    }
};