#pragma once
#include "Calculation.h"
#include <wrl.h>
#include <d3d12.h>
#include "Texture.h"

class Resource
{

public:

	void Initialize();

	D3D12_VERTEX_BUFFER_VIEW CreateVBV(const ModelData &modelData, Texture texture, Microsoft::WRL::ComPtr<ID3D12Device>& device, Microsoft::WRL::ComPtr<ID3D12Resource>& vertexResource);

	Microsoft::WRL::ComPtr<ID3D12Resource> CreateMaterial(Texture texture, Microsoft::WRL::ComPtr<ID3D12Device>& device);

	void CreateWVP(Texture texture, Microsoft::WRL::ComPtr<ID3D12Device>& device, Microsoft::WRL::ComPtr<ID3D12Resource>& wvpResource, TransformationMatrix*& wvpData);

	Microsoft::WRL::ComPtr<ID3D12Resource> CreatedirectionalLight(Texture texture, Microsoft::WRL::ComPtr<ID3D12Device>& device);

private:



};

