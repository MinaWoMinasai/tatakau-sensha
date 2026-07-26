#include "SceneFactory.h"

#include "Action3DScene.h"
#include "GameScene.h"
#include "GraphicsLabScene.h"
#include "PlayerLabScene.h"
#include "TestScene.h"
#include "TitleScene.h"
#include "../naval/scene/NavalBattleScene.h"

#include "LogWrite.h"

bool RegisterBuiltInScenes(SceneRegistry& registry)
{
	bool success = true;
	success = registry.Register<TitleScene>("TITLE") && success;
	success = registry.Register<TestScene>("TEST") && success;
	success = registry.Register<GameScene>("GAME") && success;
	success = registry.Register<PlayerLabScene>("PLAYER_LAB") && success;
	success = registry.Register<Action3DScene>("ACTION3D") && success;
	success = registry.Register<GraphicsLabScene>("GRAPHICS_LAB") && success;
	success = registry.Register<NavalBattleScene>("NAVAL_BATTLE") && success;
	return success;
}

SceneFactory::SceneFactory()
{
	if (!RegisterBuiltInScenes(registry_)) {
		LogWrite().Log("[SceneFactory] One or more built-in scenes could not be registered.\n");
	}
}

std::unique_ptr<IScene> SceneFactory::CreateScene(const std::string& sceneName)
{
	return registry_.Create(sceneName);
}
