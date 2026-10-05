#define DIRECTINPUT_VERSION 0x0800
#include "Game.h"
#include "LogWrite.h"
#include "StartupTrace.h"

#include <shellapi.h>

#include <exception>
#include <filesystem>
#include <string>
#include <vector>

namespace {

/// @brief Utf8コマンド線引数を返す。
std::vector<std::string> GetUtf8CommandLineArguments()
{
	int argumentCount = 0;
	wchar_t** wideArguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
	if (!wideArguments) {
		cg2::LogWrite().Log("[GameProject] Failed to read the UTF-16 command line. Using default project selection.\n");
		return {};
	}

	std::vector<std::string> arguments;
	try {
		arguments.reserve(static_cast<std::size_t>(argumentCount));
		cg2::LogWrite stringConverter;
		for (int index = 0; index < argumentCount; ++index) {
			arguments.push_back(stringConverter.ConvertString(wideArguments[index]));
		}
	} catch (const std::exception& error) {
		cg2::LogWrite().Log(
			"[GameProject] Failed to convert command-line arguments to UTF-8: " +
			std::string(error.what()) + ". Using default project selection.\n");
		arguments.clear();
	} catch (...) {
		cg2::LogWrite().Log(
			"[GameProject] Failed to convert command-line arguments to UTF-8. "
			"Using default project selection.\n");
		arguments.clear();
	}

	LocalFree(wideArguments);
	return arguments;
}

}

/// @brief Windowsアプリの入口としてゲームを起動し、終了コードを返す。
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

    cg2::StartupTrace::Mark("process.entry");

    const GameProjectCommandLineOptions projectOptions =
        ParseGameProjectCommandLine(GetUtf8CommandLineArguments());

    // A distributed EXE can be launched from a shortcut or an unrelated cwd.
    // Explicit project paths and existing development resource roots keep their cwd.
    std::error_code resourcePathError;
    if (projectOptions.projectFilePath.empty() && !std::filesystem::is_directory(L"resources", resourcePathError)) {
        wchar_t executablePath[32768]{};
        const DWORD length=GetModuleFileNameW(nullptr,executablePath,32768);
        if(length>0&&length<32768) {
            const auto directory=std::filesystem::path(executablePath).parent_path();
            if(std::filesystem::is_directory(directory/L"resources",resourcePathError))
                std::filesystem::current_path(directory,resourcePathError);
        }
    }

    cg2::D3DResourceLeakChecker leakCheck;

    Game game;
    if (!game.Initialize(projectOptions)) {
        cg2::StartupTrace::Mark("initialization.failed");
        cg2::StartupTrace::Flush();
        return -1;
    }

    cg2::StartupTrace::Mark("initialization.ready");
    cg2::StartupTrace::Flush();

    const int exitCode = game.Run();
    game.Finalize();
    cg2::StartupTrace::Mark("process.finalized");
    cg2::StartupTrace::Flush();

    return exitCode;
}
