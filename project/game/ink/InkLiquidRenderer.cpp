#include "InkLiquidRenderer.h"
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
        const std::string message = std::string("[InkLiquidRenderer] ") + operation +
            " failed (HRESULT " + std::to_string(static_cast<unsigned long>(result)) + ").\n";
        OutputDebugStringA(message.c_str());
        throw std::runtime_error(message);
    }
}

ComPtr<IDxcBlob> Compile(DirectXCommon* dx, const wchar_t* path, const wchar_t* target) {
    ComPtr<IDxcBlobEncoding> source;
    Check(dx->GetDxcUtils()->LoadFile(path, nullptr, &source), "load liquid shader");
    DxcBuffer buffer{ source->GetBufferPointer(), source->GetBufferSize(), DXC_CP_UTF8 };
    const wchar_t* arguments[] = { path, L"-E", L"main", L"-T", target, L"-Zpr", L"-O3" };
    ComPtr<IDxcResult> result;
    Check(dx->GetDxcCompiler()->Compile(&buffer, arguments, _countof(arguments),
        dx->GetIncludeHandler(), IID_PPV_ARGS(&result)), "compile liquid shader");
    ComPtr<IDxcBlobUtf8> errors;
    result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
    if (errors && errors->GetStringLength()) { OutputDebugStringA(errors->GetStringPointer()); }
    HRESULT status = E_FAIL;
    Check(result->GetStatus(&status), "get liquid shader status");
    if (FAILED(status) && errors && errors->GetStringLength()) {
        throw std::runtime_error(errors->GetStringPointer());
    }
    Check(status, "liquid shader compilation");
    ComPtr<IDxcBlob> shader;
    Check(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader), nullptr), "get liquid shader");
    return shader;
}

bool Finite(Vector3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
Vector3 Unit(Vector3 value, Vector3 fallback) {
    const float length = std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    if (!std::isfinite(length) || length < 0.00001f) { return fallback; }
    return { value.x / length, value.y / length, value.z / length };
}
struct FrameConstants {
    Matrix4x4 viewProjection;
    Vector4 eye;
    Vector4 right;
    Vector4 up;
};
static_assert(sizeof(FrameConstants) == 112);
}

InkLiquidRenderer::~InkLiquidRenderer() { Release(); }

void InkLiquidRenderer::Release() {
    // The engine retires its submitted frame before updating or destroying the
    // scene. This upload mapping can therefore be reused on the next draw.
    if (instanceBuffer_ && mappedInstances_) { instanceBuffer_->Unmap(0, nullptr); }
    mappedInstances_ = nullptr;
    instanceBuffer_.Reset();
    pso_.Reset();
    root_.Reset();
    particles_.clear();
    dxCommon_ = nullptr;
}

void InkLiquidRenderer::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    if (!dxCommon || !srvManager) { throw std::invalid_argument("InkLiquidRenderer needs renderer services"); }
    Release();
    dxCommon_ = dxCommon;
    try {
        CreatePipeline();
        static_assert(sizeof(ParticleGpu) == 64);
        instanceBuffer_ = dxCommon_->CreateBufferResource(sizeof(ParticleGpu) * kMaxParticles);
        if (!instanceBuffer_) { throw std::runtime_error("InkLiquidRenderer instance allocation failed"); }
        instanceBuffer_->SetName(L"Ink Lab liquid particle instances");
        const D3D12_RANGE noRead{ 0, 0 };
        Check(instanceBuffer_->Map(0, &noRead, reinterpret_cast<void**>(&mappedInstances_)), "map liquid instance buffer");
        particles_.reserve(kMaxParticles);
        droppedCount_ = 0;
    } catch (...) {
        Release();
        throw;
    }
}

void InkLiquidRenderer::CreatePipeline() {
    auto* device = dxCommon_->GetDevice().Get();
    D3D12_ROOT_PARAMETER parameters[2]{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[0].Constants = { 0, 0, sizeof(FrameConstants) / 4 };
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    parameters[1].Descriptor.ShaderRegister = 0;
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    D3D12_ROOT_SIGNATURE_DESC rootDesc{};
    rootDesc.NumParameters = _countof(parameters);
    rootDesc.pParameters = parameters;
    ComPtr<ID3DBlob> rootBlob, errors;
    const HRESULT result = D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootBlob, &errors);
    if (errors) {
        const std::string message(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize());
        OutputDebugStringA(message.c_str());
    }
    Check(result, "serialize liquid root signature");
    Check(device->CreateRootSignature(0, rootBlob->GetBufferPointer(), rootBlob->GetBufferSize(),
        IID_PPV_ARGS(&root_)), "create liquid root signature");
    const auto vs = Compile(dxCommon_, L"resources/shaders/InkLiquid.VS.hlsl", L"vs_6_0");
    const auto ps = Compile(dxCommon_, L"resources/shaders/InkLiquid.PS.hlsl", L"ps_6_0");
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = root_.Get();
    desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    desc.RasterizerState.DepthClipEnable = TRUE;
    desc.BlendState.IndependentBlendEnable = TRUE;
    for (uint32_t i = 0; i < 3; ++i) {
        auto& blend = desc.BlendState.RenderTarget[i];
        blend.BlendEnable = i == 0;
        blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        blend.BlendOp = D3D12_BLEND_OP_ADD;
        blend.SrcBlendAlpha = D3D12_BLEND_ONE;
        blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blend.LogicOp = D3D12_LOGIC_OP_NOOP;
        // Match CG2's transparent-object convention. Without particle depth
        // writes the normals/material must still describe the opaque depth;
        // replacing them produces false SSAO/SSR halos around the billboards.
        blend.RenderTargetWriteMask = i == 0 ? D3D12_COLOR_WRITE_ENABLE_ALL : 0;
    }
    desc.DepthStencilState.DepthEnable = TRUE;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 3;
    desc.RTVFormats[0] = DirectXCommon::kSceneRenderTargetFormat;
    desc.RTVFormats[1] = DirectXCommon::kNormalBufferFormat;
    desc.RTVFormats[2] = DirectXCommon::kMaterialBufferFormat;
    desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.SampleMask = UINT_MAX;
    desc.SampleDesc.Count = 1;
    Check(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso_)), "create liquid pipeline");
}

void InkLiquidRenderer::BeginFrame() {
    particles_.clear();
    droppedCount_ = 0;
}

void InkLiquidRenderer::QueueBlob(const InkLiquidParticle& particle) {
    QueueBlob(particle.center, particle.velocity, particle.radius, particle.color,
        particle.stretch, particle.age, particle.lifetime, particle.kind);
}

void InkLiquidRenderer::QueueBlob(Vector3 center, Vector3 velocity, float radius, Vector4 color,
    float stretch, float age, float lifetime, InkLiquidKind kind) {
    if (!Finite(center) || !Finite(velocity) || !std::isfinite(radius) || radius <= 0 ||
        !std::isfinite(stretch) || !std::isfinite(age) || !std::isfinite(lifetime) ||
        !std::isfinite(color.x) || !std::isfinite(color.y) || !std::isfinite(color.z) ||
        !std::isfinite(color.w) || color.w <= 0 || (lifetime > 0 && age >= lifetime)) { return; }
    if (particles_.size() >= kMaxParticles) { ++droppedCount_; return; }
    const float normalizedAge = lifetime > 0 ? std::clamp(age / lifetime, 0.0f, 1.0f) : 0.0f;
    color = { std::clamp(color.x, 0.0f, 1.0f), std::clamp(color.y, 0.0f, 1.0f),
        std::clamp(color.z, 0.0f, 1.0f), std::clamp(color.w, 0.0f, 1.0f) };
    particles_.push_back({ {center.x, center.y, center.z, radius},
        {velocity.x, velocity.y, velocity.z, std::clamp(stretch, 0.25f, 4.0f)}, color,
        {normalizedAge, static_cast<float>(kind), 0.07f, lifetime > 0 ? 1.0f : 0.0f} });
}

void InkLiquidRenderer::QueueRipple(Vector3 center, Vector3 normal, float radius, Vector4 color,
    float age, float lifetime, float width) {
    if (!std::isfinite(width)) { return; }
    const size_t before = particles_.size();
    QueueBlob(center, Unit(normal, {0, 1, 0}), radius, color, 1, age, lifetime, InkLiquidKind::Ripple);
    if (particles_.size() > before) { particles_.back().parameters.z = std::clamp(width, 0.015f, 0.8f); }
}

void InkLiquidRenderer::Draw(const Matrix4x4& viewProjection, const Vector3& eyePosition,
    const Vector3& cameraRight, const Vector3& cameraUp) {
    if (!mappedInstances_ || particles_.empty()) { return; }
    const Vector3 right = Unit(cameraRight, {1, 0, 0});
    const Vector3 up = Unit(cameraUp, {0, 1, 0});
    const Vector3 forward = {right.y * up.z - right.z * up.y,
        right.z * up.x - right.x * up.z, right.x * up.y - right.y * up.x};
    auto depth = [&](const ParticleGpu& particle) {
        return (particle.centerRadius.x - eyePosition.x) * forward.x +
            (particle.centerRadius.y - eyePosition.y) * forward.y +
            (particle.centerRadius.z - eyePosition.z) * forward.z;
    };
    // Straight-alpha translucent lumps need far-to-near ordering, unlike the
    // additive lines they replace. All kinds still share one instanced draw.
    std::stable_sort(particles_.begin(), particles_.end(), [&](const auto& a, const auto& b) {
        return depth(a) > depth(b);
    });
    std::memcpy(mappedInstances_, particles_.data(), particles_.size() * sizeof(ParticleGpu));
    const FrameConstants frame{viewProjection, {eyePosition.x, eyePosition.y, eyePosition.z, 1},
        {right.x, right.y, right.z, 0}, {up.x, up.y, up.z, 0}};
    auto* list = dxCommon_->GetList().Get();
    list->SetGraphicsRootSignature(root_.Get());
    list->SetPipelineState(pso_.Get());
    list->SetGraphicsRoot32BitConstants(0, sizeof(frame) / 4, &frame, 0);
    list->SetGraphicsRootShaderResourceView(1, instanceBuffer_->GetGPUVirtualAddress());
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->DrawInstanced(6, static_cast<UINT>(particles_.size()), 0, 0);
}
