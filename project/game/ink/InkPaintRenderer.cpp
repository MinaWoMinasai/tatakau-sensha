#include "InkPaintRenderer.h"
#include "DirectXCommon.h"
#include "SrvManager.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>

using Microsoft::WRL::ComPtr;

namespace {
void Check(HRESULT result, const char* operation) {
    if (FAILED(result)) {
        const std::string message = std::string("[InkPaintRenderer] ") + operation +
            " failed (HRESULT " + std::to_string(static_cast<unsigned long>(result)) + ").\n";
        OutputDebugStringA(message.c_str());
        throw std::runtime_error(message);
    }
}

// Keep shader errors visible in release builds, and release DXC output objects.
ComPtr<IDxcBlob> Compile(DirectXCommon* dx, const wchar_t* path, const wchar_t* target) {
    ComPtr<IDxcBlobEncoding> source;
    Check(dx->GetDxcUtils()->LoadFile(path, nullptr, &source), "load paint shader");
    DxcBuffer buffer{ source->GetBufferPointer(), source->GetBufferSize(), DXC_CP_UTF8 };
    const wchar_t* arguments[] = { path, L"-E", L"main", L"-T", target, L"-Zpr", L"-O3" };
    ComPtr<IDxcResult> result;
    Check(dx->GetDxcCompiler()->Compile(&buffer, arguments, _countof(arguments),
        dx->GetIncludeHandler(), IID_PPV_ARGS(&result)), "compile paint shader");
    ComPtr<IDxcBlobUtf8> errors;
    result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
    if (errors && errors->GetStringLength()) {
        OutputDebugStringA(errors->GetStringPointer());
    }
    HRESULT status = E_FAIL;
    Check(result->GetStatus(&status), "get paint shader status");
    if (FAILED(status) && errors && errors->GetStringLength()) {
        throw std::runtime_error(errors->GetStringPointer());
    }
    Check(status, "paint shader compilation");
    ComPtr<IDxcBlob> shader;
    Check(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader), nullptr), "get paint shader");
    return shader;
}

ComPtr<ID3D12RootSignature> MakeRoot(ID3D12Device* device, const D3D12_ROOT_SIGNATURE_DESC& desc) {
    ComPtr<ID3DBlob> blob, errors;
    const HRESULT result = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors);
    if (errors) {
        const std::string message(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize());
        OutputDebugStringA(message.c_str());
    }
    Check(result, "serialize paint root signature");
    ComPtr<ID3D12RootSignature> root;
    Check(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
        IID_PPV_ARGS(&root)), "create paint root signature");
    return root;
}

struct SurfaceGpu {
    Vector4 originWidth;
    Vector4 uHeight;
    Vector4 vInkable;
    Vector4 normalTile;
    Vector4 color;
};
static_assert(sizeof(SurfaceGpu) == 80);

struct StampConstants {
    float centerU, centerV, radiusU, radiusV;
    float cosine, sine, surfaceWidth, surfaceHeight;
    uint32_t tileX, tileY, rectX, rectY;
    uint32_t rectWidth, rectHeight, team, tileSize;
};
static_assert(sizeof(StampConstants) == 64);

struct FrameConstants {
    Matrix4x4 viewProjection;
    Vector4 eye;
};
static_assert(sizeof(FrameConstants) == 80);

void UavBarrier(ID3D12GraphicsCommandList* list, ID3D12Resource* resource) {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = resource;
    list->ResourceBarrier(1, &barrier);
}
}

InkPaintRenderer::~InkPaintRenderer() { Release(); }

void InkPaintRenderer::Release() {
    // The engine waits for its submitted frame before scene destruction. A
    // caller replacing this renderer must likewise retire its previous draw.
    if (srvManager_) {
        srvManager_->Free(maskSrvIndex_);
        srvManager_->Free(maskUavIndex_);
    }
    maskSrvIndex_ = maskUavIndex_ = 0;
    mask_.Reset();
    clearUavHeap_.Reset();
    surfaceBuffer_.Reset();
    paintPso_.Reset();
    surfacePso_.Reset();
    paintRoot_.Reset();
    surfaceRoot_.Reset();
    pendingStamps_.clear();
    surfaces_.clear();
    dxCommon_ = nullptr;
    srvManager_ = nullptr;
}

void InkPaintRenderer::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager,
    const std::vector<ink::Surface>& surfaces) {
    if (!dxCommon || !srvManager || surfaces.empty() || surfaces.size() > kMaxSurfaces) {
        throw std::invalid_argument("InkPaintRenderer requires 1 to 16 surfaces and valid renderer services");
    }
    for (const auto& surface : surfaces) {
        if (!std::isfinite(surface.width) || !std::isfinite(surface.height) ||
            surface.width <= 0 || surface.height <= 0) {
            throw std::invalid_argument("InkPaintRenderer surface dimensions must be positive and finite");
        }
    }
    Release();
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    surfaces_ = surfaces;
    try {
        CreatePipelines();
        auto* device = dxCommon_->GetDevice().Get();
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC texture{};
        texture.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texture.Width = kAtlasSize;
        texture.Height = kAtlasSize;
        texture.DepthOrArraySize = 1;
        texture.MipLevels = 1;
        texture.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texture.SampleDesc.Count = 1;
        texture.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        Check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &texture,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&mask_)), "create persistent paint atlas");
        mask_->SetName(L"Ink Lab persistent team paint atlas");
        maskState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        maskSrvIndex_ = srvManager_->Allocate();
        maskUavIndex_ = srvManager_->Allocate();
        if (!maskSrvIndex_ || !maskUavIndex_) { throw std::runtime_error("InkPaintRenderer descriptor allocation failed"); }
        srvManager_->CreateSRVforTexture2D(maskSrvIndex_, mask_.Get(), texture.Format, 1);
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
        uav.Format = texture.Format;
        uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        device->CreateUnorderedAccessView(mask_.Get(), nullptr, &uav, srvManager_->GetCPUDescriptorHandle(maskUavIndex_));
        // ClearUAV requires a duplicate CPU descriptor in a NON shader-visible
        // heap (the shared SrvManager heap is only valid for its GPU argument).
        D3D12_DESCRIPTOR_HEAP_DESC clearHeapDesc{};
        clearHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        clearHeapDesc.NumDescriptors = 1;
        Check(device->CreateDescriptorHeap(&clearHeapDesc, IID_PPV_ARGS(&clearUavHeap_)), "create paint clear CPU descriptor");
        device->CreateUnorderedAccessView(mask_.Get(), nullptr, &uav, clearUavHeap_->GetCPUDescriptorHandleForHeapStart());

        surfaceBuffer_ = dxCommon_->CreateBufferResource(sizeof(SurfaceGpu) * surfaces_.size());
        if (!surfaceBuffer_) { throw std::runtime_error("InkPaintRenderer surface buffer allocation failed"); }
        SurfaceGpu* data = nullptr;
        const D3D12_RANGE readRange{ 0, 0 };
        Check(surfaceBuffer_->Map(0, &readRange, reinterpret_cast<void**>(&data)), "map static paint surface buffer");
        for (size_t i = 0; i < surfaces_.size(); ++i) {
            const auto& s = surfaces_[i];
            data[i] = {
                { s.origin.x, s.origin.y, s.origin.z, s.width },
                { s.u.x, s.u.y, s.u.z, s.height },
                { s.v.x, s.v.y, s.v.z, s.inkable ? 1.0f : 0.0f },
                { s.normal.x, s.normal.y, s.normal.z, static_cast<float>(i) },
                { s.color.x, s.color.y, s.color.z, 1.0f }
            };
        }
        surfaceBuffer_->Unmap(0, nullptr);
        clearPending_ = true;
        lastStampCount_ = 0;
    } catch (...) {
        Release();
        throw;
    }
}

void InkPaintRenderer::CreatePipelines() {
    auto* device = dxCommon_->GetDevice().Get();
    D3D12_DESCRIPTOR_RANGE uavRange{};
    uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    uavRange.NumDescriptors = 1;
    D3D12_ROOT_PARAMETER paintParameters[2]{};
    paintParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    paintParameters[0].Constants = { 0, 0, sizeof(StampConstants) / 4 };
    paintParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    paintParameters[1].DescriptorTable = { 1, &uavRange };
    D3D12_ROOT_SIGNATURE_DESC paintRootDesc{};
    paintRootDesc.NumParameters = _countof(paintParameters);
    paintRootDesc.pParameters = paintParameters;
    paintRoot_ = MakeRoot(device, paintRootDesc);
    const auto cs = Compile(dxCommon_, L"resources/shaders/InkPaint.CS.hlsl", L"cs_6_0");
    D3D12_COMPUTE_PIPELINE_STATE_DESC computeDesc{};
    computeDesc.pRootSignature = paintRoot_.Get();
    computeDesc.CS = { cs->GetBufferPointer(), cs->GetBufferSize() };
    Check(device->CreateComputePipelineState(&computeDesc, IID_PPV_ARGS(&paintPso_)), "create paint compute pipeline");

    D3D12_DESCRIPTOR_RANGE srvRange{};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 1;
    D3D12_ROOT_PARAMETER surfaceParameters[3]{};
    surfaceParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    surfaceParameters[0].Constants = { 0, 0, sizeof(FrameConstants) / 4 };
    surfaceParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    surfaceParameters[1].Descriptor.ShaderRegister = 1;
    surfaceParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    surfaceParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    surfaceParameters[2].DescriptorTable = { 1, &srvRange };
    surfaceParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC surfaceRootDesc{};
    surfaceRootDesc.NumParameters = _countof(surfaceParameters);
    surfaceRootDesc.pParameters = surfaceParameters;
    surfaceRoot_ = MakeRoot(device, surfaceRootDesc);
    const auto vs = Compile(dxCommon_, L"resources/shaders/InkPaint.VS.hlsl", L"vs_6_0");
    const auto ps = Compile(dxCommon_, L"resources/shaders/InkPaint.PS.hlsl", L"ps_6_0");
    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsDesc{};
    graphicsDesc.pRootSignature = surfaceRoot_.Get();
    graphicsDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    graphicsDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    graphicsDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    graphicsDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    graphicsDesc.RasterizerState.DepthClipEnable = TRUE;
    graphicsDesc.BlendState.IndependentBlendEnable = TRUE;
    for (uint32_t i = 0; i < 3; ++i) {
        auto& blend = graphicsDesc.BlendState.RenderTarget[i];
        blend.SrcBlend = blend.SrcBlendAlpha = D3D12_BLEND_ONE;
        blend.DestBlend = blend.DestBlendAlpha = D3D12_BLEND_ZERO;
        blend.BlendOp = blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blend.LogicOp = D3D12_LOGIC_OP_NOOP;
        blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }
    graphicsDesc.DepthStencilState.DepthEnable = TRUE;
    graphicsDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    graphicsDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    graphicsDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    graphicsDesc.NumRenderTargets = 3;
    graphicsDesc.RTVFormats[0] = DirectXCommon::kSceneRenderTargetFormat;
    graphicsDesc.RTVFormats[1] = DirectXCommon::kNormalBufferFormat;
    graphicsDesc.RTVFormats[2] = DirectXCommon::kMaterialBufferFormat;
    graphicsDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    graphicsDesc.SampleMask = UINT_MAX;
    graphicsDesc.SampleDesc.Count = 1;
    Check(device->CreateGraphicsPipelineState(&graphicsDesc, IID_PPV_ARGS(&surfacePso_)), "create painted surface pipeline");
}

void InkPaintRenderer::QueueStamps(const std::vector<ink::PaintStamp>& stamps) {
    for (const auto& stamp : stamps) {
        if (stamp.surface >= surfaces_.size() || !surfaces_[stamp.surface].inkable ||
            stamp.team > 2 || !std::isfinite(stamp.u) || !std::isfinite(stamp.v) ||
            !std::isfinite(stamp.radiusU) || !std::isfinite(stamp.radiusV) ||
            !std::isfinite(stamp.angle) || stamp.radiusU <= 0 || stamp.radiusV <= 0) { continue; }
        pendingStamps_.push_back(stamp);
    }
}

void InkPaintRenderer::Clear() {
    clearPending_ = true;
    pendingStamps_.clear();
}

void InkPaintRenderer::TransitionMask(D3D12_RESOURCE_STATES state) {
    if (maskState_ == state) { return; }
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition = { mask_.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, maskState_, state };
    dxCommon_->GetList()->ResourceBarrier(1, &barrier);
    maskState_ = state;
}

void InkPaintRenderer::FlushStamps() {
    lastStampCount_ = 0;
    if (!clearPending_ && pendingStamps_.empty()) { return; }
    auto* list = dxCommon_->GetList().Get();
    TransitionMask(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    if (clearPending_) {
        const float clear[4] = { 0, 0, 0, 0 };
        list->ClearUnorderedAccessViewFloat(srvManager_->GetGPUDescriptorHandle(maskUavIndex_),
            clearUavHeap_->GetCPUDescriptorHandleForHeapStart(), mask_.Get(), clear, 0, nullptr);
        UavBarrier(list, mask_.Get());
        clearPending_ = false;
    }
    list->SetComputeRootSignature(paintRoot_.Get());
    list->SetPipelineState(paintPso_.Get());
    list->SetComputeRootDescriptorTable(1, srvManager_->GetGPUDescriptorHandle(maskUavIndex_));
    for (const auto& stamp : pendingStamps_) {
        const auto& s = surfaces_[stamp.surface];
        const float c = std::cos(stamp.angle), sn = std::sin(stamp.angle);
        const float extentU = std::hypot(stamp.radiusU * c, stamp.radiusV * sn);
        const float extentV = std::hypot(stamp.radiusU * sn, stamp.radiusV * c);
        auto pixel = [](float value, float length) {
            return std::clamp(value / length * static_cast<float>(kTileSize), 0.0f, static_cast<float>(kTileSize));
        };
        const uint32_t x0 = static_cast<uint32_t>(std::floor(pixel(stamp.u - extentU, s.width)));
        const uint32_t y0 = static_cast<uint32_t>(std::floor(pixel(stamp.v - extentV, s.height)));
        const uint32_t x1 = static_cast<uint32_t>(std::ceil(pixel(stamp.u + extentU, s.width)));
        const uint32_t y1 = static_cast<uint32_t>(std::ceil(pixel(stamp.v + extentV, s.height)));
        if (x1 <= x0 || y1 <= y0) { continue; }
        const StampConstants constants{
            stamp.u, stamp.v, stamp.radiusU, stamp.radiusV, c, sn, s.width, s.height,
            (stamp.surface % kTilesPerAxis) * kTileSize, (stamp.surface / kTilesPerAxis) * kTileSize,
            x0, y0, x1 - x0, y1 - y0, stamp.team, kTileSize
        };
        list->SetComputeRoot32BitConstants(0, sizeof(constants) / 4, &constants, 0);
        list->Dispatch((constants.rectWidth + 7) / 8, (constants.rectHeight + 7) / 8, 1);
        // Ordered overwrite semantics match the CPU mask even for opposing ink.
        UavBarrier(list, mask_.Get());
        ++lastStampCount_;
    }
    pendingStamps_.clear();
    TransitionMask(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void InkPaintRenderer::Draw(const Matrix4x4& viewProjection, const Vector3& eyePosition) {
    if (!mask_ || surfaces_.empty()) { return; }
    auto* list = dxCommon_->GetList().Get();
    ID3D12DescriptorHeap* heaps[] = { srvManager_->GetSrvHeap().Get() };
    list->SetDescriptorHeaps(1, heaps);
    FlushStamps();
    const FrameConstants frame{ viewProjection, { eyePosition.x, eyePosition.y, eyePosition.z, 1.0f } };
    list->SetGraphicsRootSignature(surfaceRoot_.Get());
    list->SetPipelineState(surfacePso_.Get());
    list->SetGraphicsRoot32BitConstants(0, sizeof(frame) / 4, &frame, 0);
    list->SetGraphicsRootShaderResourceView(1, surfaceBuffer_->GetGPUVirtualAddress());
    list->SetGraphicsRootDescriptorTable(2, srvManager_->GetGPUDescriptorHandle(maskSrvIndex_));
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->DrawInstanced(6, static_cast<UINT>(surfaces_.size()), 0, 0);
}
