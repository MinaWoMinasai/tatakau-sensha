#pragma once
#include "DirectXCommon.h"

namespace cg2 {

class ModelCommon
{
public:

	void Initialize(DirectXCommon* dxCommon);

	DirectXCommon* GetDxCommon() const { return dxCommon_; }

private:
	DirectXCommon* dxCommon_;

};

} // namespace cg2
