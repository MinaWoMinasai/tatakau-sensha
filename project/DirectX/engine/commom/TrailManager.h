#pragma once
#include <vector>
#include <memory>
#include "TrailInstance.h"

class TrailManager {
public:
    // 全体で許容する最大頂点数（必要に応じて調整）
    static const uint32_t kMaxVertices = 8192;

#if defined(USE_IMGUI) && !defined(NDEBUG)
    struct DrawStats {
        size_t totalInstances = 0;
        size_t activeInstances = 0;
        size_t drawableInstances = 0;
        size_t totalPoints = 0;
        uint64_t requestedVertices = 0;
        uint64_t generatedVertices = 0;
        uint32_t drawCalls = 0;
        uint32_t vertexCapacity = kMaxVertices;
        bool capacityHit = false;
        uint64_t truncatedVertices = 0;
        float drawCpuMs = 0.0f;
        float vertexBuildCpuMs = 0.0f;
        float drawCommandCpuMs = 0.0f;
    };
#endif

    void Initialize(DirectXCommon* dxcommon, Object3dCommon* object3dCommon, const std::string& textureFilePath);

    // インスタンスの生成
    TrailInstance* CreateInstance();

    void Update(float deltaTime);

    // 全インスタンスの描画
    void DrawAll(const Matrix4x4& viewProjection);

    // インスタンスのクリア（シーン切り替え時など）
    void ClearInstances() { instances_.clear(); }
    size_t GetInstanceCount() const { return instances_.size(); }
#if defined(USE_IMGUI) && !defined(NDEBUG)
    const DrawStats& GetDrawStats() const { return drawStats_; }
#endif

    // 以前の計算用ヘルパー（staticにしてManagerが所有）
    static Vector3 CatmullRom(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t);
    static Vector4 Lerp(const Vector4& start, const Vector4& end, float t);

private:
    Object3dCommon* object3dCommon_ = nullptr;
    DirectXCommon* dxCommon_ = nullptr;

    // インスタンス管理
    std::vector<std::unique_ptr<TrailInstance>> instances_;

    // 共有頂点バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    TrailVertex* vertexData_ = nullptr;

    // 共有定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> constResource_;
    Matrix4x4* constData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;

    std::string textureFilePath_;
#if defined(USE_IMGUI) && !defined(NDEBUG)
    DrawStats drawStats_{};
#endif
};
