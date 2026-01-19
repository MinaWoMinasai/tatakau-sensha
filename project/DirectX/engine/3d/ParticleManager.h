#pragma once
#include "Struct.h"
#include "SrvManager.h"
#include <unordered_map>
#include "Calculation.h"
#include "Texture.h"

class ParticleManager
{

public:

	// シングルトン
	static ParticleManager* GetInstance();
	void Finalize();

	void CreateParticleGroup(const std::string name, const std::string textureFilePath);

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	void Update();

	void Draw();

private:
	std::unordered_map<std::string, ParticleGroup> particleGroups;

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
	// バッファリソース内のデータをさすポインタ
	VertexData* vertexData;
	// バッファリソースの使い道を補足するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

	Texture texture;

	static ParticleManager* instance;
};

