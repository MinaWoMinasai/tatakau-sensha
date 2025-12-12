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

	Model* FindModel(const std::string& filePath);

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

