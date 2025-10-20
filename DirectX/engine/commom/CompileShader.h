#pragma once
#include <cassert>
#include <filesystem>
#include <chrono>
#include <dxgidebug.h>
#include <dxcapi.h>
#include "LogWrite.h"


class CompileShader
{
public:

	/// <summary>
	/// シェーダーをコンパイル
	/// </summary>
	/// <param name="filePath"></param>
	/// <param name="profile"></param>
	/// <param name="dxcUtils"></param>
	/// <param name="dxcCompiler"></param>
	/// <param name="includeHandler"></param>
	/// <returns></returns>
	IDxcBlob* Initialize(
		const std::wstring& filePath,
		const wchar_t* profile,
		IDxcUtils* dxcUtils,
		IDxcCompiler3* dxcCompiler,
		IDxcIncludeHandler* includeHandler);
};

