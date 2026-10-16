#include "Stage.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include "game/render/StageSurfaceGeometry.h"
#include "TextureManager.h"
#include <bit>

namespace {

/// @brief 対応する障害物prefabのモデル名・地形種類を出力する。未対応ならfalseで出力を変更しない。
bool TryGetObstacleModel(const std::string& prefab, std::string& model, MapChipType& type)
{
    if (prefab == "Default" || prefab == "Wall" || prefab == "Block") {
        model = "cube.obj";
        type = MapChipType::kBlock;
        return true;
    }
    if (prefab == "DamageBlock") {
        model = "cubeDamage.obj";
        type = MapChipType::kDamageBlock;
        return true;
    }
    return false;
}

} // namespace

void Stage::InitializeSurfaceModels()
{
    auto* models = myEngine::ModelManager::GetInstance();
    auto* textures = myEngine::TextureManager::GetInstance();
    textures->LoadTexture("resources/skybox.dds");
    models->CreateModelFromData(stagesurface::kFloorModel, stagesurface::FloorMesh());
    models->CreateModelFromData(stagesurface::kBlockModel, stagesurface::BlockMesh());
    models->CreateModelFromData(stagesurface::kWallModel, stagesurface::BlockMesh(0.0f));
    for (int levels = 1; levels <= 6; ++levels) {
        models->CreateModelFromData(stagesurface::StackModelName(levels), stagesurface::StackMesh(levels));
    }
    if (!surfaceFloor_) {
        surfaceFloor_ = std::make_unique<myEngine::Object3d>();
        surfaceFloor_->Initialize();
        surfaceFloor_->SetModel(models->FindModel(stagesurface::kFloorModel));
        surfaceFloor_->SetEnvironmentMap(textures->GetSrvIndex("resources/skybox.dds"));
    }
}

namespace {
/// @brief 表面描画用のマテリアル設定をオブジェクトへ適用する。
void ApplySurfaceMaterial(myEngine::Object3d& object, const myEngine::Vector4& color, float metallic, float roughness, float lightIntensity)
{
    object.SetColor(color);
    object.SetLighting(true);
    object.SetLightingMode(2); // 既存のCook-Torrance/シーンの法線 + マテリアル出力。
    object.SetMetallic(metallic);
    object.SetRoughness(roughness);
    object.SetAmbientOcclusion(1.0f);
    object.SetEmissive({0.0f, 0.0f, 0.0f}, 0.0f);
    object.SetEnvironmentCoefficient(0.10f);
    object.SetIBLIntensity(0.10f, 0.16f);
    object.SetShadowReceiveStrength(0.0f); // 現在、ステージ用のシャドウマップのパスは無い。
    object.SetNormalDetail(0.0f, 1.0f);
    object.SetPointLightPosition({-10000.0f, -10000.0f, -10000.0f});
    object.Update();
    // Updateは全体のライト方向をコピーする。行列がそろった後で、このオブジェクトの
    // キーライトだけを上書きする。アクターと影用のカメラは変えない。
    object.SetDirectionalLightDirection({-0.40f, -0.55f, 0.80f});
    object.SetInsensity(lightIntensity);
}

/// @brief ブロックの表面描画用オブジェクトを作成する。
void InitializeSurfaceBlock(Block& block)
{
    block.surfaceObject = std::make_unique<myEngine::Object3d>();
    block.surfaceObject->Initialize();
    block.surfaceObject->SetModel(myEngine::ModelManager::GetInstance()->FindModel(stagesurface::kBlockModel));
    block.surfaceObject->SetEnvironmentMap(myEngine::TextureManager::GetInstance()->GetSrvIndex("resources/skybox.dds"));
}
} // namespace

uint32_t Stage::DrawSurfaceFloor(const stagesurface::Settings& settings, const myEngine::Vector3& cameraPos, float halfWidth,
                                 float halfHeight, const myEngine::Vector3& lightPosition)
{
    if (!surfaceFloor_ || !settings.enabled || !settings.floorEnabled) {
        return 0;
    }
    // 空のセルも含めたすべてのセルと、既存のカメラの可視範囲を含める。
    float minX = -MapChip::kBlockWidth * 0.5f, minY = -MapChip::kBlockHeight * 0.5f;
    float maxX = MapChip::kBlockWidth * (MapChip::kNumBlockHorizontal - 0.5f);
    float maxY = MapChip::kBlockHeight * (MapChip::kNumBlockVirtical - 0.5f);
    for (const auto& row : blocks_) {
        for (const auto& block : row) {
            if (!block.isActive) {
                continue;
            }
            minX = (std::min)(minX, block.aabb.min.x);
            minY = (std::min)(minY, block.aabb.min.y);
            maxX = (std::max)(maxX, block.aabb.max.x);
            maxY = (std::max)(maxY, block.aabb.max.y);
        }
    }
    if (settings.wallStyle == 1 && settings.blocksEnabled) {
        for (const auto& wall : GetSurfaceWalls(settings)) {
            minX = (std::min)(minX, wall.gameplay.translate.x - std::abs(wall.gameplay.scale.x));
            maxX = (std::max)(maxX, wall.gameplay.translate.x + std::abs(wall.gameplay.scale.x));
            minY = (std::min)(minY, wall.gameplay.translate.y - std::abs(wall.gameplay.scale.y));
            maxY = (std::max)(maxY, wall.gameplay.translate.y + std::abs(wall.gameplay.scale.y));
        }
    }
    minX = (std::min)(minX, cameraPos.x - halfWidth) - settings.floorPadding;
    minY = (std::min)(minY, cameraPos.y - halfHeight) - settings.floorPadding;
    maxX = (std::max)(maxX, cameraPos.x + halfWidth) + settings.floorPadding;
    maxY = (std::max)(maxY, cameraPos.y + halfHeight) + settings.floorPadding;
    surfaceFloor_->SetTransform({{(maxX - minX) * 0.5f, (maxY - minY) * 0.5f, 1.0f},
                                 {0.0f, 0.0f, 0.0f},
                                 {(minX + maxX) * 0.5f, (minY + maxY) * 0.5f, settings.floorDepth}});
    ApplySurfaceMaterial(*surfaceFloor_, settings.floorColor, settings.floorMetallic, settings.floorRoughness,
                         settings.floorLightIntensity);
    if (settings.floorDetail > 0.0f) {
        surfaceFloor_->SetNormalDetail(settings.floorDetail, 120.0f);
    }
    if (settings.floorLocalLight) {
        surfaceFloor_->SetPointLight({lightPosition.x, lightPosition.y, -settings.floorLocalHeight},
                                     {settings.floorLightColor.x, settings.floorLightColor.y, settings.floorLightColor.z},
                                     settings.floorLocalIntensity, settings.floorLocalRadius, 2.0f);
    }
    surfaceFloor_->Draw();
    return 1;
}

uint32_t Stage::DrawSurfaceBlocks(const stagesurface::Settings& settings, const myEngine::Vector3& cameraPos, float halfWidth,
                                  float halfHeight, bool drawNormalBlocks)
{
    surfaceTriangles_ = 0;
    if (!settings.enabled || !settings.blocksEnabled) {
        return 0;
    }
    uint32_t draws = 0;
    if (settings.wallStyle == 1) {
        const auto& walls = GetSurfaceWalls(settings);
        for (size_t i = 0; i < walls.size(); ++i) {
            const auto& wall = walls[i];
            if (!drawNormalBlocks && wall.type == MapChipType::kBlock) {
                continue;
            }
            const auto& t = wall.gameplay;
            if (t.translate.x + std::abs(t.scale.x) < cameraPos.x - halfWidth - 3 ||
                t.translate.x - std::abs(t.scale.x) > cameraPos.x + halfWidth + 3 ||
                t.translate.y + std::abs(t.scale.y) < cameraPos.y - halfHeight - 3 ||
                t.translate.y - std::abs(t.scale.y) > cameraPos.y + halfHeight + 3) {
                continue;
            }
            while (surfaceWallObjects_.size() <= i) {
                auto object = std::make_unique<myEngine::Object3d>();
                object->Initialize();
                object->SetModel(myEngine::ModelManager::GetInstance()->FindModel(stagesurface::kWallModel));
                object->SetEnvironmentMap(myEngine::TextureManager::GetInstance()->GetSrvIndex("resources/skybox.dds"));
                surfaceWallObjects_.push_back(std::move(object));
            }
            auto& object = *surfaceWallObjects_[i];
            const int levels = wall.isolated ? 1
                                             : (wall.exterior ? (std::clamp)(int(std::round(settings.exteriorHeight / 2.0f)), 1, 6)
                                                              : settings.stackLevels);
            object.SetModel(myEngine::ModelManager::GetInstance()->FindModel(stagesurface::StackModelName(levels)));
            object.SetTransform(SurfaceWallTransform(wall, settings));
            const auto color = wall.exterior ? myEngine::Vector4{.022f, .006f, .014f, 1}
                                             : (wall.type == MapChipType::kDamageBlock ? settings.damageBlockColor : settings.blockColor);
            ApplySurfaceMaterial(object, color, settings.blockMetallic, settings.blockRoughness, settings.blockLightIntensity);
            object.Draw();
            ++draws;
            surfaceTriangles_ += 12u * static_cast<uint32_t>(levels);
        }
        return draws;
    }
    const float marginX = MapChip::kBlockWidth * 1.5f, marginY = MapChip::kBlockHeight * 1.5f;
    for (auto& row : blocks_) {
        for (auto& block : row) {
            if (!block.isActive || !block.surfaceObject || (!drawNormalBlocks && block.type == MapChipType::kBlock)) {
                continue;
            }
            const auto& pos = block.originalPos;
            if (pos.x < cameraPos.x - halfWidth - marginX || pos.x > cameraPos.x + halfWidth + marginX ||
                pos.y < cameraPos.y - halfHeight - marginY || pos.y > cameraPos.y + halfHeight + marginY) {
                continue;
            }
            block.surfaceObject->SetTransform(stagesurface::BlockTransform(block.worldTransform, settings));
            ApplySurfaceMaterial(*block.surfaceObject,
                                 block.type == MapChipType::kDamageBlock ? settings.damageBlockColor : settings.blockColor,
                                 settings.blockMetallic, settings.blockRoughness, settings.blockLightIntensity);
            block.surfaceObject->Draw();
            ++draws;
            surfaceTriangles_ += 44;
        }
    }
    return draws;
}

myEngine::Transform Stage::SurfaceWallTransform(const SurfaceWall& wall, const stagesurface::Settings& settings)
{
    auto shape = settings;
    if (wall.exterior) {
        shape.blockHeight = settings.exteriorHeight;
    } else if (wall.isolated) {
        shape.blockHeight /= static_cast<float>(settings.stackLevels);
    }
    return stagesurface::BlockTransform(wall.gameplay, shape);
}

const std::vector<SurfaceWall>& Stage::GetSurfaceWalls(const stagesurface::Settings& settings)
{
    uint64_t hash = 1469598103934665603ull;
    auto append = [&](uint64_t v) {
        hash = (hash ^ v) * 1099511628211ull;
    };
    append(settings.exteriorEnabled);
    append(std::bit_cast<uint32_t>(settings.exteriorGap));
    append(std::bit_cast<uint32_t>(settings.exteriorWidth));
    std::vector<int> cells(MapChip::kNumBlockHorizontal * MapChip::kNumBlockVirtical, 0);
    float minX = 1e6f, minY = 1e6f, maxX = -1e6f, maxY = -1e6f;
    for (size_t y = 0; y < blocks_.size(); ++y) {
        for (size_t x = 0; x < blocks_[y].size(); ++x) {
            const auto& block = blocks_[y][x];
            append(block.isActive);
            if (!block.isActive) {
                continue;
            }
            append(static_cast<unsigned>(block.type));
            const auto& t = block.worldTransform;
            for (float v :
                 {t.translate.x, t.translate.y, t.translate.z, t.rotate.x, t.rotate.y, t.rotate.z, t.scale.x, t.scale.y, t.scale.z}) {
                append(std::bit_cast<uint32_t>(v));
            }
            minX = (std::min)(minX, block.aabb.min.x);
            minY = (std::min)(minY, block.aabb.min.y);
            maxX = (std::max)(maxX, block.aabb.max.x);
            maxY = (std::max)(maxY, block.aabb.max.y);
            if (!block.isLevelObject && y < MapChip::kNumBlockVirtical && x < MapChip::kNumBlockHorizontal) {
                cells[y * MapChip::kNumBlockHorizontal + x] = block.type == MapChipType::kDamageBlock ? 2 : 1;
            }
        }
    }
    if (hash == surfaceWallHash_) {
        return surfaceWalls_;
    }
    surfaceWallHash_ = hash;
    surfaceWalls_.clear();
    const auto rectangles = stagesurface::MergeWallCells(cells, MapChip::kNumBlockHorizontal, MapChip::kNumBlockVirtical);
    // マップの最も外側の帯は、外周を囲む連続した城壁の一部として描く。
    int minCellX = MapChip::kNumBlockHorizontal, minCellY = MapChip::kNumBlockVirtical, maxCellX = -1, maxCellY = -1;
    for (const auto& rectangle : rectangles) {
        const int x = static_cast<int>(rectangle.x), y = static_cast<int>(rectangle.y);
        minCellX = (std::min)(minCellX, x);
        minCellY = (std::min)(minCellY, y);
        maxCellX = (std::max)(maxCellX, x + static_cast<int>(rectangle.width) - 1);
        maxCellY = (std::max)(maxCellY, y + static_cast<int>(rectangle.height) - 1);
    }
    auto isBorder = [&](const auto& rectangle) {
        const int x = static_cast<int>(rectangle.x), y = static_cast<int>(rectangle.y);
        const int w = static_cast<int>(rectangle.width), h = static_cast<int>(rectangle.height);
        const bool column = w <= 2 && (x == minCellX || x + w - 1 == maxCellX);
        const bool row = h <= 2 && (y == minCellY || y + h - 1 == maxCellY);
        return settings.exteriorEnabled && (column || row);
    };
    for (const auto& rectangle : rectangles) {
        const auto& a = blocks_[rectangle.y][rectangle.x];
        const auto& b = blocks_[rectangle.y + rectangle.height - 1][rectangle.x + rectangle.width - 1];
        const float left = (std::min)(a.aabb.min.x, b.aabb.min.x), right = (std::max)(a.aabb.max.x, b.aabb.max.x);
        const float bottom = (std::min)(a.aabb.min.y, b.aabb.min.y), top = (std::max)(a.aabb.max.y, b.aabb.max.y);
        myEngine::Transform t{{(right - left) * .5f, (top - bottom) * .5f, 1},
                              {0, 0, 0},
                              {(left + right) * .5f, (bottom + top) * .5f, a.worldTransform.translate.z}};
        const bool border = a.type == MapChipType::kBlock && isBorder(rectangle);
        surfaceWalls_.push_back({t, a.type, border, !border && rectangle.width * rectangle.height == 1});
    }
    for (const auto& row : blocks_) {
        for (const auto& block : row) {
            if (block.isActive && block.isLevelObject) {
                surfaceWalls_.push_back({block.worldTransform, block.type, false, false});
            }
        }
    }
    if (settings.exteriorEnabled && minX < maxX && minY < maxY) {
        const float width = settings.exteriorWidth, gap = settings.exteriorGap;
        const float left = minX - gap - width, right = maxX + gap + width, bottom = minY - gap - width, top = maxY + gap + width;
        auto rail = [&](float x, float y, float sx, float sy) {
            surfaceWalls_.push_back({{{sx, sy, 1}, {0, 0, 0}, {x, y, 0}}, MapChipType::kBlock, true, false});
        };
        rail(left + width * .5f, (bottom + top) * .5f, width * .5f, (top - bottom) * .5f);
        rail(right - width * .5f, (bottom + top) * .5f, width * .5f, (top - bottom) * .5f);
        rail((left + right) * .5f, bottom + width * .5f, (right - left) * .5f - width, width * .5f);
        rail((left + right) * .5f, top - width * .5f, (right - left) * .5f - width, width * .5f);
    }
    return surfaceWalls_;
}

void Stage::Initialize()
{
    auto initialization = BuildInitialization();
    while (!initialization.Advance()) {
    }
}

myEngine::InitializationTask Stage::BuildInitialization()
{
    mapChip_ = std::make_unique<MapChip>();
    mapChip_->LoadMapChipCsv("resources/map.csv");
    auto blocks = BuildBlocks();
    while (!blocks.Advance()) {
        co_await std::suspend_always{};
    }
}

bool Stage::LoadRunMap(const std::string& csvPath, bool deferBlockBuild)
{
    std::ifstream input(csvPath);
    if (!input) {
        return false;
    }
    auto nextMap = std::make_unique<MapChip>();
    nextMap->ResetMapChipData();
    std::string line;
    for (uint32_t y = 0; y < MapChip::kNumBlockVirtical; ++y) {
        if (!std::getline(input, line)) {
            return false;
        }
        std::istringstream row(line);
        for (uint32_t x = 0; x < MapChip::kNumBlockHorizontal; ++x) {
            std::string token;
            if (!std::getline(row, token, ',')) {
                return false;
            }
            std::istringstream cell(token);
            int value = -1;
            if (!(cell >> value) || value < 0 || value > 2) {
                return false;
            }
            cell >> std::ws;
            if (!cell.eof()) {
                return false;
            }
            nextMap->mapChipData_.data[y][x] = static_cast<MapChipType>(value);
        }
        // 45セルの後に残る列や末尾カンマを拒否し、列の対応がずれたマップを採用しない。
        if (!row.eof()) {
            return false;
        }
    }
    while (std::getline(input, line)) {
        if (line.find_first_not_of(" \t\r") != std::string::npos) {
            return false;
        }
    }
    // 全行の検証後に差し替える。途中の読み込み失敗では既存のマップ・モデルを保持する。
    mapChip_ = std::move(nextMap);
    // GenerateBlocksは空白セルの旧ブロックを消さないため、前の部屋の有効ブロックを先に全消去する。
    ClearBlocksForPreview();
    if (!deferBlockBuild) {
        GenerateBlocks();
    }
    return true;
}

void Stage::Update() {}

void Stage::Draw()
{
    for (auto& line : blocks_) {
        for (auto& block : line) {
            if (!block.isActive) {
                continue;
            }

            block.object->Update();
            block.object->Draw();
        }
    }
}

void Stage::DrawVisible(const myEngine::Vector3& cameraPos, float halfWidth, float halfHeight, bool drawNormalBlocks)
{
    const float minX = cameraPos.x - halfWidth;
    const float maxX = cameraPos.x + halfWidth;
    const float minY = cameraPos.y - halfHeight;
    const float maxY = cameraPos.y + halfHeight;
    const float marginX = MapChip::kBlockWidth * 1.5f;
    const float marginY = MapChip::kBlockHeight * 1.5f;

    for (auto& line : blocks_) {
        for (auto& block : line) {
            if (!block.isActive) {
                continue;
            }
            if (!drawNormalBlocks && block.type == MapChipType::kBlock) {
                continue;
            }

            const myEngine::Vector3& pos = block.originalPos;
            if (pos.x < minX - marginX || pos.x > maxX + marginX || pos.y < minY - marginY || pos.y > maxY + marginY) {
                continue;
            }

            block.object->Update();
            block.object->Draw();
        }
    }
}

void Stage::ClearBlocksForPreview()
{
    blocks_.clear();
    surfaceWallHash_ = 0;
    surfaceWalls_.clear();
    mergedBlocks_.clear();
}

bool Stage::AddLevelObstacle(const myEngine::Transform& transform, const std::string& prefab)
{
    std::string model;
    MapChipType type = MapChipType::kBlock;
    if (!TryGetObstacleModel(prefab, model, type)) {
        std::cerr << "[LevelLoader] Unsupported Obstacle prefab: " << prefab << std::endl;
        return false;
    }
    InitializeSurfaceModels();

    Block block{};
    block.worldTransform = transform;
    block.object = std::make_unique<myEngine::Object3d>();
    block.object->Initialize();
    block.object->SetModel(model);
    block.object->SetTransform(block.worldTransform);
    block.object->Update();
    InitializeSurfaceBlock(block);
    block.originalPos = block.worldTransform.translate;
    block.type = type;
    block.isActive = true;
    block.isLevelObject = true;

    // 描画はtransform全体を使う一方、衝突形状は位置とスケールだけの軸平行形状にする。
    const myEngine::Vector3& pos = block.worldTransform.translate;
    const myEngine::Vector3 halfSize = {MapChip::kBlockWidth * block.worldTransform.scale.x * 0.5f,
                                        MapChip::kBlockHeight * block.worldTransform.scale.y * 0.5f,
                                        MapChip::kBlockWidth * block.worldTransform.scale.z * 0.5f};
    block.aabb.min = {pos.x - halfSize.x, pos.y - halfSize.y, pos.z - halfSize.z};
    block.aabb.max = {pos.x + halfSize.x, pos.y + halfSize.y, pos.z + halfSize.z};
    block.obb.center = pos;
    block.obb.halfExtents = halfSize;
    block.obb.orientation[0] = {1.0f, 0.0f, 0.0f};
    block.obb.orientation[1] = {0.0f, 1.0f, 0.0f};
    block.obb.orientation[2] = {0.0f, 0.0f, 1.0f};

    std::vector<Block> row;
    row.push_back(std::move(block));
    blocks_.push_back(std::move(row));
    mergedBlocks_.push_back({blocks_.back().front().aabb, type});
    return true;
}

void Stage::ClearLevelObstacles()
{
    blocks_.erase(std::remove_if(blocks_.begin(), blocks_.end(),
                                 [](const std::vector<Block>& row) {
                                     return row.size() == 1 && row.front().isLevelObject;
                                 }),
                  blocks_.end());
    RebuildMergedBlocks();
}

void Stage::GenerateBlocks()
{
    auto blocks = BuildBlocks();
    while (!blocks.Advance()) {
    }
}

myEngine::InitializationTask Stage::BuildBlocks()
{
    InitializeSurfaceModels();

    uint32_t numBlockHorizontal = mapChip_->GetNumBlockHorizontal();
    uint32_t numBlockVirtical = mapChip_->GetNumBlockVirtical();

    blocks_.resize(numBlockVirtical);
    for (uint32_t y = 0; y < numBlockVirtical; ++y) {
        blocks_[y].resize(numBlockHorizontal);
    }

    // 配列はCSVの行y・列x。セル中心はMapChipが行の上下を反転してワールド座標へ変換する。
    for (uint32_t y = 0; y < numBlockVirtical; ++y) {
        for (uint32_t x = 0; x < numBlockHorizontal; ++x) {
            if (mapChip_->GetMapChipTypeByIndex(x, y) == MapChipType::kBlock ||
                mapChip_->GetMapChipTypeByIndex(x, y) == MapChipType::kDamageBlock) {
                Block& block = blocks_[y][x];

                block.worldTransform = myEngine::InitWorldTransform();
                block.worldTransform.translate = mapChip_->GetMapChipPositionByIndex(x, y);
                block.object = std::make_unique<myEngine::Object3d>();
                block.object->Initialize();
                if (mapChip_->GetMapChipTypeByIndex(x, y) == MapChipType::kBlock) {
                    block.object->SetModel("cube.obj");
                } else {
                    block.object->SetModel("cubeDamage.obj");
                }

                block.object->SetTransform(block.worldTransform);
                InitializeSurfaceBlock(block);

                // 中心座標
                const auto& pos = block.worldTransform.translate;

                // AABB設定
                block.aabb.min = {pos.x - mapChip_->kBlockWidth / 2.0f, pos.y - mapChip_->kBlockHeight / 2.0f,
                                  pos.z - mapChip_->kBlockWidth / 2.0f};
                block.aabb.max = {pos.x + mapChip_->kBlockWidth / 2.0f, pos.y + mapChip_->kBlockHeight / 2.0f,
                                  pos.z + mapChip_->kBlockWidth / 2.0f};

                // OBB設定
                block.originalPos = pos;
                block.obb.center = pos;

                block.obb.halfExtents = {mapChip_->kBlockWidth * 0.5f, mapChip_->kBlockHeight * 0.5f, mapChip_->kBlockWidth * 0.5f};
                float rad = 45.0f * (3.14159265f / 180.0f);

                // 衝突OBBの軸はワールド軸と一致させる。直前のradはこの形状には使わない。
                block.obb.orientation[0] = {1, 0, 0};
                block.obb.orientation[1] = {0, 1, 0};
                block.obb.orientation[2] = {0, 0, 1};
                block.isActive = true;
                block.type = mapChip_->GetMapChipTypeByIndex(x, y);
            }
        }
        co_await std::suspend_always{};
    }

    RebuildMergedBlocks();
}

void Stage::RebuildMergedBlocks()
{
    // 各マップ行を左から走査し、同種の連続セルだけをX方向にまとめる。レベル障害物は独立したAABB。
    auto FixAABB = [](myEngine::AABB& aabb) {
        if (aabb.min.x > aabb.max.x) {
            std::swap(aabb.min.x, aabb.max.x);
        }
        if (aabb.min.y > aabb.max.y) {
            std::swap(aabb.min.y, aabb.max.y);
        }
        if (aabb.min.z > aabb.max.z) {
            std::swap(aabb.min.z, aabb.max.z);
        }
    };

    mergedBlocks_.clear();
    for (uint32_t i = 0; i < blocks_.size(); ++i) {
        if (blocks_[i].size() == 1 && blocks_[i].front().isLevelObject) {
            const Block& block = blocks_[i].front();
            if (block.isActive) {
                mergedBlocks_.push_back({block.aabb, block.type});
            }
            continue;
        }
        bool merging = false;
        myEngine::AABB mergedAABB{};
        MapChipType currentType{};

        for (uint32_t j = 0; j < blocks_[i].size(); ++j) {
            const Block& block = blocks_[i][j];

            if (block.isActive) {
                if (!merging) {
                    mergedAABB = block.aabb;
                    currentType = block.type;
                    merging = true;
                }
                // タイプが同じなら統合
                else if (block.type == currentType) {
                    mergedAABB.max.x = block.aabb.max.x;
                }
                // タイプが違うなら確定
                else {
                    mergedBlocks_.push_back({mergedAABB, currentType});
                    mergedAABB = block.aabb;
                    currentType = block.type;
                }
            } else if (merging) {
                mergedBlocks_.push_back({mergedAABB, currentType});
                merging = false;
            }
        }

        if (merging) {
            mergedBlocks_.push_back({mergedAABB, currentType});
        }
    }
}

void Stage::ResolvePlayerCollision(Player& player, myEngine::AxisXYZ axis)
{
    myEngine::AABB playerAABB = player.GetAABB();
    myEngine::Vector3 playerPos = player.GetWorldPosition();
    myEngine::Vector3 velocity = player.GetMove();

    const float kEpsilon = 0.01f;

    for (const MergedBlock& block : mergedBlocks_) {
        if (!myEngine::IsCollision(playerAABB, block.aabb)) {
            continue;
        }

        // AABBの境界接触も含めて通知する。無敵などの受付条件はPlayer::Damage側で扱う。
        if (block.type == MapChipType::kDamageBlock) {
            player.Damage(damageBlockDamage_);
        }

        myEngine::Vector3 overlap = {std::min(playerAABB.max.x, block.aabb.max.x) - std::max(playerAABB.min.x, block.aabb.min.x),
                                     std::min(playerAABB.max.y, block.aabb.max.y) - std::max(playerAABB.min.y, block.aabb.min.y), 0.0f};

        // --------------------
        // X方向衝突
        // --------------------
        if (axis == myEngine::X) {
            if (playerAABB.min.x < block.aabb.min.x) {
                playerPos.x -= overlap.x + kEpsilon;
            } else {
                playerPos.x += overlap.x + kEpsilon;
            }
            velocity.x = 0.0f;
        }

        // --------------------
        // Y方向衝突
        // --------------------
        if (axis == myEngine::Y) {
            if (playerAABB.min.y < block.aabb.min.y) {
                playerPos.y -= overlap.y + kEpsilon;
            } else {
                playerPos.y += overlap.y + kEpsilon;
                player.SetOnGround(true);
            }
            velocity.y = 0.0f;
        }

        // 次のブロックは補正後のAABBで判定する。同一軸で複数の壁を処理しても古い位置を使わない。
        player.SetWorldPosition(playerPos);
        player.SetVelocity(velocity);
        playerAABB = player.GetAABB();
    }
}

void Stage::ResolvePlayerDroneCollision(PlayerDrone& playerDrone, myEngine::AxisXYZ axis)
{
    myEngine::AABB playerAABB = playerDrone.GetAABB();
    myEngine::Vector3 playerPos = playerDrone.GetWorldPosition();
    myEngine::Vector3 velocity = playerDrone.GetMove();

    const float kEpsilon = 0.01f;

    for (const MergedBlock& block : mergedBlocks_) {
        if (!myEngine::IsCollision(playerAABB, block.aabb)) {
            continue;
        }

        // ドローンのダメージブロック分岐は現在空で、地形接触によるHP減算は行わない。
        if (block.type == MapChipType::kDamageBlock) {
        }

        myEngine::Vector3 overlap = {std::min(playerAABB.max.x, block.aabb.max.x) - std::max(playerAABB.min.x, block.aabb.min.x),
                                     std::min(playerAABB.max.y, block.aabb.max.y) - std::max(playerAABB.min.y, block.aabb.min.y), 0.0f};

        // --------------------
        // X方向衝突
        // --------------------
        if (axis == myEngine::X) {
            if (playerAABB.min.x < block.aabb.min.x) {
                playerPos.x -= overlap.x + kEpsilon;
            } else {
                playerPos.x += overlap.x + kEpsilon;
            }
            velocity.x = 0.0f;
        }

        // --------------------
        // Y方向衝突
        // --------------------
        if (axis == myEngine::Y) {
            if (playerAABB.min.y < block.aabb.min.y) {
                playerPos.y -= overlap.y + kEpsilon;
            } else {
                playerPos.y += overlap.y + kEpsilon;
            }
            velocity.y = 0.0f;
        }

        playerDrone.SetWorldPosition(playerPos);
        playerDrone.SetVelocity(velocity);
        playerAABB = playerDrone.GetAABB();
    }
}

void Stage::ResolveEnemyCollision(Enemy& enemy, myEngine::AxisXYZ axis)
{
    myEngine::AABB enemyAABB = enemy.GetAABB();
    myEngine::Vector3 enemyPos = enemy.GetWorldPosition();
    const float kEpsilon = 0.01f;

    for (const MergedBlock& block : mergedBlocks_) {
        if (!myEngine::IsCollision(enemyAABB, block.aabb)) {
            continue;
        }

        myEngine::Vector3 overlap = {std::min(enemyAABB.max.x, block.aabb.max.x) - std::max(enemyAABB.min.x, block.aabb.min.x),
                                     std::min(enemyAABB.max.y, block.aabb.max.y) - std::max(enemyAABB.min.y, block.aabb.min.y), 0.0f};

        if (axis == myEngine::X) {
            enemyPos.x += (enemyAABB.min.x < block.aabb.min.x) ? -(overlap.x + kEpsilon) : +(overlap.x + kEpsilon);
        } else if (axis == myEngine::Y) {
            enemyPos.y += (enemyAABB.min.y < block.aabb.min.y) ? -(overlap.y + kEpsilon) : +(overlap.y + kEpsilon);
        }

        enemy.SetWorldPosition(enemyPos);
        enemyAABB = enemy.GetAABB();
    }
}

void Stage::ResolveExpEnemyCollision(ExpEnemy& enemy, myEngine::AxisXYZ axis)
{
    myEngine::AABB enemyAABB = enemy.GetAABB();
    myEngine::Vector3 enemyPos = enemy.GetWorldPosition();
    const float kEpsilon = 0.01f;

    for (const MergedBlock& block : mergedBlocks_) {
        if (!myEngine::IsCollision(enemyAABB, block.aabb)) {
            continue;
        }

        myEngine::Vector3 overlap = {std::min(enemyAABB.max.x, block.aabb.max.x) - std::max(enemyAABB.min.x, block.aabb.min.x),
                                     std::min(enemyAABB.max.y, block.aabb.max.y) - std::max(enemyAABB.min.y, block.aabb.min.y), 0.0f};

        if (axis == myEngine::X) {
            enemyPos.x += (enemyAABB.min.x < block.aabb.min.x) ? -(overlap.x + kEpsilon) : +(overlap.x + kEpsilon);
        } else if (axis == myEngine::Y) {
            enemyPos.y += (enemyAABB.min.y < block.aabb.min.y) ? -(overlap.y + kEpsilon) : +(overlap.y + kEpsilon);
        }

        enemy.SetWorldPosition(enemyPos);
        enemyAABB = enemy.GetAABB();
    }
}

void Stage::ResolveBulletsCollision(const std::vector<Bullet*>& bullets)
{
    for (Bullet* bullet : bullets) {
        if (!bullet || bullet->IsDead()) {
            continue;
        }

        myEngine::Vector3 bulletPos = bullet->GetWorldPosition();
        float radius = bullet->GetRadius();
        myEngine::Sphere bulletSphere{bulletPos, radius};

        bool isCollided = false;
        float nearestDist = std::numeric_limits<float>::max();
        myEngine::Vector3 nearestClosestPoint{};
        myEngine::Vector3 nearestNormal{};

        for (const auto& block : mergedBlocks_) {
            if (!myEngine::IsCollision(block.aabb, bulletSphere)) {
                continue;
            }

            myEngine::Vector3 closestPoint{std::clamp(bulletSphere.center.x, block.aabb.min.x, block.aabb.max.x),
                                           std::clamp(bulletSphere.center.y, block.aabb.min.y, block.aabb.max.y),
                                           std::clamp(bulletSphere.center.z, block.aabb.min.z, block.aabb.max.z)};

            // 差分ベクトルを計算
            myEngine::Vector3 diff = bulletSphere.center - closestPoint;
            diff.z = 0.0f; // 2D的な処理のためZを無視

            float dist = myEngine::Length(diff);

            // XYの最近接点との差がほぼ0なら、外向き法線を正規化からは求められない。
            if (dist < 0.0001f) {
                if (!bullet->UsesRunProjectileRules()) {
                    // 成長弾ルールを使わない経路は、埋まった弾を死亡させて壁通知を行わない。
                    bullet->Die();
                    isCollided = false;
                    break;
                }
                // 成長弾ルールの弾は、移動後の中心が壁内でも最寄りのXY面と外向き法線を選ぶ。
                // 壁外の位置を通知することで、反射・分裂の起点も壁の外へ補正できる。
                const float distances[4] = {bulletPos.x - block.aabb.min.x, block.aabb.max.x - bulletPos.x, bulletPos.y - block.aabb.min.y,
                                            block.aabb.max.y - bulletPos.y};
                int face = 0;
                for (int index = 1; index < 4; ++index) {
                    if (distances[index] < distances[face]) {
                        face = index;
                    }
                }
                nearestClosestPoint = bulletPos;
                nearestNormal = {};
                if (face == 0) {
                    nearestClosestPoint.x = block.aabb.min.x;
                    nearestNormal.x = -1.0f;
                }
                if (face == 1) {
                    nearestClosestPoint.x = block.aabb.max.x;
                    nearestNormal.x = 1.0f;
                }
                if (face == 2) {
                    nearestClosestPoint.y = block.aabb.min.y;
                    nearestNormal.y = -1.0f;
                }
                if (face == 3) {
                    nearestClosestPoint.y = block.aabb.max.y;
                    nearestNormal.y = 1.0f;
                }
                isCollided = true;
                break;
            }

            // ここまで来たら「外側で接している」ので正規化できる
            myEngine::Vector3 normal = myEngine::Normalize(diff);

            if (dist < nearestDist) {
                nearestDist = dist;
                nearestClosestPoint = closestPoint;
                nearestNormal = normal;
                isCollided = true;
            }
        }

        // ループを抜けた後、埋まって死んだ弾は処理しない
        if (bullet->IsDead()) {
            continue;
        }

        if (isCollided) {
            // 半径と余白だけ外へ離した位置を渡す。位置・速度の設定、分裂予約、死亡はBullet側の担当。
            bulletPos = nearestClosestPoint + nearestNormal * (radius + 0.01f);

            bullet->OnWallImpact(bulletPos, nearestNormal);
        }
    }
}

void Stage::ResolvePlayerCollisionSphere(Player& player)
{
    myEngine::Sphere sphere = player.GetSphere();
    myEngine::Vector3 pos = sphere.center;
    myEngine::Vector3 vel = player.GetMove();

    for (const auto& line : blocks_) {
        for (const Block& block : line) {
            if (!block.isActive) {
                continue;
            }

            // ここで Sphere vs OBB
            myEngine::CollisionResult hit = myEngine::CheckSphereVsOBB(sphere, block.obb);

            if (!hit.hit) {
                continue;
            }

            // 押し戻し
            pos += hit.normal * hit.depth;

            // 壁へ向かう法線成分だけを除き、接線方向の速度は保持する。
            float vn = myEngine::Dot(vel, hit.normal);
            if (vn < 0.0f) {
                vel -= hit.normal * vn;
            }

            // 接地判定
            if (myEngine::Dot(hit.normal, myEngine::Vector3(0, 1, 0)) > 0.7f) {
                player.SetOnGround(true);
            }

            // 更新
            sphere.center = pos;
        }
    }

    player.SetWorldPosition(pos);
    player.SetVelocity(vel);
}

void Stage::ResolvePlayerCollisionSphereY(Player& player)
{
    myEngine::Sphere sphere = player.GetSphere();
    myEngine::Vector3 pos = sphere.center;
    myEngine::Vector3 vel = player.GetMove();

    for (const auto& line : blocks_) {
        for (const Block& block : line) {
            if (!block.isActive) {
                continue;
            }

            myEngine::CollisionResult hit = myEngine::CheckSphereVsOBB(sphere, block.obb);
            if (!hit.hit) {
                continue;
            }

            float upDot = myEngine::Dot(hit.normal, myEngine::Vector3(0, 1, 0));

            // 法線のY成分が0.7を上回るか-0.7を下回る接触だけを使う。境界の±0.7は除く。
            if (upDot > 0.7f || upDot < -0.7f) {
                // 押し戻し（Y成分だけ）
                pos.y += hit.normal.y * hit.depth;

                // Y方向の速度を止める
                if (vel.y * hit.normal.y < 0.0f) {
                    vel.y = 0.0f;
                }

                // このY専用経路では接地フラグは設定しない。

                sphere.center = pos;
            }
        }
    }

    player.SetWorldPosition(pos);
    player.SetVelocity(vel);
}

void Stage::ResolvePlayerCollisionSphereX(Player& player)
{
    myEngine::Sphere sphere = player.GetSphere();
    myEngine::Vector3 pos = sphere.center;
    myEngine::Vector3 vel = player.GetMove();

    for (const auto& line : blocks_) {
        for (const Block& block : line) {
            if (!block.isActive) {
                continue;
            }

            myEngine::CollisionResult hit = myEngine::CheckSphereVsOBB(sphere, block.obb);
            if (!hit.hit) {
                continue;
            }

            // 法線からYだけを除く。残るX/Z方向を正規化して位置・速度を補正する。
            myEngine::Vector3 lateralNormal = hit.normal;
            lateralNormal.y = 0.0f;

            float lenSq = myEngine::Dot(lateralNormal, lateralNormal);
            if (lenSq < 0.0001f) {
                continue;
            }

            lateralNormal = myEngine::Normalize(lateralNormal);

            // Yを除いた法線方向へ押し戻す。純粋なX軸だけの補正ではない。
            pos += lateralNormal * hit.depth;

            // 横速度を止める
            float vn = myEngine::Dot(vel, lateralNormal);
            if (vn < 0.0f) {
                vel -= lateralNormal * vn;
            }

            sphere.center = pos;
        }
    }

    player.SetWorldPosition(pos);
    player.SetVelocity(vel);
}

bool Stage::IsCollisionWithAnyBlock(const myEngine::Vector3& pos, float radius)
{
    myEngine::Sphere spawnSphere = {pos, radius};
    for (const auto& line : blocks_) {
        for (const Block& block : line) {
            if (!block.isActive) {
                continue;
            }
            // 既存の判定関数（CheckSphereVsOBB）を利用
            myEngine::CollisionResult hit = myEngine::CheckSphereVsOBB(spawnSphere, block.obb);
            if (hit.hit) {
                return true; // 壁に当たっている
            }
        }
    }
    return false; // 安全
}

const std::vector<MergedBlock>& Stage::GetMergedBlocks() const
{
    return mergedBlocks_;
}

const std::vector<std::vector<Block>>& Stage::GetBlocks() const
{
    return blocks_;
}
