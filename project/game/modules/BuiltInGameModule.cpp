#include "BuiltInGameModule.h"

#include "../runtime/SceneRegistry.h"
#include "../scene/Action3DScene.h"
#include "../scene/GameScene.h"
#include "../scene/GraphicsLabScene.h"
#include "../scene/PlayerLabScene.h"
#include "../scene/TestScene.h"
#include "../scene/TitleScene.h"
#include "../naval/scene/NavalBattleScene.h"

std::string_view BuiltInGameModule::GetId() const
{
	return kId;
}

std::string_view BuiltInGameModule::GetDisplayName() const
{
	return kDisplayName;
}

bool BuiltInGameModule::RegisterScenes(SceneRegistry& registry) const
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
