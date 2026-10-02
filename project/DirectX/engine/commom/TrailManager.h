#pragma once
#include <vector>
#include <memory>
#include "TrailInstance.h"

namespace cg2 {

class TrailManager {
public:
    // Initial allocation. Grow on demand without shortening existing trails.
    static constexpr uint32_t kMaxVertices = 8192;
    // Keep pathological editor settings bounded (9 MiB of vertex data).
    static constexpr uint32_t kMaxBatchVertices = 262144;

    struct DrawStats {
        size_t totalInstances = 0;
        size_t activeInstances = 0;
        size_t drawableInstances = 0;
        size_t totalPoints = 0;
        uint64_t requestedVertices = 0;
        uint64_t generatedVertices = 0;
        uint64_t submittedVertices = 0;
        uint64_t uploadedBytes = 0;
        uint32_t drawCalls = 0;
        uint32_t vertexCapacity = kMaxVertices;
        bool capacityHit = false;
        bool geometryRebuilt = false;
        bool batchingEnabled = true;
        uint64_t truncatedVertices = 0;
        float drawCpuMs = 0.0f;
        float vertexBuildCpuMs = 0.0f;
        float drawCommandCpuMs = 0.0f;
    };

    void Initialize(DirectXCommon* dxcommon, Object3dCommon* object3dCommon, const std::string& textureFilePath);

    // インスタンスの生成
    TrailInstance* CreateInstance();

    void Update(float deltaTime);

    // 全インスタンスの描画
    void DrawAll(const Matrix4x4& viewProjection);

    // インスタンスのクリア（シーン切り替え時など）
    void ClearInstances() { instances_.clear(); geometryDirty_ = true; }
    size_t GetInstanceCount() const { return instances_.size(); }
    bool HasDrawableInstances() const {
        for (const auto& instance : instances_) {
            if (instance->GetPoints().size() >= 4) return true;
        }
        return false;
    }
    const DrawStats& GetDrawStats() const { return drawStats_; }

    // 以前の計算用ヘルパー（staticにしてManagerが所有）
    static Vector3 CatmullRom(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t);
    static Vector4 Lerp(const Vector4& start, const Vector4& end, float t);

private:
    bool NeedsGeometryRebuild() const;
    void BuildVertices();
    void PrepareVertexBuffer(uint32_t requiredVertices, uint64_t completedFence);
    D3D12_GPU_VIRTUAL_ADDRESS PrepareViewProjection(const Matrix4x4& viewProjection, uint64_t completedFence);

    Object3dCommon* object3dCommon_ = nullptr;
    DirectXCommon* dxCommon_ = nullptr;

    // インスタンス管理
    std::vector<std::unique_ptr<TrailInstance>> instances_;

    // 共有頂点バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    TrailVertex* vertexData_ = nullptr;
    uint32_t vertexCapacity_ = 0;
    uint64_t vertexLastUsedFence_ = 0;
    struct RetiredVertexBuffer {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        uint64_t lastUsedFence = 0;
    };
    std::vector<RetiredVertexBuffer> retiredVertexBuffers_;
    std::vector<TrailVertex> builtVertices_;
    struct DrawRange { uint32_t firstVertex = 0; uint32_t vertexCount = 0; };
    std::vector<DrawRange> builtDrawRanges_;
    std::vector<uint64_t> builtRevisions_;
    uint64_t builtGeometryVertices_ = 0;
    bool geometryDirty_ = true;
    bool batchingEnabled_ = true;

    // Each draw gets immutable constants until its submission fence completes.
    struct ViewProjectionBuffer {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        Matrix4x4* data = nullptr;
        uint64_t lastUsedFence = 0;
    };
    std::vector<ViewProjectionBuffer> viewProjectionBuffers_;
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;

    std::string textureFilePath_;
    DrawStats drawStats_{};
};

} // namespace cg2
