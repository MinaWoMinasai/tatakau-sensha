#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>
#include <format>
#include <filesystem>
#include <fstream>

class LogWrite
{
public:

	// 文字列を変換する
	std::wstring ConvertString(const std::string& str);
	std::string ConvertString(const std::wstring& str);

	// ログを書き出す
	void Log(const std::string& message);
	void Log(std::ostream& os, const std::string& message);

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	std::ofstream& GetLogStream() { return logStream_; };

private:
	std::ofstream logStream_;

};

