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
#include <array>
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
std::vector<size_t> requestedUploadBytes;
DirectXCommon* testDx = nullptr;
ComPtr<ID3D12DescriptorHeap> testSrvHeap;
uint32_t nextTestSrvIndex = 16; // 0: Palette, 1/2: BaseColor, 3..15: authored-mask fixtures.
std::array<bool, 64> allocatedTestSrvIndices{};
unsigned testDescriptorFrees = 0;

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
	const bool surfaceShader = filePath.find(L"NeonSkinned.PS.hlsl") != std::wstring::npos || geometryShader;
	const bool outlineShader = filePath.find(L"NeonSkinnedOutline.PS.hlsl") != std::wstring::npos
		|| filePath.find(L"NeonSkinnedOutline.VS.hlsl") != std::wstring::npos;
	if (surfaceShader || outlineShader) {
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
			{ "gBodyEmissionIntensity", static_cast<UINT>(offsetof(NeonSkinnedParams, bodyEmissionIntensity)) },
			{ "gFeatureMaskColor", static_cast<UINT>(offsetof(NeonSkinnedParams, featureMaskColor)) },
			{ "gFeatureMaskIntensity", static_cast<UINT>(offsetof(NeonSkinnedParams, featureMaskIntensity)) },
			{ "gFeatureMaskBlend", static_cast<UINT>(offsetof(NeonSkinnedParams, featureMaskBlend)) },
			{ "gFeatureMaskDebugMode", static_cast<UINT>(offsetof(NeonSkinnedParams, featureMaskDebugMode)) },
			{ "gLineCoreColor", static_cast<UINT>(offsetof(NeonSkinnedParams, lineCoreColor)) },
			{ "gLineCoreIntensity", static_cast<UINT>(offsetof(NeonSkinnedParams, lineCoreIntensity)) },
			{ "gLineHaloColor", static_cast<UINT>(offsetof(NeonSkinnedParams, lineHaloColor)) },
			{ "gLineHaloIntensity", static_cast<UINT>(offsetof(NeonSkinnedParams, lineHaloIntensity)) },
			{ "gOutlineCoreColor", static_cast<UINT>(offsetof(NeonSkinnedParams, outlineCoreColor)) },
			{ "gOutlineCoreIntensity", static_cast<UINT>(offsetof(NeonSkinnedParams, outlineCoreIntensity)) },
			{ "gFeatureMaskRenderMode", static_cast<UINT>(offsetof(NeonSkinnedParams, featureMaskRenderMode)) },
			{ "gSplitLineEmission", static_cast<UINT>(offsetof(NeonSkinnedParams, splitLineEmission)) },
			{ "gLineDiagnosticMode", static_cast<UINT>(offsetof(NeonSkinnedParams, lineDiagnosticMode)) },
			{ "gSdfRangeTexels", static_cast<UINT>(offsetof(NeonSkinnedParams, sdfRangeTexels)) },
			{ "gSdfHaloWidthTexels", static_cast<UINT>(offsetof(NeonSkinnedParams, sdfHaloWidthTexels)) },
			{ "gSdfLodBlendStart", static_cast<UINT>(offsetof(NeonSkinnedParams, sdfLodBlendStart)) },
			{ "gSdfLodBlendEnd", static_cast<UINT>(offsetof(NeonSkinnedParams, sdfLodBlendEnd)) },
			{ "gDissolveDirection", offsetof(NeonSkinnedParams, dissolve) + offsetof(NeonDissolveParams, direction) },
			{ "gDissolveScanMin", offsetof(NeonSkinnedParams, dissolve) + offsetof(NeonDissolveParams, scanMin) },
			{ "gDissolveScanMax", offsetof(NeonSkinnedParams, dissolve) + offsetof(NeonDissolveParams, scanMax) },
			{ "gDissolveProgress", offsetof(NeonSkinnedParams, dissolve) + offsetof(NeonDissolveParams, progress) },
			{ "gDissolveNoiseStrength", offsetof(NeonSkinnedParams, dissolve) + offsetof(NeonDissolveParams, noiseStrength) },
			{ "gDissolveNoiseScale", offsetof(NeonSkinnedParams, dissolve) + offsetof(NeonDissolveParams, noiseScale) },
			{ "gDissolveEnabled", offsetof(NeonSkinnedParams, dissolve) + offsetof(NeonDissolveParams, enabled) },
			{ "gDissolveEdgeEnabled", offsetof(NeonSkinnedParams, dissolve) + offsetof(NeonDissolveParams, edgeEnabled) },
			{ "gDissolveSeed", offsetof(NeonSkinnedParams, dissolve) + offsetof(NeonDissolveParams, seed) },
			{ "gDissolvePadding", offsetof(NeonSkinnedParams, dissolve) + offsetof(NeonDissolveParams, padding) },
			{ "gDissolveEdgeColor", offsetof(NeonSkinnedParams, dissolve) + offsetof(NeonDissolveParams, edgeColor) },
			{ "gDissolveEdgeIntensity", offsetof(NeonSkinnedParams, dissolve) + offsetof(NeonDissolveParams, edgeIntensity) },
			{ "gDissolveEdgeWidth", offsetof(NeonSkinnedParams, dissolve) + offsetof(NeonDissolveParams, edgeWidth) },
			{ "gDissolveEdgePadding", offsetof(NeonSkinnedParams, dissolve) + offsetof(NeonDissolveParams, edgePadding) },
			{ "gCameraWorldPosition", sizeof(NeonSkinnedParams) },
			{ "gViewportSize", sizeof(NeonSkinnedParams) + 16 },
		};
		for (const auto& field : expected) {
			D3D12_SHADER_VARIABLE_DESC variable{};
			Check(constants->GetVariableByName(field.name)->GetDesc(&variable), "Neon constant field reflection");
			Require(variable.StartOffset == field.offset, "C++ / HLSL constant field offset differs");
			if (std::strncmp(field.name,"gDissolve",8)==0) {
				D3D12_SHADER_TYPE_DESC type{};
				Check(constants->GetVariableByName(field.name)->GetType()->GetDesc(&type),"Dissolve constant type reflection");
				const bool integer=std::strcmp(field.name,"gDissolveEnabled")==0 || std::strcmp(field.name,"gDissolveEdgeEnabled")==0
					|| std::strcmp(field.name,"gDissolveSeed")==0 || std::strcmp(field.name,"gDissolvePadding")==0;
				const UINT columns=std::strcmp(field.name,"gDissolveDirection")==0 || std::strcmp(field.name,"gDissolveEdgeColor")==0
					|| std::strcmp(field.name,"gDissolveEdgePadding")==0 ? 3u : 1u;
				Require(type.Type==(integer ? D3D_SVT_UINT : D3D_SVT_FLOAT) && type.Rows==1 && type.Columns==columns,
					"Dissolve C++ / HLSL scalar type and vector dimensions must agree");
			}
		}
		if (surfaceShader) {
		auto* submeshConstants = reflection->GetConstantBufferByName("NeonSubmeshConstants");
		D3D12_SHADER_BUFFER_DESC submeshDesc{};
		Check(submeshConstants->GetDesc(&submeshDesc), "Submesh constant reflection");
		Require(submeshDesc.Size == 32 && sizeof(NeonSkinnedSubmeshParams) == 16,
			"Root constants must preserve the four surface DWORDs and append the distance-valid flag and padding");
		const struct { const char* name; UINT offset; } submeshFields[] = {
			{ "gSubmeshLineStrength", offsetof(NeonSkinnedSubmeshParams, lineStrength) },
			{ "gAlphaCutoff", offsetof(NeonSkinnedSubmeshParams, alphaCutoff) },
			{ "gSubmeshGeometryStrength", offsetof(NeonSkinnedSubmeshParams, geometryLineStrength) },
			{ "gSubmeshInternalThresholdScale", offsetof(NeonSkinnedSubmeshParams, internalLineThresholdScale) },
			{ "gHasDistanceMask", 16 },
			{ "gSubmeshPadding", 20 },
		};
		for (const auto& field : submeshFields) {
			D3D12_SHADER_VARIABLE_DESC variable{};
			Check(submeshConstants->GetVariableByName(field.name)->GetDesc(&variable), "Submesh field reflection");
			Require(variable.StartOffset == field.offset, "Submesh root constant offset differs");
		}
		D3D12_SHADER_INPUT_BIND_DESC maskBinding{};
		Check(reflection->GetResourceBindingDescByName("gFeatureMaskTexture", &maskBinding), "Feature mask SRV reflection");
		Require(maskBinding.Type == D3D_SIT_TEXTURE && maskBinding.BindPoint == 1 && maskBinding.BindCount == 1
			&& maskBinding.Space == 0 && maskBinding.Dimension == D3D_SRV_DIMENSION_TEXTURE2D,
			"Authored line data must bind its own Texture2D SRV at t1 space0");
		D3D12_SHADER_INPUT_BIND_DESC distanceBinding{};
		Check(reflection->GetResourceBindingDescByName("gFeatureDistanceTexture", &distanceBinding), "Distance mask SRV reflection");
		Require(distanceBinding.Type == D3D_SIT_TEXTURE && distanceBinding.BindPoint == 2 && distanceBinding.BindCount == 1
			&& distanceBinding.Space == 0 && distanceBinding.Dimension == D3D_SRV_DIMENSION_TEXTURE2D,
			"Signed distance data must have its independent Texture2D at t2 space0");
		if (geometryShader) {
			// Native barycentricsはDXIL intrinsicとして扱われ、通常の入力signatureには現れない。
			Require((reflection->GetRequiresFlags() & D3D_SHADER_REQUIRES_BARYCENTRICS) != 0,
				"Geometry shader DXIL must require native barycentrics");
		}
		}
	}
	const bool neonVertex = filePath.find(L"NeonSkinned.VS.hlsl") != std::wstring::npos
		|| filePath.find(L"NeonSkinnedOutline.VS.hlsl") != std::wstring::npos;
	if (neonVertex || surfaceShader || filePath.find(L"NeonSkinnedOutline.PS.hlsl") != std::wstring::npos) {
		ComPtr<IDxcBlob> data;
		Check(result->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&data), nullptr), "Model-space position reflection data");
		DxcBuffer buffer{data->GetBufferPointer(),data->GetBufferSize(),0};
		ComPtr<ID3D12ShaderReflection> reflection;
		Check(utils->CreateReflection(&buffer,IID_PPV_ARGS(&reflection)),"Model-space position signature reflection");
		D3D12_SHADER_DESC desc{};
		Check(reflection->GetDesc(&desc),"Neon signature description");
		bool modelPosition=false;
		for (UINT i=0;i<(neonVertex ? desc.OutputParameters : desc.InputParameters);++i) {
			D3D12_SIGNATURE_PARAMETER_DESC parameter{};
			Check(neonVertex ? reflection->GetOutputParameterDesc(i,&parameter) : reflection->GetInputParameterDesc(i,&parameter),
				"Neon model-space signature parameter");
			if (_stricmp(parameter.SemanticName,"TEXCOORD")==0 && parameter.SemanticIndex==1) {
				modelPosition=parameter.ComponentType==D3D_REGISTER_COMPONENT_FLOAT32 && parameter.Mask==7;
			}
		}
		Require(modelPosition,"Body and Hull must share a full float3 pre-expansion skinned model position at TEXCOORD1");
	}
	if (filePath.find(L"NeonSkinned") != std::wstring::npos) ++compiledNeonShaders;
	ComPtr<IDxcBlob> blob;
	Check(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&blob), nullptr), "DXIL object");
	return blob.Detach();
}

ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResource(size_t bytes) {
	requestedUploadBytes.push_back(bytes);
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

const uint32_t SrvManager::kMaxSrvCount = 8192;

uint32_t SrvManager::Allocate() {
	Require(nextTestSrvIndex < 64, "Test SRV heap exhausted");
	allocatedTestSrvIndices[nextTestSrvIndex] = true;
	return nextTestSrvIndex++;
}

void SrvManager::Free(uint32_t index) {
	Require(index >= 16 && index < allocatedTestSrvIndices.size() && allocatedTestSrvIndices[index],
		"Renderer descriptor was invalid or returned twice");
	allocatedTestSrvIndices[index] = false;
	++testDescriptorFrees;
}

D3D12_CPU_DESCRIPTOR_HANDLE SrvManager::GetCPUDescriptorHandle(uint32_t index) {
	auto handle = testSrvHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += static_cast<SIZE_T>(index) * testDx->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	return handle;
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

ComPtr<ID3D12Resource> CreateTestMask(DirectXCommon& dx, SrvManager& srv, UINT index,
	const std::array<uint8_t, 4>& channels, bool quadrants = false) {
	// Known coverage values, not color. A=0 deliberately checks that RGB is neither premultiplied nor clipped.
	D3D12_RESOURCE_DESC desc{};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = desc.Height = 4;
	desc.DepthOrArraySize = desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	D3D12_HEAP_PROPERTIES heap{};
	heap.Type = D3D12_HEAP_TYPE_DEFAULT;
	ComPtr<ID3D12Resource> texture;
	Check(dx.GetDevice()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&texture)), "Linear mask texture");
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
	UINT64 total = 0;
	dx.GetDevice()->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, nullptr, nullptr, &total);
	auto upload = dx.CreateBufferResource(static_cast<size_t>(total));
	uint8_t* mapped = nullptr;
	Check(upload->Map(0, nullptr, reinterpret_cast<void**>(&mapped)), "Mask upload map");
	for (UINT y = 0; y < 4; ++y) for (UINT x = 0; x < 4; ++x) {
		auto* pixel = mapped + footprint.Offset + y * footprint.Footprint.RowPitch + x * 4;
		std::memcpy(pixel, channels.data(), channels.size());
		if (quadrants) pixel[0] = static_cast<uint8_t>(64 * (1 + (x >= 2 ? 1 : 0) + (y >= 2 ? 2 : 0)) - (x >= 2 && y >= 2 ? 1 : 0));
	}
	upload->Unmap(0, nullptr);
	D3D12_TEXTURE_COPY_LOCATION source{}, destination{};
	source.pResource = upload.Get();
	source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	source.PlacedFootprint = footprint;
	destination.pResource = texture.Get();
	destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dx.GetList()->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
	Transition(dx.GetList().Get(), texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	dx.PostDraw();
	D3D12_SHADER_RESOURCE_VIEW_DESC view{};
	view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	view.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // LinearData contract: no SRGB conversion.
	view.Texture2D.MipLevels = 1;
	dx.GetDevice()->CreateShaderResourceView(texture.Get(), &view, srv.GetCPUDescriptorHandle(index));
	return texture;
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

	void End(uint8_t expectedStencil = 0x25) {
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
			Require(stencil.Pixel(x, y)[stencil.rowBytes / kSize - 1] == expectedStencil,
				"Scene must preserve the expected reserved stencil bit and other stencil bits");
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

void TestBodyEmission(DirectXCommon& dx, SrvManager& srv, NeonSkinnedRenderer& renderer) {
	auto whiteTexture = CreateTestTexture(dx, srv, 1, false);
	auto featureTexture = CreateTestTexture(dx, srv, 2, true);
	SkinnedModel model;
	model.Initialize(&dx, &srv, "geometry fixture");
	OffscreenScene scene(dx);
	auto transformResource = dx.CreateBufferResource(256);
	TransformationMatrix* transform = nullptr;
	Check(transformResource->Map(0, nullptr, reinterpret_cast<void**>(&transform)), "Body emission transform map");
	*transform = { Identity(), Identity(), Identity() };
	transform->WVP.m[2][2] = 0.4f;
	transform->WVP.m[3][2] = 0.5f;
	const Vector2 viewport{ static_cast<float>(kSize), static_cast<float>(kSize) };
	NeonSkinnedParams params;
	Require(params.bodyEmissionIntensity == 0 && params.geometryLineEnabled == 0,
		"Body emission and geometry lines must remain opt-in");
	params.outlineEnabled = 0;
	params.internalLineEnabled = 0;
	params.bodyColor = { 0.025f, 0.007f, 0.04f, 1 };
	const Vector3 frontalCamera{ 0, 0, -2 };
	auto render = [&](const Vector3& camera = Vector3{ 0, 0, -2 }, float depth = 1.0f) {
		renderer.BeginFrame();
		renderer.SetParams(params);
		scene.Begin(depth);
		renderer.Draw(model, transformResource->GetGPUVirtualAddress(), camera, viewport);
		scene.End();
	};
	renderer.SetSubmeshParams({ { 0, 0, 0 }, { 0, 0, 0 } });
	render();
	const Capture legacy[] = { scene.captures[0], scene.captures[1], scene.captures[2], scene.captures[3], scene.captures[4] };
	Require(std::abs(legacy[0].Float(64, 64, 0) - params.bodyColor.x) < 0.0001f,
		"Body emission default zero must retain the dark body color");
	params.bodyEmissionIntensity = 1;
	render();
	const Capture emitted = scene.captures[0];
	// 平面のworld normal=(0,0,-1)、WVPはxy等倍。このpixelのworld positionから独立に評価する。
	const Vector3 worldPosition{ (64.5f / 64.0f) - 1.0f, 1.0f - (64.5f / 64.0f), 0 };
	const float viewX = frontalCamera.x - worldPosition.x;
	const float viewY = frontalCamera.y - worldPosition.y;
	const float viewZ = frontalCamera.z - worldPosition.z;
	const float facing = -viewZ / std::sqrt(viewX * viewX + viewY * viewY + viewZ * viewZ);
	const float multiplier = 1.0f + params.bodyEmissionIntensity * (0.25f + 0.75f * facing);
	const float bodyComponents[] = { params.bodyColor.x, params.bodyColor.y, params.bodyColor.z };
	for (UINT component = 0; component < 3; ++component)
		Require(std::abs(emitted.Float(64, 64, component) - bodyComponents[component] * multiplier) < 0.0001f,
			"Weak view-facing body emission must follow the independent pink / purple body color");
	Require(emitted.Float(64, 64, 3) == legacy[0].Float(64, 64, 3), "Body emission must preserve surface alpha");
	for (UINT i = 1; i < 5; ++i) RequireSameCapture(scene.captures[i], legacy[i],
		"Body emission must not alter Normal / Material / Depth / Stencil output");
	if (renderer.IsGeometryLinesSupported()) {
		params.geometryLineEnabled = 1;
		renderer.SetSubmeshParams({ { 0, 0, 1 }, { 0, 0, 1 } });
		render();
		for (UINT component = 0; component < 3; ++component)
			Require(scene.captures[0].Float(48, 80, component) == emitted.Float(48, 80, component),
				"Geometry PS variant must preserve the same weak body emission away from mesh edges");
		Require(scene.captures[0].Float(64, 64, 2) > emitted.Float(64, 64, 2) + 1,
			"Body emission must coexist with independently enabled geometry diagnostic lines");
		params.geometryLineEnabled = 0;
		renderer.SetSubmeshParams({ { 0, 0, 0 }, { 0, 0, 0 } });
	}
	render({ 2, 0, -0.05f });
	Require(scene.captures[0].Float(64, 64, 0) > params.bodyColor.x
		&& scene.captures[0].Float(64, 64, 0) < emitted.Float(64, 64, 0) * 0.8f,
		"Body emission must retain a weak base and soften on view-grazing surfaces");
	params.emissiveColor = { 0, 0, 0 };
	params.emissiveIntensity = 0;
	render();
	RequireSameCapture(scene.captures[0], emitted, "Body emission must not depend on outline / texture emissive color or intensity");
	params.bodyEmissionIntensity = 0;
	render();
	for (UINT i = 0; i < 5; ++i) RequireSameCapture(scene.captures[i], legacy[i],
		"Returning body emission to zero must restore the legacy path exactly");
	params.bodyEmissionIntensity = 1;
	render(frontalCamera, 0.1f);
	Require(CountBrightPixels(scene.captures[0], 0) == 0 && scene.captures[0].Float(64, 64, 0) == 0,
		"Foreground depth must hide body emission");

	// 同一frameの別Drawで、bodyIntensityが次のDrawの値に上書きされない。
	auto rightResource = dx.CreateBufferResource(256);
	TransformationMatrix* right = nullptr;
	Check(rightResource->Map(0, nullptr, reinterpret_cast<void**>(&right)), "Second body emission transform map");
	transform->WVP.m[0][0] = transform->WVP.m[1][1] = 0.5f;
	transform->WVP.m[3][0] = -0.5f;
	*right = *transform;
	right->WVP.m[3][0] = 0.5f;
	renderer.BeginFrame();
	scene.Begin();
	params.bodyEmissionIntensity = 0;
	renderer.SetParams(params);
	renderer.Draw(model, transformResource->GetGPUVirtualAddress(), frontalCamera, viewport);
	params.bodyEmissionIntensity = 1;
	renderer.SetParams(params);
	renderer.Draw(model, rightResource->GetGPUVirtualAddress(), frontalCamera, viewport);
	scene.End();
	Require(std::abs(scene.captures[0].Float(32, 64, 0) - params.bodyColor.x) < 0.0001f
		&& scene.captures[0].Float(96, 64, 0) > params.bodyColor.x * 1.9f,
		"Multiple Draw body intensities must retain independent CB snapshots");

	SkinnedModel cutoutModel;
	cutoutModel.Initialize(&dx, &srv, "geometry feature fixture");
	*transform = { Identity(), Identity(), Identity() };
	transform->WVP.m[2][2] = 0.4f;
	transform->WVP.m[3][2] = 0.5f;
	renderer.SetSubmeshParams({ { 0, 0.5f, 0 }, { 0, 0.5f, 0 } });
	renderer.BeginFrame();
	renderer.SetParams(params);
	scene.Begin();
	renderer.Draw(cutoutModel, transformResource->GetGPUVirtualAddress(), frontalCamera, viewport);
	scene.End();
	uint32_t holeDepth = 0;
	std::memcpy(&holeDepth, scene.captures[3].Pixel(64, 64), sizeof(holeDepth));
	Require(scene.captures[0].Float(64, 64, 0) == 0 && (holeDepth & 0xffffff) == 0xffffff,
		"Body emission must leave alpha cutout holes transparent with no depth");
	Require(scene.captures[0].Float(48, 80, 0) > params.bodyColor.x,
		"Body emission must remain visible on opaque cutout texels");
	std::cout << "PASS: optional view-facing body emission, independent body color / line emission, default-zero restoration, "
		"MRT / depth / stencil preservation, alpha cutout and per-Draw snapshots.\n";
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

	// 白黒の同じTextureでthresholdScaleだけを変更し、特徴線強度やGeometryとは独立に検証する。
	Require(NeonSkinnedSubmeshParams{}.internalLineThresholdScale == 1,
		"Per-submesh internal threshold must preserve the legacy default scale one");
	params.internalLineThreshold = 0.4f;
	renderer.SetSubmeshParams({ { 1, 0, 0, 1 }, { 1, 0, 0, 1 } });
	renderFeatures();
	const Capture defaultThreshold = scene.captures[0];
	Require(CountBrightPixels(defaultThreshold, 0) > 50, "Default threshold scale must retain texture feature lines");
	renderer.SetSubmeshParams({ { 1, 0, 0, 4 }, { 1, 0, 0, 4 } });
	renderFeatures();
	const Capture highThreshold = scene.captures[0];
	Require(CountBrightPixels(highThreshold, 0) == 0, "High per-submesh threshold must suppress texture features independently");
	renderer.SetSubmeshParams({ { 1, 0, 0, 0.25f }, { 1, 0, 0, 0.25f } });
	renderFeatures();
	Require(CountBrightPixels(scene.captures[0], 0) > 50, "Low per-submesh threshold must preserve texture features");
	for (const float invalidScale : { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() }) {
		renderer.SetSubmeshParams({ { 1, 0, 0, invalidScale }, { 1, 0, 0, invalidScale } });
		renderFeatures();
		RequireSameCapture(scene.captures[0], defaultThreshold, "Nonfinite submesh threshold must use the legacy scale one");
	}
	renderer.SetSubmeshParams({ { 1, 0, 0, -1 }, { 1, 0, 0, -1 } });
	renderFeatures();
	const Capture negativeThreshold = scene.captures[0];
	renderer.SetSubmeshParams({ { 1, 0, 0, 0.1f }, { 1, 0, 0, 0.1f } });
	renderFeatures();
	RequireSameCapture(scene.captures[0], negativeThreshold, "Negative submesh threshold must clamp to 0.1");
	renderer.SetSubmeshParams({ { 1, 0, 0, 20 }, { 1, 0, 0, 20 } });
	renderFeatures();
	RequireSameCapture(scene.captures[0], highThreshold, "Excessive submesh threshold must clamp to four");
	renderer.SetSubmeshParams({ { 0, 0, 0, 0.25f }, { 0, 0, 0, 0.25f } });
	renderFeatures();
	Require(CountBrightPixels(scene.captures[0], 0) == 0, "Threshold sensitivity must remain independent from submesh feature strength");
	std::cout << "PASS: independent per-submesh texture threshold, legacy scale one, high / low sensitivity and finite-range sanitation.\n";
}

void TestFeatureMasks(DirectXCommon& dx, SrvManager& srv, NeonSkinnedRenderer& renderer) {
	auto whiteTexture = CreateTestTexture(dx, srv, 1, false);
	auto featureTexture = CreateTestTexture(dx, srv, 2, true);
	// All resources outlive every command referencing their fixed descriptors.
	auto retainAuto = CreateTestMask(dx, srv, 3, { 255, 0, 255, 0 });
	auto eraseAuto = CreateTestMask(dx, srv, 4, { 0, 255, 255, 0 });
	auto halfCoverage = CreateTestMask(dx, srv, 5, { 128, 255, 255, 0 });
	auto halfReplace = CreateTestMask(dx, srv, 6, { 128, 128, 255, 0 });
	auto fullCoverage = CreateTestMask(dx, srv, 7, { 255, 255, 255, 0 });
	auto uvQuadrants = CreateTestMask(dx, srv, 8, { 0, 255, 0, 0 }, true);
	SkinnedModel model;
	model.Initialize(&dx, &srv, "geometry feature fixture");
	OffscreenScene scene(dx);
	auto transformResource = dx.CreateBufferResource(256);
	TransformationMatrix* transform = nullptr;
	Check(transformResource->Map(0, nullptr, reinterpret_cast<void**>(&transform)), "Mask transform map");
	*transform = { Identity(), Identity(), Identity() };
	transform->WVP.m[2][2] = 0.4f;
	transform->WVP.m[3][2] = 0.5f;
	const Vector2 viewport{ static_cast<float>(kSize), static_cast<float>(kSize) };
	NeonSkinnedParams params;
	Require(params.featureMaskBlend == 0 && params.featureMaskDebugMode == 0,
		"Authored masks must be disabled by default");
	params.outlineEnabled = 0;
	params.internalLineEnabled = 1;
	params.rimStrength = 0;
	params.featureMaskColor = { 1, 0.5f, 0.125f };
	params.featureMaskIntensity = 4;
	renderer.SetSubmeshParams({ { 1, 0, 0, 1 }, { 1, 0, 0, 1 } });
	auto render = [&](float depth = 1.0f) {
		renderer.BeginFrame();
		renderer.SetParams(params);
		scene.Begin(depth);
		renderer.Draw(model, transformResource->GetGPUVirtualAddress(), { 0, 0, -2 }, viewport);
		scene.End();
	};
	renderer.SetSubmeshFeatureMasks({});
	render();
	const std::array<Capture, 5> original = { scene.captures[0], scene.captures[1], scene.captures[2], scene.captures[3], scene.captures[4] };
	Require(CountBrightPixels(original[0], 0) > 50, "Mask regression fixture must contain real automatic internal lines");
	auto requireOriginal = [&] {
		for (UINT i = 0; i < 5; ++i) RequireSameCapture(scene.captures[i], original[i],
			"Disabled / absent / G-zero mask must restore legacy HDR, MRT, Depth and Stencil exactly");
	};
	for (const size_t count : { model.GetSubmeshCount() - 1, model.GetSubmeshCount() + 1 }) {
		renderer.BeginFrame();
		renderer.SetParams(params);
		scene.Begin();
		renderer.SetSubmeshFeatureMasks(std::vector<std::optional<uint32_t>>(count, std::optional<uint32_t>{ 5 }));
		bool rejected = false;
		try {
			renderer.Draw(model, transformResource->GetGPUVirtualAddress(), { 0, 0, -2 }, viewport);
		} catch (const std::invalid_argument&) { rejected = true; }
		Require(rejected, "Short and long feature-mask binding arrays must reject mismatched submesh counts");
		scene.End(0xa5); // Reject before clearing the reserved stencil bit or drawing any geometry.
		for (UINT y = 0; y < kSize; ++y) {
			for (UINT target = 0; target < 3; ++target) {
				const auto& capture = scene.captures[target];
				Require(std::all_of(capture.Pixel(0, y), capture.Pixel(0, y) + capture.rowBytes,
					[](uint8_t value) { return value == 0; }),
					"Rejected feature-mask arrays must not write HDR, Normal or Material targets");
			}
			for (UINT x = 0; x < kSize; ++x) {
				uint32_t depth = 0;
				std::memcpy(&depth, scene.captures[3].Pixel(x, y), sizeof(depth));
				Require((depth & 0xffffff) == 0xffffff, "Rejected feature-mask arrays must not write depth");
			}
		}
		renderer.SetSubmeshFeatureMasks({});
		render();
		requireOriginal(); // Failure must not prevent a subsequent valid legacy draw.
	}
	renderer.SetSubmeshFeatureMasks({ 5, 5 });
	for (UINT debug = 0; debug <= 2; ++debug) {
		params.featureMaskDebugMode = debug;
		render();
		requireOriginal();
	}
	params.featureMaskDebugMode = 0;
	params.featureMaskBlend = 1;
	for (const std::vector<std::optional<uint32_t>>& masks : {
		std::vector<std::optional<uint32_t>>{}, { std::nullopt, std::nullopt }, { 0, 0 }, { UINT32_MAX, UINT32_MAX }, { 3, 3 } }) {
		renderer.SetSubmeshFeatureMasks(masks);
		render();
		requireOriginal();
	}
	params.internalLineEnabled = 0;
	renderer.SetSubmeshFeatureMasks({});
	render();
	const Capture plainBody = scene.captures[0];
	params.internalLineEnabled = 1;
	renderer.SetSubmeshFeatureMasks({ 4, 4 });
	render();
	RequireSameCapture(scene.captures[0], plainBody, "G-one R-zero must suppress only automatic internal emission and retain the body");
	for (UINT i = 1; i < 5; ++i) RequireSameCapture(scene.captures[i], original[i],
		"Authored erase regions must preserve Normal, Material, Depth and all stencil bits");
	renderer.SetSubmeshFeatureMasks({ 5, 5 });
	render();
	const Capture authored = scene.captures[0];
	const float linearCoverage = 128.0f / 255.0f;
	const float color[] = { params.featureMaskColor.x, params.featureMaskColor.y, params.featureMaskColor.z };
	for (UINT c = 0; c < 3; ++c) Require(std::abs(authored.Float(40, 80, c)
		- plainBody.Float(40, 80, c) - color[c] * params.featureMaskIntensity * linearCoverage) < 0.003f,
		"R coverage must produce linear HDR emission, ignoring mask B and zero A (no SRGB or premultiplication)");
	for (UINT i = 1; i < 5; ++i) RequireSameCapture(scene.captures[i], original[i],
		"Authored visible lines must preserve Normal, Material, Depth and Stencil");
	params.featureMaskBlend = 0.5f;
	render();
	for (UINT y = 17; y < 111; ++y) for (UINT x = 17; x < 111; ++x)
		Require(std::abs(scene.captures[0].Float(x, y, 0) - (original[0].Float(x, y, 0) + authored.Float(x, y, 0)) * 0.5f) < 0.01f,
			"Global mask application must blend emission rather than erase geometry");
	params.featureMaskBlend = 1;
	renderer.SetSubmeshFeatureMasks({ 6, 6 });
	render();
	for (UINT y = 17; y < 111; ++y) for (UINT x = 17; x < 111; ++x)
		Require(std::abs(scene.captures[0].Float(x, y, 0)
			- (original[0].Float(x, y, 0) * (1 - linearCoverage) + authored.Float(x, y, 0) * linearCoverage)) < 0.01f,
			"Fractional mask G must retain its linear replacement rate");

	// Distinct per-triangle masks check single- / double-sided paths and root-table rebinding.
	renderer.SetSubmeshFeatureMasks({ 5, 4 });
	render();
	Require(std::abs(scene.captures[0].Float(32, 80, 0) - authored.Float(32, 80, 0)) < 0.001f
		&& scene.captures[0].Float(96, 48, 0) == plainBody.Float(96, 48, 0),
		"Different submesh masks must not leak through the subsequent double-sided draw");
	renderer.SetSubmeshFeatureMasks({ 5, std::nullopt });
	render();
	Require(scene.captures[0].Float(96, 48, 0) == original[0].Float(96, 48, 0),
		"An unassigned submesh after a mask must bind a valid G-zero fallback, not retain the prior SRV");

	renderer.SetSubmeshFeatureMasks({ 8, 8 });
	params.internalLineEnabled = 0;
	render();
	const struct { UINT x, y; float value; } quadrantExpected[] = {
		{ 32, 32, 64.0f / 255 }, { 96, 32, 128.0f / 255 }, { 32, 96, 192.0f / 255 }, { 96, 96, 1 },
	};
	for (const auto& sample : quadrantExpected)
		Require(std::abs(scene.captures[0].Float(sample.x, sample.y, 0)
			- plainBody.Float(sample.x, sample.y, 0) - sample.value * params.featureMaskIntensity) < 0.003f,
			"Mask UV sampling must retain left/right and upper/lower orientation from the model input");
	renderer.SetSubmeshFeatureMasks({ 5, 5 });
	for (UINT debug = 1; debug <= 2; ++debug) {
		params.featureMaskDebugMode = debug;
		render();
		const float expected = debug == 1 ? linearCoverage : 1;
		for (UINT c = 0; c < 3; ++c) Require(std::abs(scene.captures[0].Float(40, 80, c) - expected) < 0.001f,
			"Mask R/G diagnostics must display raw linear channel data without body or other emissions");
		for (UINT i = 1; i < 5; ++i) RequireSameCapture(scene.captures[i], original[i],
			"Mask diagnostics may only replace HDR color, preserving MRT / Depth / Stencil");
	}
	params.featureMaskDebugMode = 0;
	params.internalLineEnabled = 1;
	// A flat fixture's normals point directly into the view and cannot extrude an external screen-space hull.
	// Use the spherical skinned fixture to test real outline pixels instead of accepting a vacuous comparison.
	SkinnedModel outlineModel;
	outlineModel.Initialize(&dx, &srv, "feature sphere fixture");
	auto renderOutlineModel = [&] {
		renderer.BeginFrame();
		renderer.SetParams(params);
		scene.Begin();
		renderer.Draw(outlineModel, transformResource->GetGPUVirtualAddress(), { 0, 0, -2 }, viewport);
		scene.End();
	};
	params.featureMaskBlend = 0;
	renderOutlineModel();
	const Capture sphereBody = scene.captures[0];
	params.outlineEnabled = 1;
	renderOutlineModel();
	const Capture outlineBaseline = scene.captures[0];
	params.featureMaskBlend = 1;
	renderOutlineModel();
	UINT exterior = 0;
	for (UINT y = 0; y < kSize; ++y) for (UINT x = 0; x < kSize; ++x) {
		if (sphereBody.Float(x, y, 0) == 0 && outlineBaseline.Float(x, y, 0) > 1) {
			++exterior;
			for (UINT c = 0; c < 3; ++c) Require(scene.captures[0].Float(x, y, c) == outlineBaseline.Float(x, y, c),
				"Authored feature replacement must retain the independent external hull outline");
		}
	}
	Require(exterior > 200, "Mask outline preservation must exercise actual exterior pixels");
	params.outlineEnabled = 0;
	render(0.1f);
	Require(CountBrightPixels(scene.captures[0], 0) == 0 && scene.captures[0].Float(64, 64, 0) == 0,
		"Foreground depth must occlude authored lines instead of drawing them through surfaces");

	params.internalLineEnabled = 0;
	renderer.SetSubmeshParams({ { 1, 0.5f, 0, 1 }, { 1, 0.5f, 0, 1 } });
	params.featureMaskBlend = 0;
	render();
	const std::array<Capture, 5> cutoutOriginal = { scene.captures[0], scene.captures[1], scene.captures[2], scene.captures[3], scene.captures[4] };
	params.featureMaskBlend = 1;
	render();
	uint32_t holeDepth = 0;
	std::memcpy(&holeDepth, scene.captures[3].Pixel(64, 64), sizeof(holeDepth));
	Require(scene.captures[0].Float(64, 64, 0) == 0 && (holeDepth & 0xffffff) == 0xffffff,
		"BaseColor alpha cutout must remove authored color and depth even when mask coverage / replacement are nonzero");
	Require(scene.captures[0].Float(40, 80, 0) > 1,
		"Non-cutout surfaces must retain authored HDR despite zero mask alpha");
	for (UINT i = 1; i < 5; ++i) RequireSameCapture(scene.captures[i], cutoutOriginal[i],
		"Authored alpha-cutout draw must preserve original MRT / Depth / Stencil");
	if (renderer.IsGeometryLinesSupported()) {
		params.geometryLineEnabled = 1;
		renderer.SetSubmeshParams({ { 1, 0.5f, 1, 1 }, { 1, 0.5f, 1, 1 } });
		render();
		Require(std::abs(scene.captures[0].Float(40, 80, 0) - authored.Float(40, 80, 0)) < 0.002f,
			"Geometry shader variant must share authored emission away from mesh edges");
		Require(scene.captures[0].Float(64, 64, 0) == 0,
			"Geometry + authored masks must retain shared BaseColor cutout holes");
		Require(scene.captures[0].Float(32, 32, 2) > authored.Float(32, 32, 2) + 1,
			"Authored masks must not replace independent cyan geometry diagnostic emission");
	}
	params.geometryLineEnabled = 0;
	renderer.SetSubmeshParams({ { 1, 0, 0, 1 }, { 1, 0, 0, 1 } });
	// Updating the CPU-side vector and CB for a second Draw cannot overwrite the first command's bindings.
	auto rightResource = dx.CreateBufferResource(256);
	TransformationMatrix* right = nullptr;
	Check(rightResource->Map(0, nullptr, reinterpret_cast<void**>(&right)), "Second mask transform map");
	transform->WVP.m[0][0] = transform->WVP.m[1][1] = 0.5f;
	transform->WVP.m[3][0] = -0.5f;
	*right = *transform;
	right->WVP.m[3][0] = 0.5f;
	renderer.BeginFrame();
	scene.Begin();
	renderer.SetSubmeshFeatureMasks({ 5, 5 });
	params.featureMaskColor = { 1, 0, 0 };
	params.featureMaskIntensity = 4;
	renderer.SetParams(params);
	renderer.Draw(model, transformResource->GetGPUVirtualAddress(), { 0, 0, -2 }, viewport);
	renderer.SetSubmeshFeatureMasks({ 7, 7 });
	params.featureMaskColor = { 0, 1, 0 };
	params.featureMaskIntensity = 3;
	renderer.SetParams(params);
	renderer.Draw(model, rightResource->GetGPUVirtualAddress(), { 0, 0, -2 }, viewport);
	scene.End();
	Require(std::abs(scene.captures[0].Float(32, 64, 0) - params.bodyColor.x - 4 * linearCoverage) < 0.003f
		&& scene.captures[0].Float(32, 64, 1) < 0.1f
		&& std::abs(scene.captures[0].Float(96, 64, 1) - params.bodyColor.y - 3) < 0.003f
		&& scene.captures[0].Float(96, 64, 0) < 0.1f,
		"Multiple mask Draws must retain independent descriptor-table and constant snapshots");
	renderer.SetSubmeshFeatureMasks({});
	std::cout << "PASS: authored RG mask binding-count rejection before rendering and valid-draw recovery; "
		"OFF / absent / invalid / G-zero legacy restoration; R-zero selective erase; "
		"linear fractional R / G and blend; UV orientation; mask-alpha independence; R/G diagnostics; "
		"MRT / depth / stencil / outline preservation, original alpha cutout, occlusion, Geometry PS sharing, "
		"single / double-sided submesh and multiple-Draw SRV / CB isolation.\n";
}

ComPtr<ID3D12Resource> CreateQualityStrokeTexture(DirectXCommon& dx, SrvManager& srv, UINT index, bool distance) {
	// Same oblique narrow stroke for both techniques. Full linear area-coverage mips are
	// uploaded independently; SDF mips intentionally average away the geometry at 1x1.
	constexpr UINT size = 64, levels = 7;
	std::vector<std::vector<uint8_t>> pixels(levels);
	pixels[0].resize(size * size * 4);
	for (UINT y = 0; y < size; ++y) for (UINT x = 0; x < size; ++x) {
		const float signedDistance = 1.35f - std::abs((x + .5f - 32) + .125f * (y + .5f - 32)) / std::sqrt(1.015625f);
		const float core = (std::clamp)(.5f + signedDistance, 0.0f, 1.0f);
		const float halo = (std::clamp)(1 + (std::min)(signedDistance, 0.0f) / 4, 0.0f, 1.0f);
		auto* p = pixels[0].data() + (y * size + x) * 4;
		p[0] = static_cast<uint8_t>(std::lround((distance ? (std::clamp)(.5f + signedDistance / 32, 0.0f, 1.0f) : core) * 255));
		p[1] = p[3] = 255;
		p[2] = distance ? 255 : static_cast<uint8_t>(std::lround(halo * halo * 255));
	}
	for (UINT level = 1; level < levels; ++level) {
		const UINT current = size >> level, previous = current * 2;
		pixels[level].resize(current * current * 4);
		for (UINT y = 0; y < current; ++y) for (UINT x = 0; x < current; ++x) for (UINT c = 0; c < 4; ++c) {
			UINT sum = 0;
			for (UINT dy = 0; dy < 2; ++dy) for (UINT dxOffset = 0; dxOffset < 2; ++dxOffset)
				sum += pixels[level - 1][((y * 2 + dy) * previous + x * 2 + dxOffset) * 4 + c];
			pixels[level][(y * current + x) * 4 + c] = static_cast<uint8_t>((sum + 2) / 4);
		}
	}
	D3D12_RESOURCE_DESC desc{};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = desc.Height = size;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = levels;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	D3D12_HEAP_PROPERTIES heap{};
	heap.Type = D3D12_HEAP_TYPE_DEFAULT;
	ComPtr<ID3D12Resource> texture;
	Check(dx.GetDevice()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&texture)), "Quality texture");
	std::array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT, levels> footprints{};
	UINT64 total = 0;
	dx.GetDevice()->GetCopyableFootprints(&desc, 0, levels, 0, footprints.data(), nullptr, nullptr, &total);
	auto upload = dx.CreateBufferResource(static_cast<size_t>(total));
	uint8_t* mapped = nullptr;
	Check(upload->Map(0, nullptr, reinterpret_cast<void**>(&mapped)), "Quality texture upload");
	for (UINT level = 0; level < levels; ++level) {
		const UINT current = size >> level;
		for (UINT y = 0; y < current; ++y)
			std::memcpy(mapped + footprints[level].Offset + y * footprints[level].Footprint.RowPitch,
				pixels[level].data() + y * current * 4, current * 4);
	}
	upload->Unmap(0, nullptr);
	for (UINT level = 0; level < levels; ++level) {
		D3D12_TEXTURE_COPY_LOCATION source{}, destination{};
		source.pResource = upload.Get();
		source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		source.PlacedFootprint = footprints[level];
		destination.pResource = texture.Get();
		destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		destination.SubresourceIndex = level;
		dx.GetList()->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
	}
	Transition(dx.GetList().Get(), texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	dx.PostDraw(); // Retire upload only after all seven subresource copies have completed.
	D3D12_SHADER_RESOURCE_VIEW_DESC view{};
	view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	view.Format = desc.Format;
	view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	view.Texture2D.MipLevels = levels;
	dx.GetDevice()->CreateShaderResourceView(texture.Get(), &view, srv.GetCPUDescriptorHandle(index));
	return texture;
}

void TestQualityLineMasks(DirectXCommon& dx, SrvManager& srv, NeonSkinnedRenderer& renderer) {
	NeonSkinnedParams invalid;
	Require(invalid.featureMaskRenderMode==0 && invalid.splitLineEmission==0 && invalid.lineDiagnosticMode==0,
		"Quality reconstruction, split emissions and diagnostics must all be opt-in");
	invalid.lineCoreColor={-1,std::numeric_limits<float>::quiet_NaN(),200};
	invalid.lineHaloColor=invalid.outlineCoreColor=invalid.lineCoreColor;
	invalid.lineCoreIntensity=std::numeric_limits<float>::infinity();
	invalid.lineHaloIntensity=-2;
	invalid.outlineCoreIntensity=200;
	invalid.featureMaskRenderMode=invalid.lineDiagnosticMode=UINT32_MAX;
	invalid.splitLineEmission=UINT32_MAX;
	invalid.sdfRangeTexels=invalid.sdfHaloWidthTexels=invalid.sdfLodBlendStart=invalid.sdfLodBlendEnd=std::numeric_limits<float>::quiet_NaN();
	invalid.qualityPadding=200;
	renderer.SetParams(invalid);
	const auto& safe=renderer.GetParams();
	Require(safe.lineCoreColor.x==0 && safe.lineCoreColor.y==0 && safe.lineCoreColor.z==100
		&& safe.lineHaloColor.x==0 && safe.lineHaloColor.y==0 && safe.lineHaloColor.z==100
		&& safe.outlineCoreColor.x==0 && safe.outlineCoreColor.y==0 && safe.outlineCoreColor.z==100
		&& safe.lineCoreIntensity==0 && safe.lineHaloIntensity==0 && safe.outlineCoreIntensity==100
		&& safe.featureMaskRenderMode==0 && safe.lineDiagnosticMode==0 && safe.splitLineEmission==1
		&& safe.sdfRangeTexels==16 && safe.sdfHaloWidthTexels==0 && safe.sdfLodBlendStart==1
		&& safe.sdfLodBlendEnd==2 && safe.qualityPadding==0,
		"Quality HDR colors, intensities, enum values and SDF controls must sanitize nonfinite / invalid inputs");
	auto whiteTexture = CreateTestTexture(dx, srv, 1, false);
	auto featureTexture = CreateTestTexture(dx, srv, 2, true);
	const UINT coverageIndex = srv.Allocate(), insideIndex = srv.Allocate(), outsideIndex = srv.Allocate();
	const UINT eraseIndex = srv.Allocate(), retainIndex = srv.Allocate(), fullIndex = srv.Allocate();
	const UINT strokeIndex = srv.Allocate(), strokeDistanceIndex = srv.Allocate();
	auto coverage = CreateTestMask(dx, srv, coverageIndex, {128,255,64,255});
	auto inside = CreateTestMask(dx, srv, insideIndex, {192,255,64,255});
	auto outside = CreateTestMask(dx, srv, outsideIndex, {0,255,255,255});
	auto erase = CreateTestMask(dx, srv, eraseIndex, {0,255,0,255});
	auto retain = CreateTestMask(dx, srv, retainIndex, {255,0,255,255});
	auto full = CreateTestMask(dx, srv, fullIndex, {255,255,255,255});
	auto stroke = CreateQualityStrokeTexture(dx, srv, strokeIndex, false);
	auto strokeDistance = CreateQualityStrokeTexture(dx, srv, strokeDistanceIndex, true);
	SkinnedModel model;
	model.Initialize(&dx, &srv, "geometry feature quality fixture");
	OffscreenScene scene(dx);
	auto resource = dx.CreateBufferResource(256);
	TransformationMatrix* transform = nullptr;
	Check(resource->Map(0, nullptr, reinterpret_cast<void**>(&transform)), "Quality transform map");
	*transform = {Identity(),Identity(),Identity()};
	transform->WVP.m[2][2] = .4f;
	transform->WVP.m[3][2] = .5f;
	const Vector2 viewport{static_cast<float>(kSize),static_cast<float>(kSize)};
	NeonSkinnedParams params;
	params.outlineEnabled = 0;
	params.rimStrength = 0;
	params.internalLineEnabled = 1;
	params.featureMaskIntensity = 4;
	params.featureMaskColor = {1,.5f,.125f};
	params.lineCoreColor = {1,0,0};
	params.lineCoreIntensity = 4;
	params.lineHaloColor = {0,1,0};
	params.lineHaloIntensity = 2;
	renderer.SetSubmeshParams({{1,0,0,1},{1,0,0,1}});
	renderer.SetSubmeshFeatureMasks({});
	renderer.SetSubmeshFeatureDistanceMasks({});
	auto render = [&](float depth = 1.0f) {
		renderer.BeginFrame();
		renderer.SetParams(params);
		scene.Begin(depth);
		renderer.Draw(model,resource->GetGPUVirtualAddress(),{0,0,-2},viewport);
		scene.End();
	};
	render();
	const std::array<Capture,5> original = {scene.captures[0],scene.captures[1],scene.captures[2],scene.captures[3],scene.captures[4]};
	params.internalLineEnabled = 0;
	render();
	const Capture body = scene.captures[0];
	params.internalLineEnabled = 1;
	auto preserveSurfaces = [&] {
		for (UINT i=1;i<5;++i) RequireSameCapture(scene.captures[i],original[i],"Quality lines must preserve Normal / Material / Depth / Stencil");
	};
	for (UINT mode : {1u,2u}) {
		params.featureMaskRenderMode = mode;
		params.splitLineEmission = 1;
		renderer.SetSubmeshFeatureMasks({coverageIndex,coverageIndex});
		renderer.SetSubmeshFeatureDistanceMasks({insideIndex,insideIndex});
		params.featureMaskBlend = 0;
		render();
		for (UINT i=0;i<5;++i) RequireSameCapture(scene.captures[i],original[i],"Quality mode with blend zero must exactly restore legacy output");
		params.featureMaskBlend = 1;
		for (const std::vector<std::optional<uint32_t>>& absentCoverage : {
			std::vector<std::optional<uint32_t>>{}, {std::nullopt,std::nullopt},{0,0},{UINT32_MAX,UINT32_MAX}}) {
			renderer.SetSubmeshFeatureMasks(absentCoverage);
			render();
			for (UINT i=0;i<5;++i) RequireSameCapture(scene.captures[i],original[i],
				"Absent / invalid coverage must preserve legacy output even with a valid distance map requested");
		}
		renderer.SetSubmeshFeatureMasks({retainIndex,retainIndex});
		render();
		for (UINT i=0;i<5;++i) RequireSameCapture(scene.captures[i],original[i],"G-zero must retain original auto lines even when distance map is present");
		renderer.SetSubmeshFeatureMasks({eraseIndex,eraseIndex});
		renderer.SetSubmeshFeatureDistanceMasks({outsideIndex,outsideIndex});
		render();
		RequireSameCapture(scene.captures[0],body,"G-one erase-only region must remove Core / Halo without removing body");
		preserveSurfaces();
	}
	params.featureMaskRenderMode = 1;
	renderer.SetSubmeshFeatureMasks({coverageIndex,coverageIndex});
	render();
	const Capture covered = scene.captures[0];
	const float half=128.0f/255,weak=64.0f/255;
	Require(std::abs(covered.Float(40,80,0)-body.Float(40,80,0)-4*half)<.003f
		&& std::abs(covered.Float(40,80,1)-body.Float(40,80,1)-2*weak)<.003f,
		"Coverage Core and Halo must retain independent logical R/B channels and colors");
	params.featureMaskRenderMode = 2;
	for (const std::vector<std::optional<uint32_t>>& invalidBindings : {
		std::vector<std::optional<uint32_t>>{}, {std::nullopt,std::nullopt},{0,0},{UINT32_MAX,UINT32_MAX}}) {
		renderer.SetSubmeshFeatureDistanceMasks(invalidBindings);
		render();
		RequireSameCapture(scene.captures[0],covered,"Missing / invalid SDF must fall back to exact coverage Core / Halo");
		preserveSurfaces();
	}
	renderer.SetSubmeshFeatureDistanceMasks({insideIndex,insideIndex});
	render();
	Require(std::abs(scene.captures[0].Float(40,80,0)-body.Float(40,80,0)-4*weak)<.003f
		&& std::abs(scene.captures[0].Float(40,80,1)-body.Float(40,80,1)-2*weak)<.003f,
		"Constant zero-gradient SDF must reconstruct finite geometric inside shape with independent weak opacity");
	preserveSurfaces();
	for (UINT diagnostic : {1u,2u,3u}) {
		params.lineDiagnosticMode=diagnostic;
		render();
		const float red=diagnostic==1 ? 4*weak : diagnostic==3 ? body.Float(40,80,0) : 0;
		const float green=diagnostic==2 ? 2*weak : diagnostic==3 ? body.Float(40,80,1) : 0;
		Require(std::abs(scene.captures[0].Float(40,80,0)-red)<.003f && std::abs(scene.captures[0].Float(40,80,1)-green)<.003f,
			"Core / Halo / Body diagnostics must isolate their own color and intensity");
		preserveSurfaces();
	}
	params.lineDiagnosticMode=0;
	params.lineCoreIntensity=0;
	params.lineDiagnosticMode=1;
	render();
	Require(scene.captures[0].Float(40,80,0)==0 && scene.captures[0].Float(40,80,1)==0,
		"Core strength zero must smoothly extinguish the line, without a forced minimum brightness");
	preserveSurfaces();
	params.lineCoreIntensity=4;
	params.lineHaloIntensity=0;
	params.lineDiagnosticMode=2;
	render();
	Require(scene.captures[0].Float(40,80,0)==0 && scene.captures[0].Float(40,80,1)==0,
		"Halo strength zero must extinguish only its diagnostic contribution");
	preserveSurfaces();
	params.lineHaloIntensity=2;
	params.lineDiagnosticMode=0;
	// One submesh uses valid distance, the following double-sided triangle uses coverage fallback.
	renderer.SetSubmeshFeatureDistanceMasks({insideIndex,std::nullopt});
	render();
	Require(std::abs(scene.captures[0].Float(32,80,0)-body.Float(32,80,0)-4*weak)<.003f
		&& std::abs(scene.captures[0].Float(96,48,0)-covered.Float(96,48,0))<.003f,
		"Per-submesh SDF descriptor / valid flag must not leak to the double-sided coverage fallback");
	transform->WVP.m[0][0]=-1;
	render();
	Require(std::abs(scene.captures[0].Float(32,48,0)-params.bodyColor.x-4*half)<.003f
		&& scene.captures[0].Float(96,80,0)==0 && scene.captures[1].Float(32,48,2)>.99f,
		"Reversed quality geometry must keep double-sided fallback visible with flipped normal while culling the single-sided triangle");
	transform->WVP.m[0][0]=1;
	for (size_t count : {model.GetSubmeshCount()-1,model.GetSubmeshCount()+1}) {
		renderer.BeginFrame();
		scene.Begin();
		renderer.SetSubmeshFeatureDistanceMasks(std::vector<std::optional<uint32_t>>(count,insideIndex));
		bool rejected=false;
		try { renderer.Draw(model,resource->GetGPUVirtualAddress(),{0,0,-2},viewport); }
		catch (const std::invalid_argument&) { rejected=true; }
		Require(rejected,"Short / long SDF arrays must reject before partial rendering");
		scene.End(0xa5);
		for (UINT y=0;y<kSize;++y) {
			for (UINT target=0;target<3;++target) {
				const auto& capture=scene.captures[target];
				Require(std::all_of(capture.Pixel(0,y),capture.Pixel(0,y)+capture.rowBytes,
					[](uint8_t value) { return value==0; }),"Rejected SDF array must leave all MRTs clear");
			}
			for (UINT x=0;x<kSize;++x) {
				uint32_t z=0;
				std::memcpy(&z,scene.captures[3].Pixel(x,y),sizeof(z));
				Require((z&0xffffff)==0xffffff,"Rejected SDF array must leave depth unchanged");
			}
		}
	}
	renderer.SetSubmeshFeatureDistanceMasks({insideIndex,insideIndex});
	render(); // Rejection leaves a subsequent valid draw possible.
	params.internalLineEnabled=0;
	renderer.SetSubmeshParams({{1,.5f,0,1},{1,.5f,0,1}});
	params.featureMaskBlend=0;
	render();
	const std::array<Capture,5> cutout={scene.captures[0],scene.captures[1],scene.captures[2],scene.captures[3],scene.captures[4]};
	params.featureMaskBlend=1;
	for (UINT mode : {1u,2u}) {
		params.featureMaskRenderMode=mode;
		render();
		uint32_t z=0;
		std::memcpy(&z,scene.captures[3].Pixel(64,64),sizeof(z));
		Require(scene.captures[0].Float(64,64,0)==0 && (z&0xffffff)==0xffffff,
			"Original alpha cutout must remove quality color and depth despite authored positive SDF");
		Require(scene.captures[0].Float(40,80,0)>.5f,"Non-cutout surfaces must retain quality authored emission");
		for (UINT i=1;i<5;++i) RequireSameCapture(scene.captures[i],cutout[i],"Quality techniques must retain all original alpha-cutout MRT / Depth / Stencil pixels");
		render(.1f);
		Require(CountBrightPixels(scene.captures[0],0)==0 && scene.captures[0].Float(40,80,0)==0,
			"Foreground depth must occlude both quality techniques");
	}
	// Native barycentric variant shares the new line reconstruction and preserves cutout.
	params.featureMaskRenderMode=2;
	if (renderer.IsGeometryLinesSupported()) {
		params.geometryLineEnabled=1;
		renderer.SetSubmeshParams({{1,.5f,1,1},{1,.5f,1,1}});
		render();
		Require(std::abs(scene.captures[0].Float(40,80,0)-body.Float(40,80,0)-4*weak)<.003f,
			"Geometry variant must share SDF Core reconstruction away from geometry edges");
		Require(scene.captures[0].Float(64,64,0)==0,"Geometry + quality path must retain shared cutout");
		Require(scene.captures[0].Float(32,32,2)>1,"Independent cyan mesh emission must remain outside authored Core/Halo replacement");
	}
	params.geometryLineEnabled=0;
	renderer.SetSubmeshParams({{1,0,0,1},{1,0,0,1}});
	// Inspect a real varying distance field, rather than proving AA with constant fixtures.
	SkinnedModel plainModel;
	plainModel.Initialize(&dx,&srv,"geometry quality stroke fixture");
	renderer.SetSubmeshFeatureMasks({strokeIndex,strokeIndex});
	renderer.SetSubmeshFeatureDistanceMasks({strokeDistanceIndex,strokeDistanceIndex});
	params.lineDiagnosticMode=1;
	auto renderPlain=[&] {
		renderer.BeginFrame();
		renderer.SetParams(params);
		scene.Begin();
		renderer.Draw(plainModel,resource->GetGPUVirtualAddress(),{0,0,-2},viewport);
		scene.End();
	};
	renderPlain();
	UINT partial=0,solid=0;
	for (UINT y=16;y<112;++y) for (UINT x=16;x<112;++x) {
		const float value=scene.captures[0].Float(x,y,0);
		Require(std::isfinite(value) && value>=0 && value<=4.001f,"Distance AA must never produce NaN / invalid HDR coverage");
		partial += value>.01f && value<3.99f;
		solid += value>=3.99f;
	}
	Require(partial>50 && solid>50,"A varying oblique SDF stroke must retain a solid core and fractional screen-space AA boundary");
	params.lineDiagnosticMode=2;
	params.sdfHaloWidthTexels=0;
	auto requireNoHalo=[&] {
		for (UINT y=0;y<kSize;++y) for (UINT x=0;x<kSize;++x) for (UINT c=0;c<3;++c)
			Require(scene.captures[0].Float(x,y,c)==0,"SDF Halo width zero must remain off, including baked-coverage minification fallback");
	};
	renderPlain();
	requireNoHalo();
	transform->WVP.m[0][0]=transform->WVP.m[1][1]=.1f;
	renderPlain();
	requireNoHalo();
	params.sdfHaloWidthTexels=4;
	params.lineDiagnosticMode=1;
	params.featureMaskRenderMode=1;
	renderPlain();
	const Capture farCoverage=scene.captures[0];
	params.featureMaskRenderMode=2;
	renderPlain();
	RequireSameCapture(scene.captures[0],farCoverage,"Far SDF minification must use exact filtered coverage MIPs, not averaged distance signs");
	Require(CountBrightPixels(farCoverage,0)>0,"Far fallback comparison must exercise visible authored stroke pixels");
	// Different modes, resources and colors in two Draws recorded before a single fence.
	auto rightResource=dx.CreateBufferResource(256);
	TransformationMatrix* right=nullptr;
	Check(rightResource->Map(0,nullptr,reinterpret_cast<void**>(&right)),"Quality second transform");
	transform->WVP.m[0][0]=transform->WVP.m[1][1]=.5f;
	transform->WVP.m[3][0]=-.5f;
	*right=*transform;
	right->WVP.m[3][0]=.5f;
	renderer.BeginFrame();
	scene.Begin();
	params.lineDiagnosticMode=0;
	params.featureMaskRenderMode=2;
	params.lineHaloIntensity=0;
	params.lineCoreColor={1,0,0};
	params.lineCoreIntensity=4;
	renderer.SetSubmeshFeatureMasks({coverageIndex,coverageIndex});
	renderer.SetSubmeshFeatureDistanceMasks({insideIndex,insideIndex});
	renderer.SetParams(params);
	renderer.Draw(plainModel,resource->GetGPUVirtualAddress(),{0,0,-2},viewport);
	params.featureMaskRenderMode=1;
	params.lineCoreColor={0,1,0};
	params.lineCoreIntensity=3;
	renderer.SetSubmeshFeatureMasks({fullIndex,fullIndex});
	renderer.SetSubmeshFeatureDistanceMasks({});
	renderer.SetParams(params);
	renderer.Draw(plainModel,rightResource->GetGPUVirtualAddress(),{0,0,-2},viewport);
	scene.End();
	Require(std::abs(scene.captures[0].Float(32,64,0)-params.bodyColor.x-4*weak)<.003f
		&& scene.captures[0].Float(32,64,1)<.1f
		&& std::abs(scene.captures[0].Float(96,64,1)-params.bodyColor.y-3)<.003f
		&& scene.captures[0].Float(96,64,0)<.1f,
		"Same-frame Draws must retain independent quality mode / color / SDF flag / SRV and CB snapshots");
	renderer.SetSubmeshFeatureMasks({});
	renderer.SetSubmeshFeatureDistanceMasks({});
	// Split outline color is independent from surface Core/Halo and uses actual
	// exterior sphere hull pixels, not a planar fixture with no external extrusion.
	SkinnedModel sphere;
	sphere.Initialize(&dx,&srv,"quality outline sphere fixture");
	*transform={Identity(),Identity(),Identity()};
	transform->WVP.m[2][2]=.4f;
	transform->WVP.m[3][2]=.5f;
	params=NeonSkinnedParams{};
	params.outlineEnabled=0;
	auto renderSphere=[&] {
		renderer.BeginFrame();
		renderer.SetParams(params);
		scene.Begin();
		renderer.Draw(sphere,resource->GetGPUVirtualAddress(),{0,0,-2},viewport);
		scene.End();
	};
	renderSphere();
	const std::array<Capture,5> sphereBody={scene.captures[0],scene.captures[1],scene.captures[2],scene.captures[3],scene.captures[4]};
	params.outlineEnabled=1;
	params.splitLineEmission=1;
	params.outlineCoreColor={0,0,1};
	params.outlineCoreIntensity=6;
	params.lineCoreColor={1,0,0};
	params.lineHaloColor={0,1,0};
	renderSphere();
	UINT exterior=0;
	for (UINT y=0;y<kSize;++y) for (UINT x=0;x<kSize;++x) {
		if (sphereBody[0].Float(x,y,0)==0 && scene.captures[0].Float(x,y,2)>1) {
			++exterior;
			Require(scene.captures[0].Float(x,y,0)==0 && scene.captures[0].Float(x,y,1)==0
				&& std::abs(scene.captures[0].Float(x,y,2)-6)<.004f,
				"Outline Core must use its independent HDR color and intensity");
		}
	}
	Require(exterior>100,"Split outline check must exercise real external hull pixels");
	for (UINT i=1;i<5;++i) RequireSameCapture(scene.captures[i],sphereBody[i],"Split outline must retain all Body MRT / Depth / Stencil pixels");
	std::cout << "PASS: coverage RGB Core/Halo and signed line-region reconstruction; blend-zero / G-zero legacy restoration, "
		"erase-only body preservation, absent / invalid distance fallback, weak-opacity geometry and zero-gradient finite AA; "
		"Core/Halo/Body diagnostics, varying-distance AA, exact far-LOD coverage fallback; MRT / depth / stencil / alpha / "
		"occlusion / Geometry sharing, single/double-sided validity rebinding, atomic SDF-array rejection and recovery; "
		"multiple-Draw modes / descriptors / constants remain independent; opt-in finite-range controls and independent exterior outline Core.\n";
}

void TestDirectionalDissolve(DirectXCommon& dx, SrvManager& srv) {
	// Fresh Renderer makes its first real CB allocation observable through the service stub.
	NeonSkinnedRenderer renderer;
	renderer.Initialize(&dx,&srv);
	auto whiteTexture=CreateTestTexture(dx,srv,1,false);
	auto featureTexture=CreateTestTexture(dx,srv,2,true);
	const UINT coverageIndex=srv.Allocate(),distanceIndex=srv.Allocate();
	auto coverage=CreateTestMask(dx,srv,coverageIndex,{128,255,64,255});
	auto distance=CreateTestMask(dx,srv,distanceIndex,{192,255,64,255});
	SkinnedModel model;
	model.Initialize(&dx,&srv,"geometry feature dissolve fixture");
	OffscreenScene scene(dx);
	auto resource=dx.CreateBufferResource(256);
	TransformationMatrix* transform=nullptr;
	Check(resource->Map(0,nullptr,reinterpret_cast<void**>(&transform)),"Dissolve transform map");
	*transform={Identity(),Identity(),Identity()};
	transform->WVP.m[2][2]=.4f; transform->WVP.m[3][2]=.5f;
	const Vector2 viewport{static_cast<float>(kSize),static_cast<float>(kSize)};
	NeonSkinnedParams params;
	Require(params.dissolve.enabled==0,"Dissolve must remain opt-in for existing callers");
	NeonSkinnedParams invalid;
	invalid.dissolve.enabled=UINT32_MAX; invalid.dissolve.direction={0,0,0}; invalid.dissolve.progress=std::numeric_limits<float>::quiet_NaN();
	invalid.dissolve.noiseStrength=std::numeric_limits<float>::infinity(); invalid.dissolve.edgeColor={-1,200,std::numeric_limits<float>::quiet_NaN()};
	invalid.dissolve.padding=200; invalid.dissolve.edgePadding[0]=300;
	renderer.SetParams(invalid);
	const auto sanitized=SanitizeNeonDissolveParams(invalid.dissolve);
	Require(std::memcmp(&renderer.GetParams().dissolve,&sanitized,sizeof(sanitized))==0,
		"Renderer must actually use the finite, opt-in dissolve sanitation contract");
	params.outlineEnabled=0; params.rimStrength=0; params.internalLineEnabled=1;
	params.featureMaskBlend=1; params.featureMaskRenderMode=2; params.splitLineEmission=1;
	renderer.SetSubmeshParams({{1,.5f,1,1},{1,.5f,1,1}});
	renderer.SetSubmeshFeatureMasks({coverageIndex,coverageIndex});
	renderer.SetSubmeshFeatureDistanceMasks({distanceIndex,distanceIndex});
	auto render=[&](float clearDepth=1.0f,const Vector3& camera=Vector3{0,0,-2}) {
		renderer.BeginFrame(); renderer.SetParams(params); scene.Begin(clearDepth);
		renderer.Draw(model,resource->GetGPUVirtualAddress(),camera,viewport); scene.End();
	};
	requestedUploadBytes.clear();
	render();
	Require(requestedUploadBytes==std::vector<size_t>{512},
		"Actual 320-byte Neon draw constants must allocate a rounded 512-byte GPU resource");
	const std::array<Capture,5> original={scene.captures[0],scene.captures[1],scene.captures[2],scene.captures[3],scene.captures[4]};
	params.dissolve.enabled=0; params.dissolve.progress=1;
	render();
	for(UINT target=0;target<5;++target) RequireSameCapture(scene.captures[target],original[target],"Disabled dissolve must exactly preserve every legacy target");
	params.dissolve.enabled=1; params.dissolve.direction={1,0,0};
	params.dissolve.scanMin=-.75f; params.dissolve.scanMax=.75f;
	params.dissolve.noiseStrength=0; params.dissolve.progress=0;
	params.dissolve.edgeIntensity=100; params.dissolve.edgeWidth=.1f;
	render();
	for(UINT target=0;target<5;++target) RequireSameCapture(scene.captures[target],original[target],"Progress zero must be byte-exact legacy including no boundary emission");
	params.dissolve.progress=.5f; params.dissolve.edgeEnabled=0;
	render();
	const std::array<Capture,5> partial={scene.captures[0],scene.captures[1],scene.captures[2],scene.captures[3],scene.captures[4]};
	auto depthBits=[&](const Capture& capture,UINT x,UINT y) {
		uint32_t depth=0; std::memcpy(&depth,capture.Pixel(x,y),sizeof(depth)); return depth&0xffffff;
	};
	for(UINT y=17;y<111;++y) for(UINT x=17;x<111;++x) {
		if(x<63) {
			for(UINT target=0;target<3;++target) {
				const auto* pixel=scene.captures[target].Pixel(x,y);
				Require(std::all_of(pixel,pixel+scene.captures[target].rowBytes/kSize,[](uint8_t value){return value==0;}),
					"Removed surface must leave all color / Normal / Material channels clear");
			}
			Require(depthBits(scene.captures[3],x,y)==0xffffff,"Removed surface must not write invisible depth");
		} else if(x>64) {
			for(UINT target=0;target<4;++target) Require(std::memcmp(scene.captures[target].Pixel(x,y),original[target].Pixel(x,y),
				static_cast<size_t>(scene.captures[target].rowBytes/kSize))==0,"Surviving surface must preserve legacy HDR / Normal / Material / depth values when boundary is off");
		}
	}
	params.dissolve.edgeEnabled=1; params.dissolve.edgeColor={0,0,1}; params.dissolve.edgeIntensity=8; params.dissolve.edgeWidth=.04f;
	render();
	Require(CountBrightPixels(scene.captures[0],2,64,71,80,95)>0,"Partial dissolve must emit on the surviving-side interior boundary");
	for(UINT target=1;target<5;++target) RequireSameCapture(scene.captures[target],partial[target],"Boundary HDR emission must not pollute Normal / Material / depth / stencil");
	Require(scene.captures[0].Float(68,64,0)==0 && scene.captures[0].Float(68,64,2)==0 && depthBits(scene.captures[3],68,64)==0xffffff,
		"Original Alpha cutout must intersect dissolve and never leak boundary emission/depth");
	for(UINT y=17;y<111;++y) for(UINT x=17;x<63;++x)
		Require(scene.captures[0].Float(x,y,0)==0 && scene.captures[0].Float(x,y,1)==0 && scene.captures[0].Float(x,y,2)==0,
			"Boundary emission must never resurrect removed-side surface");
	params.dissolve.edgeWidth=0; render();
	RequireSameCapture(scene.captures[0],partial[0],"Boundary width zero must exactly disable only boundary emission");
	params.dissolve.edgeWidth=.04f; params.dissolve.edgeIntensity=0; render();
	RequireSameCapture(scene.captures[0],partial[0],"Boundary intensity zero must exactly disable only boundary emission");
	params.dissolve.noiseStrength=1; params.dissolve.noiseScale=0; render();
	for(UINT target=0;target<5;++target) RequireSameCapture(scene.captures[target],partial[target],"Noise scale zero must remove both noise and its threshold padding");
	params.dissolve.noiseStrength=0; params.dissolve.noiseScale=12;
	// A camera or World-only change cannot shift the stored model-space noise/scan mask.
	transform->World.m[3][0]=5; render(1,{4,1,-3});
	for(UINT target=0;target<5;++target) RequireSameCapture(scene.captures[target],partial[target],"Fixed model-space dissolve must not swim under changed camera/World lighting coordinates");
	transform->World=Identity();
	params.dissolve.edgeIntensity=8;
	render(.1f);
	for(UINT y=0;y<kSize;++y) for(UINT target=0;target<3;++target) {
		// Material is RGBA8, while HDR/Normal are half-floats. Compare the actual stored
		// bytes (including alpha) rather than reading every attachment as float16.
		const auto& capture=scene.captures[target];
		Require(std::all_of(capture.Pixel(0,y),capture.Pixel(0,y)+capture.rowBytes,[](uint8_t value){return value==0;}),
			"Foreground depth must occlude all dissolve boundary/quality emission");
	}
	// Noise parity is checked on an unobstructed plane, not via non-monotonic final image brightness.
	params.dissolve.edgeEnabled=0; params.dissolve.noiseStrength=.3f; params.dissolve.noiseScale=8; params.dissolve.seed=1337;
	std::array<bool,kSize*kSize> removed{};
	for(float progress:{.25f,.5f,.75f}) {
		params.dissolve.progress=progress; render();
		for(UINT y=80;y<95;++y) for(UINT x=17;x<111;++x) {
			const Vector3 point{(static_cast<float>(x)+.5f)/64-1,1-(static_cast<float>(y)+.5f)/64,0};
			const float expected=EvaluateNeonDissolveSignedDistance(point,renderer.GetParams().dissolve);
			const bool present=depthBits(scene.captures[3],x,y)<0xffffff;
			if(std::abs(expected)>1.0e-4f) Require(present==(expected>=0),"GPU stable spatial noise/threshold must agree with CPU reference at actual plane points");
			Require(!removed[y*kSize+x] || !present,"A fixed plane surface point must not reappear as progress increases");
			removed[y*kSize+x]|=!present;
		}
	}
	params.dissolve.progress=.5f; render();
	const std::array<Capture,5> noisy={scene.captures[0],scene.captures[1],scene.captures[2],scene.captures[3],scene.captures[4]};
	render();
	for(UINT target=0;target<5;++target) RequireSameCapture(scene.captures[target],noisy[target],"Same pose/progress/direction/seed must reproduce exact GPU pixels");
	params.dissolve.seed=998; render();
	UINT seedDifferences=0;
	for(UINT y=80;y<95;++y) for(UINT x=17;x<111;++x) seedDifferences+=depthBits(scene.captures[3],x,y)!=depthBits(noisy[3],x,y);
	Require(seedDifferences>10,"Different seeds must exercise distinct GPU removal patterns");
	if(renderer.IsGeometryLinesSupported()) {
		params.geometryLineEnabled=1; params.dissolve.seed=1337; render();
		for(UINT target=1;target<5;++target) RequireSameCapture(scene.captures[target],noisy[target],"Barycentric + SDF variant must share dissolve surface/cutout/depth/stencil contract");
	}
	params.geometryLineEnabled=0;
	// Skinning coordinate, not raw vertex coordinate: translate the Palette before draw and use its frozen range.
	model.Update(.25f);
	params.dissolve.noiseStrength=0; params.dissolve.scanMin=-.5f; params.dissolve.scanMax=1; params.dissolve.progress=.5f;
	render();
	Require(depthBits(scene.captures[3],88,80)<0xffffff && depthBits(scene.captures[3],72,80)==0xffffff,
		"Dissolve coordinate must follow GPU Palette skinning before World rather than undeformed vertex positions");
	model.Update(0);
	params.dissolve.scanMin=-.75f; params.dissolve.scanMax=.75f; params.dissolve.noiseStrength=.4f;
	// Compare two independent reference draws against a single-fence pair. The transforms keep their footprints disjoint.
	auto rightResource=dx.CreateBufferResource(256);
	TransformationMatrix* right=nullptr;
	Check(rightResource->Map(0,nullptr,reinterpret_cast<void**>(&right)),"Dissolve second transform map");
	transform->WVP.m[0][0]=transform->WVP.m[1][1]=.5f; transform->WVP.m[3][0]=-.5f;
	*right=*transform; right->WVP.m[3][0]=.5f;
	params.dissolve.progress=.25f; params.dissolve.seed=7; render();
	const std::array<Capture,4> first={scene.captures[0],scene.captures[1],scene.captures[2],scene.captures[3]};
	params.dissolve.progress=.75f; params.dissolve.seed=11;
	renderer.BeginFrame(); renderer.SetParams(params); scene.Begin(); renderer.Draw(model,rightResource->GetGPUVirtualAddress(),{0,0,-2},viewport); scene.End();
	const std::array<Capture,4> second={scene.captures[0],scene.captures[1],scene.captures[2],scene.captures[3]};
	renderer.BeginFrame(); scene.Begin(); requestedUploadBytes.clear();
	params.dissolve.progress=.25f; params.dissolve.seed=7; renderer.SetParams(params); renderer.Draw(model,resource->GetGPUVirtualAddress(),{0,0,-2},viewport);
	params.dissolve.progress=.75f; params.dissolve.seed=11; renderer.SetParams(params); renderer.Draw(model,rightResource->GetGPUVirtualAddress(),{0,0,-2},viewport); scene.End();
	Require(requestedUploadBytes==std::vector<size_t>{512},"Additional same-frame draw must own a separate rounded 512-byte snapshot");
	for(UINT y=0;y<kSize;++y) for(UINT x=0;x<kSize;++x) for(UINT target=0;target<4;++target) {
		const auto& expected=x<64 ? first[target] : second[target];
		Require(std::memcmp(scene.captures[target].Pixel(x,y),expected.Pixel(x,y),static_cast<size_t>(expected.rowBytes/kSize))==0,
			"Same-frame draws must retain independent dissolve progress/seed CB snapshots");
	}
	// Actual extruded sphere hull must also vanish at one; it is not replaced by a planar no-outline proxy.
	SkinnedModel sphere; sphere.Initialize(&dx,&srv,"dissolve outline sphere fixture");
	*transform={Identity(),Identity(),Identity()}; transform->WVP.m[2][2]=.4f; transform->WVP.m[3][2]=.5f;
	renderer.SetSubmeshFeatureMasks({}); renderer.SetSubmeshFeatureDistanceMasks({}); renderer.SetSubmeshParams({{1,0,1,1},{1,0,1,1}});
	params=NeonSkinnedParams{}; params.rimStrength=0;
	auto renderSphere=[&] {
		renderer.BeginFrame(); renderer.SetParams(params); scene.Begin(); renderer.Draw(sphere,resource->GetGPUVirtualAddress(),{0,0,-2},viewport); scene.End();
	};
	renderSphere();
	const std::array<Capture,5> intactSphere={scene.captures[0],scene.captures[1],scene.captures[2],scene.captures[3],scene.captures[4]};
	Require(CountBrightPixels(intactSphere[0],0)>100,"Endpoint comparison must exercise a real external hull");
	params.dissolve.enabled=1; params.dissolve.direction={1,0,0}; params.dissolve.scanMin=-.5f; params.dissolve.scanMax=.5f;
	params.dissolve.noiseStrength=0; params.dissolve.progress=0; renderSphere();
	for(UINT target=0;target<5;++target) RequireSameCapture(scene.captures[target],intactSphere[target],"Hull progress zero must be byte-exact legacy before extrusion");
	params.dissolve.edgeEnabled=0; params.dissolve.progress=.5f; renderSphere();
	Require(CountBrightPixels(scene.captures[0],0,0,60)==0 && CountBrightPixels(scene.captures[0],0,68,kSize)>40,
		"Body and expanded Hull must discard the same pre-expansion side of the model");
	params.dissolve.progress=1; params.dissolve.edgeEnabled=1; params.dissolve.edgeIntensity=100;
	renderer.SetSubmeshFeatureMasks({coverageIndex,coverageIndex}); renderer.SetSubmeshFeatureDistanceMasks({distanceIndex,distanceIndex});
	params.geometryLineEnabled=1; params.internalLineEnabled=1; params.featureMaskBlend=1; renderSphere();
	for(UINT y=0;y<kSize;++y) for(UINT x=0;x<kSize;++x) {
		for(UINT target=0;target<3;++target) {
			const auto* pixel=scene.captures[target].Pixel(x,y);
			Require(std::all_of(pixel,pixel+scene.captures[target].rowBytes/kSize,[](uint8_t value){return value==0;}),
				"Progress one must leave no Body, Authored/SDF/Geometry/internal/Hull/boundary pixels in any MRT");
		}
		Require(depthBits(scene.captures[3],x,y)==0xffffff,"Fully removed Body/Hull must not write any depth");
	}
	const std::array<Capture,5> completelyRemoved={scene.captures[0],scene.captures[1],scene.captures[2],scene.captures[3],scene.captures[4]};
	// Positive coverage/SDF/boundary cannot recover a source texture that is entirely cut out.
	auto transparentTexture=CreateTestMask(dx,srv,1,{255,255,255,0});
	renderer.SetSubmeshParams({{1,.5f,1,1},{1,.5f,1,1}});
	params.dissolve.progress=.5f; renderSphere();
	for(UINT target=0;target<5;++target) RequireSameCapture(scene.captures[target],completelyRemoved[target],
		"Fully alpha-cutout real Body/Hull must remain absent even with authored/SDF/geometry and dissolve boundary enabled");
	std::cout<<"PASS: appended 80-byte dissolve / 320-byte GPU constants allocate 512 bytes per independent Draw; disabled and progress-zero byte-exact legacy, surviving-side HDR-only edge, alpha intersection and foreground occlusion; CPU/GPU stable model-space noise parity and monotonic point survival, reproducible seed, Palette-following coordinates, SDF/barycentric/cull variants, camera/World invariance, same-frame progress/seed isolation; full Body and real Hull disappearance with clear MRT/depth and preserved other stencil bits.\n";
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
		testDx = &dx;
		dx.Initialize(nullptr);
		D3D12_DESCRIPTOR_HEAP_DESC srvHeap{};
		srvHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeap.NumDescriptors = 64;
		srvHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		Check(dx.GetDevice()->CreateDescriptorHeap(&srvHeap, IID_PPV_ARGS(&testSrvHeap)), "SRV heap");
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
		invalid.bodyEmissionIntensity = std::numeric_limits<float>::quiet_NaN();
		invalid.bodyPadding = 200;
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
		Require(sanitized.bodyEmissionIntensity == 0 && sanitized.bodyPadding == 0,
			"Invalid body emission and padding must sanitize to zero");
		for (const float value : { -1.0f, 20.0f, std::numeric_limits<float>::infinity() }) {
			invalid.bodyEmissionIntensity = value;
			renderer.SetParams(invalid);
			const float expected = std::isfinite(value) ? (std::clamp)(value, 0.0f, 4.0f) : 0.0f;
			Require(renderer.GetParams().bodyEmissionIntensity == expected, "Body emission must clamp finite intensity to [0,4]");
		}
		std::cout << "PASS: geometry parameter finite / range validation and default OFF.\n";
		Require(sizeof(NeonSkinnedParams) == 288 && sizeof(NeonSkinnedParams) + 32 == 320
			&& offsetof(NeonSkinnedParams, dissolve) == 208 && sizeof(NeonDissolveParams) == 80
			&& offsetof(NeonSkinnedParams, featureMaskColor) == 96
			&& offsetof(NeonSkinnedParams, featureMaskIntensity) == 108
			&& offsetof(NeonSkinnedParams, featureMaskBlend) == 112
			&& offsetof(NeonSkinnedParams, featureMaskDebugMode) == 116,
			"Authored mask constants must append aligned data without moving existing fields");
		NeonSkinnedParams invalidMask;
		invalidMask.featureMaskColor = { -1, std::numeric_limits<float>::quiet_NaN(), 200 };
		invalidMask.featureMaskIntensity = std::numeric_limits<float>::infinity();
		invalidMask.featureMaskBlend = std::numeric_limits<float>::quiet_NaN();
		invalidMask.featureMaskDebugMode = UINT32_MAX;
		invalidMask.featureMaskPadding[0] = invalidMask.featureMaskPadding[1] = 200;
		renderer.SetParams(invalidMask);
		const auto& safeMask = renderer.GetParams();
		Require(safeMask.featureMaskColor.x == 0 && safeMask.featureMaskColor.y == 0 && safeMask.featureMaskColor.z == 100
			&& safeMask.featureMaskIntensity == 0 && safeMask.featureMaskBlend == 0 && safeMask.featureMaskDebugMode == 0
			&& safeMask.featureMaskPadding[0] == 0 && safeMask.featureMaskPadding[1] == 0,
			"Mask constants must sanitize nonfinite / invalid data without enabling masks or diagnostics");
		invalidMask.featureMaskIntensity = 200;
		invalidMask.featureMaskBlend = 2;
		renderer.SetParams(invalidMask);
		Require(renderer.GetParams().featureMaskIntensity == 100 && renderer.GetParams().featureMaskBlend == 1,
			"Mask intensity and application must clamp to finite allowed ranges");
		std::cout << "PASS: appended authored-mask constants, default OFF and finite-range sanitation.\n";
		// 通常Skinning / ShadowのShaderも同じDXC条件でコンパイルできることを確認する。
		ComPtr<IDxcBlob> standard;
		ComPtr<IDxcBlob> shadow;
		standard.Attach(dx.CompileShader(L"resources/shaders/SkinningObject3d.VS.hlsl", L"vs_6_0"));
		shadow.Attach(dx.CompileShader(L"resources/shaders/SkinningShadow.VS.hlsl", L"vs_6_0"));
		TestDraw(dx, srv, renderer);
		TestGeometryLines(dx, srv, renderer);
		TestBodyEmission(dx, srv, renderer);
		TestFeatureMasks(dx, srv, renderer);
		TestQualityLineMasks(dx, srv, renderer);
		TestDirectionalDissolve(dx,srv);
		// Test draw helpers wait for their submitted frame fences before returning.
		const unsigned freesBeforeRelease = testDescriptorFrees;
		renderer.ReleaseGpuResources();
		Require(testDescriptorFrees == freesBeforeRelease + 1, "Renderer release did not return its null fallback SRV");
		renderer.ReleaseGpuResources();
		Require(testDescriptorFrees == freesBeforeRelease + 1, "Repeated renderer release returned its SRV twice");
		std::cout << "PASS: post-fence renderer release returns its null fallback descriptor exactly once.\n";
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
