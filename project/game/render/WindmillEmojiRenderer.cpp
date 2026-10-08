#include "WindmillEmojiRenderer.h"
#include "TextureManager.h"
#include <cmath>
#include <stdexcept>

void WindmillEmojiRenderer::Initialize(cg2::DirectXCommon* dx, const std::string& texture)
{
    dx_ = dx;
    texture_ = texture;
    cg2::TextureManager::GetInstance()->LoadTexture(texture_);
    auto description = dx_->GetPSOTrailForScene().graphicsDesc_;
    Microsoft::WRL::ComPtr<IDxcBlob> pixel;
    pixel.Attach(dx_->CompileShader(L"resources/shaders/WindmillEmoji.PS.hlsl", L"ps_6_0"));
    if (!pixel)
        throw std::runtime_error("Windmill emoji shader compilation failed.");
    description.PS = {pixel->GetBufferPointer(), pixel->GetBufferSize()};
    description.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    auto& blend = description.BlendState.RenderTarget[0];
    blend.BlendEnable = TRUE;
    blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    if (FAILED(dx_->GetDevice()->CreateGraphicsPipelineState(&description, IID_PPV_ARGS(&pipeline_))))
        throw std::runtime_error("Windmill emoji HDR pipeline creation failed.");
    vertexBuffer_ = dx_->CreateBufferResource(sizeof(cg2::TrailVertex) * 48);
    vertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&data_));
    view_ = {vertexBuffer_->GetGPUVirtualAddress(), UINT(sizeof(cg2::TrailVertex) * 48), sizeof(cg2::TrailVertex)};
    matrixBuffer_ = dx_->CreateBufferResource(sizeof(cg2::Matrix4x4));
    matrixBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&matrix_));
    materialBuffer_ = dx_->CreateBufferResource(256);
    cg2::Vector4* material = nullptr;
    materialBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&material));
    *material = {1, 1, 1, 1};
}

void WindmillEmojiRenderer::BeginFrame()
{
    vertices_ = 0;
}

void WindmillEmojiRenderer::Queue(const cg2::Vector3& center, float size, float angle, const std::array<float, 4>& uv, float visibility)
{
    if (vertices_ + 6 > 48 || visibility < .001f)
        return;
    const float c = std::cos(angle), s = std::sin(angle), half = size * .5f;
    const auto point = [&](float x, float y) {
        return center + cg2::Vector3{x * c - y * s, x * s + y * c, 0};
    };
    const std::array<cg2::Vector3, 4> positions{{point(-half, half), point(half, half), point(half, -half), point(-half, -half)}};
    const std::array<cg2::Vector2, 4> texcoords{{{uv[0], uv[1]}, {uv[2], uv[1]}, {uv[2], uv[3]}, {uv[0], uv[3]}}};
    for (int index : {0, 1, 2, 0, 2, 3})
        data_[vertices_++] = {positions[index], {1, 1, 1, visibility}, texcoords[index]};
}

void WindmillEmojiRenderer::Draw(const cg2::Matrix4x4& viewProjection)
{
    if (!vertices_)
        return;
    *matrix_ = viewProjection;
    auto list = dx_->GetList();
    list->SetGraphicsRootSignature(dx_->GetPSOTrailForScene().root_.GetSignature().Get());
    list->SetPipelineState(pipeline_.Get());
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->IASetVertexBuffers(0, 1, &view_);
    list->SetGraphicsRootConstantBufferView(0, materialBuffer_->GetGPUVirtualAddress());
    list->SetGraphicsRootConstantBufferView(1, matrixBuffer_->GetGPUVirtualAddress());
    list->SetGraphicsRootDescriptorTable(2, cg2::TextureManager::GetInstance()->GetSrvHandleGPU(texture_));
    list->DrawInstanced(vertices_, 1, 0, 0);
}
