#include "ModelManager.h"

ModelManager* ModelManager::instance = nullptr;

ModelManager* ModelManager::GetInstance() {
	if (instance == nullptr) {
		instance = new ModelManager;
	}
	return instance;
}

void ModelManager::Finalize() {
	delete instance;
	instance = nullptr;
}

void ModelManager::Initialize(DirectXCommon* dxCommon) {

	modelCommon = std::make_unique<ModelCommon>();
	modelCommon->Initialize(dxCommon);

}

void ModelManager::LoadModel(const std::string& filePath)
{
	// 読み込み済みモデルを検索
	if (models.contains(filePath)) {
		// 読み込み済みなら早期return
		return;
	}

	// モデルの生成とファイルの読み込み
	std::unique_ptr<Model> model = std::make_unique<Model>();
	model->Initialize(modelCommon.get(), "resources", filePath);

	// モデルをmapコンテナに格納する
	models.insert(std::make_pair(filePath, std::move(model)));
}

void ModelManager::CreatePlaneModel(const std::string& modelName, float width, float depth)
{
	if (models.contains(modelName)) {
		return;
	}

	std::unique_ptr<Model> model = std::make_unique<Model>();
	model->InitializeFromModelData(
		modelCommon.get(),
		Model::CreatePlane(width, depth));

	models.insert(std::make_pair(modelName, std::move(model)));
}

void ModelManager::CreateGridModel(const std::string& modelName, float width, float depth, uint32_t xSegments, uint32_t zSegments)
{
	if (models.contains(modelName)) {
		return;
	}

	std::unique_ptr<Model> model = std::make_unique<Model>();
	model->InitializeFromModelData(
		modelCommon.get(),
		Model::CreateGrid(width, depth, xSegments, zSegments));

	models.insert(std::make_pair(modelName, std::move(model)));
}

void ModelManager::CreateBoxModel(const std::string& modelName, const Vector3& size)
{
	if (models.contains(modelName)) {
		return;
	}

	std::unique_ptr<Model> model = std::make_unique<Model>();
	model->InitializeFromModelData(
		modelCommon.get(),
		Model::CreateBox(size));

	models.insert(std::make_pair(modelName, std::move(model)));
}

void ModelManager::CreateCylinderModel(const std::string& modelName, float radius, float height, uint32_t segments)
{
	if (models.contains(modelName)) {
		return;
	}

	std::unique_ptr<Model> model = std::make_unique<Model>();
	model->InitializeFromModelData(
		modelCommon.get(),
		Model::CreateCylinder(radius, height, segments));

	models.insert(std::make_pair(modelName, std::move(model)));
}

void ModelManager::CreateUvSphereModel(const std::string& modelName, float radius, uint32_t latitudeSegments, uint32_t longitudeSegments)
{
	if (models.contains(modelName)) {
		return;
	}

	std::unique_ptr<Model> model = std::make_unique<Model>();
	model->InitializeFromModelData(
		modelCommon.get(),
		Model::CreateUvSphere(radius, latitudeSegments, longitudeSegments));

	models.insert(std::make_pair(modelName, std::move(model)));
}

Model* ModelManager::FindModel(const std::string& filePath)
{
	// 読み込み済みモデルを検索
	if (models.contains(filePath)) {
		// 読み込み済みモデルを戻り値としてreturn
		return models.at(filePath).get();
	}

	// ファイル名一致なし
	return nullptr;
}
