#pragma once
#include "DirectXCommon.h"

class Object3dCommon
{
public:

	// 初期化
	void Initialize(DirectXCommon* dxCommon);

	// 共通描画設定
	void PreDraw();

	DirectXCommon* GetDxCommon() { return dxCommon_; }

private:

	DirectXCommon* dxCommon_ = nullptr;
};

