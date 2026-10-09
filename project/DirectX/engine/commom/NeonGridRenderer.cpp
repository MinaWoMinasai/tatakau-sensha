#include "NeonGridRenderer.h"
#include "TextureManager.h"
#include <algorithm>

namespace cg2 {

void NeonGridRenderer::Initialize(DirectXCommon* dxCommon, const std::string& textureFilePath) {
    dxCommon_ = dxCommon;
    textureFilePath_ = textureFilePath;

    TextureManager::GetInstance()->LoadTexture(textureFilePath_);

    vertexResource_ = dxCommon_->CreateBufferResource(sizeof(TrailVertex) * kMaxVertices);
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(TrailVertex) * kMaxVertices;
    vertexBufferView_.StrideInBytes = sizeof(TrailVertex);
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
    geometry_.SetVertexBuffer(std::span<TrailVertex>(vertexData_, kMaxVertices));

    viewProjectionResource_ = dxCommon_->CreateBufferResource(sizeof(Matrix4x4));
    viewProjectionResource_->Map(0, nullptr, reinterpret_cast<void**>(&viewProjectionData_));

    materialResource_ = dxCommon_->CreateBufferResource(sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
    *materialData_ = MakeDefaultMaterial();
    materialData_->shininess = 1.0f;
}

void NeonGridRenderer::DrawAll(const Matrix4x4& viewProjection, bool noDepth) {
    DrawRange(0, geometry_.GetVertexCount(), viewProjection, noDepth);
}

void NeonGridRenderer::DrawRange(uint32_t startVertex, uint32_t vertexCount, const Matrix4x4& viewProjection, bool noDepth) {
    if (!dxCommon_ || !vertexData_ || vertexCount == 0 || startVertex >= geometry_.GetVertexCount()) {
        return;
    }
    const uint32_t drawableCount = (std::min)(vertexCount, geometry_.GetVertexCount() - startVertex);

    *viewProjectionData_ = viewProjection;

    auto commandList = dxCommon_->GetList();
    commandList->SetGraphicsRootSignature(dxCommon_->GetPSOTrailForScene().root_.GetSignature().Get());
    auto* pipeline = noDepth ? dxCommon_->GetTrailSceneNoDepthPipeline() : dxCommon_->GetPSOTrailForScene().graphicsState_.Get();
    if (!pipeline) return;
    commandList->SetPipelineState(pipeline);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1, viewProjectionResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath_));
    commandList->DrawInstanced(drawableCount, 1, startVertex, 0);
}

void NeonGridRenderer::DrawRangeSolid(uint32_t startVertex, uint32_t vertexCount, const Matrix4x4& viewProjection) {
    if (!dxCommon_ || !vertexData_ || vertexCount == 0 || startVertex >= geometry_.GetVertexCount()) {
        return;
    }
    const uint32_t drawableCount = (std::min)(vertexCount, geometry_.GetVertexCount() - startVertex);
    *viewProjectionData_ = viewProjection;

    auto commandList = dxCommon_->GetList();
    commandList->SetGraphicsRootSignature(dxCommon_->GetPSOHudRect().root_.GetSignature().Get());
    commandList->SetPipelineState(dxCommon_->GetPSOHudRect().graphicsState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1, viewProjectionResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath_));
    commandList->DrawInstanced(drawableCount, 1, startVertex, 0);
}

bool NeonGridRenderer::InitializeSceneSolidPipeline() {
    if (sceneSolidPipeline_) return true;
    if (!dxCommon_) return false;
    // HUD用のLDR PSOをHDR描画先へ転用せず、SceneのMRT形式とroot signatureを維持する。
    auto description = dxCommon_->GetPSOTrailForScene().graphicsDesc_;
    description.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    description.BlendState.RenderTarget[0].BlendEnable = FALSE;
    return SUCCEEDED(dxCommon_->GetDevice()->CreateGraphicsPipelineState(&description, IID_PPV_ARGS(&sceneSolidPipeline_)));
}

void NeonGridRenderer::DrawRangeSceneSolid(uint32_t startVertex, uint32_t vertexCount, const Matrix4x4& viewProjection) {
    if (!sceneSolidPipeline_ || !vertexData_ || !vertexCount || startVertex >= geometry_.GetVertexCount()) return;
    *viewProjectionData_ = viewProjection;
    auto list = dxCommon_->GetList();
    list->SetGraphicsRootSignature(dxCommon_->GetPSOTrailForScene().root_.GetSignature().Get());
    list->SetPipelineState(sceneSolidPipeline_.Get());
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->IASetVertexBuffers(0, 1, &vertexBufferView_);
    list->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
    list->SetGraphicsRootConstantBufferView(1, viewProjectionResource_->GetGPUVirtualAddress());
    list->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath_));
    list->DrawInstanced((std::min)(vertexCount, geometry_.GetVertexCount() - startVertex), 1, startVertex, 0);
}

} // namespace cg2
