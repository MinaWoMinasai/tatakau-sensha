#include "GraphicsLabGameModule.h"

#include "../runtime/SceneRegistry.h"
#include "../scene/GraphicsLabScene.h"
#include "../scene/TitleScene.h"

std::string_view GraphicsLabGameModule::GetId() const
{
	return kId;
}

std::string_view GraphicsLabGameModule::GetDisplayName() const
{
	return kDisplayName;
}

bool GraphicsLabGameModule::RegisterScenes(SceneRegistry& registry) const
{
	bool success = true;
	success = registry.Register<TitleScene>("TITLE") && success;
	success = registry.Register<GraphicsLabScene>("GRAPHICS_LAB") && success;
	return success;
}
