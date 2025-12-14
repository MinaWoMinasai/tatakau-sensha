#pragma once
#include "DirectXCommon.h"

class SrvManager
{
public:

	void Initialize(DirectXCommon* dxCommon);

	uint32_t Allocate();

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

	// SRV生成(テクスチャ用)
	void CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT format, UINT MipLevels);
	// SRV生成(Structured Buffer用)
	void CreateSRVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResouce, UINT numElements, UINT structureBytrStrike);

	void PreDraw();

	void SetGraphicsRootDescriptorTable(UINT rootParameterIndex, uint32_t srvIndex);

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetSrvHeap() { return descriptorHeap_; };
	// 最大srv数
	static const uint32_t kMaxSrvCount;
private:
	DirectXCommon* dxCommon_ = nullptr;

	// デスクリプタサイズ
	uint32_t descriptorSize;
	// SRV用デスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;

	// 次に使用するSRVインデックス
	uint32_t useIndex_ = 0;

};

