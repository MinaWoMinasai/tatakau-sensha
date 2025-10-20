#include "WindowScreenSize.h"

void WindowScreenSize::Initialize(const uint32_t& kClientWidth, const uint32_t& kClientHeight)
{

	//ビューポート

	//クライアント領域のサイズと一緒にして画面全体に表示
	viewport.Width = FLOAT(kClientWidth);
	viewport.Height = FLOAT(kClientHeight);
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	//シザー矩形

	//基本的にビューポートと同じ矩形が構成されるようにする
	scissorRect.left = 0;
	scissorRect.right = LONG(kClientWidth);
	scissorRect.top = 0;
	scissorRect.bottom = LONG(kClientHeight);

}