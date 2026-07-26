#pragma once

#include "../runtime/IGameModule.h"

#include <string_view>

class BuiltInGameModule final : public IGameModule {
public:
	static constexpr std::string_view kId = "builtin";
	static constexpr std::string_view kDisplayName = "CG2 Built-in";

	std::string_view GetId() const override;
	std::string_view GetDisplayName() const override;
	bool RegisterScenes(SceneRegistry& registry) const override;
};
