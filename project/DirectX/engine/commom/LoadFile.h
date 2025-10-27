#pragma once
#include <Windows.h>
#include <cstdint>
#include <dxcapi.h>
#include <strsafe.h>
#include <vector>
#include "Calculation.h"
#include <sstream>
#include <fstream>

class LoadFile
{

public:

	MaterialData MaterialTemplate(const std::string& directoryPath, const std::string& filename);

	ModelData Obj(const std::string& directoryPath, const std::string& filename);

};

