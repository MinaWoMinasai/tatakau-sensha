#pragma once

#include "../scene/IScene.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

using SceneCreator = std::function<std::unique_ptr<IScene>()>;

class SceneRegistry {
public:
	bool Register(std::string name, SceneCreator creator);

	template<class T>
	bool Register(std::string name)
	{
		static_assert(std::is_base_of_v<IScene, T>, "Registered scene types must derive from IScene.");
		return Register(std::move(name), []() {
			return std::make_unique<T>();
		});
	}

	std::unique_ptr<IScene> Create(std::string_view name) const;
	bool Contains(std::string_view name) const;
	std::vector<std::string> GetRegisteredNames() const;

private:
	std::unordered_map<std::string, SceneCreator> creators_;
};
