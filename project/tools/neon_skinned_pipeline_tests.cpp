// 独立したWARP / 実GPU検証。ゲーム/Preview導線やモデル素材は必要としない。
// 描画サービスを選択Deviceへ接続し、実際のRendererのPSO作成・Drawを実行する。
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "SkinCluster.h"
#include "../DirectX/engine/3d/neon/NeonSkinnedRenderer.h"
#include <d3d12shader.h>
#include <d3d12sdklayers.h>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <limits>
#include <DirectXPackedVector.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")

using Microsoft::WRL::ComPtr;
using namespace cg2;

namespace {
void Require(bool success, const char* message) {
	if (!success) throw std::runtime_error(message);
}

void Check(HRESULT hr, const char* message) {
	Require(SUCCEEDED(hr), message);
}

unsigned int compiledNeonShaders = 0;
DirectXCommon* testDx = nullptr;
ComPtr<ID3D12DescriptorHeap> testSrvHeap;

Matrix4x4 Identity() {
	Matrix4x4 result{};
	for (int i = 0; i < 4; ++i) result.m[i][i] = 1.0f;
	return result;
}
}

// 本テストではDirectXCommon.cppをリンクしない。DeviceはWARP、Shaderは実DXCを利用する。
// EngineのWindow/SwapChainを必要とせず、実DXCと実GPUコマンドを利用する。
IDxcBlob* DirectXCommon::CompileShader(const std::wstring& filePath, const wchar_t* profile) {
	ComPtr<IDxcUtils> utils;
	ComPtr<IDxcCompiler3> compiler;
	ComPtr<IDxcIncludeHandler> includes;
	Check(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils)), "DXC utils");
	Check(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)), "DXC compiler");
	Check(utils->CreateDefaultIncludeHandler(&includes), "DXC includes");
	ComPtr<IDxcBlobEncoding> source;
	Check(utils->LoadFile(filePath.c_str(), nullptr, &source), "Shader source");
	DxcBuffer input{ source->GetBufferPointer(), source->GetBufferSize(), DXC_CP_UTF8 };
	// Development / Releaseの実エンジンと同じ-O3 / -Zpr。
	LPCWSTR args[] = { filePath.c_str(), L"-E", L"main", L"-T", profile, L"-O3", L"-Zpr", L"-WX" };
	ComPtr<IDxcResult> result;
	Check(compiler->Compile(&input, args, _countof(args), includes.Get(), IID_PPV_ARGS(&result)), "Compile call");
	HRESULT status = E_FAIL;
	Check(result->GetStatus(&status), "Compile status");
	if (FAILED(status)) {
		ComPtr<IDxcBlobUtf8> errors;
		result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
		if (errors) std::cerr << errors->GetStringPointer();
	}
	Check(status, "Shader compilation failed");

	const bool geometryShader = filePath.find(L"NeonSkinnedGeometry.PS.hlsl") != std::wstring::npos;
	if (geometryShader) {
		Require(std::wcscmp(profile, L"ps_6_1") == 0, "Geometry PS must use the minimum ps_6_1 profile");
		std::cout << "PASS: native geometry ps_6_1 DXC compilation.\n";
	}
	if (filePath.find(L"NeonSkinned.PS.hlsl") != std::wstring::npos || geometryShader) {
		ComPtr<IDxcBlob> reflectionData;
		Check(result->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&reflectionData), nullptr), "Reflection data");
		DxcBuffer reflectionBuffer{ reflectionData->GetBufferPointer(), reflectionData->GetBufferSize(), 0 };
		ComPtr<ID3D12ShaderReflection> reflection;
		Check(utils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(&reflection)), "Shader reflection");
		auto* constants = reflection->GetConstantBufferByName("NeonSkinnedConstants");
		D3D12_SHADER_BUFFER_DESC bufferDesc{};
		Check(constants->GetDesc(&bufferDesc), "Neon constants reflection");
		Require(bufferDesc.Size == sizeof(NeonSkinnedParams) + 32, "C++ / HLSL constant buffer size differs");
		const struct { const char* name; UINT offset; } expected[] = {
			{ "gBodyColor", static_cast<UINT>(offsetof(NeonSkinnedParams, bodyColor)) },
			{ "gEmissiveColor", static_cast<UINT>(offsetof(NeonSkinnedParams, emissiveColor)) },
			{ "gEmissiveIntensity", static_cast<UINT>(offsetof(NeonSkinnedParams, emissiveIntensity)) },
			{ "gRimStrength", static_cast<UINT>(offsetof(NeonSkinnedParams, rimStrength)) },
			{ "gRimPower", static_cast<UINT>(offsetof(NeonSkinnedParams, rimPower)) },
			{ "gOutlineWidthPixels", static_cast<UINT>(offsetof(NeonSkinnedParams, outlineWidthPixels)) },
			{ "gOutlineEnabled", static_cast<UINT>(offsetof(NeonSkinnedParams, outlineEnabled)) },
			{ "gInternalLineEnabled", static_cast<UINT>(offsetof(NeonSkinnedParams, internalLineEnabled)) },
			{ "gInternalLineWidthPixels", static_cast<UINT>(offsetof(NeonSkinnedParams, internalLineWidthPixels)) },
			{ "gInternalLineIntensity", static_cast<UINT>(offsetof(NeonSkinnedParams, internalLineIntensity)) },
			{ "gInternalLineThreshold", static_cast<UINT>(offsetof(NeonSkinnedParams, internalLineThreshold)) },
			{ "gGeometryLineColor", static_cast<UINT>(offsetof(NeonSkinnedParams, geometryLineColor)) },
			{ "gGeometryLineIntensity", static_cast<UINT>(offsetof(NeonSkinnedParams, geometryLineIntensity)) },
			{ "gGeometryLineEnabled", static_cast<UINT>(offsetof(NeonSkinnedParams, geometryLineEnabled)) },
			{ "gGeometryLineWidthPixels", static_cast<UINT>(offsetof(NeonSkinnedParams, geometryLineWidthPixels)) },
			{ "gCameraWorldPosition", sizeof(NeonSkinnedParams) },
			{ "gViewportSize", sizeof(NeonSkinnedParams) + 16 },
		};
		for (const auto& field : expected) {
			D3D12_SHADER_VARIABLE_DESC variable{};
			Check(constants->GetVariableByName(field.name)->GetDesc(&variable), "Neon constant field reflection");
			Require(variable.StartOffset == field.offset, "C++ / HLSL constant field offset differs");
		}
		auto* submeshConstants = reflection->GetConstantBufferByName("NeonSubmeshConstants");
		D3D12_SHADER_BUFFER_DESC submeshDesc{};
		Check(submeshConstants->GetDesc(&submeshDesc), "Submesh constant reflection");
		Require(submeshDesc.Size == 16 && sizeof(NeonSkinnedSubmeshParams) == 12,
			"Submesh root constants must contain three DWORDs within one HLSL register");
		const struct { const char* name; UINT offset; } submeshFields[] = {
			{ "gSubmeshLineStrength", offsetof(NeonSkinnedSubmeshParams, lineStrength) },
			{ "gAlphaCutoff", offsetof(NeonSkinnedSubmeshParams, alphaCutoff) },
			{ "gSubmeshGeometryStrength", offsetof(NeonSkinnedSubmeshParams, geometryLineStrength) },
		};
		for (const auto& field : submeshFields) {
			D3D12_SHADER_VARIABLE_DESC variable{};
			Check(submeshConstants->GetVariableByName(field.name)->GetDesc(&variable), "Submesh field reflection");
			Require(variable.StartOffset == field.offset, "Submesh root constant offset differs");
		}
		if (geometryShader) {
			// Native barycentricsはDXIL intrinsicとして扱われ、通常の入力signatureには現れない。
			Require((reflection->GetRequiresFlags() & D3D_SHADER_REQUIRES_BARYCENTRICS) != 0,
				"Geometry shader DXIL must require native barycentrics");
		}
	}
	if (filePath.find(L"NeonSkinned") != std::wstring::npos) ++compiledNeonShaders;
	ComPtr<IDxcBlob> blob;
	Check(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&blob), nullptr), "DXIL object");
	return blob.Detach();
}

ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResource(size_t bytes) {
	D3D12_HEAP_PROPERTIES heap{};
	heap.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC desc{};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width = bytes;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ComPtr<ID3D12Resource> resource;
	Check(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource)), "Upload buffer");
	return resource;
}

void DirectXCommon::Initialize(WinApp*) {
	D3D12_COMMAND_QUEUE_DESC desc{};
	Check(device_->CreateCommandQueue(&desc, IID_PPV_ARGS(&queue_)), "Queue");
	Check(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator_)), "Allocator");
	Check(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator_.Get(), nullptr,
		IID_PPV_ARGS(&list_)), "Command list");
	Check(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)), "Fence");
}

void DirectXCommon::PostDraw() {
	Check(list_->Close(), "Close list");
	ID3D12CommandList* lists[] = { list_.Get() };
	queue_->ExecuteCommandLists(1, lists);
	Check(queue_->Signal(fence_.Get(), ++fenceValue_), "Signal fence");
	Check(fence_->SetEventOnCompletion(fenceValue_, fenceEvent_), "Fence event");
	Require(WaitForSingleObject(fenceEvent_, 30000) == WAIT_OBJECT_0, "GPU timeout");
	Check(allocator_->Reset(), "Reset allocator");
	Check(list_->Reset(allocator_.Get(), nullptr), "Reset list");
}

void DirectXCommon::SetRenderTargets(D3D12_CPU_DESCRIPTOR_HANDLE color, D3D12_CPU_DESCRIPTOR_HANDLE normal,
	D3D12_CPU_DESCRIPTOR_HANDLE material, D3D12_CPU_DESCRIPTOR_HANDLE depth) {
	D3D12_CPU_DESCRIPTOR_HANDLE targets[] = { color, normal, material };
	list_->OMSetRenderTargets(3, targets, FALSE, &depth);
	currentRtvHandle_ = color;
	currentDsvHandle_ = depth;
	currentHasDsv_ = true;
}

void SrvManager::PreDraw() {
	ID3D12DescriptorHeap* heaps[] = { testSrvHeap.Get() };
	testDx->GetList()->SetDescriptorHeaps(1, heaps);
}

D3D12_GPU_DESCRIPTOR_HANDLE SrvManager::GetGPUDescriptorHandle(uint32_t index) {
	auto handle = testSrvHeap->GetGPUDescriptorHandleForHeapStart();
	handle.ptr += static_cast<UINT64>(index) * testDx->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	return handle;
}

// 素材を追加せず、滑らかな球を二つのSubmeshに分けたSkinning Geometryを準備する。
// 本テストのサービスだけで使用。ProductionのSkinnedModel実装には変更を加えない。
void SkinnedModel::Initialize(DirectXCommon* dx, SrvManager*, const std::string& fixtureName) {
	dxCommon_ = dx;
	constexpr uint32_t rings = 24, segments = 48;
	std::vector<VertexData> vertices;
	std::vector<uint32_t> indices;
	for (uint32_t row = 0; row <= rings; ++row) {
		const float latitude = static_cast<float>(row) * 3.14159265f / rings;
		for (uint32_t column = 0; column <= segments; ++column) {
			const float longitude = static_cast<float>(column) * 6.2831853f / segments;
			const Vector3 normal{ std::sin(latitude) * std::cos(longitude), std::cos(latitude),
				std::sin(latitude) * std::sin(longitude) };
			vertices.push_back({ { normal.x * 0.5f, normal.y * 0.5f, normal.z * 0.5f, 1.0f },
				{ static_cast<float>(column) / segments, static_cast<float>(row) / rings }, normal });
		}
	}
	for (uint32_t row = 0; row < rings; ++row) {
		for (uint32_t column = 0; column < segments; ++column) {
			const uint32_t a = row * (segments + 1) + column;
			const uint32_t b = a + segments + 1;
			indices.insert(indices.end(), { a, a + 1, b, a + 1, b + 1, b });
		}
	}
	if (fixtureName.find("geometry") != std::string::npos) {
		// 同一平面の2三角形。対角線はシルエットとは独立したBody内部の辺。
		vertices = {
			{ { -0.75f, -0.75f, 0, 1 }, { 0, 1 }, { 0, 0, -1 } },
			{ { -0.75f,  0.75f, 0, 1 }, { 0, 0 }, { 0, 0, -1 } },
			{ {  0.75f, -0.75f, 0, 1 }, { 1, 1 }, { 0, 0, -1 } },
			{ {  0.75f,  0.75f, 0, 1 }, { 1, 0 }, { 0, 0, -1 } },
		};
		indices = { 0, 1, 2, 2, 1, 3 };
	}
	std::vector<VertexInfluence> influences(vertices.size());
	for (auto& influence : influences) influence.weights[0] = 1.0f;
	auto upload = [dx](const void* source, size_t bytes) {
		auto resource = dx->CreateBufferResource(bytes);
		void* mapped = nullptr;
		Check(resource->Map(0, nullptr, &mapped), "Map geometry");
		std::memcpy(mapped, source, bytes);
		resource->Unmap(0, nullptr);
		return resource;
	};
	vertexResource_ = upload(vertices.data(), vertices.size() * sizeof(VertexData));
	influenceResource_ = upload(influences.data(), influences.size() * sizeof(VertexInfluence));
	indexResource_ = upload(indices.data(), indices.size() * sizeof(uint32_t));
	vertexBufferViews_[0] = { vertexResource_->GetGPUVirtualAddress(), static_cast<UINT>(vertices.size() * sizeof(VertexData)), sizeof(VertexData) };
	vertexBufferViews_[1] = { influenceResource_->GetGPUVirtualAddress(), static_cast<UINT>(influences.size() * sizeof(VertexInfluence)), sizeof(VertexInfluence) };
	indexBufferView_ = { indexResource_->GetGPUVirtualAddress(), static_cast<UINT>(indices.size() * sizeof(uint32_t)), DXGI_FORMAT_R32_UINT };
	const UINT half = static_cast<UINT>(indices.size() / 2);
	asset_.submeshes = { { 0, half, 0, "single-sided", {}, false }, { half, half, 0, "double-sided", {}, true } };
	asset_.modelData.materials = { MaterialData{} };
	asset_.modelData.materials[0].textureIndex = fixtureName.find("feature") != std::string::npos ? 2 : 1;
	paletteResource_ = dx->CreateBufferResource(sizeof(SkinningPaletteEntry));
	Check(paletteResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedPalette_)), "Map palette");
	Update(0.0f);
	D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
	srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srv.Buffer.NumElements = 1;
	srv.Buffer.StructureByteStride = sizeof(SkinningPaletteEntry);
	dx->GetDevice()->CreateShaderResourceView(paletteResource_.Get(), &srv, testSrvHeap->GetCPUDescriptorHandleForHeapStart());
}

void SkinnedModel::Update(float paletteTranslationX) {
	mappedPalette_->skeletonSpaceMatrix = Identity();
	mappedPalette_->skeletonSpaceMatrix.m[3][0] = paletteTranslationX;
	mappedPalette_->skeletonSpaceInverseTransposeMatrix = Identity();
}

void SkinnedModel::BindGeometry() const {
	dxCommon_->GetList()->IASetVertexBuffers(0, 2, vertexBufferViews_);
	dxCommon_->GetList()->IASetIndexBuffer(&indexBufferView_);
}

void SkinnedModel::BindSkinningPalette(uint32_t rootParameter) const {
	dxCommon_->GetList()->SetGraphicsRootDescriptorTable(rootParameter, testSrvHeap->GetGPUDescriptorHandleForHeapStart());
}

void SkinnedModel::DrawSubmesh(size_t index) const {
	const auto& submesh = GetSubmesh(index);
	dxCommon_->GetList()->DrawIndexedInstanced(submesh.indexCount, 1, submesh.indexStart, 0, 0);
}

namespace {
constexpr UINT kSize = 128;

void Transition(ID3D12GraphicsCommandList* list, ID3D12Resource* resource,
	D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition = { resource, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, before, after };
	list->ResourceBarrier(1, &barrier);
}

ComPtr<ID3D12Resource> CreateTestTexture(DirectXCommon& dx, SrvManager& srv, UINT index, bool patterned) {
	constexpr UINT size = 64;
	D3D12_RESOURCE_DESC desc{};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = size;
	desc.Height = size;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	D3D12_HEAP_PROPERTIES heap{};
	heap.Type = D3D12_HEAP_TYPE_DEFAULT;
	ComPtr<ID3D12Resource> resource;
	Check(dx.GetDevice()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&resource)), "Feature texture");
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
	UINT64 total = 0;
	dx.GetDevice()->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, nullptr, nullptr, &total);
	auto upload = dx.CreateBufferResource(static_cast<size_t>(total));
	uint8_t* data = nullptr;
	Check(upload->Map(0, nullptr, reinterpret_cast<void**>(&data)), "Texture upload map");
	for (UINT y = 0; y < size; ++y) for (UINT x = 0; x < size; ++x) {
		auto* pixel = data + footprint.Offset + y * footprint.Footprint.RowPitch + x * 4;
		// 不透明の色境界と透明な穴を別々に含む、回帰用の小さなTexture。
		const uint8_t color = patterned && ((x / 8) % 2 == 0) ? 0 : 255;
		pixel[0] = pixel[1] = pixel[2] = color;
		pixel[3] = patterned && y > 24 && y < 40 ? 0 : 255;
	}
	upload->Unmap(0, nullptr);
	D3D12_TEXTURE_COPY_LOCATION source{};
	source.pResource = upload.Get();
	source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	source.PlacedFootprint = footprint;
	D3D12_TEXTURE_COPY_LOCATION destination{};
	destination.pResource = resource.Get();
	destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dx.GetList()->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
	Transition(dx.GetList().Get(), resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	dx.PostDraw(); // Upload resourceを破棄する前にGPUを待つ。
	D3D12_SHADER_RESOURCE_VIEW_DESC view{};
	view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	view.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	view.Texture2D.MipLevels = 1;
	auto handle = testSrvHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += index * dx.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	dx.GetDevice()->CreateShaderResourceView(resource.Get(), &view, handle);
	(void)srv;
	return resource;
}

struct Capture {
	ComPtr<ID3D12Resource> buffer;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
	UINT64 rowBytes = 0;
	std::vector<uint8_t> bytes;
	void Read() {
		void* mapped = nullptr;
		Check(buffer->Map(0, nullptr, &mapped), "Map readback");
		std::memcpy(bytes.data(), mapped, bytes.size());
		buffer->Unmap(0, nullptr);
	}
	const uint8_t* Pixel(UINT x, UINT y) const {
		return bytes.data() + footprint.Offset + y * footprint.Footprint.RowPitch + x * (rowBytes / kSize);
	}
	float Float(UINT x, UINT y, UINT component) const {
		uint16_t value;
		std::memcpy(&value, Pixel(x, y) + component * 2, sizeof(value));
		return DirectX::PackedVector::XMConvertHalfToFloat(value);
	}
};

class OffscreenScene {
public:
	explicit OffscreenScene(DirectXCommon& dx) : dx_(dx) {
		D3D12_DESCRIPTOR_HEAP_DESC heap{};
		heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		heap.NumDescriptors = 3;
		Check(dx.GetDevice()->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&rtvHeap_)), "RTV heap");
		heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		heap.NumDescriptors = 1;
		Check(dx.GetDevice()->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&dsvHeap_)), "DSV heap");
		const DXGI_FORMAT formats[] = { DirectXCommon::kSceneRenderTargetFormat,
			DirectXCommon::kNormalBufferFormat, DirectXCommon::kMaterialBufferFormat };
		for (UINT i = 0; i < 4; ++i) {
			D3D12_RESOURCE_DESC desc{};
			desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			desc.Width = kSize;
			desc.Height = kSize;
			desc.DepthOrArraySize = 1;
			desc.MipLevels = 1;
			desc.SampleDesc.Count = 1;
			desc.Format = i < 3 ? formats[i] : DXGI_FORMAT_R24G8_TYPELESS;
			desc.Flags = i < 3 ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			D3D12_HEAP_PROPERTIES properties{};
			properties.Type = D3D12_HEAP_TYPE_DEFAULT;
			D3D12_CLEAR_VALUE clear{};
			clear.Format = i < 3 ? formats[i] : DXGI_FORMAT_D24_UNORM_S8_UINT;
			if (i == 3) clear.DepthStencil.Depth = 1.0f;
			Check(dx.GetDevice()->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &desc,
				i < 3 ? D3D12_RESOURCE_STATE_RENDER_TARGET : D3D12_RESOURCE_STATE_DEPTH_WRITE,
				&clear, IID_PPV_ARGS(&targets_[i])), "Scene target");
			if (i < 3) {
				rtv_[i] = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
				rtv_[i].ptr += i * dx.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
				dx.GetDevice()->CreateRenderTargetView(targets_[i].Get(), nullptr, rtv_[i]);
			} else {
				D3D12_DEPTH_STENCIL_VIEW_DESC dsv{};
				dsv.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
				dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
				dsv_ = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
				dx.GetDevice()->CreateDepthStencilView(targets_[i].Get(), &dsv, dsv_);
			}
		}
		for (UINT i = 0; i < 5; ++i) {
			auto desc = targets_[(std::min)(i, 3u)]->GetDesc();
			UINT64 total = 0;
			dx.GetDevice()->GetCopyableFootprints(&desc, i == 4 ? 1 : 0, 1, 0,
				&captures[i].footprint, nullptr, &captures[i].rowBytes, &total);
			D3D12_HEAP_PROPERTIES heapProperties{};
			heapProperties.Type = D3D12_HEAP_TYPE_READBACK;
			D3D12_RESOURCE_DESC bufferDesc{};
			bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			bufferDesc.Width = total;
			bufferDesc.Height = 1;
			bufferDesc.DepthOrArraySize = 1;
			bufferDesc.MipLevels = 1;
			bufferDesc.SampleDesc.Count = 1;
			bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			Check(dx.GetDevice()->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE,
				&bufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&captures[i].buffer)), "Readback buffer");
			captures[i].bytes.resize(static_cast<size_t>(total));
		}
	}

	void Begin(float clearDepth = 1.0f) {
		auto list = dx_.GetList();
		dx_.SetRenderTargets(rtv_[0], rtv_[1], rtv_[2], dsv_);
		D3D12_VIEWPORT viewport{ 0, 0, static_cast<float>(kSize), static_cast<float>(kSize), 0, 1 };
		D3D12_RECT scissor{ 0, 0, kSize, kSize };
		list->RSSetViewports(1, &viewport);
		list->RSSetScissorRects(1, &scissor);
		const float clear[4]{};
		for (const auto handle : rtv_) list->ClearRenderTargetView(handle, clear, 0, nullptr);
		// 予約bitには前の値、下位7bitには他の利用者の値を入れて保持を検証する。
		list->ClearDepthStencilView(dsv_, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, clearDepth, 0xa5, 0, nullptr);
	}

	void End() {
		auto list = dx_.GetList();
		for (UINT i = 0; i < 4; ++i) Transition(list.Get(), targets_[i].Get(),
			i < 3 ? D3D12_RESOURCE_STATE_RENDER_TARGET : D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_COPY_SOURCE);
		for (UINT i = 0; i < 5; ++i) {
			D3D12_TEXTURE_COPY_LOCATION source{};
			source.pResource = targets_[(std::min)(i, 3u)].Get();
			source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			source.SubresourceIndex = i == 4 ? 1 : 0;
			D3D12_TEXTURE_COPY_LOCATION destination{};
			destination.pResource = captures[i].buffer.Get();
			destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			destination.PlacedFootprint = captures[i].footprint;
			list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
		}
		for (UINT i = 0; i < 4; ++i) Transition(list.Get(), targets_[i].Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
			i < 3 ? D3D12_RESOURCE_STATE_RENDER_TARGET : D3D12_RESOURCE_STATE_DEPTH_WRITE);
		dx_.PostDraw();
		for (auto& capture : captures) capture.Read();
		for (UINT y = 0; y < kSize; ++y) for (UINT x = 0; x < kSize; ++x) {
			const auto& stencil = captures[4];
			Require(stencil.Pixel(x, y)[stencil.rowBytes / kSize - 1] == 0x25,
				"Outline must clean up bit 0x80 and preserve other stencil bits");
		}
	}

	Capture captures[5]; // HDR / Normal / Material / Depth / Stencil
private:
	DirectXCommon& dx_;
	ComPtr<ID3D12Resource> targets_[4];
	ComPtr<ID3D12DescriptorHeap> rtvHeap_, dsvHeap_;
	D3D12_CPU_DESCRIPTOR_HANDLE rtv_[3]{}, dsv_{};
};

void RequireSameCapture(const Capture& actual, const Capture& expected, const char* message) {
	Require(actual.rowBytes == expected.rowBytes, "Capture format differs");
	for (UINT y = 0; y < kSize; ++y) {
		Require(std::memcmp(actual.Pixel(0, y), expected.Pixel(0, y), static_cast<size_t>(actual.rowBytes)) == 0, message);
	}
}

UINT CountBrightPixels(const Capture& capture, UINT component, UINT minX = 0, UINT maxX = kSize,
	UINT minY = 0, UINT maxY = kSize) {
	UINT count = 0;
	for (UINT y = minY; y < maxY; ++y) for (UINT x = minX; x < maxX; ++x)
		if (capture.Float(x, y, component) > 1.0f) ++count;
	return count;
}

void TestGeometryLines(DirectXCommon& dx, SrvManager& srv, NeonSkinnedRenderer& renderer) {
	// TestDrawのTextureは前のscopeで破棄済み。SRVが指すTextureをこの検証のGPU完了まで保持する。
	auto whiteTexture = CreateTestTexture(dx, srv, 1, false);
	auto featureTexture = CreateTestTexture(dx, srv, 2, true);
	SkinnedModel model;
	model.Initialize(&dx, &srv, "geometry fixture");
	OffscreenScene scene(dx);
	auto transformResource = dx.CreateBufferResource(256);
	TransformationMatrix* transform = nullptr;
	Check(transformResource->Map(0, nullptr, reinterpret_cast<void**>(&transform)), "Geometry transform map");
	*transform = { Identity(), Identity(), Identity() };
	transform->WVP.m[2][2] = 0.4f;
	transform->WVP.m[3][2] = 0.5f;
	const Vector2 viewport{ static_cast<float>(kSize), static_cast<float>(kSize) };
	NeonSkinnedParams params;
	params.outlineEnabled = 0;
	params.internalLineEnabled = 0;
	params.geometryLineColor = { 0, 0, 1 };
	params.geometryLineIntensity = 8;
	params.geometryLineWidthPixels = 2;
	auto render = [&](NeonSkinnedRenderer& drawRenderer, float depth = 1.0f) {
		drawRenderer.BeginFrame();
		drawRenderer.SetParams(params);
		scene.Begin(depth);
		drawRenderer.Draw(model, transformResource->GetGPUVirtualAddress(), { 0, 0, -2 }, viewport);
		scene.End();
	};
	renderer.SetSubmeshParams({ { 1, 0, 1 }, { 1, 0, 1 } });
	render(renderer);
	const Capture legacy[] = { scene.captures[0], scene.captures[1], scene.captures[2], scene.captures[3] };
	Require(CountBrightPixels(legacy[0], 2) == 0, "Geometry default must be OFF");
	Require(legacy[0].Float(48, 80, 0) > 0.001f && legacy[0].Float(80, 48, 0) > 0.001f,
		"Geometry fixture must exercise both cull variants");

	// 明示的に機能を無効化したRendererは、対応GPU上でもlegacyのみで利用できる。
	NeonSkinnedRenderer disabledRenderer;
	disabledRenderer.Initialize(&dx, &srv, false);
	Require(!disabledRenderer.IsGeometryLinesSupported() && !disabledRenderer.GetGeometryLinesStatus().empty(),
		"Disabled geometry pipeline must explain its fallback state");
	disabledRenderer.SetSubmeshParams({ { 1, 0, 1 }, { 1, 0, 1 } });
	params.geometryLineEnabled = 1;
	render(disabledRenderer);
	for (UINT i = 0; i < 4; ++i) RequireSameCapture(scene.captures[i], legacy[i],
		"Disabled geometry pipeline must retain the exact legacy body / MRT / depth path");
	std::cout << "PASS: forced geometry-pipeline disable preserves legacy rendering even when requested ON.\n";

	if (!renderer.IsGeometryLinesSupported()) {
		render(renderer);
		for (UINT i = 0; i < 4; ++i) RequireSameCapture(scene.captures[i], legacy[i],
			"Unsupported geometry device must retain legacy rendering when requested ON");
		std::cout << "SKIP: native geometry GPU draw checks: " << renderer.GetGeometryLinesStatus()
			<< ". Legacy / unsupported fallback drawing passed.\n";
		return;
	}

	// 強度0では、そのSubmeshの構造線だけが消える。Texture設定とは独立。
	renderer.SetSubmeshParams({ { 1, 0, 0 }, { 1, 0, 0 } });
	render(renderer);
	for (UINT i = 0; i < 4; ++i) RequireSameCapture(scene.captures[i], legacy[i],
		"Geometry strength zero must retain the legacy body without changing other MRTs");
	renderer.SetSubmeshParams({ { 1, 0, std::numeric_limits<float>::quiet_NaN() }, { 1, 0, -1 } });
	render(renderer);
	for (UINT i = 0; i < 4; ++i) RequireSameCapture(scene.captures[i], legacy[i],
		"Invalid per-submesh geometry strength must sanitize to the legacy body path");
	renderer.SetSubmeshParams({ { 0, 0, 1 }, { 0, 0, 1 } });
	params.geometryLineWidthPixels = 0;
	render(renderer);
	RequireSameCapture(scene.captures[0], legacy[0], "Geometry width zero must preserve the legacy body");
	params.geometryLineWidthPixels = 2;
	params.geometryLineIntensity = 0;
	render(renderer);
	RequireSameCapture(scene.captures[0], legacy[0], "Geometry intensity zero must preserve the legacy body");
	params.geometryLineIntensity = 8;
	renderer.SetSubmeshParams({ { 0, 0, 1 }, { 0, 0, 1 } });
	render(renderer);
	const Capture thin = scene.captures[0];
	Require(thin.Float(64, 64, 2) > 1.0f && thin.Float(48, 80, 2) < 1.0f,
		"Native geometry must illuminate the body-interior diagonal while flat interiors stay dark");
	Require(CountBrightPixels(thin, 2, 24, 104, 24, 104) > 100,
		"Geometry lines must appear inside the body, independently from texture strength zero");
	for (UINT i = 1; i < 4; ++i) RequireSameCapture(scene.captures[i], legacy[i],
		"Geometry emission must not modify Normal / Material MRT or body depth");
	UINT thinWidth = 0;
	for (UINT x = 52; x < 77; ++x) if (thin.Float(x, 64, 2) > 4.0f) ++thinWidth;
	Require(thinWidth >= 2 && thinWidth <= 4, "Two-pixel full width must retain a narrow antialiased diagonal");
	bool foundAntialias = false;
	for (UINT x = 52; x < 77; ++x) {
		const float emission = thin.Float(x, 64, 2) - params.bodyColor.z;
		if (emission > 0.05f && emission < params.geometryLineIntensity - 0.05f) foundAntialias = true;
	}
	Require(foundAntialias, "Native geometry boundary must contain smooth antialias coverage");
	params.geometryLineWidthPixels = 6;
	render(renderer);
	UINT wideWidth = 0;
	for (UINT x = 52; x < 77; ++x) if (scene.captures[0].Float(x, 64, 2) > 4.0f) ++wideWidth;
	Require(wideWidth >= 7 && wideWidth <= 10 && wideWidth > thinWidth * 2,
		"Six-pixel full width must enlarge diagonal coverage in pixel units");
	transform->WVP.m[0][0] = transform->WVP.m[1][1] = 0.5f;
	render(renderer);
	UINT smallTriangleWidth = 0;
	for (UINT x = 52; x < 77; ++x) if (scene.captures[0].Float(x, 64, 2) > 4.0f) ++smallTriangleWidth;
	Require(std::abs(static_cast<int>(smallTriangleWidth) - static_cast<int>(wideWidth)) <= 1,
		"Geometry line pixel width must remain stable when triangle screen size changes");
	transform->WVP.m[0][0] = transform->WVP.m[1][1] = 1;
	params.geometryLineWidthPixels = 2;
	renderer.SetSubmeshParams({ { 0, 0, 0 }, { 0, 0, 1 } });
	render(renderer);
	Require(scene.captures[0].Float(16, 80, 2) < 1.0f && scene.captures[0].Float(111, 48, 2) > 1.0f,
		"Per-submesh geometry strength must independently disable one cull variant");

	// GPU使用中に定数を共有上書きせず、同じBodyの2 Drawを別色で記録する。
	auto rightResource = dx.CreateBufferResource(256);
	TransformationMatrix* right = nullptr;
	Check(rightResource->Map(0, nullptr, reinterpret_cast<void**>(&right)), "Second geometry transform map");
	transform->WVP.m[0][0] = transform->WVP.m[1][1] = 0.5f;
	transform->WVP.m[3][0] = -0.5f;
	*right = *transform;
	right->WVP.m[3][0] = 0.5f;
	renderer.SetSubmeshParams({ { 0, 0, 1 }, { 0, 0, 1 } });
	renderer.BeginFrame();
	scene.Begin();
	params.geometryLineColor = { 1, 0, 0 };
	renderer.SetParams(params);
	renderer.Draw(model, transformResource->GetGPUVirtualAddress(), { 0, 0, -2 }, viewport);
	params.geometryLineColor = { 0, 1, 0 };
	renderer.SetParams(params);
	renderer.Draw(model, rightResource->GetGPUVirtualAddress(), { 0, 0, -2 }, viewport);
	scene.End();
	Require(scene.captures[0].Float(32, 64, 0) > 1.0f && scene.captures[0].Float(96, 64, 1) > 1.0f
		&& scene.captures[0].Float(32, 64, 1) < 1.0f && scene.captures[0].Float(96, 64, 0) < 1.0f,
		"Multiple Draw geometry colors must retain separate constant-buffer snapshots");

	// 同じ更新済みPaletteで構造線の位置が動く。Drawの中では姿勢更新しない。
	*transform = { Identity(), Identity(), Identity() };
	transform->WVP.m[2][2] = 0.4f;
	transform->WVP.m[3][2] = 0.5f;
	params.geometryLineColor = { 0, 0, 1 };
	model.Update(0.125f);
	render(renderer);
	Require(scene.captures[0].Float(72, 64, 2) > 1.0f && scene.captures[0].Float(64, 64, 2) < 1.0f,
		"Native geometry lines must follow the same GPU skinning palette as the body");
	model.Update(0);
	render(renderer, 0.1f);
	Require(CountBrightPixels(scene.captures[0], 2) == 0,
		"Foreground depth must occlude geometry lines without see-through emission");
	std::cout << "PASS: native geometry body-interior edges, zero / per-submesh strength, independent texture toggle, "
		"HDR color, smooth AA, full pixel width (" << thinWidth << " / " << wideWidth << " diagonal pixels), "
		"screen-size stability, single/double-sided PSOs, MRT / depth / stencil preservation, "
		"palette motion and per-Draw constant snapshots.\n";

	// Alpha Cutoutは構造線の有無によらずBody/Depthを同じ場所で除去する。
	SkinnedModel featureModel;
	featureModel.Initialize(&dx, &srv, "geometry feature fixture");
	params.geometryLineEnabled = 0;
	params.internalLineEnabled = 1;
	renderer.SetSubmeshParams({ { 1, 0.5f, 1 }, { 1, 0.5f, 1 } });
	auto renderCutout = [&]() {
		renderer.BeginFrame();
		renderer.SetParams(params);
		scene.Begin();
		renderer.Draw(featureModel, transformResource->GetGPUVirtualAddress(), { 0, 0, -2 }, viewport);
		scene.End();
	};
	renderCutout();
	const Capture textureOnly = scene.captures[0];
	const Capture cutoutMrt[] = { scene.captures[1], scene.captures[2], scene.captures[3] };
	params.geometryLineEnabled = 1;
	renderCutout();
	Require(scene.captures[0].Float(64, 64, 2) == 0, "Cutout holes must contain no geometry emission");
	uint32_t holeDepth = 0;
	std::memcpy(&holeDepth, scene.captures[3].Pixel(64, 64), sizeof(holeDepth));
	Require((holeDepth & 0xffffff) == 0xffffff, "Geometry cutout holes must preserve clear depth");
	for (UINT i = 1; i < 4; ++i) RequireSameCapture(scene.captures[i], cutoutMrt[i - 1],
		"Geometry variant must preserve cutout Normal / Material / depth output");
	UINT geometryOnlyPixels = 0, sharedFeaturePixels = 0;
	for (UINT y = 0; y < kSize; ++y) for (UINT x = 0; x < kSize; ++x) {
		const float addedBlue = scene.captures[0].Float(x, y, 2) - textureOnly.Float(x, y, 2);
		if (addedBlue > 1) ++geometryOnlyPixels;
		if (textureOnly.Float(x, y, 0) > 1) {
			++sharedFeaturePixels;
			Require(std::abs(scene.captures[0].Float(x, y, 0) - textureOnly.Float(x, y, 0)) < 0.001f,
				"Cyan / blue geometry must not change pink texture feature emission");
		}
	}
	Require(geometryOnlyPixels > 50 && sharedFeaturePixels > 50,
		"Geometry and texture feature layers must emit independently in the same body pass");
	params.internalLineEnabled = 0;
	renderCutout();
	Require(CountBrightPixels(scene.captures[0], 0) == 0 && CountBrightPixels(scene.captures[0], 2) > 50,
		"Texture feature disable must preserve independently enabled geometry lines");
	std::cout << "PASS: geometry + texture feature layers, independent toggles, alpha cutout holes / depth.\n";
}

void TestDraw(DirectXCommon& dx, SrvManager& srv, NeonSkinnedRenderer& renderer) {
	testDx = &dx;
	dx.Initialize(nullptr);
	D3D12_DESCRIPTOR_HEAP_DESC heap{};
	heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heap.NumDescriptors = 3;
	heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	Check(dx.GetDevice()->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&testSrvHeap)), "SRV heap");
	auto whiteTexture = CreateTestTexture(dx, srv, 1, false);
	auto featureTexture = CreateTestTexture(dx, srv, 2, true);
	SkinnedModel model;
	model.Initialize(&dx, &srv, "synthetic test fixture");
	OffscreenScene scene(dx);
	auto transformResource = dx.CreateBufferResource(256);
	TransformationMatrix* transform = nullptr;
	Check(transformResource->Map(0, nullptr, reinterpret_cast<void**>(&transform)), "Transform map");
	*transform = { Identity(), Identity(), Identity() };
	transform->WVP.m[2][2] = 0.4f;
	transform->WVP.m[3][2] = 0.5f;
	const Vector2 viewport{ static_cast<float>(kSize), static_cast<float>(kSize) };
	NeonSkinnedParams params;
	auto render = [&](float depth = 1.0f) {
		renderer.BeginFrame();
		renderer.SetParams(params);
		scene.Begin(depth);
		renderer.Draw(model, transformResource->GetGPUVirtualAddress(), { 0, 0, -2 }, viewport);
		scene.End();
	};
	params.outlineEnabled = 0;
	render();
	const auto body = scene.captures[0];
	Require(std::abs(body.Float(64, 64, 0) - params.bodyColor.x) < 0.0001f, "Default body must stay dark");
	params.outlineEnabled = 1;
	params.outlineWidthPixels = 2.0f;
	render();
	UINT thinPixels = 0;
	for (UINT y = 0; y < kSize; ++y) for (UINT x = 0; x < kSize; ++x) {
		const float red = scene.captures[0].Float(x, y, 0);
		if (body.Float(x, y, 0) > 0.001f) Require(std::abs(red - body.Float(x, y, 0)) < 0.0001f,
			"Outline must not illuminate model interiors or submesh seams");
		if (red > 1.0f) {
			++thinPixels;
			Require(std::abs(red - params.emissiveIntensity) < 0.01f, "Outline must retain HDR emission");
			Require(scene.captures[1].Float(x, y, 0) == 0.0f && scene.captures[2].Pixel(x, y)[0] == 0,
				"Outline must preserve Normal / Material MRT outside the body");
			uint32_t depth;
			std::memcpy(&depth, scene.captures[3].Pixel(x, y), sizeof(depth));
			Require((depth & 0xffffff) == 0xffffff, "Outline must not write depth");
		}
	}
	Require(thinPixels > 200 && thinPixels < 700, "Thin exterior outline must render");
	params.outlineWidthPixels = 4.0f;
	render();
	UINT widePixels = 0;
	for (UINT y = 0; y < kSize; ++y) for (UINT x = 0; x < kSize; ++x)
		if (scene.captures[0].Float(x, y, 0) > 1.0f) ++widePixels;
	Require(widePixels > thinPixels * 1.5f, "Pixel width control must enlarge the outline");
	render(0.1f);
	for (UINT y = 0; y < kSize; ++y) for (UINT x = 0; x < kSize; ++x)
		Require(scene.captures[0].Float(x, y, 0) == 0.0f, "Foreground depth must occlude body and outline");

	// Paletteの移動にBodyとOutlineの両方が追従することを確認する。
	params.outlineEnabled = 0;
	model.Update(0.25f);
	render();
	double originalX = 0, movedX = 0;
	UINT originalCount = 0, movedCount = 0;
	for (UINT y = 0; y < kSize; ++y) for (UINT x = 0; x < kSize; ++x) {
		if (body.Float(x, y, 0) > 0.001f) { originalX += x; ++originalCount; }
		if (scene.captures[0].Float(x, y, 0) > 0.001f) { movedX += x; ++movedCount; }
	}
	Require(movedCount > 0 && std::abs(movedX / movedCount - originalX / originalCount - 16.0) < 1.0,
		"Body GPU skinning must follow the updated palette");
	params.outlineEnabled = 1;
	render();
	Require(scene.captures[0].Float(114, 64, 0) > 1.0f, "Outline must follow the same updated skinning palette");
	model.Update(0.0f);

	// 同一フレームの複数Drawで、それぞれの色のCB snapshotが保持されることを確認する。
	auto rightResource = dx.CreateBufferResource(256);
	TransformationMatrix* right = nullptr;
	Check(rightResource->Map(0, nullptr, reinterpret_cast<void**>(&right)), "Second transform map");
	transform->WVP.m[0][0] = transform->WVP.m[1][1] = 0.6f;
	transform->WVP.m[3][0] = -0.5f;
	*right = *transform;
	right->WVP.m[3][0] = 0.5f;
	renderer.BeginFrame();
	scene.Begin();
	params.emissiveColor = { 1, 0, 0 };
	renderer.SetParams(params);
	renderer.Draw(model, transformResource->GetGPUVirtualAddress(), { 0, 0, -2 }, viewport);
	params.emissiveColor = { 0, 1, 0 };
	renderer.SetParams(params);
	renderer.Draw(model, rightResource->GetGPUVirtualAddress(), { 0, 0, -2 }, viewport);
	scene.End();
	Require(scene.captures[0].Float(10, 64, 0) > 1.0f && scene.captures[0].Float(117, 64, 1) > 1.0f,
		"Multiple Draw constants must not overwrite earlier outline colors");
	std::cout << "PASS: GPU draw checks dark interiors, single/double-sided submeshes, HDR exterior outline ("
		<< thinPixels << " / " << widePixels << " pixels), width, depth occlusion, MRT preservation, "
		"stencil cleanup / other bits, palette motion and per-draw constants.\n";

	// 同じGeometryでTexture由来の内部線のみを切り替える。
	SkinnedModel featureModel;
	featureModel.Initialize(&dx, &srv, "feature fixture");
	*transform = { Identity(), Identity(), Identity() };
	transform->WVP.m[2][2] = 0.4f;
	transform->WVP.m[3][2] = 0.5f;
	params = NeonSkinnedParams{};
	params.outlineEnabled = 0;
	params.internalLineEnabled = 1;
	auto renderFeatures = [&]() {
		renderer.BeginFrame();
		renderer.SetParams(params);
		scene.Begin();
		renderer.Draw(featureModel, transformResource->GetGPUVirtualAddress(), { 0, 0, -2 }, viewport);
		scene.End();
	};
	renderFeatures();
	UINT internalPixels = 0, darkPixels = 0;
	for (UINT y = 0; y < kSize; ++y) for (UINT x = 0; x < kSize; ++x) {
		const float red = scene.captures[0].Float(x, y, 0);
		if (red > 1.0f) ++internalPixels;
		else if (red > 0.001f) ++darkPixels;
	}
	Require(internalPixels > 50 && darkPixels > 500, "Texture edges must emit HDR while flat body stays dark");
	params.internalLineEnabled = 0;
	renderFeatures();
	for (UINT y = 0; y < kSize; ++y) for (UINT x = 0; x < kSize; ++x)
		Require(scene.captures[0].Float(x, y, 0) < 1.0f, "Internal line disable must restore the dark body");
	Require(scene.captures[0].Float(64, 64, 0) > 0.001f, "Default Neon must keep the legacy opaque surface");
	renderer.SetSubmeshParams({ { 1.0f, 0.5f }, { 1.0f, 0.5f } });
	renderFeatures();
	Require(scene.captures[0].Float(64, 64, 0) == 0.0f, "Alpha cutout must expose transparent texels");
	uint32_t holeDepth;
	std::memcpy(&holeDepth, scene.captures[3].Pixel(64, 64), sizeof(holeDepth));
	Require((holeDepth & 0xffffff) == 0xffffff, "Alpha cutout must not write depth in transparent holes");
	params.internalLineEnabled = 1;
	renderer.SetSubmeshParams({ { 0.0f, 0.0f }, { 0.0f, 0.0f } });
	renderFeatures();
	for (UINT y = 0; y < kSize; ++y) for (UINT x = 0; x < kSize; ++x)
		Require(scene.captures[0].Float(x, y, 0) < 1.0f, "Submesh strength zero must disable its feature lines");
	std::cout << "PASS: texture feature lines (" << internalPixels << " HDR pixels), dark flat regions, "
		"toggle, per-submesh strength and opt-in alpha cutout / transparent depth.\n";
}
}

int main(int argc, char** argv) {
	try {
		std::cout << std::unitbuf;
		std::wcout << std::unitbuf;
		const bool hardware = argc == 2 && std::strcmp(argv[1], "--hardware") == 0;
		Require(argc == 1 || hardware, "Usage: neon_skinned_pipeline_tests [--hardware]");
		ComPtr<ID3D12Debug> debug;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) debug->EnableDebugLayer();
		ComPtr<IDXGIFactory4> factory;
		Check(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "DXGI factory");
		ComPtr<IDXGIAdapter1> adapter;
		DirectXCommon dx;
		if (hardware) {
			ComPtr<IDXGIFactory6> preferenceFactory;
			Check(factory.As(&preferenceFactory), "GPU preference factory");
			for (UINT index = 0;; ++index) {
				const HRESULT enumeration = preferenceFactory->EnumAdapterByGpuPreference(index,
					DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter));
				if (enumeration == DXGI_ERROR_NOT_FOUND) break;
				Check(enumeration, "Hardware adapter enumeration");
				DXGI_ADAPTER_DESC1 desc{};
				Check(adapter->GetDesc1(&desc), "Hardware adapter description");
				if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0
					&& SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&dx.GetDevice())))) break;
				adapter.Reset();
			}
			if (!dx.GetDevice()) {
				std::cout << "SKIP: no DirectX 12 hardware adapter is available. Run without --hardware for WARP tests.\n";
				return 0;
			}
		} else {
			Check(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)), "WARP adapter");
			Check(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&dx.GetDevice())), "WARP device");
		}
		DXGI_ADAPTER_DESC1 adapterDesc{};
		Check(adapter->GetDesc1(&adapterDesc), "Adapter description");
		std::wcout << L"Device: " << adapterDesc.Description << (hardware ? L" (hardware)\n" : L" (WARP)\n");
		D3D12_FEATURE_DATA_SHADER_MODEL shaderModel{ D3D_SHADER_MODEL_6_1 };
		const HRESULT shaderModelResult = dx.GetDevice()->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL,
			&shaderModel, sizeof(shaderModel));
		D3D12_FEATURE_DATA_D3D12_OPTIONS3 options3{};
		const HRESULT optionsResult = dx.GetDevice()->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS3,
			&options3, sizeof(options3));
		const bool deviceSupportsGeometry = SUCCEEDED(shaderModelResult) && shaderModel.HighestShaderModel >= D3D_SHADER_MODEL_6_1
			&& SUCCEEDED(optionsResult) && options3.BarycentricsSupported;
		std::cout << "Shader model query: HRESULT 0x" << std::hex << static_cast<UINT>(shaderModelResult)
			<< ", returned 0x" << static_cast<UINT>(shaderModel.HighestShaderModel)
			<< "; OPTIONS3: HRESULT 0x" << static_cast<UINT>(optionsResult) << std::dec
			<< ", BarycentricsSupported=" << (options3.BarycentricsSupported ? "true" : "false") << '\n';
		SrvManager srv;
		NeonSkinnedRenderer renderer;
		renderer.Initialize(&dx, &srv);
		Require(renderer.IsGeometryLinesSupported() == deviceSupportsGeometry,
			"Renderer native geometry capability must agree with both Device feature queries");
		Require(!renderer.GetGeometryLinesStatus().empty(), "Geometry support status must explain the Device result");
		Require(compiledNeonShaders == (deviceSupportsGeometry ? 7u : 6u),
			"Renderer must compile six legacy shaders and only the supported-device geometry variant");
		std::cout << "Geometry status: " << renderer.GetGeometryLinesStatus() << '\n';
		if (!deviceSupportsGeometry) {
			ComPtr<IDxcBlob> geometry;
			geometry.Attach(dx.CompileShader(L"resources/shaders/NeonSkinnedGeometry.PS.hlsl", L"ps_6_1"));
		}
		Require(compiledNeonShaders == 7, "DXC must compile the geometry ps_6_1 variant even on an unsupported Device");

		// GUIや将来の呼び出しからの不正値は、構造線パラメータだけ安全な有限範囲へ収める。
		NeonSkinnedParams invalid;
		invalid.geometryLineEnabled = UINT32_MAX;
		invalid.geometryLineWidthPixels = std::numeric_limits<float>::quiet_NaN();
		invalid.geometryLineIntensity = std::numeric_limits<float>::infinity();
		invalid.geometryLineColor = { -1, std::numeric_limits<float>::quiet_NaN(), 200 };
		renderer.SetParams(invalid);
		const auto& sanitized = renderer.GetParams();
		Require(sanitized.geometryLineEnabled == 1 && std::isfinite(sanitized.geometryLineWidthPixels)
			&& sanitized.geometryLineWidthPixels >= 0 && sanitized.geometryLineWidthPixels <= 8
			&& std::isfinite(sanitized.geometryLineIntensity) && sanitized.geometryLineIntensity >= 0
			&& sanitized.geometryLineIntensity <= 100 && std::isfinite(sanitized.geometryLineColor.x)
			&& sanitized.geometryLineColor.x >= 0 && sanitized.geometryLineColor.x <= 100
			&& std::isfinite(sanitized.geometryLineColor.y) && sanitized.geometryLineColor.y >= 0
			&& sanitized.geometryLineColor.y <= 100 && std::isfinite(sanitized.geometryLineColor.z)
			&& sanitized.geometryLineColor.z >= 0 && sanitized.geometryLineColor.z <= 100,
			"Geometry parameters must sanitize finite values and valid ranges");
		std::cout << "PASS: geometry parameter finite / range validation and default OFF.\n";
		// 通常Skinning / ShadowのShaderも同じDXC条件でコンパイルできることを確認する。
		ComPtr<IDxcBlob> standard;
		ComPtr<IDxcBlob> shadow;
		standard.Attach(dx.CompileShader(L"resources/shaders/SkinningObject3d.VS.hlsl", L"vs_6_0"));
		shadow.Attach(dx.CompileShader(L"resources/shaders/SkinningShadow.VS.hlsl", L"vs_6_0"));
		TestDraw(dx, srv, renderer);
		TestGeometryLines(dx, srv, renderer);
		ComPtr<ID3D12InfoQueue> infoQueue;
		if (SUCCEEDED(dx.GetDevice().As(&infoQueue))) {
			for (UINT64 index = 0; index < infoQueue->GetNumStoredMessages(); ++index) {
				SIZE_T bytes = 0;
				Check(infoQueue->GetMessage(index, nullptr, &bytes), "Debug message size");
				std::vector<char> storage(bytes);
				auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
				Check(infoQueue->GetMessage(index, message, &bytes), "Debug message");
				if (message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR) {
					std::cerr << message->pDescription << '\n';
					throw std::runtime_error("D3D12 debug layer reported a pipeline error");
				}
			}
		}
		std::cout << "PASS: actual Neon Renderer creates its root signature and five legacy Body / Outline / Stencil PSOs"
			<< (deviceSupportsGeometry ? " plus two native geometry cull variants; " : "; ")
			<< "C++ / HLSL CB / root-constant layouts match; Neon, geometry ps_6_1, standard Skinning and Shadow DXIL compile. "
			"Synthetic offscreen draw verified; live AvatarSample_B preview is a separate manual check.\n";
		return 0;
	} catch (const std::exception& error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
