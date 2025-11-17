#pragma once
#include "DirectXCommon.h"

class SpriteCommon
{
public:

	// 初期化
	void Initialize(DirectXCommon* dxCommon);

	// 共通描画設定
	void PreDraw();

	DirectXCommon* GetDxCommon() { return dxCommon_; }

private:
	// ルートシグネチャの作成
	void CreateRootSignature();
	// グラフィックパイプラインの生成
	void CreateGraphicsPipelineState();

	DirectXCommon* dxCommon_ = nullptr;
};
