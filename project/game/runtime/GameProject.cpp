#include "GameProject.h"

#include "LogWrite.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string_view>
#include <utility>

namespace {

/// @brief プロジェクトエラーを診断ログへ出力する。
void LogProjectError(const std::string& message)
{
	cg2::LogWrite().Log("[GameProject] " + message + "\n");
}

/// @brief Utf8パスを作成して返す。
std::filesystem::path MakeUtf8Path(const std::string& path)
{
	std::u8string utf8Path;
	utf8Path.reserve(path.size());
	for (const char byte : path) {
		utf8Path.push_back(static_cast<char8_t>(static_cast<unsigned char>(byte)));
	}
	return std::filesystem::path(utf8Path);
}

/// @brief Required文字列を検証する。
bool ValidateRequiredString(
	const nlohmann::json& json,
	const char* fieldName,
	const std::string& filePath)
{
	if (!json.contains(fieldName)) {
		LogProjectError(
			"Missing required field '" + std::string(fieldName) + "' in '" + filePath + "'.");
		return false;
	}
	if (!json[fieldName].is_string()) {
		LogProjectError(
			"Field '" + std::string(fieldName) + "' must be a string in '" + filePath + "'.");
		return false;
	}
	if (json[fieldName].get_ref<const std::string&>().empty()) {
		LogProjectError(
			"Field '" + std::string(fieldName) + "' must not be empty in '" + filePath + "'.");
		return false;
	}
	return true;
}

}

GameProjectCommandLineOptions ParseGameProjectCommandLine(
	const std::vector<std::string>& arguments)
{
	GameProjectCommandLineOptions options;

	for (std::size_t index = 1; index < arguments.size(); ++index) {
		const std::string& argument = arguments[index];
		if (argument == "--project") {
			options.projectOptionProvided = true;
			if (index + 1 >= arguments.size() || arguments[index + 1].empty()) {
				LogProjectError("'--project' requires a non-empty file path.");
				return options;
			}
			options.projectFilePath = arguments[index + 1];
			return options;
		}

		constexpr std::string_view kProjectPrefix = "--project=";
		if (argument.starts_with(kProjectPrefix)) {
			options.projectOptionProvided = true;
			options.projectFilePath = argument.substr(kProjectPrefix.size());
			if (options.projectFilePath.empty()) {
				LogProjectError("'--project=' requires a non-empty file path.");
			}
			return options;
		}
	}

	return options;
}

bool GameProjectLoader::Load(const std::string& filePath, GameProject& outProject) const
{
	try {
		const std::filesystem::path projectPath = MakeUtf8Path(filePath);
		std::ifstream file(projectPath);
		if (!file.is_open()) {
			LogProjectError("Failed to open project file '" + filePath + "'.");
			return false;
		}

		nlohmann::json json;
		file >> json;

		if (!json.is_object()) {
			LogProjectError("Project root must be an object in '" + filePath + "'.");
			return false;
		}

		if (!json.contains("schemaVersion")) {
			LogProjectError("Missing required field 'schemaVersion' in '" + filePath + "'.");
			return false;
		}
		if (!json["schemaVersion"].is_number_integer()) {
			LogProjectError("Field 'schemaVersion' must be an integer in '" + filePath + "'.");
			return false;
		}

		const int schemaVersion = json["schemaVersion"].get<int>();
		if (schemaVersion != 1) {
			LogProjectError(
				"Unsupported schemaVersion " + std::to_string(schemaVersion) +
				" in '" + filePath + "'. Only schemaVersion 1 is supported.");
			return false;
		}

		if (!ValidateRequiredString(json, "projectName", filePath) ||
			!ValidateRequiredString(json, "startupScene", filePath) ||
			!ValidateRequiredString(json, "resourceRoot", filePath)) {
			return false;
		}
		if (json.contains("gameModule")) {
			if (!json["gameModule"].is_string()) {
				LogProjectError(
					"Field 'gameModule' must be a string in '" + filePath + "'.");
				return false;
			}
			if (json["gameModule"].get_ref<const std::string&>().empty()) {
				LogProjectError(
					"Field 'gameModule' must not be empty in '" + filePath + "'.");
				return false;
			}
		}

		GameProject loadedProject;
		loadedProject.schemaVersion = schemaVersion;
		loadedProject.projectName = json["projectName"].get<std::string>();
		if (json.contains("gameModule")) {
			loadedProject.gameModule = json["gameModule"].get<std::string>();
		}
		loadedProject.startupScene = json["startupScene"].get<std::string>();
		loadedProject.resourceRoot = json["resourceRoot"].get<std::string>();
		loadedProject.sourceFilePath = filePath;

		outProject = std::move(loadedProject);
		return true;
	} catch (const nlohmann::json::parse_error& error) {
		LogProjectError(
			"Failed to parse project file '" + filePath + "': " + error.what());
	} catch (const nlohmann::json::exception& error) {
		LogProjectError(
			"Invalid JSON data in project file '" + filePath + "': " + error.what());
	} catch (const std::exception& error) {
		LogProjectError(
			"Failed to load project file '" + filePath + "': " + error.what());
	} catch (...) {
		LogProjectError(
			"Failed to load project file '" + filePath + "' because of an unknown error.");
	}

	return false;
}
