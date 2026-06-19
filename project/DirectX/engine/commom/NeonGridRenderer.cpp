#include "NeonGridRenderer.h"
#include "TextureManager.h"
#include <algorithm>
#include <cmath>

void NeonGridRenderer::Initialize(DirectXCommon* dxCommon, const std::string& textureFilePath) {
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

void NeonGridRenderer::BeginFrame() {
    vertexCount_ = 0;
}

void NeonGridRenderer::SetLineStyle(float softEdgeRatio, float coreIntensity) {
    lineSoftEdgeRatio_ = std::clamp(softEdgeRatio, 0.0f, 0.95f);
    lineCoreIntensity_ = (std::max)(0.0f, coreIntensity);
}

void NeonGridRenderer::QueueLine(const Vector3& a, const Vector3& b, float lineWidth, const Vector4& color) {
    AddLineQuad(a, b, lineWidth, color);
}

void NeonGridRenderer::QueueCameraFacingLine(const Vector3& a, const Vector3& b, float lineWidth, const Vector4& color, const Vector3& cameraForward) {
    AddCameraFacingLineQuad(a, b, lineWidth, color, cameraForward);
}

void NeonGridRenderer::QueueTriangle(const Vector3& a, const Vector3& b, const Vector3& c, float lineWidth, const Vector4& color) {
    AddLineQuad(a, b, lineWidth, color);
    AddLineQuad(b, c, lineWidth, color);
    AddLineQuad(c, a, lineWidth, color);
}

void NeonGridRenderer::QueueBillboardTriangle(
    const Vector3& center,
    float radius,
    float rotationRad,
    float lineWidth,
    const Vector4& color,
    const Vector3& cameraRight,
    const Vector3& cameraUp,
    const Vector3& cameraForward) {
    if (radius <= 0.0f || lineWidth <= 0.0f || color.w <= 0.001f) {
        return;
    }

    constexpr float kTwoPi = 6.28318530718f;
    constexpr float kStartAngle = -kTwoPi * 0.25f;
    Vector3 points[3]{};
    for (int i = 0; i < 3; ++i) {
        const float angle = kStartAngle + rotationRad + static_cast<float>(i) * (kTwoPi / 3.0f);
        const float x = std::cos(angle) * radius;
        const float y = std::sin(angle) * radius;
        points[i] = center + cameraRight * x + cameraUp * y;
    }

    AddCameraFacingLineQuad(points[0], points[1], lineWidth, color, cameraForward);
    AddCameraFacingLineQuad(points[1], points[2], lineWidth, color, cameraForward);
    AddCameraFacingLineQuad(points[2], points[0], lineWidth, color, cameraForward);
}

void NeonGridRenderer::QueueBillboardRectangle(
    const Vector3& center,
    const Vector2& size,
    float rotationRad,
    float lineWidth,
    const Vector4& color,
    const Vector3& cameraRight,
    const Vector3& cameraUp,
    const Vector3& cameraForward) {
    if (size.x <= 0.0f || size.y <= 0.0f || lineWidth <= 0.0f || color.w <= 0.001f) {
        return;
    }

    const float c = std::cos(rotationRad);
    const float s = std::sin(rotationRad);
    auto makePoint = [&](float x, float y) {
        const float rx = x * c - y * s;
        const float ry = x * s + y * c;
        return center + cameraRight * rx + cameraUp * ry;
    };

    const float halfX = size.x * 0.5f;
    const float halfY = size.y * 0.5f;
    const Vector3 points[4] = {
        makePoint(-halfX, halfY),
        makePoint(halfX, halfY),
        makePoint(halfX, -halfY),
        makePoint(-halfX, -halfY)
    };

    AddCameraFacingLineQuad(points[0], points[1], lineWidth, color, cameraForward);
    AddCameraFacingLineQuad(points[1], points[2], lineWidth, color, cameraForward);
    AddCameraFacingLineQuad(points[2], points[3], lineWidth, color, cameraForward);
    AddCameraFacingLineQuad(points[3], points[0], lineWidth, color, cameraForward);
}

void NeonGridRenderer::QueueWorldGrid(float minX, float maxX, float minY, float maxY, float spacing, float lineWidth, const Vector4& color) {
    if (spacing <= 0.0f || lineWidth <= 0.0f) {
        return;
    }

    const float startX = std::floor(minX / spacing) * spacing;
    const float startY = std::floor(minY / spacing) * spacing;
    const float z = -0.04f;
    for (float x = startX; x <= maxX + 0.001f; x += spacing) {
        AddLineQuad({ x, minY, z }, { x, maxY, z }, lineWidth, color);
    }
    for (float y = startY; y <= maxY + 0.001f; y += spacing) {
        AddLineQuad({ minX, y, z }, { maxX, y, z }, lineWidth, color);
    }
}

void NeonGridRenderer::QueueRectangle(const Vector3& center, const Vector3& size, float lineWidth, const Vector4& color) {
    if (size.x <= 0.0f || size.y <= 0.0f || lineWidth <= 0.0f) {
        return;
    }

    const float halfX = size.x * 0.5f;
    const float halfY = size.y * 0.5f;
    const float z = center.z - 0.02f;
    const Vector3 leftTop = { center.x - halfX, center.y + halfY, z };
    const Vector3 rightTop = { center.x + halfX, center.y + halfY, z };
    const Vector3 rightBottom = { center.x + halfX, center.y - halfY, z };
    const Vector3 leftBottom = { center.x - halfX, center.y - halfY, z };

    AddLineQuad(leftTop, rightTop, lineWidth, color);
    AddLineQuad(rightTop, rightBottom, lineWidth, color);
    AddLineQuad(rightBottom, leftBottom, lineWidth, color);
    AddLineQuad(leftBottom, leftTop, lineWidth, color);
}

void NeonGridRenderer::QueueLocalGrid(const Vector3& center, float radius, float spacing, float lineWidth, const Vector4& color) {
    QueueLocalGridClipped(center, radius, spacing, lineWidth, color, center.x - radius, center.x + radius, center.y - radius, center.y + radius);
}

void NeonGridRenderer::QueueLocalGridClipped(const Vector3& center, float radius, float spacing, float lineWidth, const Vector4& color, float minX, float maxX, float minY, float maxY) {
    if (radius <= 0.0f || spacing <= 0.0f || lineWidth <= 0.0f) {
        return;
    }

    const float circleMinX = center.x - radius;
    const float circleMaxX = center.x + radius;
    const float circleMinY = center.y - radius;
    const float circleMaxY = center.y + radius;
    const float clippedMinX = (std::max)(minX, circleMinX);
    const float clippedMaxX = (std::min)(maxX, circleMaxX);
    const float clippedMinY = (std::max)(minY, circleMinY);
    const float clippedMaxY = (std::min)(maxY, circleMaxY);
    if (clippedMinX >= clippedMaxX || clippedMinY >= clippedMaxY) {
        return;
    }

    const float startX = std::floor(clippedMinX / spacing) * spacing;
    const float startY = std::floor(clippedMinY / spacing) * spacing;
    const float z = -0.03f;

    auto fadeAt = [&](float x, float y) {
        const float dx = x - center.x;
        const float dy = y - center.y;
        const float normalizedDistance = std::sqrt(dx * dx + dy * dy) / radius;
        const float fade = std::clamp(1.0f - normalizedDistance * normalizedDistance, 0.0f, 1.0f);
        return fade * fade;
    };

    auto addFadedSegment = [&](const Vector3& a, const Vector3& b) {
        const float length = Length(b - a);
        const int segmentCount = (std::max)(1, static_cast<int>(std::ceil(length / (spacing * 0.5f))));
        for (int i = 0; i < segmentCount; ++i) {
            const float t0 = static_cast<float>(i) / static_cast<float>(segmentCount);
            const float t1 = static_cast<float>(i + 1) / static_cast<float>(segmentCount);
            Vector3 p0 = a + (b - a) * t0;
            Vector3 p1 = a + (b - a) * t1;
            const float midX = (p0.x + p1.x) * 0.5f;
            const float midY = (p0.y + p1.y) * 0.5f;
            Vector4 segmentColor = color;
            segmentColor.w *= fadeAt(midX, midY);
            AddLineQuad(p0, p1, lineWidth, segmentColor);
        }
    };

    for (float x = startX; x <= clippedMaxX + 0.001f; x += spacing) {
        if (x < clippedMinX - 0.001f) {
            continue;
        }
        const float dx = x - center.x;
        const float inside = radius * radius - dx * dx;
        if (inside <= 0.0f) {
            continue;
        }
        const float yExtent = std::sqrt(inside);
        const float y0 = (std::max)(clippedMinY, center.y - yExtent);
        const float y1 = (std::min)(clippedMaxY, center.y + yExtent);
        if (y0 < y1) {
            addFadedSegment({ x, y0, z }, { x, y1, z });
        }
    }
    for (float y = startY; y <= clippedMaxY + 0.001f; y += spacing) {
        if (y < clippedMinY - 0.001f) {
            continue;
        }
        const float dy = y - center.y;
        const float inside = radius * radius - dy * dy;
        if (inside <= 0.0f) {
            continue;
        }
        const float xExtent = std::sqrt(inside);
        const float x0 = (std::max)(clippedMinX, center.x - xExtent);
        const float x1 = (std::min)(clippedMaxX, center.x + xExtent);
        if (x0 < x1) {
            addFadedSegment({ x0, y, z }, { x1, y, z });
        }
    }
}

void NeonGridRenderer::DrawAll(const Matrix4x4& viewProjection) {
    DrawRange(0, vertexCount_, viewProjection);
}

void NeonGridRenderer::DrawRange(uint32_t startVertex, uint32_t vertexCount, const Matrix4x4& viewProjection) {
    if (!dxCommon_ || !vertexData_ || vertexCount == 0 || startVertex >= vertexCount_) {
        return;
    }
    const uint32_t drawableCount = (std::min)(vertexCount, vertexCount_ - startVertex);

    *viewProjectionData_ = viewProjection;

    auto commandList = dxCommon_->GetList();
    commandList->SetGraphicsRootSignature(dxCommon_->GetPSOTrail().root_.GetSignature().Get());
    commandList->SetPipelineState(dxCommon_->GetPSOTrail().graphicsState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1, viewProjectionResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath_));
    commandList->DrawInstanced(drawableCount, 1, startVertex, 0);
}

void NeonGridRenderer::AddLineQuad(const Vector3& a, const Vector3& b, float width, const Vector4& color) {
    if (color.w <= 0.001f || vertexCount_ + 6 > kMaxVertices) {
        return;
    }

    Vector3 dir = b - a;
    const float len = Length(dir);
    if (len <= 0.0001f) {
        return;
    }
    dir = dir / len;
    Vector3 normal = { -dir.y, dir.x, 0.0f };
    AddSoftLineQuad(a, b, normal, width, color);
}

void NeonGridRenderer::AddCameraFacingLineQuad(const Vector3& a, const Vector3& b, float width, const Vector4& color, const Vector3& cameraForward) {
    if (color.w <= 0.001f || vertexCount_ + 6 > kMaxVertices) {
        return;
    }

    Vector3 dir = b - a;
    const float len = Length(dir);
    if (len <= 0.0001f) {
        return;
    }
    dir = dir / len;

    Vector3 forward = cameraForward;
    if (Length(forward) <= 0.0001f) {
        forward = { 0.0f, 0.0f, 1.0f };
    } else {
        forward = Normalize(forward);
    }

    Vector3 normal = Cross(forward, dir);
    if (Length(normal) <= 0.0001f) {
        normal = { -dir.y, dir.x, 0.0f };
    } else {
        normal = Normalize(normal);
    }
    AddSoftLineQuad(a, b, normal, width, color);
}

void NeonGridRenderer::AddSoftLineQuad(const Vector3& a, const Vector3& b, const Vector3& normalDir, float width, const Vector4& color) {
    if (color.w <= 0.001f || width <= 0.0f || vertexCount_ + 18 > kMaxVertices) {
        return;
    }

    Vector3 normal = normalDir;
    const float normalLength = Length(normal);
    if (normalLength <= 0.0001f) {
        return;
    }
    normal = normal / normalLength;

    const float halfWidth = width * 0.5f;
    const float softRatio = std::clamp(lineSoftEdgeRatio_, 0.0f, 0.95f);
    if (softRatio <= 0.001f) {
        Vector4 hardColor = color;
        hardColor.x *= lineCoreIntensity_;
        hardColor.y *= lineCoreIntensity_;
        hardColor.z *= lineCoreIntensity_;
        PushLineStrip(a, b, normal, -halfWidth, halfWidth, hardColor, hardColor);
        return;
    }

    const float coreHalfWidth = (std::max)(halfWidth * (1.0f - softRatio), halfWidth * 0.05f);
    Vector4 outerColor = color;
    outerColor.x *= 0.35f;
    outerColor.y *= 0.35f;
    outerColor.z *= 0.35f;
    outerColor.w = 0.0f;

    Vector4 coreColor = color;
    coreColor.x *= lineCoreIntensity_;
    coreColor.y *= lineCoreIntensity_;
    coreColor.z *= lineCoreIntensity_;

    PushLineStrip(a, b, normal, -halfWidth, -coreHalfWidth, outerColor, coreColor);
    PushLineStrip(a, b, normal, -coreHalfWidth, coreHalfWidth, coreColor, coreColor);
    PushLineStrip(a, b, normal, coreHalfWidth, halfWidth, coreColor, outerColor);
}

void NeonGridRenderer::PushLineStrip(
    const Vector3& a,
    const Vector3& b,
    const Vector3& normalDir,
    float offset0,
    float offset1,
    const Vector4& color0,
    const Vector4& color1) {
    if (vertexCount_ + 6 > kMaxVertices) {
        return;
    }

    const Vector3 p0 = a + normalDir * offset0;
    const Vector3 p1 = a + normalDir * offset1;
    const Vector3 p2 = b + normalDir * offset0;
    const Vector3 p3 = b + normalDir * offset1;

    PushVertex(p0, color0, { 0.0f, 0.0f });
    PushVertex(p1, color1, { 0.0f, 1.0f });
    PushVertex(p2, color0, { 1.0f, 0.0f });
    PushVertex(p1, color1, { 0.0f, 1.0f });
    PushVertex(p3, color1, { 1.0f, 1.0f });
    PushVertex(p2, color0, { 1.0f, 0.0f });
}

void NeonGridRenderer::PushVertex(const Vector3& pos, const Vector4& color, const Vector2& uv) {
    if (vertexCount_ >= kMaxVertices) {
        return;
    }
    vertexData_[vertexCount_].pos = pos;
    vertexData_[vertexCount_].color = color;
    vertexData_[vertexCount_].uv = uv;
    ++vertexCount_;
}
