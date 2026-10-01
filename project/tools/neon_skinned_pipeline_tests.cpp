// 独立したWARP検証。ゲーム/Preview導線やモデル素材は必要としない。
// 描画サービスをWARPへ接続し、実際のRendererのPSO作成・Drawを実行する。
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
#include <DirectXPackedVector.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")

using Microsoft::WRL::ComPtr;

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

	if (filePath.find(L"NeonSkinned.PS.hlsl") != std::wstring::npos) {
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
			{ "gCameraWorldPosition", sizeof(NeonSkinnedParams) },
			{ "gViewportSize", sizeof(NeonSkinnedParams) + 16 },
		};
		for (const auto& field : expected) {
			D3D12_SHADER_VARIABLE_DESC variable{};
			Check(constants->GetVariableByName(field.name)->GetDesc(&variable), "Neon constant field reflection");
			Require(variable.StartOffset == field.offset, "C++ / HLSL constant field offset differs");
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
	asset_.modelData.materials[0].textureIndex = fixtureName == "feature fixture" ? 2 : 1;
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

int main() {
	try {
		ComPtr<ID3D12Debug> debug;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) debug->EnableDebugLayer();
		ComPtr<IDXGIFactory4> factory;
		Check(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "DXGI factory");
		ComPtr<IDXGIAdapter> warp;
		Check(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)), "WARP adapter");
		DirectXCommon dx;
		Check(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&dx.GetDevice())), "WARP device");
		SrvManager srv;
		NeonSkinnedRenderer renderer;
		renderer.Initialize(&dx, &srv);
		Require(compiledNeonShaders == 6, "Neon shaders were not compiled by the actual Renderer");
		// 通常Skinning / ShadowのShaderも同じDXC条件でコンパイルできることを確認する。
		ComPtr<IDxcBlob> standard;
		ComPtr<IDxcBlob> shadow;
		standard.Attach(dx.CompileShader(L"resources/shaders/SkinningObject3d.VS.hlsl", L"vs_6_0"));
		shadow.Attach(dx.CompileShader(L"resources/shaders/SkinningShadow.VS.hlsl", L"vs_6_0"));
		TestDraw(dx, srv, renderer);
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
		std::cout << "PASS: actual Neon Renderer creates its root signature and all five Body / Outline / Stencil WARP PSOs; "
			"C++ / HLSL CB layout matches; Neon, standard Skinning and Shadow DXIL compile. "
			"Synthetic offscreen draw verified; live AvatarSample_B preview is a separate manual check.\n";
		return 0;
	} catch (const std::exception& error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
