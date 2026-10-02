#pragma once

#include <string>
#include <vector>

/// @brief 起動するゲームモジュール・初期シーン・設定ファイルを指定する。
struct GameProject {
    int schemaVersion = 1;
    std::string projectName = "CG2 Default";
    std::string gameModule = "builtin";
    std::string startupScene = "TITLE";
    std::string resourceRoot = "resources";
    std::string sourceFilePath;
};

/// @brief 起動引数で上書きするプロジェクトとシーンを表す。
struct GameProjectCommandLineOptions {
    bool projectOptionProvided = false;
    std::string projectFilePath;
};

/// @brief 起動引数を読み取り、プロジェクトとシーンの指定へ変換する。
GameProjectCommandLineOptions ParseGameProjectCommandLine(const std::vector<std::string>& arguments);

/// @brief プロジェクト設定と起動引数を読み込み、起動条件を検証する。
class GameProjectLoader {
public:
    /// @brief 保存されたデータを読み込む。
    bool Load(const std::string& filePath, GameProject& outProject) const;
};
