#pragma once
#include <Windows.h>
#include <wrl.h>
#include <dxgi1_6.h>
#include <cassert>
#include <d3d12.h>

class UseAdapterDevice
{

public: 
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(Microsoft::WRL::ComPtr<IDXGIFactory7>& dxgiFactory);

	/// <summary>
	/// デバイスの作成
	/// </summary>
	void Create(Microsoft::WRL::ComPtr<ID3D12Device>& device);

private:
	Microsoft::WRL::ComPtr<IDXGIAdapter4> useAdapter_ = nullptr;

};

