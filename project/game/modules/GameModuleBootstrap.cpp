#include "GameModuleBootstrap.h"

#include "BuiltInGameModule.h"
#include "GraphicsLabGameModule.h"
#include "../runtime/GameModuleRegistry.h"

#include <string>

bool RegisterAvailableGameModules(GameModuleRegistry& registry)
{
	bool success = true;
	success = registry.Register<BuiltInGameModule>(
		std::string(BuiltInGameModule::kId)) && success;
	success = registry.Register<GraphicsLabGameModule>(
		std::string(GraphicsLabGameModule::kId)) && success;
	return success;
}
