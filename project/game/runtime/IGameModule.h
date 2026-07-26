#pragma once

#include <string_view>

class SceneRegistry;

class IGameModule {
public:
	virtual ~IGameModule() = default;

	virtual std::string_view GetId() const = 0;
	virtual std::string_view GetDisplayName() const = 0;
	virtual bool RegisterScenes(SceneRegistry& registry) const = 0;
};
