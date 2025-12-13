#pragma once
#include "DirectXCommon.h"

class SrvManager
{

	void Initialize(DirectXCommon* dxCommon);

private:
	DirectXCommon* dxCommon_ = nullptr;

	static const uint32_t kMaxSrvCount;

};

