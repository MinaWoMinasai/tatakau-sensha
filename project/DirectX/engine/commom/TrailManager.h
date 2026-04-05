//#pragma once
//#include <vector>
//#include <memory>
//#include "TrailInstance.h"
//
//class TrailManager {
//public:
//    // 全体で許容する最大頂点数（必要に応じて調整）
//    static const uint32_t kMaxVertices = 8192;
//
//    void Initialize(DirectXCommon* dxcommon, Object3dCommon* object3dCommon, const std::string& textureFilePath);
//
//    // インスタンスの生成
//    TrailInstance* CreateInstance();
//
//    void Update();
//    
//    // 全インスタンスの描画
//    void DrawAll(const Matrix4x4& viewProjection);
//
//    // インスタンスのクリア（シーン切り替え時など）
//    void ClearInstances() { instances_.clear(); }
//
//    // 以前の計算用ヘルパー（staticにしてManagerが所有）
//    static Vector3 CatmullRom(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t);
//    static Vector4 Lerp(const Vector4& start, const Vector4& end, float t);
//
//private:
//    Object3dCommon* object3dCommon_ = nullptr;
//    DirectXCommon* dxCommon_ = nullptr;
//
//    // インスタンス管理
//    std::vector<std::unique_ptr<TrailInstance>> instances_;
//
//    // 共有頂点バッファ
//    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
//    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
//    TrailVertex* vertexData_ = nullptr;
//
//    // 共有定数バッファ
//    Microsoft::WRL::ComPtr<ID3D12Resource> constResource_;
//    Matrix4x4* constData_ = nullptr;
//
//    std::string textureFilePath_;
//};