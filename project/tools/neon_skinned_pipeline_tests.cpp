// 独立したWARP検証。ゲーム/Preview導線やモデル素材は必要としない。
// Shader読み込みサービスだけをDXCへ接続し、実際のRendererのPSO作成コードを実行する。
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "SkinCluster.h"
#include "../DirectX/engine/3d/neon/NeonSkinnedRenderer.h"
#include <d3d12shader.h>
#include <d3d12sdklayers.h>
#include <iostream>
#include <stdexcept>

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
}

// 本テストではDirectXCommon.cppをリンクしない。DeviceはWARP、Shaderは実DXCを利用する。
// Drawは実行しない。描画サービスは下のthrow-only stubで意図しない呼び出しを検出する。
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
		Require(bufferDesc.Size == sizeof(NeonSkinnedParams) + 16, "C++ / HLSL constant buffer size differs");
		const struct { const char* name; UINT offset; } expected[] = {
			{ "gBodyColor", static_cast<UINT>(offsetof(NeonSkinnedParams, bodyColor)) },
			{ "gEmissiveColor", static_cast<UINT>(offsetof(NeonSkinnedParams, emissiveColor)) },
			{ "gEmissiveIntensity", static_cast<UINT>(offsetof(NeonSkinnedParams, emissiveIntensity)) },
			{ "gRimStrength", static_cast<UINT>(offsetof(NeonSkinnedParams, rimStrength)) },
			{ "gRimPower", static_cast<UINT>(offsetof(NeonSkinnedParams, rimPower)) },
			{ "gCameraWorldPosition", sizeof(NeonSkinnedParams) },
		};
		for (const auto& field : expected) {
			D3D12_SHADER_VARIABLE_DESC variable{};
			Check(constants->GetVariableByName(field.name)->GetDesc(&variable), "Neon constant field reflection");
			Require(variable.StartOffset == field.offset, "C++ / HLSL constant field offset differs");
		}
	}
	if (filePath.find(L"NeonSkinned.") != std::wstring::npos) ++compiledNeonShaders;
	ComPtr<IDxcBlob> blob;
	Check(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&blob), nullptr), "DXIL object");
	return blob.Detach();
}

ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResource(size_t) {
	throw std::logic_error("Pipeline initialization must not allocate draw constants");
}

void SrvManager::PreDraw() {
	throw std::logic_error("Pipeline test does not bind a descriptor heap");
}

void SkinnedModel::BindGeometry() const {
	throw std::logic_error("Pipeline test does not bind model geometry");
}

void SkinnedModel::BindSkinningPalette(uint32_t) const {
	throw std::logic_error("Pipeline test does not bind a model palette");
}

void SkinnedModel::DrawSubmesh(size_t) const {
	throw std::logic_error("Pipeline test does not draw a model");
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
		Require(compiledNeonShaders == 2, "Neon shaders were not compiled by the actual Renderer");
		// 通常Skinning / ShadowのShaderも同じDXC条件でコンパイルできることを確認する。
		ComPtr<IDxcBlob> standard;
		ComPtr<IDxcBlob> shadow;
		standard.Attach(dx.CompileShader(L"resources/shaders/SkinningObject3d.VS.hlsl", L"vs_6_0"));
		shadow.Attach(dx.CompileShader(L"resources/shaders/SkinningShadow.VS.hlsl", L"vs_6_0"));
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
		std::cout << "PASS: actual Neon Renderer creates its root signature and both WARP PSOs; "
			"C++ / HLSL CB layout matches; Neon, standard Skinning and Shadow DXIL compile. "
			"No model draw or visual verification.\n";
		return 0;
	} catch (const std::exception& error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
