#include "TrailManager.h"
#include "Calculation.h"
#include <cmath>
#if defined(USE_IMGUI) && !defined(NDEBUG)
#include <chrono>
#endif

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

    // 頂点リソース作成
    vertexResource_ = dxCommon_->CreateBufferResource(sizeof(TrailVertex) * kMaxVertices);
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(TrailVertex) * kMaxVertices;
    vertexBufferView_.StrideInBytes = sizeof(TrailVertex);
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

    // 定数リソース作成
    constResource_ = dxCommon_->CreateBufferResource(sizeof(Matrix4x4));
    constResource_->Map(0, nullptr, reinterpret_cast<void**>(&constData_));

    materialResource_ = dxCommon_->CreateBufferResource(sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
    *materialData_ = MakeDefaultMaterial();
    materialData_->shininess = 1.0f;
}

TrailInstance* TrailManager::CreateInstance() {
    instances_.push_back(std::make_unique<TrailInstance>());
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

    std::erase_if(instances_, [](const auto& instance) {
        // 「常駐ではない」かつ「非アクティブ」かつ「空」の場合のみ削除
        return !instance->IsPermanent() && !instance->IsActive() && instance->GetPoints().empty();
        });
}

void TrailManager::DrawAll(const Matrix4x4& viewProjection) {
#if defined(USE_IMGUI) && !defined(NDEBUG)
    const auto drawStart = std::chrono::steady_clock::now();
    drawStats_ = {};
    drawStats_.totalInstances = instances_.size();
    drawStats_.vertexCapacity = kMaxVertices;
#endif
    auto commandList = dxCommon_->GetList();
    *constData_ = viewProjection;

    // パイプライン設定（1回だけ）
    commandList->SetGraphicsRootSignature(dxCommon_->GetPSOTrailForScene().root_.GetSignature().Get());
    commandList->SetPipelineState(dxCommon_->GetPSOTrailForScene().graphicsState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1, constResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath_));

    uint32_t currentVertexOffset = 0;

    for (auto& instance : instances_) {
        const auto& points = instance->GetPoints();
#if defined(USE_IMGUI) && !defined(NDEBUG)
        drawStats_.activeInstances += instance->IsActive() ? 1 : 0;
        drawStats_.totalPoints += points.size();
#endif
        if (points.size() < 4) continue;

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
        uint32_t instanceVertexCount = 0;
#if defined(USE_IMGUI) && !defined(NDEBUG)
        ++drawStats_.drawableInstances;
        drawStats_.requestedVertices +=
            static_cast<uint64_t>(segmentCount) * static_cast<uint64_t>(steps) * 2u;
#endif

#if defined(USE_IMGUI) && !defined(NDEBUG)
        const auto vertexBuildStart = std::chrono::steady_clock::now();
#endif
        // --- 頂点データの構築 ---
        for (size_t i = 0; i < segmentCount; ++i) {
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
                if (currentVertexOffset + instanceVertexCount + 2 >= kMaxVertices) break;

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

                // 書き込み
                uint32_t vIdx = currentVertexOffset + instanceVertexCount;
                vertexData_[vIdx].pos = tip;
                vertexData_[vIdx].color = color;
                vertexData_[vIdx].uv = { globalRatio, 0.0f };

                vertexData_[vIdx + 1].pos = base;
                vertexData_[vIdx + 1].color = color;
                vertexData_[vIdx + 1].uv = { globalRatio, 1.0f };

                instanceVertexCount += 2;
            }
        }
#if defined(USE_IMGUI) && !defined(NDEBUG)
        drawStats_.vertexBuildCpuMs += std::chrono::duration<float, std::milli>(
            std::chrono::steady_clock::now() - vertexBuildStart).count();
#endif

        // インスタンスごとの描画命令
        if (instanceVertexCount > 0) {
#if defined(USE_IMGUI) && !defined(NDEBUG)
            const auto drawCommandStart = std::chrono::steady_clock::now();
#endif
            commandList->DrawInstanced(instanceVertexCount, 1, currentVertexOffset, 0);
#if defined(USE_IMGUI) && !defined(NDEBUG)
            drawStats_.drawCommandCpuMs += std::chrono::duration<float, std::milli>(
                std::chrono::steady_clock::now() - drawCommandStart).count();
#endif
            currentVertexOffset += instanceVertexCount;
#if defined(USE_IMGUI) && !defined(NDEBUG)
            ++drawStats_.drawCalls;
#endif
        }
    }
#if defined(USE_IMGUI) && !defined(NDEBUG)
    drawStats_.generatedVertices = currentVertexOffset;
    drawStats_.truncatedVertices = drawStats_.requestedVertices > drawStats_.generatedVertices
        ? drawStats_.requestedVertices - drawStats_.generatedVertices
        : 0;
    drawStats_.capacityHit = drawStats_.truncatedVertices > 0;
    drawStats_.drawCpuMs = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - drawStart).count();
#endif
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
