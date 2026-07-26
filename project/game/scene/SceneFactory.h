#pragma once

#include "AbstractSceneFactory.h"
#include "../runtime/SceneRegistry.h"

class SceneFactory : public AbstractSceneFactory {
public:
	explicit SceneFactory(SceneRegistry registry);

	std::unique_ptr<IScene> CreateScene(const std::string& sceneName) override;
	bool ContainsScene(std::string_view sceneName) const override;
	std::vector<std::string> GetRegisteredSceneNames() const override;

private:
	SceneRegistry registry_;
};
