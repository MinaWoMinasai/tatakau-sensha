#pragma once

#include "IGameModule.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

using GameModuleCreator = std::function<std::unique_ptr<IGameModule>()>;

class GameModuleRegistry {
public:
	bool Register(std::string id, GameModuleCreator creator);

	template<class T>
	bool Register(std::string id)
	{
		static_assert(
			std::is_base_of_v<IGameModule, T>,
			"Registered game module types must derive from IGameModule.");
		return Register(std::move(id), []() {
			return std::make_unique<T>();
		});
	}

	std::unique_ptr<IGameModule> Create(std::string_view id) const;
	bool Contains(std::string_view id) const;
	std::vector<std::string> GetRegisteredIds() const;

private:
	std::unordered_map<std::string, GameModuleCreator> creators_;
};
