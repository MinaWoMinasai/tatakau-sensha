#pragma once
#include "DirectXCommon.h"

class SpriteCommon
{
public:

	// シングルトン
	static SpriteCommon* GetInstance();

	// 初期化
	void Initialize(DirectXCommon* dxCommon);

	// 共通描画設定
	void PreDraw(BlendMode blendMode = kNone);

	DirectXCommon* GetDxCommon() { return dxCommon_; }

private:

	DirectXCommon* dxCommon_ = nullptr;
};
