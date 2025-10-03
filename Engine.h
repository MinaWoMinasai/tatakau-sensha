#pragma once
#define DIRECTINPUT_VERSION 0x0800
#include "debugCamera.h"
#include "WinApp.h"
#include "LogWrite.h"
#include "CompileShader.h"
#include "LoadFile.h"
#include "Dump.h"
#include "Texture.h"
#include "DebugLayer.h"
#include "Renderer.h"
#include "useAdapterDevice.h"
#include "Command.h"
#include "Descriptor.h"
#include "InputDesc.h"
#include "Root.h"
#include "SwapChain.h"
#include "State.h"
#include "PSO.h"
#include "Resource.h"
#include "WindowScreenSize.h"
#include "View.h"
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

	void Initialize();

	void PreDraw();

	void EndDraw();

	void Finalize();

private:

	D3DResouceLeakCheaker leakCheck;
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Device> device = nullptr;

	Dump dump;

	// ログの初期化
	LogWrite log;

	WinApp winApp;

	WNDCLASS wc{};

	HWND hwnd;

	// アダプターとデバイスを初期化
	UseAdapterDevice useAdapterDevice;

	// キーの初期化
	Input input;

	// コマンドリスト
	Command command;

	SwapChain swapChain;

	Descriptor descriptor;

	Texture texture;

	View view;

	PSO pso;

	LoadFile loadFile;

	// モデル読み込み
	ModelData modelData = loadFile.Obj("resources", "block.obj");
	ModelData modelSkydome = loadFile.Obj("resources", "skydome2.obj");

	// リソースを作成する
	Resource resource;

	// 天球リソース
	Microsoft::WRL::ComPtr <ID3D12Resource> vertexResourceSkydome;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSkydome = resource.CreateVBV(modelSkydome, texture, device, vertexResourceSkydome);

	// マテリアル用のリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource = resource.CreateMaterial(texture, device);

	// WVP用のリソースを作る。Matrix4x41つ分のサイズを用意する
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource;
	// データを書き込む	
	TransformationMatrix* wvpData = nullptr;

	Transform transform{ {1.0f, 1.0f,1.0f}, {0.0f, 0.0f,0.0f}, {0.0f,0.0f,0.0f} };

	// 平行光源用のリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource = resource.CreatedirectionalLight(texture, device);

	WindowScreenSize windowScreenSize;

	// Textureを読んで転送する
	DirectX::ScratchImage mipImages = texture.Load("resources/block.png");
	const DirectX::TexMetadata metadata = mipImages.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource = texture.CreateResource(device, metadata);
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = texture.UploadData(textureResource, mipImages, device, command.GetList());
	DirectX::ScratchImage mipImagesS = texture.Load("resources/uvChecker.png");
	const DirectX::TexMetadata metadataS = mipImagesS.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResourceS = texture.CreateResource(device, metadataS);
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResourceS = texture.UploadData(textureResourceS, mipImagesS, device, command.GetList());
	// コマンドリストの内容を確定させるすべてのコマンドを積んでからCloseすること

	// metaDataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};

	// metaDataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};

	// DescriptorSizeを取得しておく
	const uint32_t descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	const uint32_t descriptorSizeRTV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	const uint32_t descriptorSizeDSV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	// SRVを作成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = descriptor.GetSrvHeap()->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = descriptor.GetSrvHeap()->GetGPUDescriptorHandleForHeapStart();

	// SRVを作成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPUS = descriptor.GetCPUHandle(descriptor.GetSrvHeap(), descriptorSizeSRV, 2);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPUS = descriptor.GetGPUHandle(descriptor.GetSrvHeap(), descriptorSizeSRV, 2);

	DebugCamera debugCamera;

	Audio audio;

	Renderer renderer;

	MSG msg{};

};