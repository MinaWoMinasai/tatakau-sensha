#include "AbstractSceneFactory.h"
#include "TitleScene.h"
#include "GameScene.h"
#include "TestScene.h"
#include "PlayerLabScene.h"
#include "Action3DScene.h"
#include "../naval/scene/NavalBattleScene.h"

class SceneFactory : public AbstractSceneFactory {
public:
    std::unique_ptr<IScene> CreateScene(const std::string& sceneName) override {
        if (sceneName == "TITLE") return std::make_unique<TitleScene>();
        if (sceneName == "TEST")  return std::make_unique<TestScene>();
        if (sceneName == "GAME")  return std::make_unique<GameScene>();
        if (sceneName == "PLAYER_LAB") return std::make_unique<PlayerLabScene>();
        if (sceneName == "ACTION3D") return std::make_unique<Action3DScene>();
        if (sceneName == "NAVAL_BATTLE") return std::make_unique<NavalBattleScene>();
        return nullptr;
    }
};
