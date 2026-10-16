#include "ModelManager.h"
#include "StartupTrace.h"
#include "BackgroundLoader.h"
#include "TextureManager.h"
#include <filesystem>

namespace myEngine {

ModelManager* ModelManager::GetInstance() {
	static ModelManager instance;
	return &instance;
}

void ModelManager::Finalize() {
	pendingModels_.clear();
	texturePreloadsQueued_.clear();
	models.clear();
	modelCommon.reset();
}

void ModelManager::Initialize(DirectXCommon* dxCommon) {

	modelCommon = std::make_unique<ModelCommon>();
	modelCommon->Initialize(dxCommon);

}

void ModelManager::PreloadObjModel(const std::string& filePath)
{
    if (models.contains(filePath) || pendingModels_.contains(filePath)) return;
    if (std::filesystem::path(filePath).extension() != ".obj" ||
        !std::filesystem::is_regular_file(std::filesystem::path("resources") / filePath)) return;
    pendingModels_.emplace(filePath, BackgroundLoader::Get().Submit([filePath] {
        StartupTrace::Scope scope("FileThread.Model." + filePath);
        return Model::LoadObjFile("resources", filePath);
    }));
}

void ModelManager::PumpPreloads()
{
    // A parsed OBJ names its material textures; decode them on the file threads
    // too instead of synchronously when the model is first used.
    for (const auto& [path, result] : pendingModels_) {
        if (texturePreloadsQueued_.contains(path) ||
            result.wait_for(std::chrono::seconds(0)) != std::future_status::ready) continue;
        texturePreloadsQueued_.insert(path);
        try {
            const ModelData& data = result.get();
            const auto& materials = data.materials.empty() ? std::vector<MaterialData>{data.material} : data.materials;
            for (const MaterialData& material : materials) {
                if (!material.textureFilePath.empty()) TextureManager::GetInstance()->PreloadTexture(material.textureFilePath);
                if (material.hasNormalTexture && !material.normalTextureFilePath.empty())
                    TextureManager::GetInstance()->PreloadTexture(material.normalTextureFilePath,
                        TextureManager::TextureColorSpace::LinearData);
            }
        } catch (const std::exception&) {} // LoadModel reports and falls back.
    }
}

bool ModelManager::ArePreloadsReady() const
{
    for (const auto& entry : pendingModels_)
        if (entry.second.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return false;
    return true;
}

void ModelManager::LoadModel(const std::string& filePath)
{
	// 読み込み済みモデルを検索
	if (models.contains(filePath)) {
		// 読み込み済みなら早期return
		return;
	}

	// モデルの生成とファイルの読み込み
	StartupTrace::Scope startupScope("Model.Load." + filePath);
	std::unique_ptr<Model> model = std::make_unique<Model>();
    auto pending = pendingModels_.find(filePath);
    if (pending != pendingModels_.end()) {
        auto result = pending->second;
        pendingModels_.erase(pending);
        try {
            model->InitializeFromModelData(modelCommon.get(), result.get());
            StartupTrace::Count("Models.BackgroundUsed");
        } catch (const std::exception& error) {
            LogWrite().Log("[BackgroundLoader] Model fallback: " + std::string(error.what()) + "\n");
            model->Initialize(modelCommon.get(), "resources", filePath);
        }
    } else model->Initialize(modelCommon.get(), "resources", filePath);

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

void ModelManager::CreateModelFromData(const std::string& modelName, const ModelData& data)
{
	if (models.contains(modelName)) return;
	auto model = std::make_unique<Model>();
	model->InitializeFromModelData(modelCommon.get(), data);
	models.emplace(modelName, std::move(model));
}

void ModelManager::CreateFacetedCrystalModel(
	const std::string& modelName,
	float radius,
	float height,
	uint32_t sides)
{
	if (models.contains(modelName)) {
		return;
	}

	std::unique_ptr<Model> model = std::make_unique<Model>();
	model->InitializeFromModelData(
		modelCommon.get(),
		Model::CreateFacetedCrystal(radius, height, sides));

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

} // namespace myEngine
