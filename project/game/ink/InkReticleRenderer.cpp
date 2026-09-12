#include "InkReticleRenderer.h"
#include "InkReticleMath.h"
#include "DirectXCommon.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

using Microsoft::WRL::ComPtr;
namespace {
void Check(HRESULT result, const char* operation) {
    if (SUCCEEDED(result)) return;
    const std::string message = std::string("[InkReticleRenderer] ") + operation +
        " failed (HRESULT " + std::to_string(static_cast<unsigned long>(result)) + ").\n";
    OutputDebugStringA(message.c_str());
    throw std::runtime_error(message);
}

ComPtr<IDxcBlob> Compile(DirectXCommon* dx, const wchar_t* path, const wchar_t* target) {
    ComPtr<IDxcBlobEncoding> source;
    Check(dx->GetDxcUtils()->LoadFile(path, nullptr, &source), "load reticle shader");
    const DxcBuffer buffer{source->GetBufferPointer(), source->GetBufferSize(), DXC_CP_UTF8};
    const wchar_t* arguments[] = {path, L"-E", L"main", L"-T", target, L"-Zpr", L"-O3", L"-WX"};
    ComPtr<IDxcResult> result;
    Check(dx->GetDxcCompiler()->Compile(&buffer, arguments, _countof(arguments),
        dx->GetIncludeHandler(), IID_PPV_ARGS(&result)), "compile reticle shader");
    ComPtr<IDxcBlobUtf8> errors;
    result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
    if (errors && errors->GetStringLength()) OutputDebugStringA(errors->GetStringPointer());
    HRESULT status = E_FAIL;
    Check(result->GetStatus(&status), "get reticle shader status");
    if (FAILED(status) && errors && errors->GetStringLength())
        throw std::runtime_error(errors->GetStringPointer());
    Check(status, "reticle shader compilation");
    ComPtr<IDxcBlob> shader;
    Check(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader), nullptr), "get reticle shader");
    return shader;
}

struct Constants {
    Vector4 centerViewport; // center x/y, viewport width/height
    Vector4 shape; // corner half-width/height, center-ring radius, line thickness
    Vector4 quad; // tight quad half-width/height, corner half-length, dot radius
    Vector4 feedback; // color RGB, global alpha
    Vector4 charge; // stringer arrow count (zero for shooter), vertical flag, arcs
    Vector4 detail; // side-dot spacing, ring alpha, UI scale, target flag
};
static_assert(sizeof(Constants) == 96);
}

void InkReticleRenderer::Initialize(DirectXCommon* dxCommon) {
    if (!dxCommon) throw std::invalid_argument("InkReticleRenderer needs DirectXCommon");
    dxCommon_ = nullptr;
    pso_.Reset(); root_.Reset();
    auto* device = dxCommon->GetDevice().Get();
    D3D12_ROOT_PARAMETER parameter{};
    parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameter.Constants = {0, 0, sizeof(Constants) / 4};
    parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    D3D12_ROOT_SIGNATURE_DESC rootDesc{};
    rootDesc.NumParameters = 1; rootDesc.pParameters = &parameter;
    ComPtr<ID3DBlob> blob, errors;
    const HRESULT serialized = D3D12SerializeRootSignature(&rootDesc,
        D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors);
    if (errors) {
        const std::string message(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize());
        OutputDebugStringA(message.c_str());
    }
    Check(serialized, "serialize reticle root signature");
    ComPtr<ID3D12RootSignature> root;
    Check(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
        IID_PPV_ARGS(&root)), "create reticle root signature");
    const auto vs = Compile(dxCommon, L"resources/shaders/InkReticle.VS.hlsl", L"vs_6_0");
    const auto ps = Compile(dxCommon, L"resources/shaders/InkReticle.PS.hlsl", L"ps_6_0");
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = root.Get();
    desc.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    desc.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    desc.RasterizerState.DepthClipEnable = TRUE;
    auto& blend = desc.BlendState.RenderTarget[0];
    blend.BlendEnable = TRUE;
    // Shader composites its layers into premultiplied alpha. This is ordinary
    // coverage blending; the HUD never enters HDR/bloom or a scene MRT.
    blend.SrcBlend = D3D12_BLEND_ONE;
    blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blend.BlendOp = D3D12_BLEND_OP_ADD;
    blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.LogicOp = D3D12_LOGIC_OP_NOOP;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    desc.DepthStencilState.DepthEnable = FALSE;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DirectXCommon::kBackBufferRenderTargetFormat;
    // SetBackBuffer keeps the engine's D24 depth view bound during the HUD pass.
    desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.SampleMask = UINT_MAX;
    desc.SampleDesc.Count = 1;
    ComPtr<ID3D12PipelineState> pso;
    Check(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso)), "create reticle pipeline");
    root_ = std::move(root); pso_ = std::move(pso); dxCommon_ = dxCommon;
}

void InkReticleRenderer::Draw(const InkReticleState& state, float viewportWidth,
                             float viewportHeight, float verticalFovRadians) {
    if (!dxCommon_ || !pso_ || !std::isfinite(viewportWidth) || !std::isfinite(viewportHeight) ||
        viewportWidth <= 0.0f || viewportHeight <= 0.0f) return;
    using ink::reticle::ClampFinite;
    const float scale = viewportHeight / 720.0f * ClampFinite(state.displayScale, 0.5f, 3.0f, 1.0f);
    const float major = (std::min)(ink::reticle::ProjectSpreadPixels(state.spreadDegrees,
        verticalFovRadians, viewportHeight), (std::max)(viewportWidth, viewportHeight));
    const float minor = (std::min)(ink::reticle::ProjectSpreadPixels(state.verticalSpreadDegrees,
        verticalFovRadians, viewportHeight), (std::max)(viewportWidth, viewportHeight));
    const float base = (state.stringer ? 20.0f : 14.0f) * scale;
    const float majorExtent = (std::max)(base, major);
    const float minorExtent = (std::max)(base, minor);
    const bool vertical = state.stringer && state.vertical;
    const float halfX = vertical ? minorExtent : majorExtent;
    const float halfY = vertical ? majorExtent : minorExtent;
    const auto arcs = ink::reticle::SplitCharge(state.charge, state.firstChargeRatio);
    const float centerX = std::isfinite(state.center.x) && state.center.x >= 0 ? state.center.x : viewportWidth * 0.5f;
    const float centerY = std::isfinite(state.center.y) && state.center.y >= 0 ? state.center.y : viewportHeight * 0.5f;
    const Vector3 color = state.outOfInk ? Vector3{1.0f, 0.20f, 0.055f} :
        state.target ? Vector3{1.0f, 0.72f, 0.18f} : Vector3{0.95f, 0.98f, 1.0f};
    const float alpha = state.submerged ? 0.35f : 1.0f;
    const Constants constants{
        {centerX, centerY, viewportWidth, viewportHeight},
        {halfX, halfY, 6.5f * scale, 2.0f * scale},
        {halfX + 9.0f * scale, halfY + 9.0f * scale, 4.5f * scale, 1.4f * scale},
        {color.x, color.y, color.z, alpha},
        {state.stringer ? static_cast<float>(std::clamp(state.projectileCount, 1, 3)) : 0.0f,
            vertical ? 1.0f : 0.0f, arcs.first, arcs.second},
        {major, 0.65f, scale, state.target ? 1.0f : 0.0f}
    };
    auto* list = dxCommon_->GetList().Get();
    list->SetGraphicsRootSignature(root_.Get());
    list->SetPipelineState(pso_.Get());
    list->SetGraphicsRoot32BitConstants(0, sizeof(constants) / 4, &constants, 0);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->DrawInstanced(6, 1, 0, 0);
}
