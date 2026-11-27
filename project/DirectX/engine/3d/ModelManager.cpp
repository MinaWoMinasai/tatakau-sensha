#include "ModelManager.h"
#include "ResourcePaths.h"
#include "StartupTrace.h"
#include "BackgroundLoader.h"
#include "TextureManager.h"
#include "Utf8Path.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace myEngine {

ModelManager* ModelManager::GetInstance()
{
    static ModelManager instance;
    return &instance;
}

void ModelManager::Finalize()
{
    pendingModels_.clear();
    texturePreloadsQueued_.clear();
    models.clear();
    dxCommon_ = nullptr;
}

void ModelManager::Initialize(DirectXCommon* dxCommon)
{
    dxCommon_ = dxCommon;
}

void ModelManager::PreloadObjModel(const std::string& filePath)
{
    if (models.contains(filePath) || pendingModels_.contains(filePath)) {
        return;
    }
    if (Utf8ToPath(filePath).extension() != ".obj" ||
        !std::filesystem::is_regular_file(Utf8ToPath(ResourcePaths::GameRoot()) / Utf8ToPath(filePath))) {
        return;
    }
    pendingModels_.emplace(filePath, BackgroundLoader::Get().Submit([filePath] {
        StartupTrace::Scope scope("FileThread.Model." + filePath);
        return Model::LoadObjFile(ResourcePaths::GameRoot(), filePath);
    }));
}

void ModelManager::PumpPreloads()
{
    // 解析済みOBJはマテリアルのテクスチャ名を持つ。初回使用時に同期デコードせず、
    // それらもファイルスレッドでデコードしておく。
    for (const auto& [path, result] : pendingModels_) {
        if (texturePreloadsQueued_.contains(path) || result.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            continue;
        }
        texturePreloadsQueued_.insert(path);
        try {
            const ModelData& data = result.get();
            const auto& materials = data.materials.empty() ? std::vector<MaterialData>{data.material} : data.materials;
            for (const MaterialData& material : materials) {
                if (!material.textureFilePath.empty()) {
                    TextureManager::GetInstance()->PreloadTexture(material.textureFilePath);
                }
                if (material.hasNormalTexture && !material.normalTextureFilePath.empty()) {
                    TextureManager::GetInstance()->PreloadTexture(material.normalTextureFilePath,
                                                                  TextureManager::TextureColorSpace::LinearData);
                }
            }
        }
        catch (const std::exception&) {
        } // LoadModelがエラーを報告し、代替モデルへ切り替える。
    }
}

bool ModelManager::ArePreloadsReady() const
{
    for (const auto& entry : pendingModels_) {
        if (entry.second.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            return false;
        }
    }
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
            model->InitializeFromModelData(dxCommon_, result.get());
            StartupTrace::Count("Models.BackgroundUsed");
        }
        catch (const std::exception& error) {
            LogWrite().Log("[BackgroundLoader] Model fallback: " + std::string(error.what()) + "\n");
            model->Initialize(dxCommon_, ResourcePaths::GameRoot(), filePath);
        }
    } else {
        model->Initialize(dxCommon_, ResourcePaths::GameRoot(), filePath);
    }

    // モデルをmapコンテナに格納する
    models.insert(std::make_pair(filePath, std::move(model)));
}

void ModelManager::CreateModelFromData(const std::string& modelName, const ModelData& data)
{
    if (models.contains(modelName)) {
        return;
    }
    auto model = std::make_unique<Model>();
    model->InitializeFromModelData(dxCommon_, data);
    models.emplace(modelName, std::move(model));
}

int ModelManager::LoadPrimitivePresets(const std::string& filePath, std::string& error)
{
    error.clear();
    std::ifstream input(Utf8ToPath(ResourcePaths::GameRoot()) / Utf8ToPath(filePath));
    if (!input) {
        return 0;
    }
    const nlohmann::json data = nlohmann::json::parse(input, nullptr, false);
    if (data.is_discarded() || !data.is_object() || !data.contains("presets") || !data.at("presets").is_array()) {
        error = "基本の形のプリセットが読めない: " + filePath;
        return 0;
    }
    int registered = 0;
    for (const auto& item : data.at("presets")) {
        const std::string name = item.is_object() ? item.value("name", std::string()) : std::string();
        PrimitiveShape shape{};
        if (name.empty() || !ParsePrimitiveShape(item.value("shape", std::string()), shape)) {
            error += (error.empty() ? "" : "\n") + std::string("名前か形が読めないプリセットを飛ばした: ") + item.dump();
            continue;
        }
        PrimitiveParams params;
        if (item.contains("size") && item.at("size").is_array() && item.at("size").size() == 3) {
            params.size = {item.at("size")[0].get<float>(), item.at("size")[1].get<float>(), item.at("size")[2].get<float>()};
        }
        params.radius = item.value("radius", params.radius);
        params.segments = item.value("segments", params.segments);
        params.rings = item.value("rings", params.rings);
        CreateModelFromData(name, Model::CreatePrimitive(shape, params));
        ++registered;
    }
    return registered;
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
