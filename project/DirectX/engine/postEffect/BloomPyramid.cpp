#include "BloomPyramid.h"
#include "StartupTrace.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace cg2 {
namespace {
float FiniteClamp(float value, float fallback, float minimum, float maximum) {
    return (std::clamp)(std::isfinite(value) ? value : fallback, minimum, maximum);
}
void Check(HRESULT result, const char* message) {
    if (FAILED(result)) throw std::runtime_error(message);
}
} // namespace

struct BloomPyramid::PipelineBundle {
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> extract, down, up;
};

int BloomPyramid::modeOverride_ = -1;

void BloomPyramid::SetModeOverride(int mode) {
    modeOverride_ = mode >= 0 && mode <= 3 ? mode : -1;
}

uint32_t BloomPyramid::EffectiveMode(const BloomParam& param) {
    if (modeOverride_ >= 0) return static_cast<uint32_t>(modeOverride_);
    return param.bloomMode <= 3 ? param.bloomMode : 2;
}

void BloomPyramid::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager,
    uint32_t sourceWidth, uint32_t sourceHeight) {
    if (!dxCommon || !srvManager || sourceWidth == 0 || sourceHeight == 0)
        throw std::invalid_argument("BloomPyramid requires a device, SRV manager and nonzero source size.");
    if (dxCommon_) throw std::logic_error("BloomPyramid is initialized once; recreate only after GPU idle.");
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    sourceWidth_ = sourceWidth;
    sourceHeight_ = sourceHeight;
    rtvManager_ = std::make_unique<RtvManager>();
    rtvManager_->Initialize(dxCommon_);
    uint32_t width = sourceWidth, height = sourceHeight;
    for (uint32_t level = 0; level < kMaxLevels; ++level) {
        width = (std::max)(1u, width / 2 + width % 2);
        height = (std::max)(1u, height / 2 + height % 2);
        dimensions_[level] = { width, height };
        down_[level] = std::make_unique<RenderTexture>();
        down_[level]->Initialize(dxCommon_, srvManager_, rtvManager_.get(), width, height,
            { 0, 0, 0, 0 }, false, DirectXCommon::kSceneRenderTargetFormat);
        if (level < kMaxLevels - 1) {
            up_[level] = std::make_unique<RenderTexture>();
            up_[level]->Initialize(dxCommon_, srvManager_, rtvManager_.get(), width, height,
                { 0, 0, 0, 0 }, false, DirectXCommon::kSceneRenderTargetFormat);
        }
    }
    CreatePipeline();
    outputSrv_ = down_[0]->GetSrvIndex();
}

uint32_t BloomPyramid::GetExtractSrvIndex() const {
    return down_[0] ? down_[0]->GetSrvIndex() : 0;
}

uint32_t BloomPyramid::Render(uint32_t sourceSrv, const BloomParam& param, uint32_t materialSrv) {
    if (!srvManager_) throw std::logic_error("BloomPyramid is not initialized.");
    const auto source = srvManager_->GetGPUDescriptorHandle(sourceSrv);
    const auto material = materialSrv == (std::numeric_limits<uint32_t>::max)()
        ? D3D12_GPU_DESCRIPTOR_HANDLE{} : srvManager_->GetGPUDescriptorHandle(materialSrv);
    return Render(source, param, material);
}

uint32_t BloomPyramid::Render(D3D12_GPU_DESCRIPTOR_HANDLE source, const BloomParam& param,
    D3D12_GPU_DESCRIPTOR_HANDLE material) {
    if (!dxCommon_ || !source.ptr) throw std::logic_error("BloomPyramid requires an initialized source descriptor.");
    const uint32_t mode = EffectiveMode(param);
    if (mode == 1) throw std::logic_error("Legacy Bloom is rendered by the caller's existing filter path.");
    if (mode == 0) {
        Transition(*down_[0], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        const float black[4]{};
        dxCommon_->GetList()->ClearRenderTargetView(down_[0]->GetRTVHandle(), black, 0, nullptr);
        Transition(*down_[0], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        activeLevelCount_ = 0;
        return outputSrv_ = down_[0]->GetSrvIndex();
    }
    const PassConstants constants{ sourceWidth_, sourceHeight_,
        FiniteClamp(param.threshold, 1, 0, 64), FiniteClamp(param.bloomSoftKnee, .5f, 0, 1),
        FiniteClamp(param.bloomScatter, .3f, 0, .9f), FiniteClamp(param.bloomRadius, .75f, .25f, 2),
        material.ptr != 0 ? 1u : 0u, FiniteClamp(param.waterBloomEnabled, 1, 0, 1),
        FiniteClamp(param.waterDiagnosticsEnabled, 0, 0, 1) };
    activeLevelCount_ = mode == 3 ? 3u : kMaxLevels;
    DrawPass(*down_[0], dimensions_[0], pipelines_->extract.Get(), source,
        material.ptr ? material : source, constants);
    for (uint32_t level = 1; level < activeLevelCount_; ++level) {
        const auto input = down_[level - 1]->GetGPUHandle();
        DrawPass(*down_[level], dimensions_[level], pipelines_->down.Get(), input, input, constants);
    }
    D3D12_GPU_DESCRIPTOR_HANDLE coarse = down_[activeLevelCount_ - 1]->GetGPUHandle();
    for (uint32_t level = activeLevelCount_ - 1; level-- > 0;) {
        DrawPass(*up_[level], dimensions_[level], pipelines_->up.Get(), coarse,
            down_[level]->GetGPUHandle(), constants);
        coarse = up_[level]->GetGPUHandle();
    }
    return outputSrv_ = up_[0]->GetSrvIndex();
}

void BloomPyramid::DrawPass(RenderTexture& output, const LevelDimensions& dimensions,
    ID3D12PipelineState* pipeline, D3D12_GPU_DESCRIPTOR_HANDLE source,
    D3D12_GPU_DESCRIPTOR_HANDLE auxiliary, const PassConstants& constants) {
    Transition(output, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    dxCommon_->SetRenderTargetNoDepth(output.GetRTVHandle());
    dxCommon_->SetViewport(dimensions.width, dimensions.height);
    auto list = dxCommon_->GetList();
    ID3D12DescriptorHeap* heaps[] = { srvManager_->GetSrvHeap().Get() };
    list->SetDescriptorHeaps(1, heaps);
    list->SetGraphicsRootSignature(pipelines_->rootSignature.Get());
    list->SetPipelineState(pipeline);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->SetGraphicsRootDescriptorTable(0, source);
    list->SetGraphicsRootDescriptorTable(1, auxiliary);
    list->SetGraphicsRoot32BitConstants(2, sizeof(PassConstants) / sizeof(uint32_t), &constants, 0);
    list->DrawInstanced(3, 1, 0, 0);
    Transition(output, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void BloomPyramid::Transition(RenderTexture& target, D3D12_RESOURCE_STATES before,
    D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = target.GetResource();
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    dxCommon_->GetList()->ResourceBarrier(1, &barrier);
}

void BloomPyramid::CreatePipeline() {
    // ObjectPostEffect has several categories, but their filtering PSOs are
    // immutable and identical. Weak ownership drops the bundle with its last
    // instance, and device identity prevents reuse across hardware/WARP devices.
    static std::weak_ptr<PipelineBundle> sharedPipeline;
    if (auto existing = sharedPipeline.lock(); existing && existing->device.Get() == dxCommon_->GetDevice().Get()) {
        pipelines_ = std::move(existing);
        return;
    }
    auto bundle = std::make_shared<PipelineBundle>();
    bundle->device = dxCommon_->GetDevice();
    D3D12_DESCRIPTOR_RANGE ranges[2]{};
    D3D12_ROOT_PARAMETER parameters[3]{};
    for (uint32_t index = 0; index < 2; ++index) {
        ranges[index] = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, index, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND };
        parameters[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[index].DescriptorTable = { 1, &ranges[index] };
        parameters[index].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    }
    parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[2].Constants = { 0, 0, sizeof(PassConstants) / sizeof(uint32_t) };
    parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC root{};
    root.NumParameters = 3;
    root.pParameters = parameters;
    root.NumStaticSamplers = 1;
    root.pStaticSamplers = &sampler;
    root.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    Microsoft::WRL::ComPtr<ID3DBlob> serialized, error;
    const HRESULT result = D3D12SerializeRootSignature(&root, D3D_ROOT_SIGNATURE_VERSION_1,
        &serialized, &error);
    if (FAILED(result)) throw std::runtime_error(error
        ? std::string(static_cast<const char*>(error->GetBufferPointer()), error->GetBufferSize())
        : "Cannot serialize BloomPyramid root signature.");
    Check(dxCommon_->GetDevice()->CreateRootSignature(0, serialized->GetBufferPointer(),
        serialized->GetBufferSize(), IID_PPV_ARGS(&bundle->rootSignature)), "Cannot create BloomPyramid root signature.");
    Microsoft::WRL::ComPtr<IDxcBlob> vertex;
    vertex.Attach(dxCommon_->CompileShader(L"resources/shaders/FullScreen.VS.hlsl", L"vs_6_0"));
    D3D12_GRAPHICS_PIPELINE_STATE_DESC description{};
    description.pRootSignature = bundle->rootSignature.Get();
    description.VS = { vertex->GetBufferPointer(), vertex->GetBufferSize() };
    description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    description.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    description.SampleDesc.Count = 1;
    description.NumRenderTargets = 1;
    description.RTVFormats[0] = DirectXCommon::kSceneRenderTargetFormat;
    description.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    description.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    description.RasterizerState.DepthClipEnable = TRUE;
    description.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    description.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    auto& blend = description.BlendState.RenderTarget[0];
    blend.SrcBlend = blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlend = blend.DestBlendAlpha = D3D12_BLEND_ZERO;
    blend.BlendOp = blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.LogicOp = D3D12_LOGIC_OP_NOOP;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    const wchar_t* files[] = { L"resources/shaders/BloomPyramidExtract.PS.hlsl",
        L"resources/shaders/BloomPyramidDownsample.PS.hlsl", L"resources/shaders/BloomPyramidUpsample.PS.hlsl" };
    Microsoft::WRL::ComPtr<ID3D12PipelineState>* pipelines[] = {
        &bundle->extract, &bundle->down, &bundle->up };
    for (uint32_t index = 0; index < 3; ++index) {
        Microsoft::WRL::ComPtr<IDxcBlob> pixel;
        pixel.Attach(dxCommon_->CompileShader(files[index], L"ps_6_0"));
        description.PS = { pixel->GetBufferPointer(), pixel->GetBufferSize() };
        Check(dxCommon_->GetDevice()->CreateGraphicsPipelineState(&description,
            IID_PPV_ARGS(pipelines[index]->GetAddressOf())), "Cannot create BloomPyramid graphics pipeline.");
        StartupTrace::Count("pipeline.graphics.created");
    }
    pipelines_ = std::move(bundle);
    sharedPipeline = pipelines_;
}

} // namespace cg2
