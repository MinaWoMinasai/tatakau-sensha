#pragma once
#include <Windows.h>
#include <cstdint>
#include <dbghelp.h>
#include <strsafe.h>
namespace cg2 {

class Dump
{

public:

	static LONG WINAPI Export(EXCEPTION_POINTERS* exception);

};

} // namespace cg2
