#pragma once
#include <Windows.h>
#include <cstdint>
#include <dbghelp.h>
#include <strsafe.h>
namespace cg2 {

/// @brief 例外時の診断用ダンプ出力を提供する。
class Dump {

public:
    /// @brief 診断結果を外部ファイルへ出力する。
    static LONG WINAPI Export(EXCEPTION_POINTERS* exception);
};

} // namespace cg2
