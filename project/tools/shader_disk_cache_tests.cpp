#include "../DirectX/engine/commom/ShaderDiskCache.h"
#include <cassert>
#include <iostream>

#pragma comment(lib, "dxcompiler.lib")

namespace fs = std::filesystem;
using Microsoft::WRL::ComPtr;
using cg2::ShaderDiskCache;

static void Write(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << text;
    output.close();
    assert(output);
}

int main() {
    assert(ShaderDiskCache::Hash("abc", 3) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    const fs::path fixtures = fs::current_path() / (L"fixtures_日本語_" +
        std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(GetTickCount64()));
    fs::create_directories(fixtures);
    const fs::path cacheDirectory = fixtures / L"cache";
    SetEnvironmentVariableW(L"CG2_SHADER_CACHE_DIR", cacheDirectory.c_str());
    SetEnvironmentVariableW(L"CG2_STARTUP_CACHE", L"1");
    const fs::path sourcePath = fixtures / L"main.hlsl";
    const fs::path includePath = fixtures / L"nested/leaf.hlsli";
    const std::string source = "#include \"root.hlsli\"\nfloat4 main() : SV_TARGET { return shade(); }\n";
    Write(sourcePath, source);
    Write(fixtures / L"root.hlsli", "#include \"nested/leaf.hlsli\"\n");
    Write(includePath, "float4 shade() { return float4(1, 0, 0, 1); }\n");

    ComPtr<IDxcUtils> utils;
    ComPtr<IDxcCompiler3> compiler;
    ComPtr<IDxcIncludeHandler> defaultIncludes;
    assert(SUCCEEDED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils))));
    assert(SUCCEEDED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler))));
    assert(SUCCEEDED(utils->CreateDefaultIncludeHandler(&defaultIncludes)));
    ShaderDiskCache cache;
    const std::wstring sourceName = sourcePath.wstring();
    LPCWSTR args[] = { sourceName.c_str(), L"-E", L"main", L"-T", L"ps_6_0", L"-O3", L"-Zpr" };
    const std::string key = cache.MakeKey(sourceName, source.data(), source.size(), args, _countof(args));
    assert(!key.empty());
    assert(!cache.Load(key, defaultIncludes.Get(), utils.Get()));
    ComPtr<ShaderDiskCache::IncludeRecorder> includes;
    includes = Microsoft::WRL::Make<ShaderDiskCache::IncludeRecorder>(defaultIncludes.Get());
    DxcBuffer input{ source.data(), source.size(), DXC_CP_UTF8 };
    ComPtr<IDxcResult> result;
    assert(SUCCEEDED(compiler->Compile(&input, args, _countof(args), includes.Get(), IID_PPV_ARGS(&result))));
    HRESULT status = E_FAIL;
    result->GetStatus(&status);
    if (FAILED(status)) {
        ComPtr<IDxcBlobUtf8> errors;
        result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
        if (errors) std::cerr << errors->GetStringPointer();
    }
    assert(SUCCEEDED(status));
    ComPtr<IDxcBlob> compiled;
    assert(SUCCEEDED(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&compiled), nullptr)));
    assert(includes->valid && !includes->dependencies.empty());
    cache.Store(key, *includes.Get(), compiled.Get());
    auto loaded = cache.Load(key, defaultIncludes.Get(), utils.Get());
    assert(loaded && loaded->GetBufferSize() == compiled->GetBufferSize());
    assert(memcmp(loaded->GetBufferPointer(), compiled->GetBufferPointer(), compiled->GetBufferSize()) == 0);

    // A same-length edit with its original timestamp must still invalidate the entry.
    const auto timestamp = fs::last_write_time(includePath);
    Write(includePath, "float4 shade() { return float4(0, 1, 0, 1); }\n");
    fs::last_write_time(includePath, timestamp);
    assert(!cache.Load(key, defaultIncludes.Get(), utils.Get()));
    Write(includePath, "float4 shade() { return float4(1, 0, 0, 1); }\n");
    assert(cache.Load(key, defaultIncludes.Get(), utils.Get()));
    fs::remove(includePath);
    assert(!cache.Load(key, defaultIncludes.Get(), utils.Get()));
    Write(includePath, "float4 shade() { return float4(1, 0, 0, 1); }\n");
    assert(cache.Load(key, defaultIncludes.Get(), utils.Get()));

    const std::string changedSource = source + "// edited\n";
    assert(cache.MakeKey(sourceName, changedSource.data(), changedSource.size(), args, _countof(args)) != key);
    args[5] = L"-Od";
    assert(cache.MakeKey(sourceName, source.data(), source.size(), args, _countof(args)) != key);
    args[5] = L"-O3";
    args[4] = L"ps_6_1";
    assert(cache.MakeKey(sourceName, source.data(), source.size(), args, _countof(args)) != key);
    args[4] = L"ps_6_0";

    // Previously missing include candidates must invalidate after their creation.
    const fs::path previouslyMissing = fixtures / (L"missing_" + std::to_wstring(GetTickCount64()) + L".hlsli");
    ComPtr<IDxcBlob> absent;
    assert(FAILED(includes->LoadSource(previouslyMissing.c_str(), &absent)));
    cache.Store(key, *includes.Get(), compiled.Get());
    assert(cache.Load(key, defaultIncludes.Get(), utils.Get()));
    Write(previouslyMissing, "// newly introduced include candidate\n");
    assert(!cache.Load(key, defaultIncludes.Get(), utils.Get()));
    includes->dependencies.pop_back();
    cache.Store(key, *includes.Get(), compiled.Get());

    // Corrupt bytecode and truncated files are treated as cache misses.
    const fs::path entry = ShaderDiskCache::CachePath(key);
    {
        std::fstream file(entry, std::ios::binary | std::ios::in | std::ios::out);
        file.seekg(-1, std::ios::end);
        char byte = 0;
        file.read(&byte, 1);
        byte ^= 0x40;
        file.seekp(-1, std::ios::end);
        file.write(&byte, 1);
    }
    assert(!cache.Load(key, defaultIncludes.Get(), utils.Get()));
    Write(entry, "truncated");
    assert(!cache.Load(key, defaultIncludes.Get(), utils.Get()));
    cache.Store(key, *includes.Get(), compiled.Get());
    assert(cache.Load(key, defaultIncludes.Get(), utils.Get()));

    // An unwritable/unusable cache location does not prevent source compilation.
    const fs::path invalidDirectory = fixtures / L"regular-file";
    Write(invalidDirectory, "This is deliberately a file.");
    SetEnvironmentVariableW(L"CG2_SHADER_CACHE_DIR", invalidDirectory.c_str());
    cache.Store(key, *includes.Get(), compiled.Get());
    assert(!cache.Load(key, defaultIncludes.Get(), utils.Get()));
    SetEnvironmentVariableW(L"CG2_STARTUP_CACHE", L"0");
    assert(!ShaderDiskCache::Enabled());
    assert(cache.MakeKey(sourceName, source.data(), source.size(), args, _countof(args)).empty());
    std::cout << "PASS: DXIL roundtrip, Unicode paths, include/source/flags/profile invalidation, "
        "missing include probes, corruption/truncation recovery, unavailable cache, baseline bypass.\n";
}
