//#include "ParticleManager.h"
//
//ParticleManager* ParticleManager::GetInstance() {
//	if (instance == nullptr) {
//		instance = new ParticleManager;
//	}
//	return instance;
//}
//
//void ParticleManager::Finalize() {
//	delete instance;
//	instance = nullptr;
//}
//
//void ParticleManager::CreateParticleGroup(const std::string name, const std::string textureFilePath)
//{
//	// まだ存在しない場合生成
//	if (particleGroups.contains(name)) {
//		return;
//	}
//
//}
//
//void ParticleManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
//	dxCommon_ = dxCommon;
//	srvManager_ = srvManager;
//
//	// 用の頂点リソースを作る
//	vertexResource = texture.CreateBufferResource(modelCommon_->GetDxCommon()->GetDevice(), sizeof(VertexData) * modelData_.vertices.size());
//
//	// リソースの先端のアドレスから使う
//	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
//	// 使用するリソースのサイズは頂点3つ分のサイズ
//	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData_.vertices.size());
//	// 1頂点あたりのサイズ
//	vertexBufferView.StrideInBytes = sizeof(VertexData);
//	// 書き込むためのアドレスを取得
//	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
//	// 頂点データをリソースにコピー
//	std::memcpy(vertexData, modelData_.vertices.data(), sizeof(VertexData) * modelData_.vertices.size());
//
//}