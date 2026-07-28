#pragma once

#include "../runtime/IGameModule.h"

#include <string_view>

class GraphicsLabGameModule final : public IGameModule {
public:
	static constexpr std::string_view kId = "graphics_lab";
	static constexpr std::string_view kDisplayName = "Graphics Lab";

	std::string_view GetId() const override;
	std::string_view GetDisplayName() const override;
	bool RegisterScenes(SceneRegistry& registry) const override;
};
