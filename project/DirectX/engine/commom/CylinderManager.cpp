#include "CylinderManager.h"
#include "TextureManager.h"
#include <algorithm>
#include <cmath>

void CylinderManager::Initialize(DirectXCommon* dxCommon, const std::string& textureFilePath) {
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

void CylinderManager::Emit(const Vector3& position, const CylinderEffectConfig& config) {
    cylinders_.push_back({ position, config, 0.0f });
}

void CylinderManager::Update(float deltaTime) {
    for (ActiveCylinder& cylinder : cylinders_) {
        cylinder.currentTime += deltaTime;
    }

    std::erase_if(cylinders_, [](const ActiveCylinder& cylinder) {
        return cylinder.currentTime >= cylinder.config.lifeTime;
    });
}

void CylinderManager::DrawAll(const Matrix4x4& viewProjection) {
    if (!dxCommon_ || !vertexData_ || cylinders_.empty()) {
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
    for (const ActiveCylinder& cylinder : cylinders_) {
        const CylinderEffectConfig& config = cylinder.config;
        const uint32_t divisions = (std::max)(3u, config.divisions);
        const uint32_t vertexCount = (divisions + 1) * 2;
        if (currentVertexOffset + vertexCount >= kMaxVertices) {
            break;
        }

        const float t = config.lifeTime > 0.0f ? std::clamp(cylinder.currentTime / config.lifeTime, 0.0f, 1.0f) : 1.0f;
        const float easedT = EaseOut(t);
        const float radius = config.startRadius + (config.endRadius - config.startRadius) * easedT;
        const float height = config.startHeight + (config.endHeight - config.startHeight) * easedT;
        const float halfHeight = height * 0.5f;
        const Vector4 color = Lerp(config.startColor, config.endColor, t);
        const Matrix4x4 transform = MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, config.rotate, cylinder.position);

        for (uint32_t i = 0; i <= divisions; ++i) {
            const float ratio = static_cast<float>(i) / static_cast<float>(divisions);
            const float angle = ratio * 2.0f * pi;
            const float c = std::cos(angle);
            const float s = std::sin(angle);
            const uint32_t vertexIndex = currentVertexOffset + i * 2;

            vertexData_[vertexIndex].pos = TransformPoint({ c * radius, halfHeight, s * radius }, transform);
            vertexData_[vertexIndex].color = color;
            vertexData_[vertexIndex].uv = { ratio, 0.0f };

            vertexData_[vertexIndex + 1].pos = TransformPoint({ c * radius, -halfHeight, s * radius }, transform);
            vertexData_[vertexIndex + 1].color = color;
            vertexData_[vertexIndex + 1].uv = { ratio, 1.0f };
        }

        commandList->DrawInstanced(vertexCount, 1, currentVertexOffset, 0);
        currentVertexOffset += vertexCount;
    }
}

float CylinderManager::EaseOut(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return 1.0f - (1.0f - t) * (1.0f - t);
}

Vector4 CylinderManager::Lerp(const Vector4& start, const Vector4& end, float t) {
    return start + (end - start) * std::clamp(t, 0.0f, 1.0f);
}

Vector3 CylinderManager::TransformPoint(const Vector3& point, const Matrix4x4& matrix) {
    Vector3 result = TransformNormal(point, matrix);
    result.x += matrix.m[3][0];
    result.y += matrix.m[3][1];
    result.z += matrix.m[3][2];
    return result;
}
