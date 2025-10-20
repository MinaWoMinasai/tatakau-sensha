#pragma once
#include <d3d12.h>
#include <span>

class InputDesc
{

public:

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	std::span<const D3D12_INPUT_ELEMENT_DESC> GetElementDescs() const {
		return std::span{ ElementDescs_, _countof(ElementDescs_) };
	}
	D3D12_INPUT_LAYOUT_DESC GetLayout() { return Layout_; }


private:

	// InputLayout
	D3D12_INPUT_ELEMENT_DESC ElementDescs_[3] = {};
	D3D12_INPUT_LAYOUT_DESC Layout_{};
};

