#include "SceneFactory.h"

#include <utility>

SceneFactory::SceneFactory(SceneRegistry registry)
	: registry_(std::move(registry))
{}

std::unique_ptr<IScene> SceneFactory::CreateScene(const std::string& sceneName)
{
	return registry_.Create(sceneName);
}

bool SceneFactory::ContainsScene(std::string_view sceneName) const
{
	return registry_.Contains(sceneName);
}

std::vector<std::string> SceneFactory::GetRegisteredSceneNames() const
{
	return registry_.GetRegisteredNames();
}
