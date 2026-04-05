//#include "TrailManager.h"
//
//void TrailManager::Initialize(DirectXCommon* dxcommon, Object3dCommon* object3dCommon, const std::string& textureFilePath) {
//    dxCommon_ = dxcommon;
//    object3dCommon_ = object3dCommon;
//    textureFilePath_ = textureFilePath;
//
//    // 頂点リソース作成
//    vertexResource_ = dxCommon_->CreateBufferResource(sizeof(TrailVertex) * kMaxVertices);
//    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
//    vertexBufferView_.SizeInBytes = sizeof(TrailVertex) * kMaxVertices;
//    vertexBufferView_.StrideInBytes = sizeof(TrailVertex);
//    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
//
//    // 定数リソース作成
//    constResource_ = dxCommon_->CreateBufferResource(sizeof(Matrix4x4));
//    constResource_->Map(0, nullptr, reinterpret_cast<void**>(&constData_));
//}
//
//TrailInstance* TrailManager::CreateInstance() {
//    instances_.push_back(std::make_unique<TrailInstance>());
//    return instances_.back().get();
//}
//
//void TrailManager::Update() {
//    std::erase_if(instances_, [](const auto& instance) {
//        // 更新が止まって、かつ履歴がもう空（消えきった）なら削除
//        return !instance->IsActive() && instance->GetPoints().empty();
//    });
//}
//
//void TrailManager::DrawAll(const Matrix4x4& viewProjection) {
//    auto commandList = dxCommon_->GetCommandList();
//    *constData_ = viewProjection;
//
//    // パイプライン設定（1回だけ）
//    commandList->SetGraphicsRootSignature(dxCommon_->GetPSOTrail().root_.GetSignature().Get());
//    commandList->SetPipelineState(dxCommon_->GetPSOTrail().graphicsState_.Get());
//    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
//    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
//    commandList->SetGraphicsRootConstantBufferView(1, constResource_->GetGPUVirtualAddress());
//    commandList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath_));
//
//    uint32_t currentVertexOffset = 0;
//
//    for (auto& instance : instances_) {
//        const auto& points = instance->GetPoints();
//        if (points.size() < 4) continue;
//
//        const auto& config = instance->GetConfig();
//        uint32_t steps = (std::max)(1u, config.interpolationSteps);
//        uint32_t instanceVertexCount = 0;
//
//        // --- 頂点データの構築 ---
//        for (size_t i = 0; i < points.size() - 1; ++i) {
//            size_t i0 = (i == 0) ? 0 : i - 1;
//            size_t i1 = i;
//            size_t i2 = i + 1;
//            size_t i3 = (i + 2 >= points.size()) ? points.size() - 1 : i + 2;
//
//            for (uint32_t j = 0; j < steps; ++j) {
//                if (currentVertexOffset + instanceVertexCount + 2 >= kMaxVertices) break;
//
//                float t = (float)j / (float)steps;
//                float globalRatio = (float)(i * steps + j) / (float)((points.size() - 1) * steps);
//
//                Vector4 color = Lerp(config.startColor, config.endColor, globalRatio);
//                Vector3 tip = CatmullRom(points[i0].tip, points[i1].tip, points[i2].tip, points[i3].tip, t);
//                Vector3 base = CatmullRom(points[i0].base, points[i1].base, points[i2].base, points[i3].base, t);
//
//                // 書き込み
//                uint32_t vIdx = currentVertexOffset + instanceVertexCount;
//                vertexData_[vIdx].pos = tip;
//                vertexData_[vIdx].color = color;
//                vertexData_[vIdx].uv = { globalRatio, 0.0f };
//
//                vertexData_[vIdx + 1].pos = base;
//                vertexData_[vIdx + 1].color = color;
//                vertexData_[vIdx + 1].uv = { globalRatio, 1.0f };
//
//                instanceVertexCount += 2;
//            }
//        }
//
//        // インスタンスごとの描画命令
//        if (instanceVertexCount > 0) {
//            commandList->DrawInstanced(instanceVertexCount, 1, currentVertexOffset, 0);
//            currentVertexOffset += instanceVertexCount;
//        }
//    }
//}
//
//Vector3 TrailManager::CatmullRom(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t)
//{
//	float t2 = t * t;
//	float t3 = t2 * t;
//
//	return ((p1 * 2.0f) +
//		(-p0 + p2) * t +
//		(p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2 +
//		(-p0 + p1 * 3.0f - p2 * 3.0f + p3) * t3) * 0.5f;
//}
//
//Vector4 TrailManager::Lerp(const Vector4& start, const Vector4& end, float t)
//{
//	// 線形補間
//	Vector4 result = start + (end - start) * t;
//	// 補間結果を返す
//	return result;
//}