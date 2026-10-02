#pragma once
#include <vector>
#include <memory>
#include "TrailInstance.h"

namespace cg2 {

/// @brief 複数の軌跡を頂点へ展開し、GPUバッファの寿命と一括描画を管理する。
class TrailManager {
public:
    // Initial allocation. Grow on demand without shortening existing trails.
    static constexpr uint32_t kMaxVertices = 8192;
    // Keep pathological editor settings bounded (9 MiB of vertex data).
    static constexpr uint32_t kMaxBatchVertices = 262144;

    /// @brief 描画した件数・頂点数などを保持し、負荷の確認に使う。
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

    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(DirectXCommon* dxcommon, Object3dCommon* object3dCommon, const std::string& textureFilePath);

    // インスタンスの生成
    TrailInstance* CreateInstance();

    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Update(float deltaTime);

    // 全インスタンスの描画
    void DrawAll(const Matrix4x4& viewProjection);

    // インスタンスのクリア（シーン切り替え時など）
    void ClearInstances()
    {
        instances_.clear();
        geometryDirty_ = true;
    }
    /// @brief インスタンス件数を返す。
    size_t GetInstanceCount() const
    {
        return instances_.size();
    }
    /// @brief 描画可能インスタンスが存在するか判定する。
    bool HasDrawableInstances() const
    {
        for (const auto& instance : instances_) {
            if (instance->GetPoints().size() >= 4)
                return true;
        }
        return false;
    }
    /// @brief 描画件数と頂点数の集計を返す。
    const DrawStats& GetDrawStats() const
    {
        return drawStats_;
    }

    // 以前の計算用ヘルパー（staticにしてManagerが所有）
    static Vector3 CatmullRom(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t);
    /// @brief 始点と終点を係数tで線形補間した値を返す。tの範囲外の扱いは呼び出し先の実装に従う。
    static Vector4 Lerp(const Vector4& start, const Vector4& end, float t);

private:
    /// @brief 描画形状を再構築する必要があるか判定する。
    bool NeedsGeometryRebuild() const;
    /// @brief 頂点を組み立てる。
    void BuildVertices();
    /// @brief 頂点バッファを利用前に準備する。
    void PrepareVertexBuffer(uint32_t requiredVertices, uint64_t completedFence);
    /// @brief ビュー・射影行列を利用前に準備する。
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
    /// @brief GPUがまだ参照しうる旧頂点バッファと解放可能なフェンス値を保持する。
    struct RetiredVertexBuffer {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        uint64_t lastUsedFence = 0;
    };
    std::vector<RetiredVertexBuffer> retiredVertexBuffers_;
    std::vector<TrailVertex> builtVertices_;
    /// @brief 一括描画する頂点範囲と描画条件を表す。
    struct DrawRange {
        uint32_t firstVertex = 0;
        uint32_t vertexCount = 0;
    };
    std::vector<DrawRange> builtDrawRanges_;
    std::vector<uint64_t> builtRevisions_;
    uint64_t builtGeometryVertices_ = 0;
    bool geometryDirty_ = true;
    bool batchingEnabled_ = true;

    // Each draw gets immutable constants until its submission fence completes.
    /// @brief 軌跡描画で使うビュー・射影行列の定数バッファを保持する。
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
