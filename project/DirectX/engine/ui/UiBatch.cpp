#include "UiBatch.h"
#include "DirectXCommon.h"
#include "DynamicTexture.h"
#include "LogWrite.h"
#include "ResourcePaths.h"
#include "SpriteCommon.h"
#include "WinApp.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace myEngine {

namespace {

float SrgbToLinear(float value)
{
    return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

} // namespace

Vector4 UiColor(uint32_t rgb, float alpha)
{
    return {SrgbToLinear(((rgb >> 16) & 0xFF) / 255.0f), SrgbToLinear(((rgb >> 8) & 0xFF) / 255.0f), SrgbToLinear((rgb & 0xFF) / 255.0f),
            alpha};
}

UiBatch::UiBatch() : screenSize_{float(WinApp::kClientWidth), float(WinApp::kClientHeight)} {}

UiBatch::~UiBatch()
{
    if (vertexBuffer_ && mappedVertices_) {
        vertexBuffer_->Unmap(0, nullptr);
    }
    // 積んだ描画命令が残っているかもしれないので、GPU が終わるまで預ける。
    if (dxCommon_ && vertexBuffer_) {
        dxCommon_->ReleaseAfterExecution(std::move(vertexBuffer_));
    }
}

bool UiBatch::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
    if (!dxCommon || !srvManager || pipeline_) {
        return false;
    }
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    if (!CreatePipeline()) {
        return false;
    }
    // 塗りつぶしは、白い 1×1 のテクスチャに頂点の色を掛けて描く。
    whiteTexture_ = std::make_unique<DynamicTexture>();
    const uint8_t white[4] = {255, 255, 255, 255};
    return whiteTexture_->Initialize(dxCommon_, srvManager_, 1, 1) && whiteTexture_->Update(white);
}

bool UiBatch::CreatePipeline()
{
    ID3D12Device* device = dxCommon_->GetDevice().Get();

    // ルートシグネチャ: [0] 画面の大きさ（定数4つ、頂点シェーダー）、[1] テクスチャ（ピクセルシェーダー）、静的サンプラー。
    D3D12_ROOT_PARAMETER parameters[2]{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    parameters[0].Constants.ShaderRegister = 0;
    parameters[0].Constants.Num32BitValues = 4;
    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 1;
    range.BaseShaderRegister = 0;
    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[1].DescriptorTable.NumDescriptorRanges = 1;
    parameters[1].DescriptorTable.pDescriptorRanges = &range;
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC rootDesc{};
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    rootDesc.NumParameters = _countof(parameters);
    rootDesc.pParameters = parameters;
    rootDesc.NumStaticSamplers = 1;
    rootDesc.pStaticSamplers = &sampler;
    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> error;
    if (FAILED(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)) ||
        FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature_)))) {
        LogWrite().Log("[UiBatch] Failed to create the root signature.\n");
        return false;
    }

    Microsoft::WRL::ComPtr<IDxcBlob> vertexShader;
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShader;
    vertexShader = dxCommon_->CompileShader(ResourcePaths::EngineShader(L"Ui.VS.hlsl"), L"vs_6_0");
    pixelShader = dxCommon_->CompileShader(ResourcePaths::EngineShader(L"Ui.PS.hlsl"), L"ps_6_0");
    if (!vertexShader || !pixelShader) {
        LogWrite().Log("[UiBatch] Failed to compile the UI shaders.\n");
        return false;
    }

    const D3D12_INPUT_ELEMENT_DESC inputElements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(UiVertex, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(UiVertex, texcoord), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(UiVertex, color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = rootSignature_.Get();
    desc.VS = {vertexShader->GetBufferPointer(), vertexShader->GetBufferSize()};
    desc.PS = {pixelShader->GetBufferPointer(), pixelShader->GetBufferSize()};
    desc.InputLayout = {inputElements, _countof(inputElements)};
    // アルファ合成（色は SRC_ALPHA / INV_SRC_ALPHA、アルファは足し合わせる）。
    D3D12_RENDER_TARGET_BLEND_DESC& blend = desc.BlendState.RenderTarget[0];
    blend.BlendEnable = TRUE;
    blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blend.BlendOp = D3D12_BLEND_OP_ADD;
    blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    desc.RasterizerState.DepthClipEnable = TRUE;
    // UI は常に手前に描く（深度は使わないが、スプライトの描画先と同じ深度バッファがつながっている）。
    desc.DepthStencilState.DepthEnable = FALSE;
    desc.DepthStencilState.StencilEnable = FALSE;
    desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DirectXCommon::kBackBufferRenderTargetFormat;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.SampleDesc.Count = 1;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    if (FAILED(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline_)))) {
        LogWrite().Log("[UiBatch] Failed to create the pipeline state.\n");
        return false;
    }
    return true;
}

void UiBatch::Clear()
{
    vertices_.clear();
    commands_.clear();
}

void UiBatch::AddQuad(const UiRect& rect, const Vector2& uvMin, const Vector2& uvMax, const Vector4& color,
                      D3D12_GPU_DESCRIPTOR_HANDLE texture)
{
    if (rect.width <= 0.0f || rect.height <= 0.0f || color.w <= 0.0f) {
        return;
    }
    const float left = rect.x;
    const float top = rect.y;
    const float right = rect.x + rect.width;
    const float bottom = rect.y + rect.height;
    const uint32_t first = static_cast<uint32_t>(vertices_.size());
    vertices_.push_back({{left, top}, {uvMin.x, uvMin.y}, color});
    vertices_.push_back({{right, top}, {uvMax.x, uvMin.y}, color});
    vertices_.push_back({{left, bottom}, {uvMin.x, uvMax.y}, color});
    vertices_.push_back({{right, top}, {uvMax.x, uvMin.y}, color});
    vertices_.push_back({{right, bottom}, {uvMax.x, uvMax.y}, color});
    vertices_.push_back({{left, bottom}, {uvMin.x, uvMax.y}, color});
    // 同じテクスチャが続く間は、1回の描画命令にまとめる。
    if (!commands_.empty() && commands_.back().texture.ptr == texture.ptr) {
        commands_.back().vertexCount += 6;
    } else {
        commands_.push_back({texture, first, 6});
    }
}

void UiBatch::AddRect(const UiRect& rect, const Vector4& color)
{
    if (whiteTexture_) {
        AddQuad(rect, {0.0f, 0.0f}, {1.0f, 1.0f}, color, whiteTexture_->GetSrvHandleGPU());
    }
}

bool UiBatch::EnsureVertexCapacity(size_t vertexCount)
{
    if (vertexCount <= vertexCapacity_) {
        return true;
    }
    if (vertexBuffer_) {
        vertexBuffer_->Unmap(0, nullptr);
        mappedVertices_ = nullptr;
        dxCommon_->ReleaseAfterExecution(std::move(vertexBuffer_));
    }
    const size_t capacity = (std::max)({vertexCount, vertexCapacity_ * 2, size_t(6 * 256)});
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = capacity * sizeof(UiVertex);
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if (FAILED(dxCommon_->GetDevice()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                                               nullptr, IID_PPV_ARGS(&vertexBuffer_)))) {
        vertexCapacity_ = 0;
        return false;
    }
    vertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertices_));
    vertexCapacity_ = capacity;
    return true;
}

void UiBatch::Draw()
{
    lastDrawCalls_ = 0;
    if (!pipeline_ || vertices_.empty() || !EnsureVertexCapacity(vertices_.size())) {
        return;
    }
    // フレームの終わりに GPU を待つ作りなので、1フレームに1回なら同じバッファを書き換えてよい。
    std::memcpy(mappedVertices_, vertices_.data(), vertices_.size() * sizeof(UiVertex));
    ID3D12GraphicsCommandList* list = dxCommon_->GetList().Get();
    list->SetGraphicsRootSignature(rootSignature_.Get());
    list->SetPipelineState(pipeline_.Get());
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    D3D12_VERTEX_BUFFER_VIEW view{};
    view.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    view.SizeInBytes = static_cast<UINT>(vertices_.size() * sizeof(UiVertex));
    view.StrideInBytes = sizeof(UiVertex);
    list->IASetVertexBuffers(0, 1, &view);
    const float screen[4] = {screenSize_.x, screenSize_.y, 0.0f, 0.0f};
    list->SetGraphicsRoot32BitConstants(0, 4, screen, 0);
    for (const Command& command : commands_) {
        list->SetGraphicsRootDescriptorTable(1, command.texture);
        list->DrawInstanced(command.vertexCount, 1, command.firstVertex, 0);
        ++lastDrawCalls_;
    }
    // この後にスプライトを描けるよう、スプライトの描画の設定に戻す。
    SpriteCommon::GetInstance()->PreDraw(kNormal);
}

} // namespace myEngine
