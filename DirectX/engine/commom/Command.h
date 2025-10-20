#pragma once
#include <wrl.h>
#include <cstdint>
#include <d3d12.h>
#include <cassert>
#include <dxcapi.h>

class Command
{

public:
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(Microsoft::WRL::ComPtr<ID3D12Device>& device);

	Microsoft::WRL::ComPtr<ID3D12Fence> GetFence() { return fence_; }
	HANDLE GetFenceEvent() { return fenceEvent_; }
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> GetQueue() { return queue_; }
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> GetAllocator() { return allocator_; }
	D3D12_COMMAND_QUEUE_DESC GetQueueDesc() { return queueDesc_; }
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> GetList() { return list_; }
	uint64_t GetFenceValue() { return fenceValue_; }
	IDxcUtils* GetDxcUtils() { return dxcUtils_; }
	IDxcCompiler3* GetDxcCompiler() { return dxcCompiler_; }
	IDxcIncludeHandler* GetIncludeHandler() { return includeHandler_; }


	void SetFenceValue(uint64_t value) { fenceValue_ = value; }

private:
	Microsoft::WRL::ComPtr<ID3D12Fence> fence_ = nullptr;
	HANDLE fenceEvent_ = CreateEvent(NULL, FALSE, FALSE, NULL);
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator_ = nullptr;
	D3D12_COMMAND_QUEUE_DESC queueDesc_{};
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list_ = nullptr;
	uint64_t fenceValue_ = 0;
	IDxcUtils* dxcUtils_ = nullptr;
	IDxcCompiler3* dxcCompiler_ = nullptr;
	IDxcIncludeHandler* includeHandler_ = nullptr;

};

