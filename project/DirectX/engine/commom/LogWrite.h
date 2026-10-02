#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>
#include <format>
#include <filesystem>
#include <fstream>

namespace cg2 {

/// @brief 診断メッセージをログ出力へ渡す窓口を提供する。
class LogWrite {
public:
    // 文字列を変換する
    std::wstring ConvertString(const std::string& str);
    /// @brief 文字列を変換する。
    std::string ConvertString(const std::wstring& str);

    // ログを書き出す
    void Log(const std::string& message);
    /// @brief 診断メッセージをログへ出力する。
    void Log(std::ostream& os, const std::string& message);

    /// @brief 初期化
    void Initialize();

    /// @brief LogStreamを返す。
    const std::ofstream& GetLogStream() const
    {
        return logStream_;
    };

private:
    std::ofstream logStream_;
};

} // namespace cg2
