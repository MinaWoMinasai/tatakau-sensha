#include "GameModuleRegistry.h"

#include "LogWrite.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace {

std::string FormatRegisteredIds(const std::vector<std::string>& ids)
{
	if (ids.empty()) {
		return "<none>";
	}

	std::string result;
	for (std::size_t index = 0; index < ids.size(); ++index) {
		if (index != 0) {
			result += ", ";
		}
		result += ids[index];
	}
	return result;
}

void LogRegistryError(const std::string& message)
{
	cg2::LogWrite().Log("[GameModuleRegistry] " + message + "\n");
}

}

bool GameModuleRegistry::Register(std::string id, GameModuleCreator creator)
{
	if (id.empty()) {
		LogRegistryError("Registration rejected because the module ID is empty.");
		return false;
	}
	if (!creator) {
		LogRegistryError(
			"Registration rejected for '" + id + "' because the creator is empty.");
		return false;
	}
	if (creators_.contains(id)) {
		LogRegistryError("Duplicate registration rejected for '" + id + "'.");
		return false;
	}

	creators_.emplace(std::move(id), std::move(creator));
	return true;
}

std::unique_ptr<IGameModule> GameModuleRegistry::Create(std::string_view id) const
{
	const auto found = creators_.find(std::string(id));
	if (found == creators_.end()) {
		LogRegistryError(
			"Cannot create unregistered module '" + std::string(id) +
			"'. Registered modules: [" + FormatRegisteredIds(GetRegisteredIds()) + "].");
		return nullptr;
	}

	std::unique_ptr<IGameModule> module = found->second();
	if (!module) {
		LogRegistryError(
			"Creator returned nullptr for module '" + std::string(id) +
			"'. Registered modules: [" + FormatRegisteredIds(GetRegisteredIds()) + "].");
		return nullptr;
	}

	const std::string registeredId = found->first;
	const std::string moduleId(module->GetId());
	const std::string displayName(module->GetDisplayName());
	if (moduleId.empty()) {
		LogRegistryError(
			"Created module reported an empty ID. Registered ID: '" + registeredId +
			"', module ID: '<empty>', display name: '" + displayName + "'.");
		return nullptr;
	}
	if (moduleId != registeredId) {
		LogRegistryError(
			"Created module ID does not match its registration key. Registered ID: '" +
			registeredId + "', module ID: '" + moduleId +
			"', display name: '" + displayName + "'.");
		return nullptr;
	}
	return module;
}

bool GameModuleRegistry::Contains(std::string_view id) const
{
	return creators_.contains(std::string(id));
}

std::vector<std::string> GameModuleRegistry::GetRegisteredIds() const
{
	std::vector<std::string> ids;
	ids.reserve(creators_.size());
	for (const auto& entry : creators_) {
		ids.push_back(entry.first);
	}
	std::sort(ids.begin(), ids.end());
	return ids;
}
