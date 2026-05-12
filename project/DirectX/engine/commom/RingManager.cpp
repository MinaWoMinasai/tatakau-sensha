#include "RingManager.h"
#include "TextureManager.h"
#include <algorithm>
#include <cmath>

void RingManager::Initialize(DirectXCommon* dxCommon, const std::string& textureFilePath) {
    dxCommon_ = dxCommon;
    textureFilePath_ = textureFilePath;

    TextureManager::GetInstance()->LoadTexture(textureFilePath_);

    vertexResource_ = dxCommon_->CreateBufferResource(sizeof(TrailVertex) * kMaxVertices);
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(TrailVertex) * kMaxVertices;
    vertexBufferView_.StrideInBytes = sizeof(TrailVertex);
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

    viewProjectionResource_ = dxCommon_->CreateBufferResource(sizeof(Matrix4x4));
    viewProjectionResource_->Map(0, nullptr, reinterpret_cast<void**>(&viewProjectionData_));

    materialResource_ = dxCommon_->CreateBufferResource(sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData_->enableLighting = false;
    materialData_->lightingMode = false;
    materialData_->environmentCoefficient = 0.0f;
    materialData_->padding = 0.0f;
    materialData_->uvTransform = MakeIdentity4x4();
    materialData_->shininess = 1.0f;
}

void RingManager::Emit(const Vector3& position, const RingEffectConfig& config) {
    rings_.push_back({ position, config, 0.0f });
}

void RingManager::Update(float deltaTime) {
    for (ActiveRing& ring : rings_) {
        ring.currentTime += deltaTime;
    }

    std::erase_if(rings_, [](const ActiveRing& ring) {
        return ring.currentTime >= ring.config.lifeTime;
    });
}

void RingManager::DrawAll(const Matrix4x4& viewProjection) {
    if (!dxCommon_ || !vertexData_ || rings_.empty()) {
        return;
    }

    *viewProjectionData_ = viewProjection;

    auto commandList = dxCommon_->GetList();
    commandList->SetGraphicsRootSignature(dxCommon_->GetPSOTrail().root_.GetSignature().Get());
    commandList->SetPipelineState(dxCommon_->GetPSOTrail().graphicsState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1, viewProjectionResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath_));

    uint32_t currentVertexOffset = 0;
    for (const ActiveRing& ring : rings_) {
        const RingEffectConfig& config = ring.config;
        const uint32_t divisions = (std::max)(3u, config.divisions);
        const uint32_t vertexCount = (divisions + 1) * 2;
        if (currentVertexOffset + vertexCount >= kMaxVertices) {
            break;
        }

        const float t = config.lifeTime > 0.0f ? std::clamp(ring.currentTime / config.lifeTime, 0.0f, 1.0f) : 1.0f;
        const float easedT = EaseOut(t);
        const float radius = config.startRadius + (config.endRadius - config.startRadius) * easedT;
        const float width = config.startWidth + (config.endWidth - config.startWidth) * easedT;
        const float innerRadius = (std::max)(0.0f, radius - width * 0.5f);
        const float outerRadius = radius + width * 0.5f;
        const Vector4 color = Lerp(config.startColor, config.endColor, t);
        const Matrix4x4 transform = MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, config.rotate, ring.position);

        for (uint32_t i = 0; i <= divisions; ++i) {
            const float ratio = static_cast<float>(i) / static_cast<float>(divisions);
            const float angle = ratio * 2.0f * pi;
            const float c = std::cos(angle);
            const float s = std::sin(angle);
            const uint32_t vertexIndex = currentVertexOffset + i * 2;

            vertexData_[vertexIndex].pos = TransformPoint({ c * outerRadius, s * outerRadius, 0.0f }, transform);
            vertexData_[vertexIndex].color = color;
            vertexData_[vertexIndex].uv = { ratio, 0.0f };

            vertexData_[vertexIndex + 1].pos = TransformPoint({ c * innerRadius, s * innerRadius, 0.0f }, transform);
            vertexData_[vertexIndex + 1].color = color;
            vertexData_[vertexIndex + 1].uv = { ratio, 1.0f };
        }

        commandList->DrawInstanced(vertexCount, 1, currentVertexOffset, 0);
        currentVertexOffset += vertexCount;
    }
}

float RingManager::EaseOut(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return 1.0f - (1.0f - t) * (1.0f - t);
}

Vector4 RingManager::Lerp(const Vector4& start, const Vector4& end, float t) {
    return start + (end - start) * std::clamp(t, 0.0f, 1.0f);
}

Vector3 RingManager::TransformPoint(const Vector3& point, const Matrix4x4& matrix) {
    Vector3 result = TransformNormal(point, matrix);
    result.x += matrix.m[3][0];
    result.y += matrix.m[3][1];
    result.z += matrix.m[3][2];
    return result;
}
