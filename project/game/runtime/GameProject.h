#pragma once

#include <string>
#include <vector>

struct GameProject {
	int schemaVersion = 1;
	std::string projectName = "CG2 Default";
	std::string gameModule = "builtin";
	std::string startupScene = "TITLE";
	std::string resourceRoot = "resources";
	std::string sourceFilePath;
};

struct GameProjectCommandLineOptions {
	bool projectOptionProvided = false;
	std::string projectFilePath;
};

GameProjectCommandLineOptions ParseGameProjectCommandLine(
	const std::vector<std::string>& arguments);

class GameProjectLoader {
public:
	bool Load(const std::string& filePath, GameProject& outProject) const;
};
