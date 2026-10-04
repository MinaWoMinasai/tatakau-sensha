// Real production BloomPyramid / BloomConstantBuffer tests, without a window or game assets.
// Service adapters only supply the selected Device, command list and descriptor allocation.
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "../DirectX/engine/postEffect/BloomPyramid.h"
#include "../DirectX/engine/postEffect/BloomConstantBuffer.h"
#include <d3d12shader.h>
#include <d3d12sdklayers.h>
#include <DirectXPackedVector.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")
using Microsoft::WRL::ComPtr;
using namespace cg2;

namespace {
void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void Check(HRESULT hr, const char* message) { Require(SUCCEEDED(hr), message); }
std::map<uint32_t, ComPtr<ID3D12Resource>> srvResources;
std::vector<std::wstring> compiledShaders;
std::filesystem::path outputDirectory;
std::ofstream measurements;

ComPtr<IDxcResult> CompileSource(const DxcBuffer& source, const wchar_t* path, const wchar_t* profile) {
    ComPtr<IDxcUtils> utils;
    ComPtr<IDxcCompiler3> compiler;
    ComPtr<IDxcIncludeHandler> include;
    Check(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils)), "DXC utils");
    Check(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)), "DXC compiler");
    Check(utils->CreateDefaultIncludeHandler(&include), "DXC includes");
    const wchar_t* args[] = { path, L"-E", L"main", L"-T", profile, L"-O3", L"-Zpr", L"-WX" };
    ComPtr<IDxcResult> result;
    Check(compiler->Compile(&source, args, _countof(args), include.Get(), IID_PPV_ARGS(&result)), "DXC call");
    HRESULT status{};
    Check(result->GetStatus(&status), "DXC status");
    if (FAILED(status)) {
        ComPtr<IDxcBlobUtf8> errors;
        result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
        if (errors) std::cerr << errors->GetStringPointer();
    }
    Check(status, "Bloom shader compilation failed");
    return result;
}

void VerifyReflection(IDxcResult* result) {
    ComPtr<IDxcUtils> utils;
    Check(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils)), "DXC reflection utils");
    ComPtr<IDxcBlob> reflectionData;
    Check(result->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&reflectionData), nullptr), "Reflection output");
    DxcBuffer reflectionInput{ reflectionData->GetBufferPointer(), reflectionData->GetBufferSize(), 0 };
    ComPtr<ID3D12ShaderReflection> reflection;
    Check(utils->CreateReflection(&reflectionInput, IID_PPV_ARGS(&reflection)), "Shader reflection");
    D3D12_SHADER_DESC shader{};
    Check(reflection->GetDesc(&shader), "Shader description");
    for (UINT index = 0; index < shader.ConstantBuffers; ++index) {
        auto* buffer = reflection->GetConstantBufferByIndex(index);
        D3D12_SHADER_BUFFER_DESC desc{};
        Check(buffer->GetDesc(&desc), "Constant buffer description");
        if (std::strcmp(desc.Name, "BloomPyramidPass") == 0) {
            Require(desc.Size == 48, "Bloom pass root constants must use exactly 12 DWORDs / 48 bytes");
            const std::pair<const char*, UINT> fields[] = {
                {"sourceWidth", 0}, {"sourceHeight", 4}, {"threshold", 8}, {"softKnee", 12},
                {"scatter", 16}, {"radius", 20}, {"materialEnabled", 24}, {"waterBloomEnabled", 28},
                {"waterDiagnosticsEnabled", 32}, {"passPadding", 36}
            };
            for (const auto& [name, offset] : fields) {
                D3D12_SHADER_VARIABLE_DESC variable{};
                Check(buffer->GetVariableByName(name)->GetDesc(&variable), "Bloom pass variable reflection");
                Require(variable.StartOffset == offset && variable.Size == (offset == 36 ? 12u : 4u),
                    "Bloom pass root-constant scalar offset differs from CPU snapshot");
            }
            continue;
        }
        if (std::strcmp(desc.Name, "BloomParam") != 0) continue;
        Require(desc.Size <= sizeof(BloomParam), "Bloom HLSL constants exceed C++ allocation");
        const std::pair<const char*, size_t> offsets[] = {
            {"threshold", offsetof(BloomParam, threshold)}, {"intensity", offsetof(BloomParam, intensity)},
            {"exposure", offsetof(BloomParam, exposure)}, {"waterBloomEnabled", offsetof(BloomParam, waterBloomEnabled)},
            {"bloomMode", offsetof(BloomParam, bloomMode)}, {"bloomSoftKnee", offsetof(BloomParam, bloomSoftKnee)},
            {"bloomScatter", offsetof(BloomParam, bloomScatter)}, {"bloomRadius", offsetof(BloomParam, bloomRadius)},
            {"bloomGain", offsetof(BloomParam, bloomGain)}, {"bloomOutputToHdr", offsetof(BloomParam, bloomOutputToHdr)},
        };
        for (const auto& [name, offset] : offsets) {
            D3D12_SHADER_VARIABLE_DESC variable{};
            if (SUCCEEDED(buffer->GetVariableByName(name)->GetDesc(&variable))) {
                Require(variable.StartOffset == offset && variable.Size == 4,
                    "C++ / HLSL Bloom constant offset or scalar size differs");
                D3D12_SHADER_TYPE_DESC type{};
                Check(buffer->GetVariableByName(name)->GetType()->GetDesc(&type), "Bloom scalar type");
                const bool unsignedInteger = std::strcmp(name, "bloomMode") == 0 || std::strcmp(name, "bloomOutputToHdr") == 0;
                Require(type.Type == (unsignedInteger ? D3D_SVT_UINT : D3D_SVT_FLOAT), "Bloom scalar C++ / HLSL type differs");
            }
        }
        if (desc.Size > offsetof(BloomParam, bloomMode)) Require(desc.Size == sizeof(BloomParam), "Full Bloom cbuffer size differs");
    }
}

void Transition(ID3D12GraphicsCommandList* list, ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
    if (before == after) return;
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition = { resource, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, before, after };
    list->ResourceBarrier(1, &barrier);
}

struct Readback {
    ComPtr<ID3D12Resource> buffer;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT width = 0, height = 0;
    std::vector<uint8_t> bytes;
    void Read() {
        void* mapped = nullptr;
        Check(buffer->Map(0, nullptr, &mapped), "Bloom readback map");
        std::memcpy(bytes.data(), mapped, bytes.size());
        buffer->Unmap(0, nullptr);
    }
    float At(UINT x, UINT y, UINT channel = 0) const {
        Require(x < width && y < height && channel < 4, "Readback component out of bounds");
        uint16_t value;
        std::memcpy(&value, bytes.data() + footprint.Offset + y * footprint.Footprint.RowPitch + x * 8 + channel * 2, 2);
        return DirectX::PackedVector::XMConvertHalfToFloat(value);
    }
};

Readback ScheduleReadback(DirectXCommon& dx, ID3D12Resource* resource,
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) {
    const auto desc = resource->GetDesc();
    Require(desc.Format == DirectXCommon::kSceneRenderTargetFormat, "Readback requires linear FP16 HDR");
    Readback result;
    result.width = static_cast<UINT>(desc.Width);
    result.height = desc.Height;
    UINT64 total = 0;
    dx.GetDevice()->GetCopyableFootprints(&desc, 0, 1, 0, &result.footprint, nullptr, nullptr, &total);
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_READBACK;
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(total);
    Check(dx.GetDevice()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&result.buffer)), "Bloom readback buffer");
    result.bytes.resize(static_cast<size_t>(total));
    Transition(dx.GetList().Get(), resource, state, D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_TEXTURE_COPY_LOCATION source{}, destination{};
    source.pResource = resource;
    source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    destination.pResource = result.buffer.Get();
    destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    destination.PlacedFootprint = result.footprint;
    dx.GetList()->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    Transition(dx.GetList().Get(), resource, D3D12_RESOURCE_STATE_COPY_SOURCE, state);
    return result;
}

struct SourceTexture {
    ComPtr<ID3D12Resource> resource;
    uint32_t srv = UINT32_MAX;
};
SourceTexture MakeSource(DirectXCommon& dx, SrvManager& srv, UINT width, UINT height,
    const std::vector<std::array<float, 4>>& values) {
    Require(values.size() == static_cast<size_t>(width) * height, "Source fixture dimensions");
    SourceTexture result;
    result.resource = dx.CreateTextureResource(width, height, DirectXCommon::kSceneRenderTargetFormat,
        D3D12_RESOURCE_FLAG_NONE, nullptr, D3D12_RESOURCE_STATE_COPY_DEST);
    const auto desc = result.resource->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT64 total = 0;
    dx.GetDevice()->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, nullptr, nullptr, &total);
    auto upload = dx.CreateBufferResource(static_cast<size_t>(total));
    uint8_t* mapped = nullptr;
    Check(upload->Map(0, nullptr, reinterpret_cast<void**>(&mapped)), "HDR source upload map");
    for (UINT y = 0; y < height; ++y) for (UINT x = 0; x < width; ++x) {
        auto* pixel = mapped + footprint.Offset + y * footprint.Footprint.RowPitch + x * 8;
        for (UINT channel = 0; channel < 4; ++channel) {
            const auto half = DirectX::PackedVector::XMConvertFloatToHalf(values[static_cast<size_t>(y) * width + x][channel]);
            std::memcpy(pixel + channel * 2, &half, 2);
        }
    }
    upload->Unmap(0, nullptr);
    D3D12_TEXTURE_COPY_LOCATION source{}, destination{};
    source.pResource = upload.Get();
    source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    source.PlacedFootprint = footprint;
    destination.pResource = result.resource.Get();
    destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dx.GetList()->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    Transition(dx.GetList().Get(), result.resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    dx.PostDraw(); // Upload lifetime extends through the existing Fence wait.
    result.srv = srv.Allocate();
    srv.CreateSRVforTexture2D(result.srv, result.resource.Get(), DirectXCommon::kSceneRenderTargetFormat, 1);
    return result;
}

std::vector<std::array<float, 4>> Solid(UINT width, UINT height, std::array<float, 4> value) {
    return std::vector<std::array<float, 4>>(static_cast<size_t>(width) * height, value);
}
void VerifyFiniteNonnegative(const Readback& image) {
    for (UINT y = 0; y < image.height; ++y) for (UINT x = 0; x < image.width; ++x)
        for (UINT channel = 0; channel < 3; ++channel)
            Require(std::isfinite(image.At(x, y, channel)) && image.At(x, y, channel) >= 0,
                "Bloom RGB must be finite and nonnegative");
}
void VerifyNear(float value, float expected, float tolerance, const char* message) {
    Require(std::isfinite(value) && std::abs(value - expected) <= tolerance, message);
}
float MaxChannel(const Readback& image, UINT channel = 0) {
    float maximum = 0;
    for (UINT y = 0; y < image.height; ++y) for (UINT x = 0; x < image.width; ++x)
        maximum = (std::max)(maximum, image.At(x, y, channel));
    return maximum;
}
bool SameImage(const Readback& a, const Readback& b) {
    if (a.width != b.width || a.height != b.height) return false;
    for (UINT y = 0; y < a.height; ++y)
        if (std::memcmp(a.bytes.data() + a.footprint.Offset + y * a.footprint.Footprint.RowPitch,
            b.bytes.data() + b.footprint.Offset + y * b.footprint.Footprint.RowPitch,
            static_cast<size_t>(a.width) * 8) != 0) return false;
    return true; // Row padding has no defined copy value and is deliberately excluded.
}

Readback RenderReadback(DirectXCommon& dx, BloomPyramid& pyramid, const SourceTexture& source,
    const BloomParam& params, uint32_t material = UINT32_MAX) {
    const uint32_t result = pyramid.Render(source.srv, params, material);
    Require(srvResources.contains(result), "Production Render must return a registered output SRV");
    auto readback = ScheduleReadback(dx, srvResources.at(result).Get());
    dx.PostDraw();
    readback.Read();
    VerifyFiniteNonnegative(readback);
    return readback;
}

// Standard maximum-RGB soft knee, evaluated independently from the production HLSL.
float BrightContribution(float brightness, float threshold, float kneeFraction) {
    const float knee = threshold * kneeFraction;
    float soft = (std::clamp)(brightness - threshold + knee, 0.0f, 2 * knee);
    soft = soft * soft / (std::max)(4 * knee, 0.00001f);
    return (std::max)(brightness - threshold, soft) / (std::max)(brightness, 0.00001f);
}

void TestModeAndLayout() {
    Require(offsetof(BloomParam, bloomMode) == offsetof(BloomParam, waterPostPadding) + sizeof(BloomParam::waterPostPadding),
        "Bloom additions must preserve every preceding constant offset");
    Require(sizeof(BloomParam) == offsetof(BloomParam, bloomMode) + 32,
        "Bloom quality controls must append exactly two 16-byte registers");
    Require(offsetof(BloomParam, bloomSoftKnee) == offsetof(BloomParam, bloomMode) + 4
        && offsetof(BloomParam, bloomScatter) == offsetof(BloomParam, bloomMode) + 8
        && offsetof(BloomParam, bloomRadius) == offsetof(BloomParam, bloomMode) + 12
        && offsetof(BloomParam, bloomGain) == offsetof(BloomParam, bloomMode) + 16
        && offsetof(BloomParam, bloomOutputToHdr) == offsetof(BloomParam, bloomMode) + 20,
        "Bloom quality control offsets must match the two appended GPU registers");
    BloomParam params{};
    Require(params.bloomMode == 2 && params.bloomSoftKnee == .5f && params.bloomScatter == .3f
        && params.bloomRadius == .75f && params.bloomGain == .15f && params.bloomOutputToHdr == 1,
        "Bloom defaults must be explicit and initialized");
    BloomPyramid::SetModeOverride(-1);
    for (uint32_t mode = 0; mode <= 3; ++mode) {
        params.bloomMode = mode;
        Require(BloomPyramid::EffectiveMode(params) == mode, "Bloom modes must remain distinct");
    }
    params.bloomMode = 2;
    for (int overrideMode = 0; overrideMode <= 3; ++overrideMode) {
        BloomPyramid::SetModeOverride(overrideMode);
        Require(BloomPyramid::EffectiveMode(params) == static_cast<uint32_t>(overrideMode), "Diagnostic override must win");
    }
    BloomPyramid::SetModeOverride(-1);
    Require(BloomPyramid::EffectiveMode(params) == 2, "Clearing override must restore parameter mode");
    std::cout << "PASS: appended Bloom controls/defaults/mode override preserve preceding C++ layout.\n";
}

void TestConstantHdr(DirectXCommon& dx, SrvManager& srv) {
    for (const auto size : { std::pair<UINT, UINT>{64, 48}, {257, 129}, {1, 1}, {3, 2} }) {
        BloomPyramid pyramid;
        pyramid.Initialize(&dx, &srv, size.first, size.second);
        for (const uint32_t mode : { 2u, 3u }) {
            BloomParam params{};
            params.threshold = .65f;
            params.bloomMode = mode;
            for (const float brightness : { 0.0f, .1f, .325f, .64f, .65f, .66f, 1.0f, 8.0f, 100.0f }) {
                const std::array<float, 4> color{ brightness, brightness * .25f, brightness * .0625f, 1 };
                auto source = MakeSource(dx, srv, size.first, size.second, Solid(size.first, size.second, color));
                const float input = DirectX::PackedVector::XMConvertHalfToFloat(DirectX::PackedVector::XMConvertFloatToHalf(brightness));
                const float expected = input * BrightContribution(input, params.threshold, params.bloomSoftKnee);
                for (const float radius : { 0.0f, .75f, 2.0f }) {
                    params.bloomRadius = radius;
                    auto result = RenderReadback(dx, pyramid, source, params);
                    for (UINT y = 0; y < result.height; ++y) for (UINT x = 0; x < result.width; ++x) {
                        VerifyNear(result.At(x, y), expected, (std::max)(.005f, expected * .006f),
                            "Pyramid constant HDR must preserve normalized DC without per-level brightness gain");
                        VerifyNear(result.At(x, y, 1), result.At(x, y) * .25f, .001f,
                            "Bloom must preserve nonwhite emitter hue");
                        VerifyNear(result.At(x, y, 2), result.At(x, y) * .0625f, .001f,
                            "Bloom must preserve weak channel ratios");
                    }
                    if (brightness == 0) Require(MaxChannel(result) == 0, "Black input must not create background haze");
                }
                srv.Free(source.srv);
            }
        }
    }
    std::cout << "PASS: Quality/Light constant HDR, maxRGB soft knee, hue, black input, radius-independent DC on even/odd/tiny dimensions.\n";
}

void TestPrefilterBeforeAverage(DirectXCommon& dx, SrvManager& srv) {
    constexpr UINT width = 32, height = 24;
    auto values = Solid(width, height, {0, 0, 0, 1});
    values[10 * width + 12] = {2, .5f, .125f, 1};
    auto source = MakeSource(dx, srv, width, height, values);
    BloomPyramid pyramid;
    pyramid.Initialize(&dx, &srv, width, height);
    BloomParam params{};
    params.threshold = 1;
    params.bloomSoftKnee = 0;
    params.bloomScatter = 0;
    auto result = RenderReadback(dx, pyramid, source, params);
    Require(MaxChannel(result) > .2f, "A bright one-pixel projectile must survive extraction before 2x2 averaging");
    srv.Free(source.srv);
    std::cout << "PASS: pre-average nonlinear extraction retains a one-pixel HDR projectile.\n";
}

void TestSoftKneeAndMaterial(DirectXCommon& dx, SrvManager& srv) {
    constexpr UINT width = 16, height = 12;
    BloomPyramid pyramid;
    pyramid.Initialize(&dx, &srv, width, height);
    BloomParam params{}; params.threshold = 1; params.bloomScatter = 0;
    float previous = -1;
    for (float value : {.49f, .5f, .501f, .99f, 1.0f, 1.01f, 1.499f, 1.5f, 1.501f}) {
        auto source = MakeSource(dx, srv, width, height, Solid(width, height, {value, value * .25f, value * .0625f, 1}));
        auto result = RenderReadback(dx, pyramid, source, params);
        Require(result.At(2, 2) >= previous, "Soft-knee output must be monotonic across both knee boundaries and threshold");
        previous = result.At(2, 2);
        srv.Free(source.srv);
    }
    auto source = MakeSource(dx, srv, width, height, Solid(width, height, {.9f, .225f, .05625f, 1}));
    params.bloomSoftKnee = 0;
    auto hard = RenderReadback(dx, pyramid, source, params);
    Require(MaxChannel(hard) == 0, "Zero knee must use the strict bright-pass threshold");
    params.bloomSoftKnee = .5f;
    auto smooth = RenderReadback(dx, pyramid, source, params);
    Require(MaxChannel(smooth) > 0, "Positive knee must retain a weak highlight just below threshold");
    srv.Free(source.srv);

    auto bright = MakeSource(dx, srv, width, height, Solid(width, height, {8, 2, .5f, 1}));
    auto material = MakeSource(dx, srv, width, height, Solid(width, height, {1, 0, 1, .75f}));
    params.waterBloomEnabled = 0; params.waterDiagnosticsEnabled = 0;
    auto disabledMask = RenderReadback(dx, pyramid, bright, params, material.srv);
    params.waterDiagnosticsEnabled = 1;
    auto waterOff = RenderReadback(dx, pyramid, bright, params, material.srv);
    Require(MaxChannel(waterOff) == 0, "Opt-in water material mask must suppress its excluded bloom pixels");
    params.waterBloomEnabled = 1;
    auto waterOn = RenderReadback(dx, pyramid, bright, params, material.srv);
    Require(SameImage(waterOn, disabledMask), "Water-enabled and inactive material diagnostics must preserve ordinary bloom");
    params.waterBloomEnabled = 0;
    auto noMaterial = RenderReadback(dx, pyramid, bright, params);
    Require(SameImage(noMaterial, disabledMask), "Absent optional material must preserve normal neon emission even with water flags");
    srv.Free(bright.srv); srv.Free(material.srv);
    std::cout << "PASS: soft-knee boundary monotonicity, hard/soft distinction, optional water/material opt-in and absent-material ordinary-neon fallback.\n";
}

void TestOffAndSnapshots(DirectXCommon& dx, SrvManager& srv) {
    constexpr UINT width = 65, height = 37;
    auto source = MakeSource(dx, srv, width, height, Solid(width, height, {8, 2, .5f, 1}));
    auto original = ScheduleReadback(dx, source.resource.Get());
    dx.PostDraw(); original.Read();
    BloomPyramid pyramid;
    pyramid.Initialize(&dx, &srv, width, height);
    UINT expectedWidth = width, expectedHeight = height;
    for (const auto& size : pyramid.GetLevelDimensions()) {
        expectedWidth = (std::max)(1u, (expectedWidth + 1) / 2);
        expectedHeight = (std::max)(1u, (expectedHeight + 1) / 2);
        Require(size.width == expectedWidth && size.height == expectedHeight, "Odd pyramid sizes must ceil-half, with minimum 1");
    }
    BloomParam params{}; params.threshold = 1;
    auto bright = RenderReadback(dx, pyramid, source, params);
    Require(pyramid.GetActiveLevelCount() == 5, "Quality must use five levels");
    params.bloomMode = 0;
    auto off = RenderReadback(dx, pyramid, source, params);
    Require(pyramid.GetActiveLevelCount() == 0, "OFF must not submit a filtering chain");
    for (UINT y = 0; y < off.height; ++y) for (UINT x = 0; x < off.width; ++x)
        for (UINT channel = 0; channel < 4; ++channel) Require(off.At(x, y, channel) == 0, "OFF must clear stale bloom including alpha");
    params.bloomMode = 1;
    bool legacyRejected = false;
    try { pyramid.Render(source.srv, params); } catch (const std::logic_error&) { legacyRejected = true; }
    Require(legacyRejected, "Legacy must retain caller's old filter path, not silently use the new pyramid");
    auto unchanged = ScheduleReadback(dx, source.resource.Get());
    dx.PostDraw(); unchanged.Read();
    Require(SameImage(original, unchanged), "OFF / Quality / Legacy dispatch must never modify the sharp source HDR texture");

    params.bloomMode = 2; params.threshold = .5f;
    const auto firstSrv = pyramid.Render(source.srv, params);
    auto first = ScheduleReadback(dx, srvResources.at(firstSrv).Get());
    params.bloomMode = 3; params.threshold = 3;
    const auto secondSrv = pyramid.Render(source.srv, params);
    auto second = ScheduleReadback(dx, srvResources.at(secondSrv).Get());
    dx.PostDraw(); first.Read(); second.Read();
    Require(pyramid.GetActiveLevelCount() == 3, "Light must use three levels");
    VerifyNear(first.At(3, 3), 7.5f, .025f, "First command-stream root constants must retain their threshold after a later Render");
    VerifyNear(second.At(3, 3), 5.0f, .025f, "Later Render must use its own threshold and mode");
    params.bloomMode = 2; params.threshold = 1;
    params.bloomGain = 4; params.intensity = .01f;
    auto gainChanged = RenderReadback(dx, pyramid, source, params);
    Require(SameImage(bright, gainChanged), "New bloom filtering must not apply composite gain / legacy intensity per pass");
    params.threshold = std::numeric_limits<float>::quiet_NaN();
    params.bloomSoftKnee = std::numeric_limits<float>::infinity();
    params.bloomScatter = -100;
    params.bloomRadius = std::numeric_limits<float>::quiet_NaN();
    auto invalid = RenderReadback(dx, pyramid, source, params);
    VerifyFiniteNonnegative(invalid);
    srv.Free(source.srv);
    std::cout << "PASS: ceil-half dimensions; Quality/Light counts; OFF clears prior output; Legacy explicit caller contract; byte-exact immutable source; per-pass command snapshot isolation; filter independent of final gain; invalid controls produce finite HDR.\n";
}

struct FullscreenPipeline {
    ComPtr<ID3D12RootSignature> root;
    ComPtr<ID3D12PipelineState> pso;
};
FullscreenPipeline MakeFullscreenPipeline(DirectXCommon& dx, IDxcBlob* pixel, UINT textureCount = 0) {
    D3D12_ROOT_PARAMETER parameters[9]{};
    D3D12_DESCRIPTOR_RANGE ranges[8]{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameters[0].Descriptor.ShaderRegister = 0;
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    for (UINT index = 0; index < textureCount; ++index) {
        ranges[index] = {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, index, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND};
        parameters[index + 1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[index + 1].DescriptorTable = {1, &ranges[index]};
        parameters[index + 1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    }
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC root{};
    root.NumParameters = textureCount + 1;
    root.pParameters = parameters;
    root.NumStaticSamplers = textureCount ? 1u : 0u;
    root.pStaticSamplers = &sampler;
    root.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ComPtr<ID3DBlob> serialized, error;
    const auto hr = D3D12SerializeRootSignature(&root, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &error);
    if (FAILED(hr) && error) std::cerr << static_cast<const char*>(error->GetBufferPointer());
    Check(hr, "Bloom probe root serialization");
    FullscreenPipeline result;
    Check(dx.GetDevice()->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
        IID_PPV_ARGS(&result.root)), "Bloom probe root");
    ComPtr<IDxcBlob> vertex;
    vertex.Attach(dx.CompileShader(L"resources/shaders/FullScreen.VS.hlsl", L"vs_6_0"));
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = result.root.Get();
    desc.VS = {vertex->GetBufferPointer(), vertex->GetBufferSize()};
    desc.PS = {pixel->GetBufferPointer(), pixel->GetBufferSize()};
    desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    desc.DepthStencilState.DepthEnable = FALSE;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DirectXCommon::kSceneRenderTargetFormat;
    desc.SampleDesc.Count = 1;
    Check(dx.GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&result.pso)), "Bloom probe PSO");
    return result;
}

ComPtr<IDxcBlob> CompileProbe(const char* source) {
    DxcBuffer input{source, std::strlen(source), DXC_CP_UTF8};
    auto result = CompileSource(input, L"BloomConstantProbe.PS.hlsl", L"ps_6_0");
    VerifyReflection(result.Get());
    ComPtr<IDxcBlob> blob;
    Check(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&blob), nullptr), "Bloom constant probe DXIL");
    return blob;
}

void TestImmutableConstants(DirectXCommon& dx) {
    auto pixel = CompileProbe(
        "#include \"resources/shaders/PostEffectCommon.hlsli\"\n"
        "float4 main(PSInput input):SV_TARGET {return float4(threshold,bloomGain,bloomScatter,(float)bloomMode);}");
    auto pipeline = MakeFullscreenPipeline(dx, pixel.Get());
    auto heap = dx.CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 3, false);
    std::array<ComPtr<ID3D12Resource>, 3> targets;
    std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 3> handles;
    const auto increment = dx.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (UINT index = 0; index < 3; ++index) {
        D3D12_CLEAR_VALUE clear{}; clear.Format = DirectXCommon::kSceneRenderTargetFormat;
        targets[index] = dx.CreateTextureResource(8, 8, clear.Format, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            &clear, D3D12_RESOURCE_STATE_RENDER_TARGET);
        handles[index] = heap->GetCPUDescriptorHandleForHeapStart();
        handles[index].ptr += static_cast<SIZE_T>(index) * increment;
        dx.GetDevice()->CreateRenderTargetView(targets[index].Get(), nullptr, handles[index]);
    }
    BloomConstantBuffer cb;
    cb.Initialize(&dx);
    const auto reusableFirstAddress = cb.GetGPUAddress();
    BloomParam first{}; first.threshold = .25f; first.bloomGain = .125f; first.bloomScatter = .5f; first.bloomMode = 2;
    cb.Update(first);
    const auto firstAddress = cb.GetGPUAddress();
    BloomParam second{}; second.threshold = 2; second.bloomGain = .5f; second.bloomScatter = .25f; second.bloomMode = 3;
    cb.Update(second);
    const auto secondAddress = cb.GetGPUAddress();
    BloomParam invalid{}; invalid.threshold = .75f; invalid.bloomGain = std::numeric_limits<float>::infinity();
    invalid.bloomScatter = -10; invalid.bloomMode = UINT32_MAX;
    cb.Update(invalid);
    const auto thirdAddress = cb.GetGPUAddress();
    Require(firstAddress != secondAddress && firstAddress != thirdAddress && secondAddress != thirdAddress,
        "Same-frame Bloom updates must retain independent immutable GPU addresses");
    Require((firstAddress & 255) == 0 && (secondAddress & 255) == 0 && (thirdAddress & 255) == 0,
        "Bloom CBV snapshots must be 256-byte aligned");
    std::array<Readback, 3> images;
    const D3D12_GPU_VIRTUAL_ADDRESS addresses[]{firstAddress, secondAddress, thirdAddress};
    for (UINT index = 0; index < 3; ++index) {
        dx.SetRenderTargetNoDepth(handles[index]); dx.SetViewport(8, 8);
        auto list = dx.GetList();
        list->SetGraphicsRootSignature(pipeline.root.Get());
        list->SetPipelineState(pipeline.pso.Get());
        list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        list->SetGraphicsRootConstantBufferView(0, addresses[index]);
        list->DrawInstanced(3, 1, 0, 0);
        images[index] = ScheduleReadback(dx, targets[index].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
    }
    dx.PostDraw();
    for (auto& image : images) image.Read();
    const float expected[3][4]{{.25f, .125f, .5f, 2}, {2, .5f, .25f, 3}, {.75f, .15f, 0, 2}};
    for (UINT index = 0; index < 3; ++index) for (UINT channel = 0; channel < 4; ++channel)
        VerifyNear(images[index].At(4, 4, channel), expected[index][channel], .0001f,
            "Immutable Bloom GPU snapshot values must survive all later same-frame updates");
    cb.Update(second);
    Require(cb.GetGPUAddress() == reusableFirstAddress, "Only a completed frame Fence may recycle the first Bloom snapshot");
    std::cout << "PASS: real BloomConstantBuffer immutable same-frame addresses and GPU data, finite control sanitation, 256-byte CBV alignment and post-Fence recycling.\n";
}

float Aces(float value) {
    return (std::clamp)((value * (2.51f * value + .03f)) / (value * (2.43f * value + .59f) + .14f), 0.0f, 1.0f);
}

void TestCompositeAndHue(DirectXCommon& dx, SrvManager& srv) {
    constexpr UINT size = 16;
    const std::array<std::array<float, 4>, 8> colors{{
        {.2f, .05f, .0125f, 1}, {8, 2, .5f, 1}, {1, 1, 1, 1}, {.5f, .5f, 1, 0},
        {0, 0, 0, 0}, {1, 0, 1, 0}, {1, 1, 1, 1}, {0, 0, 0, 0}
    }};
    std::array<SourceTexture, 8> textures;
    for (UINT index = 0; index < 8; ++index) textures[index] = MakeSource(dx, srv, size, size, Solid(size, size, colors[index]));
    ComPtr<IDxcBlob> pixel;
    pixel.Attach(dx.CompileShader(L"resources/shaders/Composite.PS.hlsl", L"ps_6_0"));
    auto pipeline = MakeFullscreenPipeline(dx, pixel.Get(), 8);
    auto heap = dx.CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, false);
    D3D12_CLEAR_VALUE clear{}; clear.Format = DirectXCommon::kSceneRenderTargetFormat;
    auto target = dx.CreateTextureResource(size, size, clear.Format, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
        &clear, D3D12_RESOURCE_STATE_RENDER_TARGET);
    const auto handle = heap->GetCPUDescriptorHandleForHeapStart();
    dx.GetDevice()->CreateRenderTargetView(target.Get(), nullptr, handle);
    BloomConstantBuffer cb;
    cb.Initialize(&dx);
    BloomParam params{};
    params.exposure = .75f;
    params.hdrWhitePoint = 4;
    params.toneMappingMode = 2;
    params.depthNearClip = .1f; params.depthFarClip = 100;
    params.intensity = .75f;
    const auto draw = [&] {
        cb.Update(params);
        dx.SetRenderTargetNoDepth(handle); dx.SetViewport(size, size); srv.PreDraw();
        const auto list = dx.GetList();
        list->SetGraphicsRootSignature(pipeline.root.Get());
        list->SetPipelineState(pipeline.pso.Get());
        list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        list->SetGraphicsRootConstantBufferView(0, cb.GetGPUAddress());
        for (UINT index = 0; index < 8; ++index) list->SetGraphicsRootDescriptorTable(index + 1, srv.GetGPUDescriptorHandle(textures[index].srv));
        list->DrawInstanced(3, 1, 0, 0);
        auto image = ScheduleReadback(dx, target.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
        dx.PostDraw(); image.Read(); VerifyFiniteNonnegative(image);
        return image;
    };
    const float source = DirectX::PackedVector::XMConvertHalfToFloat(DirectX::PackedVector::XMConvertFloatToHalf(.2f));
    for (const uint32_t mode : {0u, 1u, 2u, 3u}) for (const float gain : {0.0f, .15f, .5f, .55f, .8f, 1.2f}) {
        params.bloomMode = mode; params.bloomGain = gain;
        auto result = draw();
        const float effectiveGain = mode == 0 ? 0 : mode == 1 ? params.intensity : gain;
        const float expectedPeak = Aces((source + 8 * effectiveGain) * params.exposure);
        VerifyNear(result.At(8, 8), expectedPeak, .002f, "Actual Composite must apply gain exactly once with the original sharp scene");
        VerifyNear(result.At(8, 8, 1), result.At(8, 8) * .25f, .0004f, "Shared tone-map shoulder must preserve saturated hue ratio");
        VerifyNear(result.At(8, 8, 2), result.At(8, 8) * .0625f, .0004f, "Shared tone-map shoulder must preserve weak color channel");
    }
    params.bloomMode = 0; params.bloomGain = 4;
    auto off = draw();
    params.bloomMode = 2; params.bloomGain = 0;
    auto gainZero = draw();
    Require(SameImage(off, gainZero), "OFF must match sharp-source-only composite even when stale bloom input is bright");
    params.bloomMode = 1; params.toneMappingMode = 1;
    auto legacyTone = draw();
    for (UINT channel = 0; channel < 3; ++channel) {
        const float scale = channel == 0 ? 1 : channel == 1 ? .25f : .0625f;
        const float expected = Aces((source + 8 * params.intensity) * params.exposure * scale);
        VerifyNear(legacyTone.At(8, 8, channel), expected, .002f, "Legacy per-channel ACES must remain selectable");
    }
    for (const auto& texture : textures) srv.Free(texture.srv);
    // Extra shader compilation and reflection also cover category composition layouts.
    for (const wchar_t* shader : {L"resources/shaders/ObjectPostBloomAdd.PS.hlsl", L"resources/shaders/ObjectPostOutlineAdd.PS.hlsl",
        L"resources/shaders/ObjectPostComposite.PS.hlsl", L"resources/shaders/BloomExtract.PS.hlsl", L"resources/shaders/BloomBlurH.PS.hlsl",
        L"resources/shaders/BloomBlurV.PS.hlsl", L"resources/shaders/BloomDownsample.PS.hlsl"}) {
        ComPtr<IDxcBlob> compiled;
        compiled.Attach(dx.CompileShader(shader, L"ps_6_0"));
    }
    std::cout << "PASS: actual Composite OFF/Legacy/Quality/Light gain once and sharp source, neutral-effect identity, hue-preserving shared shoulder and selectable legacy ACES; all category/legacy Bloom shader layouts compile.\n";
}

void TestObjectHdrLdrGlow(DirectXCommon& dx, SrvManager& srv) {
    constexpr UINT size = 8;
    auto bloom = MakeSource(dx, srv, size, size, Solid(size, size, {8, 2, .5f, .02f}));
    auto sharp = MakeSource(dx, srv, size, size, Solid(size, size, {.25f, .0625f, .015625f, 1}));
    auto heap = dx.CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, false);
    D3D12_CLEAR_VALUE clear{}; clear.Format = DirectXCommon::kSceneRenderTargetFormat;
    auto target = dx.CreateTextureResource(size, size, clear.Format, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
        &clear, D3D12_RESOURCE_STATE_RENDER_TARGET);
    const auto handle = heap->GetCPUDescriptorHandleForHeapStart();
    dx.GetDevice()->CreateRenderTargetView(target.Get(), nullptr, handle);
    BloomConstantBuffer cb; cb.Initialize(&dx);
    for (const bool outline : {false, true}) {
        ComPtr<IDxcBlob> shader;
        shader.Attach(dx.CompileShader(outline ? L"resources/shaders/ObjectPostOutlineAdd.PS.hlsl"
            : L"resources/shaders/ObjectPostBloomAdd.PS.hlsl", L"ps_6_0"));
        auto pipeline = MakeFullscreenPipeline(dx, shader.Get(), 8);
        for (const uint32_t mode : {0u, 1u, 2u, 3u}) for (const uint32_t hdr : {0u, 1u}) {
            BloomParam params{}; params.bloomMode = mode; params.bloomOutputToHdr = hdr;
            params.bloomGain = .15f; params.intensity = .75f;
            params.outlineThreshold = .2f;
            cb.Update(params);
            dx.SetRenderTargetNoDepth(handle); dx.SetViewport(size, size); srv.PreDraw();
            const float black[4]{};
            auto list = dx.GetList();
            list->ClearRenderTargetView(handle, black, 0, nullptr);
            list->SetGraphicsRootSignature(pipeline.root.Get()); list->SetPipelineState(pipeline.pso.Get());
            list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            list->SetGraphicsRootConstantBufferView(0, cb.GetGPUAddress());
            for (UINT index = 0; index < 8; ++index) list->SetGraphicsRootDescriptorTable(index + 1,
                srv.GetGPUDescriptorHandle(outline && index == 0 ? sharp.srv : bloom.srv));
            list->DrawInstanced(3, 1, 0, 0);
            auto image = ScheduleReadback(dx, target.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
            dx.PostDraw(); image.Read(); VerifyFiniteNonnegative(image);
            float glow = mode == 0 ? 0 : mode == 1 ? 8 * .75f : 8 * .15f;
            if (mode >= 2 && hdr == 0) glow = .6f * glow / (1 + glow);
            const float expected = glow + (outline ? .25f : 0);
            VerifyNear(image.At(3, 3), expected, .002f,
                "Actual object Bloom/Outline add must preserve sharp core, apply one gain and distinguish HDR from bounded LDR");
            VerifyNear(image.At(3, 3, 1), image.At(3, 3) * .25f, .001f, "Object glow's shared shoulder must preserve hue");
            if (!outline && mode != 0) Require(image.At(3, 3, 3) == 1,
                "Thin-emitter bloom alpha must not attenuate already filtered additive light a second time");
        }
    }
    srv.Free(bloom.srv); srv.Free(sharp.srv);
    std::cout << "PASS: actual ObjectPostBloomAdd / OutlineAdd HDR energy and bounded direct-LDR color, one gain, sharp core preservation, opt-out and thin-coverage additive alpha.\n";
}

void RecordPattern(const char* name, UINT inputWidth, UINT inputHeight, float radius, uint32_t mode, const Readback& image) {
    double energy = 0, sumX = 0, sumY = 0;
    for (UINT y = 0; y < image.height; ++y) for (UINT x = 0; x < image.width; ++x) {
        const double value = image.At(x, y);
        energy += value;
        sumX += value * (static_cast<double>(x) + .5);
        sumY += value * (static_cast<double>(y) + .5);
    }
    Require(energy > 0, "Thin line/projectile/glyph must retain nonzero bloom energy");
    const double centerX = sumX / energy, centerY = sumY / energy;
    std::vector<std::pair<double, double>> radial;
    for (UINT y = 0; y < image.height; ++y) for (UINT x = 0; x < image.width; ++x) {
        const double dx = (static_cast<double>(x) + .5 - centerX) * inputWidth / image.width;
        const double dy = (static_cast<double>(y) + .5 - centerY) * inputHeight / image.height;
        radial.emplace_back(std::sqrt(dx * dx + dy * dy), image.At(x, y));
    }
    std::sort(radial.begin(), radial.end());
    double cumulative = 0, r50 = 0, r90 = 0;
    bool has50 = false, has90 = false;
    for (const auto& [distance, value] : radial) {
        cumulative += value;
        if (!has50 && cumulative >= energy * .5) { r50 = distance; has50 = true; }
        if (!has90 && cumulative >= energy * .9) { r90 = distance; has90 = true; }
    }
    if (measurements) measurements << name << ',' << inputWidth << ',' << inputHeight << ',' << mode << ','
        << radius << ',' << energy * inputWidth * inputHeight / (image.width * image.height) << ','
        << MaxChannel(image) << ',' << r50 << ',' << r90 << '\n';
}

void TestPatterns(DirectXCommon& dx, SrvManager& srv) {
    for (const auto size : { std::pair<UINT, UINT>{128, 128}, {257, 129}, {512, 512} }) {
        const auto [width, height] = size;
        BloomPyramid pyramid;
        pyramid.Initialize(&dx, &srv, width, height);
        std::map<std::pair<uint32_t, float>, Readback> phaseReferences;
        for (int pattern = 0; pattern < 5; ++pattern) {
            auto values = Solid(width, height, {0, 0, 0, 1});
            const UINT centerX = width / 2, centerY = height / 2;
            for (UINT y = 0; y < height; ++y) for (UINT x = 0; x < width; ++x) {
                bool bright = false;
                if (pattern < 2) bright = x == centerX + static_cast<UINT>(pattern) && y == centerY;
                if (pattern == 2) bright = x >= centerX && x < centerX + 3 && y >= centerY && y < centerY + 3;
                if (pattern == 3) bright = x == centerX && y >= centerY - 12 && y <= centerY + 12;
                if (pattern == 4) bright = (x == centerX && y >= centerY && y <= centerY + 8)
                    || (x >= centerX && x <= centerX + 5 && (y == centerY || y == centerY + 4 || y == centerY + 8));
                if (bright) values[static_cast<size_t>(y) * width + x] = {8, 2, .5f, 1};
            }
            auto source = MakeSource(dx, srv, width, height, values);
            for (const uint32_t mode : {2u, 3u}) for (const float radius : {.35f, .75f, 1.25f}) {
                BloomParam params{};
                params.threshold = 1;
                params.bloomMode = mode;
                params.bloomRadius = radius;
                auto result = RenderReadback(dx, pyramid, source, params);
                if (pattern == 0) phaseReferences[{mode, radius}] = result;
                if (pattern == 1) Require(SameImage(result, phaseReferences.at({mode, radius})),
                    "Moving an HDR point inside the same 2x2 footprint must preserve extraction/filter output exactly");
                const char* names[] = {"point_even", "point_odd", "projectile_3px", "line_1px", "glyph_E_6x9"};
                RecordPattern(names[pattern], width, height, radius, mode, result);
                for (UINT y = 0; y < result.height; ++y) for (UINT x = 0; x < result.width; ++x) {
                    VerifyNear(result.At(x, y, 1), result.At(x, y) * .25f, .002f, "Pattern hue must survive bloom filters");
                    VerifyNear(result.At(x, y, 2), result.At(x, y) * .0625f, .002f, "Pattern weak channel must survive bloom filters");
                }
            }
            srv.Free(source.srv);
        }
    }
    std::cout << "PASS: stable finite HDR point phases / 3px projectile / 1px line / 6x9 glyph in three resolutions; impulse energy/R50/R90 recorded as measurements, not quality scores.\n";
}

// Measurements are integrated *linear HDR glow* outside the unchanged emitter,
// in source-pixel units. These are not display brightness or aesthetic scores.
struct HaloAnnulus {
    double nearEnergy = 0, middleEnergy = 0, farEnergy = 0;
    float peak = 0;
    double Total() const { return nearEnergy + middleEnergy + farEnergy; }
};

HaloAnnulus MeasureHaloAnnulus(const Readback& image, UINT sourceWidth, UINT sourceHeight,
    const std::vector<std::pair<UINT, UINT>>& emitters) {
    Require(!emitters.empty(), "Halo fixture must contain an emitter");
    HaloAnnulus result;
    const double area = static_cast<double>(sourceWidth) * sourceHeight / (image.width * image.height);
    for (UINT y = 0; y < image.height; ++y) for (UINT x = 0; x < image.width; ++x) {
        const double px = (static_cast<double>(x) + .5) * sourceWidth / image.width;
        const double py = (static_cast<double>(y) + .5) * sourceHeight / image.height;
        double distanceSquared = (std::numeric_limits<double>::max)();
        for (const auto& [ex, ey] : emitters) {
            // Distance to the closest source-pixel square, rather than to a
            // centroid that would misclassify the ends of a line or glyph.
            const double vx = (std::max)(0.0, std::abs(px - (ex + .5)) - .5);
            const double vy = (std::max)(0.0, std::abs(py - (ey + .5)) - .5);
            distanceSquared = (std::min)(distanceSquared, vx * vx + vy * vy);
        }
        const double distance = std::sqrt(distanceSquared);
        const double value = image.At(x, y) * area;
        if (distance >= 1 && distance < 4) result.nearEnergy += value;
        else if (distance >= 4 && distance < 16) result.middleEnergy += value;
        else if (distance >= 16 && distance < 48) result.farEnergy += value;
        result.peak = (std::max)(result.peak, image.At(x, y));
    }
    return result;
}

void TestTunedPresetHalo(DirectXCommon& dx, SrvManager& srv) {
    ComPtr<IDxcBlob> pixel;
    pixel.Attach(dx.CompileShader(L"resources/shaders/ObjectPostBloomAdd.PS.hlsl", L"ps_6_0"));
    auto pipeline = MakeFullscreenPipeline(dx, pixel.Get(), 1);
    BloomConstantBuffer cb;
    cb.Initialize(&dx);
    std::ofstream csv;
    if (!outputDirectory.empty()) {
        csv.open(outputDirectory / "bloom_tuned_halo_measurements.csv");
        csv << "pattern,source_width,source_height,preset,threshold,soft_knee,scatter,radius,final_gain,"
            "near_1_to_4_source_px,middle_4_to_16_source_px,far_16_to_48_source_px,total_annulus_linear_hdr,"
            "filtered_far_energy_before_composite_cutoff,peak_linear_hdr\n";
    }
    const char* presetNames[] = {"previous_quality", "previous_shape_same_threshold", "tuned_global_quality", "showcase_shape_global_gain", "tuned_showcase_quality"};
    std::array<BloomParam, 5> presets{};
    for (auto& param : presets) {
        param.bloomMode = 2;
        param.bloomSoftKnee = .5f;
        param.bloomOutputToHdr = 1;
    }
    presets[0].threshold = 1; presets[0].bloomScatter = .3f; presets[0].bloomRadius = .75f; presets[0].bloomGain = .15f;
    presets[1] = presets[0]; presets[1].threshold = .65f;
    presets[2] = presets[1]; presets[2].bloomScatter = .55f; presets[2].bloomRadius = .9f; presets[2].bloomGain = .55f;
    presets[3] = presets[2]; presets[3].bloomRadius = 1.0f;
    presets[4] = presets[3]; presets[4].bloomGain = .8f;
    for (const auto size : {std::pair<UINT, UINT>{256, 256}, {257, 129}}) {
        const auto [width, height] = size;
        BloomPyramid pyramid;
        pyramid.Initialize(&dx, &srv, width, height);
        const auto targetSize = pyramid.GetLevelDimensions().front();
        auto heap = dx.CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, false);
        const auto handle = heap->GetCPUDescriptorHandleForHeapStart();
        D3D12_CLEAR_VALUE clear{}; clear.Format = DirectXCommon::kSceneRenderTargetFormat;
        auto target = dx.CreateTextureResource(targetSize.width, targetSize.height, clear.Format,
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, &clear, D3D12_RESOURCE_STATE_RENDER_TARGET);
        dx.GetDevice()->CreateRenderTargetView(target.Get(), nullptr, handle);
        for (int pattern = 0; pattern < 4; ++pattern) {
            auto values = Solid(width, height, {0, 0, 0, 1});
            std::vector<std::pair<UINT, UINT>> emitters;
            const UINT cx = width / 2, cy = height / 2;
            for (UINT y = 0; y < height; ++y) for (UINT x = 0; x < width; ++x) {
                const bool bright = (pattern == 0 && x == cx && y == cy)
                    || (pattern == 1 && x >= cx && x < cx + 3 && y >= cy && y < cy + 3)
                    || (pattern == 2 && x == cx && y >= cy - 12 && y <= cy + 12)
                    || (pattern == 3 && ((x == cx && y >= cy && y <= cy + 8)
                        || (x >= cx && x <= cx + 5 && (y == cy || y == cy + 4 || y == cy + 8))));
                if (bright) {
                    values[static_cast<size_t>(y) * width + x] = {8, 2, .5f, 1};
                    emitters.emplace_back(x, y);
                }
            }
            auto source = MakeSource(dx, srv, width, height, values);
            auto before = ScheduleReadback(dx, source.resource.Get());
            dx.PostDraw(); before.Read();
            std::array<HaloAnnulus, 5> statistics;
            std::array<HaloAnnulus, 5> filteredStatistics;
            Readback showcaseFilter;
            for (size_t preset = 0; preset < presets.size(); ++preset) {
                const auto& param = presets[preset];
                auto raw = RenderReadback(dx, pyramid, source, param);
                filteredStatistics[preset] = MeasureHaloAnnulus(raw, width, height, emitters);
                if (preset == 3) showcaseFilter = raw;
                if (preset == 4) Require(SameImage(raw, showcaseFilter),
                    "Changing only showcase composite gain must not modify the HDR filtering result");
                cb.Update(param);
                dx.SetRenderTargetNoDepth(handle); dx.SetViewport(targetSize.width, targetSize.height); srv.PreDraw();
                const auto list = dx.GetList();
                constexpr float black[4]{};
                list->ClearRenderTargetView(handle, black, 0, nullptr);
                list->SetGraphicsRootSignature(pipeline.root.Get());
                list->SetPipelineState(pipeline.pso.Get());
                list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                list->SetGraphicsRootConstantBufferView(0, cb.GetGPUAddress());
                list->SetGraphicsRootDescriptorTable(1, srv.GetGPUDescriptorHandle(pyramid.GetOutputSrvIndex()));
                list->DrawInstanced(3, 1, 0, 0);
                auto glow = ScheduleReadback(dx, target.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
                dx.PostDraw(); glow.Read(); VerifyFiniteNonnegative(glow);
                for (UINT y = 0; y < glow.height; ++y) for (UINT x = 0; x < glow.width; ++x) {
                    // The production additive pass intentionally discards
                    // filtered energy <= .001. Clear first, and preserve that
                    // contract rather than counting invisible low-energy glow.
                    const float scaled = raw.At(x, y) * param.bloomGain;
                    const float expected = scaled > .001f ? scaled : 0.0f;
                    const float tolerance = (std::max)(.00002f, raw.At(x, y) * .001f);
                    if (std::abs(glow.At(x, y) - expected) > tolerance) std::cerr << "Halo mismatch: source=" << width << 'x' << height
                        << " pattern=" << pattern << " preset=" << presetNames[preset] << " pixel=" << x << ',' << y
                        << " raw=" << raw.At(x, y) << " gain=" << param.bloomGain << " expected=" << expected
                        << " actual=" << glow.At(x, y) << " tolerance=" << tolerance << '\n';
                    VerifyNear(glow.At(x, y), expected,
                        tolerance,
                        "Actual HDR addition must apply the tuning gain exactly once");
                    VerifyNear(glow.At(x, y, 1), glow.At(x, y) * .25f, .002f, "Tuned halo must preserve saturated emitter hue");
                    VerifyNear(glow.At(x, y, 2), glow.At(x, y) * .0625f, .002f, "Tuned halo must preserve its weak color channel");
                }
                statistics[preset] = MeasureHaloAnnulus(glow, width, height, emitters);
                const auto& stats = statistics[preset];
                const char* patternNames[] = {"point_1px", "projectile_3px", "line_1px", "glyph_E_6x9"};
                if (csv) csv << patternNames[pattern] << ',' << width << ',' << height << ',' << presetNames[preset] << ','
                    << param.threshold << ',' << param.bloomSoftKnee << ',' << param.bloomScatter << ',' << param.bloomRadius << ','
                    << param.bloomGain << ',' << stats.nearEnergy << ',' << stats.middleEnergy << ',' << stats.farEnergy << ','
                    << stats.Total() << ',' << filteredStatistics[preset].farEnergy << ',' << stats.peak << '\n';
            }
            Require(statistics[0].Total() > 0 && statistics[2].Total() > statistics[0].Total(),
                "Tuned global preset must increase measured glow outside a point, projectile, line and glyph emitter");
            Require(statistics[4].Total() > statistics[3].Total(),
                "Showcase gain must increase measured halo without changing its filtering shape");
            if (pattern == 0) Require(filteredStatistics[2].farEnergy > filteredStatistics[1].farEnergy,
                "At identical threshold, tuned scatter/radius must increase distant impulse energy before composite gain/cutoff");
            auto after = ScheduleReadback(dx, source.resource.Get());
            dx.PostDraw(); after.Read();
            Require(SameImage(before, after), "Preset halo tuning must leave the original sharp HDR emitter unchanged");
            srv.Free(source.srv);
        }
    }
    std::cout << "PASS: actual previous/tuned-global/tuned-showcase HDR halo annuli on point/projectile/line/glyph; single gain, unchanged sharp source, same-threshold coarse spread and hue. Measurements are pre-tone-map energy, not visual-quality scores.\n";
}
}

// These adapters reuse the normal engine ownership interfaces. No private production state is exposed.
IDxcBlob* DirectXCommon::CompileShader(const std::wstring& path, const wchar_t* profile) {
    ComPtr<IDxcUtils> utils;
    Check(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils)), "DXC source utils");
    ComPtr<IDxcBlobEncoding> source;
    Check(utils->LoadFile(path.c_str(), nullptr, &source), "Bloom shader source");
    DxcBuffer input{source->GetBufferPointer(), source->GetBufferSize(), DXC_CP_UTF8};
    auto result = CompileSource(input, path.c_str(), profile);
    VerifyReflection(result.Get());
    compiledShaders.push_back(path);
    ComPtr<IDxcBlob> blob;
    Check(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&blob), nullptr), "Bloom DXIL");
    return blob.Detach();
}
void DirectXCommon::Initialize(WinApp*) {
    D3D12_COMMAND_QUEUE_DESC desc{};
    Check(device_->CreateCommandQueue(&desc, IID_PPV_ARGS(&queue_)), "Bloom test queue");
    Check(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator_)), "Bloom test allocator");
    Check(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator_.Get(), nullptr, IID_PPV_ARGS(&list_)), "Bloom test list");
    Check(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)), "Bloom test fence");
}
void DirectXCommon::PostDraw() {
    Check(list_->Close(), "Close Bloom test list");
    ID3D12CommandList* lists[]{list_.Get()};
    queue_->ExecuteCommandLists(1, lists);
    Check(queue_->Signal(fence_.Get(), ++fenceValue_), "Bloom test fence signal");
    Check(fence_->SetEventOnCompletion(fenceValue_, fenceEvent_), "Bloom test fence event");
    Require(WaitForSingleObject(fenceEvent_, 30000) == WAIT_OBJECT_0, "Bloom test GPU timeout");
    Check(allocator_->Reset(), "Bloom test allocator reset");
    Check(list_->Reset(allocator_.Get(), nullptr), "Bloom test list reset");
}
void DirectXCommon::ExecuteCommandListAndWait() { PostDraw(); }
ComPtr<ID3D12Resource> DirectXCommon::CreateTextureResource(uint32_t width, uint32_t height, DXGI_FORMAT format,
    D3D12_RESOURCE_FLAGS flags, const D3D12_CLEAR_VALUE* clearValue, D3D12_RESOURCE_STATES state) {
    auto desc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1);
    desc.Flags = flags;
    CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_DEFAULT);
    ComPtr<ID3D12Resource> resource;
    Check(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, state, clearValue, IID_PPV_ARGS(&resource)), "Bloom texture");
    return resource;
}
ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResource(size_t size) {
    CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
    auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);
    ComPtr<ID3D12Resource> resource;
    Check(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource)), "Bloom upload buffer");
    return resource;
}
void DirectXCommon::SetRenderTargetNoDepth(D3D12_CPU_DESCRIPTOR_HANDLE rtv) {
    list_->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    currentRtvHandle_ = rtv;
    currentHasDsv_ = false;
}
void DirectXCommon::SetViewport(uint32_t width, uint32_t height) {
    viewportRect_ = {0, 0, static_cast<float>(width), static_cast<float>(height), 0, 1};
    scissorRect_ = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    list_->RSSetViewports(1, &viewportRect_);
    list_->RSSetScissorRects(1, &scissorRect_);
}
ComPtr<ID3D12DescriptorHeap> DirectXCommon::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT count, bool visible) {
    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.Type = type;
    desc.NumDescriptors = count;
    desc.Flags = visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ComPtr<ID3D12DescriptorHeap> heap;
    Check(device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)), "Bloom descriptor heap");
    return heap;
}
D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetNewDsvHandle() { throw std::logic_error("Bloom fixture does not create depth"); }
const uint32_t SrvManager::kMaxSrvCount = 8192;
void SrvManager::Initialize(DirectXCommon* dx) {
    dxCommon_ = dx;
    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 1024;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    Check(dx->GetDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap_)), "Bloom test SRV heap");
    descriptorSize = dx->GetDevice()->GetDescriptorHandleIncrementSize(desc.Type);
    useIndex_ = 1;
}
uint32_t SrvManager::Allocate() {
    uint32_t index;
    if (!freeIndices_.empty()) { index = freeIndices_.back(); freeIndices_.pop_back(); }
    else { Require(useIndex_ < 1024, "Bloom test SRV heap exhausted"); index = useIndex_++; }
    return index;
}
void SrvManager::Free(uint32_t index) { srvResources.erase(index); freeIndices_.push_back(index); }
D3D12_CPU_DESCRIPTOR_HANDLE SrvManager::GetCPUDescriptorHandle(uint32_t index) {
    auto handle = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(index) * descriptorSize;
    return handle;
}
D3D12_GPU_DESCRIPTOR_HANDLE SrvManager::GetGPUDescriptorHandle(uint32_t index) {
    auto handle = descriptorHeap_->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<UINT64>(index) * descriptorSize;
    return handle;
}
void SrvManager::CreateSRVforTexture2D(uint32_t index, ID3D12Resource* resource, DXGI_FORMAT format, UINT levels) {
    D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
    desc.Format = format;
    desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    desc.Texture2D.MipLevels = levels;
    dxCommon_->GetDevice()->CreateShaderResourceView(resource, &desc, GetCPUDescriptorHandle(index));
    srvResources[index] = resource;
}
void SrvManager::CreateSRVforShadowMap(uint32_t, ID3D12Resource*) { throw std::logic_error("Bloom fixture does not create shadow SRVs"); }
void SrvManager::PreDraw() { ID3D12DescriptorHeap* heaps[]{descriptorHeap_.Get()}; dxCommon_->GetList()->SetDescriptorHeaps(1, heaps); }
void SrvManager::SetGraphicsRootDescriptorTable(UINT root, uint32_t index) { dxCommon_->GetList()->SetGraphicsRootDescriptorTable(root, GetGPUDescriptorHandle(index)); }

int main(int argc, char** argv) {
    try {
        std::cout << std::unitbuf;
        bool hardware = false, cpuOnly = false;
        for (int index = 1; index < argc; ++index) {
            if (std::strcmp(argv[index], "--hardware") == 0) hardware = true;
            else if (std::strcmp(argv[index], "--cpu-only") == 0) cpuOnly = true;
            else if (std::strcmp(argv[index], "--output") == 0 && index + 1 < argc) outputDirectory = argv[++index];
            else throw std::runtime_error("Usage: bloom_pipeline_tests [--hardware] [--cpu-only] [--output directory]");
        }
        TestModeAndLayout();
        if (cpuOnly) { std::cout << "PASS: CPU-only; GPU / PSO / draw checks not executed.\n"; return 0; }
        if (!outputDirectory.empty()) {
            std::filesystem::create_directories(outputDirectory);
            measurements.open(outputDirectory / "bloom_pattern_measurements.csv");
            measurements << "pattern,source_width,source_height,mode,radius,area_scaled_energy,peak,R50_source_pixels,R90_source_pixels\n";
        }
        ComPtr<ID3D12Debug> debug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) debug->EnableDebugLayer();
        ComPtr<IDXGIFactory4> factory;
        Check(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "Bloom test DXGI factory");
        ComPtr<IDXGIAdapter1> adapter;
        DirectXCommon dx;
        if (hardware) {
            ComPtr<IDXGIFactory6> preference;
            Check(factory.As(&preference), "Bloom GPU preference factory");
            for (UINT index = 0;; ++index) {
                HRESULT hr = preference->EnumAdapterByGpuPreference(index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter));
                if (hr == DXGI_ERROR_NOT_FOUND) break;
                Check(hr, "Bloom GPU enumeration");
                DXGI_ADAPTER_DESC1 desc{};
                Check(adapter->GetDesc1(&desc), "Bloom GPU adapter description");
                if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                    && SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&dx.GetDevice())))) break;
                adapter.Reset();
            }
            if (!dx.GetDevice()) { std::cout << "SKIP: DirectX 12 hardware Device unavailable.\n"; return 0; }
        } else {
            Check(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)), "Bloom WARP adapter");
            Check(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&dx.GetDevice())), "Bloom WARP Device");
        }
        DXGI_ADAPTER_DESC1 desc{};
        Check(adapter->GetDesc1(&desc), "Bloom adapter description");
        std::wcout << L"Device: " << desc.Description << (hardware ? L" (hardware)\n" : L" (WARP)\n");
        dx.Initialize(nullptr);
        SrvManager srv;
        srv.Initialize(&dx);
        srv.PreDraw();
        TestImmutableConstants(dx);
        TestConstantHdr(dx, srv);
        TestPrefilterBeforeAverage(dx, srv);
        TestSoftKneeAndMaterial(dx, srv);
        TestOffAndSnapshots(dx, srv);
        TestCompositeAndHue(dx, srv);
        TestObjectHdrLdrGlow(dx, srv);
        TestPatterns(dx, srv);
        TestTunedPresetHalo(dx, srv);
        ComPtr<ID3D12InfoQueue> info;
        if (SUCCEEDED(dx.GetDevice().As(&info))) for (UINT64 index = 0; index < info->GetNumStoredMessages(); ++index) {
            SIZE_T bytes = 0;
            Check(info->GetMessage(index, nullptr, &bytes), "Bloom debug message size");
            std::vector<char> storage(bytes);
            auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
            Check(info->GetMessage(index, message, &bytes), "Bloom debug message");
            if (message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR) {
                std::cerr << message->pDescription << '\n';
                throw std::runtime_error("D3D12 debug layer reported a Bloom pipeline error");
            }
        }
        std::cout << "PASS: production BloomPyramid GPU / shader reflection / HDR readback tests; actual game appearance/performance is a separate live check.\n";
        srvResources.clear();
        return 0;
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
