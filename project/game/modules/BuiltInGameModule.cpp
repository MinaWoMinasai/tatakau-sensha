#include "BuiltInGameModule.h"

#include "../runtime/SceneRegistry.h"
#include "../scene/GameScene.h"
#include "../scene/TitleScene.h"

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
	success = registry.Register<GameScene>("GAME") && success;
	success = registry.Register("TANK_RUN", []() { return std::make_unique<GameScene>(true); }) && success;
	success = registry.Register("TANK_EXPEDITION", []() { return std::make_unique<GameScene>(true, true); }) && success;
	return success;
}
