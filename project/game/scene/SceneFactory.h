#pragma once

#include "AbstractSceneFactory.h"
#include "../runtime/SceneRegistry.h"

class SceneFactory : public AbstractSceneFactory {
public:
	SceneFactory();

	std::unique_ptr<IScene> CreateScene(const std::string& sceneName) override;

	const SceneRegistry& GetRegistry() const { return registry_; }

private:
	SceneRegistry registry_;
};

bool RegisterBuiltInScenes(SceneRegistry& registry);
