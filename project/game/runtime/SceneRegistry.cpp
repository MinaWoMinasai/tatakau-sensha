#include "SceneRegistry.h"

#include "LogWrite.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace {

/// @brief 登録済みシーン名を診断用の文字列にする。
std::string FormatRegisteredNames(const std::vector<std::string>& names)
{
	if (names.empty()) {
		return "<none>";
	}

	std::string result;
	for (std::size_t index = 0; index < names.size(); ++index) {
		if (index != 0) {
			result += ", ";
		}
		result += names[index];
	}
	return result;
}

/// @brief Registryエラーを診断ログへ出力する。
void LogRegistryError(const std::string& message)
{
	cg2::LogWrite().Log("[SceneRegistry] " + message + "\n");
}

}

bool SceneRegistry::Register(std::string name, SceneCreator creator)
{
	if (name.empty()) {
		LogRegistryError("Registration rejected because the scene name is empty.");
		return false;
	}
	if (!creator) {
		LogRegistryError("Registration rejected for '" + name + "' because the creator is empty.");
		return false;
	}
	if (creators_.contains(name)) {
		LogRegistryError("Duplicate registration rejected for '" + name + "'.");
		return false;
	}

	creators_.emplace(std::move(name), std::move(creator));
	return true;
}

std::unique_ptr<IScene> SceneRegistry::Create(std::string_view name) const
{
	const auto found = creators_.find(std::string(name));
	if (found == creators_.end()) {
		LogRegistryError(
			"Cannot create unregistered scene '" + std::string(name) +
			"'. Registered scenes: [" + FormatRegisteredNames(GetRegisteredNames()) + "].");
		return nullptr;
	}

	std::unique_ptr<IScene> scene = found->second();
	if (!scene) {
		LogRegistryError(
			"Creator returned nullptr for scene '" + std::string(name) +
			"'. Registered scenes: [" + FormatRegisteredNames(GetRegisteredNames()) + "].");
	}
	return scene;
}

bool SceneRegistry::Contains(std::string_view name) const
{
	return creators_.contains(std::string(name));
}

std::vector<std::string> SceneRegistry::GetRegisteredNames() const
{
	std::vector<std::string> names;
	names.reserve(creators_.size());
	for (const auto& entry : creators_) {
		names.push_back(entry.first);
	}
	std::sort(names.begin(), names.end());
	return names;
}
