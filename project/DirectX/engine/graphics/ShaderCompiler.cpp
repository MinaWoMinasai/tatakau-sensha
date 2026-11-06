#include "ShaderCompiler.h"
#include "ShaderDiskCache.h"
#include "StartupTrace.h"
#include <cassert>
#include <filesystem>

#pragma comment(lib, "dxcompiler.lib")

namespace myEngine {

void ShaderCompiler::Initialize()
{
    // dxcompilerを初期化
    HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
    assert(SUCCEEDED(hr));
    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
    assert(SUCCEEDED(hr));

    // 現時点で includeはしないが、includeに対応するための設定を行っておく
    hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
    assert(SUCCEEDED(hr));
}

Microsoft::WRL::ComPtr<IDxcBlob> ShaderCompiler::Compile(const std::wstring& filePath, const wchar_t* profile)
{
    const std::wstring cacheKey = filePath + L"|" + profile;
    const auto cached = shaderCache_.find(cacheKey);
    if (cached != shaderCache_.end() && cached->second) {
        StartupTrace::Count("shader.memoryHit");
        return cached->second;
    }

    const auto shaderNameUtf8 = std::filesystem::path(filePath).filename().u8string();
    const std::string shaderName(reinterpret_cast<const char*>(shaderNameUtf8.data()), shaderNameUtf8.size());
    Microsoft::WRL::ComPtr<IDxcBlobEncoding> shaderSource;
    HRESULT hr = dxcUtils_->LoadFile(filePath.c_str(), nullptr, &shaderSource);
    if (FAILED(hr) || !shaderSource) {
        OutputDebugStringW((L"Shader source could not be read: " + filePath + L"\n").c_str());
        assert(false);
        return nullptr;
    }
    DxcBuffer shaderSourceBuffer{};
    shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
    shaderSourceBuffer.Size = shaderSource->GetBufferSize();
    shaderSourceBuffer.Encoding = DXC_CP_UTF8;

    LPCWSTR arguments[] = {filePath.c_str(), L"-E",
                           L"main",          L"-T",
                           profile,
#ifdef _DEBUG
                           L"-Zi",           L"-Qembed_debug",
                           L"-Od",
#else
                           L"-O3",
#endif
                           L"-Zpr"};

    static ShaderDiskCache diskCache;
    std::string diskKey;
    if (ShaderDiskCache::Enabled()) {
        StartupTrace::Scope cacheScope("ShaderCache::" + shaderName);
        diskKey = diskCache.MakeKey(filePath, shaderSourceBuffer.Ptr, shaderSourceBuffer.Size, arguments, _countof(arguments));
        auto diskBlob = diskCache.Load(diskKey, includeHandler_.Get(), dxcUtils_.Get());
        if (diskBlob) {
            StartupTrace::Count("shader.diskHit");
            shaderCache_[cacheKey] = diskBlob;
            return diskBlob;
        }
        StartupTrace::Count("shader.diskMiss");
    }

    Microsoft::WRL::ComPtr<ShaderDiskCache::IncludeRecorder> includes;
    if (!diskKey.empty()) {
        includes = Microsoft::WRL::Make<ShaderDiskCache::IncludeRecorder>(includeHandler_.Get());
    }
    Microsoft::WRL::ComPtr<IDxcResult> shaderResult;
    {
        StartupTrace::Scope compileScope("CompileShader::" + shaderName);
        StartupTrace::Count("shader.compile");
        hr = dxcCompiler_->Compile(&shaderSourceBuffer, arguments, _countof(arguments), includes ? includes.Get() : includeHandler_.Get(),
                                   IID_PPV_ARGS(&shaderResult));
    }
    if (FAILED(hr) || !shaderResult) {
        assert(false);
        return nullptr;
    }
    Microsoft::WRL::ComPtr<IDxcBlobUtf8> shaderError;
    shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
    if (shaderError && shaderError->GetStringLength() != 0) {
        OutputDebugStringA(shaderError->GetStringPointer());
        assert(false);
    }
    HRESULT compileStatus = E_FAIL;
    shaderResult->GetStatus(&compileStatus);
    if (FAILED(compileStatus)) {
        return nullptr;
    }
    Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob;
    hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
    if (FAILED(hr) || !shaderBlob) {
        assert(false);
        return nullptr;
    }
    if (includes) {
        diskCache.Store(diskKey, *includes.Get(), shaderBlob.Get());
    }
    shaderCache_[cacheKey] = shaderBlob;
    return shaderBlob;
}

void ShaderCompiler::Release()
{
    shaderCache_.clear();
    includeHandler_.Reset();
    dxcCompiler_.Reset();
    dxcUtils_.Reset();
}

} // namespace myEngine
