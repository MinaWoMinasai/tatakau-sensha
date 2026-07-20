#pragma once
#include <map>
#include <string>
#include <memory>
#include <Model.h>

class ModelManager
{

public:

	static ModelManager* GetInstance();

	void Finalize();

	void Initialize(DirectXCommon* dxCommon);

	void LoadModel(const std::string& filePath);
	void CreatePlaneModel(const std::string& modelName, float width = 1.0f, float depth = 1.0f);
	void CreateBoxModel(const std::string& modelName, const Vector3& size = { 1.0f, 1.0f, 1.0f });
	void CreateCylinderModel(const std::string& modelName, float radius = 1.0f, float height = 2.0f, uint32_t segments = 64);
	void CreateUvSphereModel(const std::string& modelName, float radius = 1.0f, uint32_t latitudeSegments = 64, uint32_t longitudeSegments = 128);

	Model* FindModel(const std::string& filePath);
	const std::map<std::string, std::unique_ptr<Model>>& GetModels() const { return models; }

private:

	static ModelManager* instance;

	ModelManager() = default;
	~ModelManager() = default;
	ModelManager(ModelManager&) = delete;
	ModelManager& operator=(const ModelManager&) = delete;

	// モデルデータ
	std::map<std::string, std::unique_ptr<Model>> models;

	std::unique_ptr<ModelCommon> modelCommon;
};

