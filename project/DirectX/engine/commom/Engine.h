#pragma once
#define DIRECTINPUT_VERSION 0x0800
#include "debugCamera.h"
#include "WinApp.h"
#include "LogWrite.h"
#include "LoadFile.h"
#include "Dump.h"
#include "Texture.h"
#include "InputDesc.h"
#include "Root.h"
#include "State.h"
#include "PSO.h"
#include "Resource.h"
#include "Audio.h"

struct D3DResouceLeakCheaker {
	~D3DResouceLeakCheaker()
	{
		// リソースリークチェック
		Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
			debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
		}
	}
};

class ResourceObject {
public:
	ResourceObject(Microsoft::WRL::ComPtr<ID3D12Resource>& resource)
		:resource_(resource)
	{
	}
	~ResourceObject() {
		if (resource_) {
			resource_->Release();
		}
	}
	Microsoft::WRL::ComPtr<ID3D12Resource> Get() { return resource_; };

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
};

class Engine
{

public:

private:

};