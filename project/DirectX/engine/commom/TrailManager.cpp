#include "TrailManager.h"
#include "Calculation.h"
#include <cmath>
#include <chrono>
#include <cstring>

namespace cg2 {

namespace {
struct CatmullRomCoefficients {
    Vector3 a;
    Vector3 b;
    Vector3 c;
    Vector3 d;
};

CatmullRomCoefficients MakeCatmullRomCoefficients(
    const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3) {
    return {
        p1,
        (-p0 + p2) * 0.5f,
        (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * 0.5f,
        (-p0 + p1 * 3.0f - p2 * 3.0f + p3) * 0.5f
    };
}

Vector3 EvaluateCatmullRom(const CatmullRomCoefficients& coefficients, float t) {
    return ((coefficients.d * t + coefficients.c) * t + coefficients.b) * t + coefficients.a;
}
}

void TrailManager::Initialize(DirectXCommon* dxcommon, Object3dCommon* object3dCommon, const std::string& textureFilePath) {
    dxCommon_ = dxcommon;
    object3dCommon_ = object3dCommon;
    textureFilePath_ = textureFilePath;
    char batchingSetting[8]{};
    const DWORD batchingLength = GetEnvironmentVariableA("CG2_TRAIL_BATCHING", batchingSetting, sizeof(batchingSetting));
    batchingEnabled_ = batchingLength != 1 || batchingSetting[0] != '0';

    PrepareVertexBuffer(kMaxVertices, dxCommon_->GetFence()->GetCompletedValue());
    builtVertices_.reserve(kMaxVertices);

    materialResource_ = dxCommon_->CreateBufferResource(sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
    *materialData_ = MakeDefaultMaterial();
    materialData_->shininess = 1.0f;
}

TrailInstance* TrailManager::CreateInstance() {
    instances_.push_back(std::make_unique<TrailInstance>());
    geometryDirty_ = true;
    return instances_.back().get();
}

void TrailManager::Update(float deltaTime) {
    // 非アクティブなインスタンスについても、頂点の寿命を削るためにUpdateを呼ぶ
    // (アクティブなものは各所有者(EffectSequencer等)が座標付きでUpdateを呼んでいるはず)
    for (auto& instance : instances_) {
        if (!instance->IsActive()) {
            // 非アクティブ時は座標を更新せず、時間だけ進める
            // (既存の頂点座標は維持され、寿命が来たら消える)
            instance->Update(deltaTime, { 0,0,0 }, { 0,0,0 }, instance->GetConfig());
        }
    }

    const auto removed = std::erase_if(instances_, [](const auto& instance) {
        // 「常駐ではない」かつ「非アクティブ」かつ「空」の場合のみ削除
        return !instance->IsPermanent() && !instance->IsActive() && instance->GetPoints().empty();
        });
    if (removed != 0) geometryDirty_ = true;
}

bool TrailManager::NeedsGeometryRebuild() const {
    if (geometryDirty_ || builtRevisions_.size() != instances_.size()) return true;
    for (size_t i = 0; i < instances_.size(); ++i) {
        if (builtRevisions_[i] != instances_[i]->GetGeometryRevision()) return true;
    }
    return false;
}

void TrailManager::BuildVertices() {
    builtVertices_.clear();
    builtDrawRanges_.clear();
    builtRevisions_.clear();
    builtRevisions_.reserve(instances_.size());
    builtGeometryVertices_ = 0;
    const uint64_t connectorCount = drawStats_.drawableInstances > 0
        ? (drawStats_.drawableInstances - 1) * 2u : 0;
    builtVertices_.reserve(static_cast<size_t>((std::min)(
        drawStats_.requestedVertices + connectorCount, static_cast<uint64_t>(kMaxBatchVertices))));
    for (const auto& instance : instances_) {
        builtRevisions_.push_back(instance->GetGeometryRevision());
        const auto& points = instance->GetPoints();
        if (points.size() < 4) continue;
        const uint32_t connectors = builtVertices_.empty() ? 0u : 2u;
        const auto remaining = kMaxBatchVertices - static_cast<uint32_t>(builtVertices_.size());
        // A two-vertex strip has no visible triangles. Never add a connector
        // unless there is space for at least the first complete quad.
        if (remaining < connectors + 4u) continue;
        const auto& config = instance->GetConfig();
        const size_t segmentCount = points.size() - 1;
        const uint32_t steps = (std::max)(1u, config.interpolationSteps);
        const float inverseSteps = 1.0f / static_cast<float>(steps);
        const float inverseSampleCount =
            1.0f / static_cast<float>(segmentCount * static_cast<size_t>(steps));
        const float colorCurvePower = (std::max)(0.05f, config.colorCurvePower);
        const float widthCurvePower = (std::max)(0.05f, config.widthCurvePower);
        const bool useLinearColorCurve = colorCurvePower == 1.0f;
        const bool useLinearWidthCurve = widthCurvePower == 1.0f;
        const Vector4 startColor = config.startColor;
        const Vector4 colorDelta = config.endColor - startColor;
        const float startWidthScale = config.startWidthScale;
        const float widthScaleDelta = config.endWidthScale - startWidthScale;
        const uint64_t requested = static_cast<uint64_t>(segmentCount) * steps * 2u;
        const uint32_t instanceVertexLimit = static_cast<uint32_t>((std::min)(
            requested, static_cast<uint64_t>((remaining - connectors) & ~1u)));
        const uint32_t firstVertex = static_cast<uint32_t>(builtVertices_.size()) + connectors;
        uint32_t instanceVertexCount = 0;
        // --- 頂点データの構築 ---
        for (size_t i = 0; i < segmentCount && instanceVertexCount < instanceVertexLimit; ++i) {
            size_t i0 = (i == 0) ? 0 : i - 1;
            size_t i1 = i;
            size_t i2 = i + 1;
            size_t i3 = (i + 2 >= points.size()) ? segmentCount : i + 2;
            const CatmullRomCoefficients tipCoefficients = MakeCatmullRomCoefficients(
                points[i0].tip, points[i1].tip, points[i2].tip, points[i3].tip);
            const CatmullRomCoefficients baseCoefficients = MakeCatmullRomCoefficients(
                points[i0].base, points[i1].base, points[i2].base, points[i3].base);
            const size_t segmentSampleOffset = i * static_cast<size_t>(steps);

            for (uint32_t j = 0; j < steps; ++j) {
                if (instanceVertexCount >= instanceVertexLimit) break;

                const float t = static_cast<float>(j) * inverseSteps;
                const float globalRatio =
                    static_cast<float>(segmentSampleOffset + j) * inverseSampleCount;

                const float colorRatio = useLinearColorCurve
                    ? globalRatio
                    : std::pow(globalRatio, colorCurvePower);
                const float widthRatio = useLinearWidthCurve
                    ? globalRatio
                    : std::pow(globalRatio, widthCurvePower);
                const Vector4 color = startColor + colorDelta * colorRatio;
                Vector3 tip = EvaluateCatmullRom(tipCoefficients, t);
                Vector3 base = EvaluateCatmullRom(baseCoefficients, t);
                const float widthScale = startWidthScale + widthScaleDelta * widthRatio;
                Vector3 center = (tip + base) * 0.5f;
                Vector3 halfWidth = (tip - base) * (0.5f * widthScale);
                tip = center + halfWidth;
                base = center - halfWidth;

                const TrailVertex tipVertex{ tip, color, { globalRatio, 0.0f } };
                const TrailVertex baseVertex{ base, color, { globalRatio, 1.0f } };
                if (instanceVertexCount == 0 && !builtVertices_.empty()) {
                    // Every strip has an even vertex count. Two duplicated
                    // endpoints preserve winding and create only zero-area
                    // triangles between strips, with no extra pixels/blending.
                    const TrailVertex previousLast = builtVertices_.back();
                    builtVertices_.push_back(previousLast);
                    builtVertices_.push_back(tipVertex);
                }
                builtVertices_.push_back(tipVertex);
                builtVertices_.push_back(baseVertex);

                instanceVertexCount += 2;
            }
        }
        builtGeometryVertices_ += instanceVertexCount;
        builtDrawRanges_.push_back({ firstVertex, instanceVertexCount });
    }
    geometryDirty_ = false;
}

void TrailManager::PrepareVertexBuffer(uint32_t requiredVertices, uint64_t completedFence) {
    std::erase_if(retiredVertexBuffers_, [completedFence](const auto& buffer) {
        return buffer.lastUsedFence <= completedFence;
    });
    if (requiredVertices <= vertexCapacity_ && vertexLastUsedFence_ <= completedFence) return;
    uint32_t capacity = (std::max)(vertexCapacity_, kMaxVertices);
    while (capacity < requiredVertices) capacity = (std::min)(capacity * 2u, kMaxBatchVertices);
    if (vertexResource_ && vertexLastUsedFence_ > completedFence) {
        retiredVertexBuffers_.push_back({ vertexResource_, vertexLastUsedFence_ });
    }
    vertexResource_ = dxCommon_->CreateBufferResource(sizeof(TrailVertex) * capacity);
    vertexCapacity_ = capacity;
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(TrailVertex) * capacity;
    vertexBufferView_.StrideInBytes = sizeof(TrailVertex);
    const D3D12_RANGE noRead{ 0, 0 };
    vertexResource_->Map(0, &noRead, reinterpret_cast<void**>(&vertexData_));
    vertexLastUsedFence_ = 0;
}

D3D12_GPU_VIRTUAL_ADDRESS TrailManager::PrepareViewProjection(
    const Matrix4x4& viewProjection, uint64_t completedFence) {
    ViewProjectionBuffer* available = nullptr;
    for (auto& buffer : viewProjectionBuffers_) {
        if (buffer.lastUsedFence <= completedFence) { available = &buffer; break; }
    }
    if (!available) {
        ViewProjectionBuffer buffer;
        buffer.resource = dxCommon_->CreateBufferResource(sizeof(Matrix4x4));
        const D3D12_RANGE noRead{ 0, 0 };
        buffer.resource->Map(0, &noRead, reinterpret_cast<void**>(&buffer.data));
        viewProjectionBuffers_.push_back(std::move(buffer));
        available = &viewProjectionBuffers_.back();
    }
    *available->data = viewProjection;
    available->lastUsedFence = dxCommon_->GetFenceValue() + 1;
    return available->resource->GetGPUVirtualAddress();
}

void TrailManager::DrawAll(const Matrix4x4& viewProjection) {
    using Clock = std::chrono::steady_clock;
    const auto drawStart = Clock::now();
    drawStats_ = {};
    drawStats_.totalInstances = instances_.size();
    drawStats_.batchingEnabled = batchingEnabled_;
    for (const auto& instance : instances_) {
        const auto pointCount = instance->GetPoints().size();
        drawStats_.activeInstances += instance->IsActive() ? 1 : 0;
        drawStats_.totalPoints += pointCount;
        if (pointCount < 4) continue;
        ++drawStats_.drawableInstances;
        drawStats_.requestedVertices += static_cast<uint64_t>(pointCount - 1) *
            (std::max)(1u, instance->GetConfig().interpolationSteps) * 2u;
    }
    const uint64_t completedFence = dxCommon_->GetFence()->GetCompletedValue();
    if (NeedsGeometryRebuild()) {
        const auto buildStart = Clock::now();
        BuildVertices();
        if (!builtVertices_.empty()) {
            PrepareVertexBuffer(static_cast<uint32_t>(builtVertices_.size()), completedFence);
            const size_t bytes = builtVertices_.size() * sizeof(TrailVertex);
            std::memcpy(vertexData_, builtVertices_.data(), bytes);
            drawStats_.uploadedBytes = bytes;
        }
        drawStats_.geometryRebuilt = true;
        drawStats_.vertexBuildCpuMs = std::chrono::duration<float, std::milli>(Clock::now() - buildStart).count();
    }
    drawStats_.generatedVertices = builtGeometryVertices_;
    drawStats_.submittedVertices = batchingEnabled_ ? builtVertices_.size() : builtGeometryVertices_;
    drawStats_.vertexCapacity = vertexCapacity_;
    drawStats_.truncatedVertices = drawStats_.requestedVertices > drawStats_.generatedVertices
        ? drawStats_.requestedVertices - drawStats_.generatedVertices
        : 0;
    drawStats_.capacityHit = drawStats_.truncatedVertices > 0;
    if (!builtVertices_.empty()) {
        const auto commandStart = Clock::now();
        const auto matrixAddress = PrepareViewProjection(viewProjection, completedFence);
        drawStats_.uploadedBytes += sizeof(Matrix4x4);
        auto commandList = dxCommon_->GetList();
        commandList->SetGraphicsRootSignature(dxCommon_->GetPSOTrailForScene().root_.GetSignature().Get());
        commandList->SetPipelineState(dxCommon_->GetPSOTrailForScene().graphicsState_.Get());
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
        commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
        commandList->SetGraphicsRootConstantBufferView(1, matrixAddress);
        commandList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath_));
        if (batchingEnabled_) {
            commandList->DrawInstanced(static_cast<uint32_t>(builtVertices_.size()), 1, 0, 0);
            drawStats_.drawCalls = 1;
        } else {
            // Diagnostic A/B path: identical vertices, one call per trail.
            for (const auto& range : builtDrawRanges_) {
                commandList->DrawInstanced(range.vertexCount, 1, range.firstVertex, 0);
            }
            drawStats_.drawCalls = static_cast<uint32_t>(builtDrawRanges_.size());
        }
        vertexLastUsedFence_ = dxCommon_->GetFenceValue() + 1;
        drawStats_.drawCommandCpuMs = std::chrono::duration<float, std::milli>(Clock::now() - commandStart).count();
    }
    drawStats_.drawCpuMs = std::chrono::duration<float, std::milli>(
        Clock::now() - drawStart).count();
}

Vector3 TrailManager::CatmullRom(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t)
{
    float t2 = t * t;
    float t3 = t2 * t;

    return ((p1 * 2.0f) +
        (-p0 + p2) * t +
        (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2 +
        (-p0 + p1 * 3.0f - p2 * 3.0f + p3) * t3) * 0.5f;
}

Vector4 TrailManager::Lerp(const Vector4& start, const Vector4& end, float t)
{
    // 線形補間
    Vector4 result = start + (end - start) * t;
    // 補間結果を返す
    return result;
}

} // namespace cg2
