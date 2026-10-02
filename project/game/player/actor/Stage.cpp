#include "Stage.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>

namespace {

/// @brief Obstacleモデルを取得を試みる。
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

void Stage::Initialize() {

	mapChip_ = std::make_unique<MapChip>();
	mapChip_->LoadMapChipCsv("resources/map.csv");
	GenerateBlocks();
}

bool Stage::LoadRunMap(const std::string& csvPath)
{
	std::ifstream input(csvPath);
	if (!input) return false;
	auto nextMap = std::make_unique<MapChip>();
	nextMap->ResetMapChipData();
	std::string line;
	for (uint32_t y = 0; y < MapChip::kNumBlockVirtical; ++y) {
		if (!std::getline(input, line)) return false;
		std::istringstream row(line);
		for (uint32_t x = 0; x < MapChip::kNumBlockHorizontal; ++x) {
			std::string token;
			if (!std::getline(row, token, ',')) return false;
			std::istringstream cell(token);
			int value = -1;
			if (!(cell >> value) || value < 0 || value > 2) return false;
			cell >> std::ws;
			if (!cell.eof()) return false;
			nextMap->mapChipData_.data[y][x] = static_cast<MapChipType>(value);
		}
		// Reject extra cells (including a trailing comma) rather than shifting a map.
		if (!row.eof()) return false;
	}
	while (std::getline(input, line)) {
		if (line.find_first_not_of(" \t\r") != std::string::npos) return false;
	}
	mapChip_ = std::move(nextMap);
	// GenerateBlocks alone resizes existing cells, so stale active blocks must
	// be cleared first when a previous room had walls where this room is open.
	ClearBlocksForPreview();
	GenerateBlocks();
	return true;
}

void Stage::Update() {}

void Stage::Draw() {
	for (auto& line : blocks_) {
		for (auto& block : line) {
			if (!block.isActive) continue;

			block.object->Update();
			block.object->Draw();
		}
	}
}

void Stage::DrawVisible(const cg2::Vector3& cameraPos, float halfWidth, float halfHeight, bool drawNormalBlocks) {
	const float minX = cameraPos.x - halfWidth;
	const float maxX = cameraPos.x + halfWidth;
	const float minY = cameraPos.y - halfHeight;
	const float maxY = cameraPos.y + halfHeight;
	const float marginX = MapChip::kBlockWidth * 1.5f;
	const float marginY = MapChip::kBlockHeight * 1.5f;

	for (auto& line : blocks_) {
		for (auto& block : line) {
			if (!block.isActive) continue;
			if (!drawNormalBlocks && block.type == MapChipType::kBlock) {
				continue;
			}

			const cg2::Vector3& pos = block.originalPos;
			if (pos.x < minX - marginX || pos.x > maxX + marginX ||
				pos.y < minY - marginY || pos.y > maxY + marginY) {
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
	mergedBlocks_.clear();
}

bool Stage::AddLevelObstacle(const cg2::Transform& transform, const std::string& prefab)
{
	std::string model;
	MapChipType type = MapChipType::kBlock;
	if (!TryGetObstacleModel(prefab, model, type)) {
		std::cerr << "[LevelLoader] Unsupported Obstacle prefab: " << prefab << std::endl;
		return false;
	}

	Block block{};
	block.worldTransform = transform;
	block.object = std::make_unique<cg2::Object3d>();
	block.object->Initialize();
	block.object->SetModel(model);
	block.object->SetTransform(block.worldTransform);
	block.object->Update();
	block.originalPos = block.worldTransform.translate;
	block.type = type;
	block.isActive = true;
	block.isLevelObject = true;

	const cg2::Vector3& pos = block.worldTransform.translate;
	const cg2::Vector3 halfSize = {
		MapChip::kBlockWidth * block.worldTransform.scale.x * 0.5f,
		MapChip::kBlockHeight * block.worldTransform.scale.y * 0.5f,
		MapChip::kBlockWidth * block.worldTransform.scale.z * 0.5f
	};
	block.aabb.min = { pos.x - halfSize.x, pos.y - halfSize.y, pos.z - halfSize.z };
	block.aabb.max = { pos.x + halfSize.x, pos.y + halfSize.y, pos.z + halfSize.z };
	block.obb.center = pos;
	block.obb.halfExtents = halfSize;
	block.obb.orientation[0] = { 1.0f, 0.0f, 0.0f };
	block.obb.orientation[1] = { 0.0f, 1.0f, 0.0f };
	block.obb.orientation[2] = { 0.0f, 0.0f, 1.0f };

	std::vector<Block> row;
	row.push_back(std::move(block));
	blocks_.push_back(std::move(row));
	mergedBlocks_.push_back({ blocks_.back().front().aabb, type });
	return true;
}

void Stage::ClearLevelObstacles()
{
	blocks_.erase(
		std::remove_if(
			blocks_.begin(),
			blocks_.end(),
			[](const std::vector<Block>& row) {
				return row.size() == 1 && row.front().isLevelObject;
			}),
		blocks_.end());
	RebuildMergedBlocks();
}

void Stage::GenerateBlocks() {

	uint32_t numBlockHorizontal = mapChip_->GetNumBlockHorizontal();
	uint32_t numBlockVirtical = mapChip_->GetNumBlockVirtical();

	blocks_.resize(numBlockVirtical);
	for (uint32_t y = 0; y < numBlockVirtical; ++y) {
		blocks_[y].resize(numBlockHorizontal);
	}

	for (uint32_t y = 0; y < numBlockVirtical; ++y) {
		for (uint32_t x = 0; x < numBlockHorizontal; ++x) {

			if (mapChip_->GetMapChipTypeByIndex(x, y) == MapChipType::kBlock || mapChip_->GetMapChipTypeByIndex(x, y) == MapChipType::kDamageBlock) {

				Block& block = blocks_[y][x];

				block.worldTransform = cg2::InitWorldTransform();
				block.worldTransform.translate = mapChip_->GetMapChipPositionByIndex(x, y);
				block.object = std::make_unique<cg2::Object3d>();
				block.object->Initialize();
				if (mapChip_->GetMapChipTypeByIndex(x, y) == MapChipType::kBlock) {
					block.object->SetModel("cube.obj");
				} else {
					block.object->SetModel("cubeDamage.obj");
				}

				block.object->SetTransform(block.worldTransform);

				// 中心座標
				const auto& pos = block.worldTransform.translate;

				// AABB設定
				block.aabb.min = { pos.x - mapChip_->kBlockWidth / 2.0f, pos.y - mapChip_->kBlockHeight / 2.0f, pos.z - mapChip_->kBlockWidth / 2.0f };
				block.aabb.max = { pos.x + mapChip_->kBlockWidth / 2.0f, pos.y + mapChip_->kBlockHeight / 2.0f, pos.z + mapChip_->kBlockWidth / 2.0f };

				// OBB設定
				block.originalPos = pos;
				block.obb.center = pos;

				block.obb.halfExtents = {
					mapChip_->kBlockWidth * 0.5f,
					mapChip_->kBlockHeight * 0.5f,
					mapChip_->kBlockWidth * 0.5f
				};
				float rad = 45.0f * (3.14159265f / 180.0f);

				//// 軸はワールド基準（今は回さない）
				block.obb.orientation[0] = { 1, 0, 0 };
				block.obb.orientation[1] = { 0, 1, 0 };
				block.obb.orientation[2] = { 0, 0, 1 };
				block.isActive = true;
				block.type = mapChip_->GetMapChipTypeByIndex(x, y);
			}
		}
	}

	RebuildMergedBlocks();
}

void Stage::RebuildMergedBlocks()
{
	// ブロック統合処理 
	auto FixAABB = [](cg2::AABB& aabb) {
		if (aabb.min.x > aabb.max.x)
			std::swap(aabb.min.x, aabb.max.x);
		if (aabb.min.y > aabb.max.y)
			std::swap(aabb.min.y, aabb.max.y);
		if (aabb.min.z > aabb.max.z)
			std::swap(aabb.min.z, aabb.max.z);
		};

	mergedBlocks_.clear();
	for (uint32_t i = 0; i < blocks_.size(); ++i) {
		if (blocks_[i].size() == 1 && blocks_[i].front().isLevelObject) {
			const Block& block = blocks_[i].front();
			if (block.isActive) {
				mergedBlocks_.push_back({ block.aabb, block.type });
			}
			continue;
		}
		bool merging = false;
		cg2::AABB mergedAABB{};
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
					mergedBlocks_.push_back({ mergedAABB, currentType });
					mergedAABB = block.aabb;
					currentType = block.type;
				}
			} else if (merging) {
				mergedBlocks_.push_back({ mergedAABB, currentType });
				merging = false;
			}
		}

		if (merging) {
			mergedBlocks_.push_back({ mergedAABB, currentType });
		}
	}
}

void Stage::ResolvePlayerCollision(Player& player, cg2::AxisXYZ axis)
{

	cg2::AABB playerAABB = player.GetAABB();
	cg2::Vector3 playerPos = player.GetWorldPosition();
	cg2::Vector3 velocity = player.GetMove();

	const float kEpsilon = 0.01f;

	for (const MergedBlock& block : mergedBlocks_) {

		if (!cg2::IsCollision(playerAABB, block.aabb)) {
			continue;
		}

		// ダメージ床
		if (block.type == MapChipType::kDamageBlock) {
			player.Damage(damageBlockDamage_);
		}

		cg2::Vector3 overlap = {
			std::min(playerAABB.max.x, block.aabb.max.x) - std::max(playerAABB.min.x, block.aabb.min.x),
			std::min(playerAABB.max.y, block.aabb.max.y) - std::max(playerAABB.min.y, block.aabb.min.y),
			0.0f
		};

		// --------------------
		// X方向衝突
		// --------------------
		if (axis == cg2::X) {
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
		if (axis == cg2::Y) {
			if (playerAABB.min.y < block.aabb.min.y) {
				playerPos.y -= overlap.y + kEpsilon;
			} else {
				playerPos.y += overlap.y + kEpsilon;
				player.SetOnGround(true);
			}
			velocity.y = 0.0f;
		}

		player.SetWorldPosition(playerPos);
		player.SetVelocity(velocity);
		playerAABB = player.GetAABB();
	}
}

void Stage::ResolvePlayerDroneCollision(PlayerDrone& playerDrone, cg2::AxisXYZ axis)
{

	cg2::AABB playerAABB = playerDrone.GetAABB();
	cg2::Vector3 playerPos = playerDrone.GetWorldPosition();
	cg2::Vector3 velocity = playerDrone.GetMove();

	const float kEpsilon = 0.01f;

	for (const MergedBlock& block : mergedBlocks_) {

		if (!cg2::IsCollision(playerAABB, block.aabb)) {
			continue;
		}

		// ダメージ床
		if (block.type == MapChipType::kDamageBlock) {

		}

		cg2::Vector3 overlap = {
			std::min(playerAABB.max.x, block.aabb.max.x) - std::max(playerAABB.min.x, block.aabb.min.x),
			std::min(playerAABB.max.y, block.aabb.max.y) - std::max(playerAABB.min.y, block.aabb.min.y),
			0.0f
		};

		// --------------------
		// X方向衝突
		// --------------------
		if (axis == cg2::X) {
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
		if (axis == cg2::Y) {
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

void Stage::ResolveEnemyCollision(Enemy& enemy, cg2::AxisXYZ axis)
{
	cg2::AABB enemyAABB = enemy.GetAABB();
	cg2::Vector3 enemyPos = enemy.GetWorldPosition();
	const float kEpsilon = 0.01f;


	for (const MergedBlock& block : mergedBlocks_) {

		if (!cg2::IsCollision(enemyAABB, block.aabb)) {
			continue;
		}

		cg2::Vector3 overlap = {
			std::min(enemyAABB.max.x, block.aabb.max.x) - std::max(enemyAABB.min.x, block.aabb.min.x),
			std::min(enemyAABB.max.y, block.aabb.max.y) - std::max(enemyAABB.min.y, block.aabb.min.y),
			0.0f
		};

		if (axis == cg2::X) {
			enemyPos.x += (enemyAABB.min.x < block.aabb.min.x)
				? -(overlap.x + kEpsilon)
				: +(overlap.x + kEpsilon);
		} else if (axis == cg2::Y) {
			enemyPos.y += (enemyAABB.min.y < block.aabb.min.y)
				? -(overlap.y + kEpsilon)
				: +(overlap.y + kEpsilon);
		}

		enemy.SetWorldPosition(enemyPos);
		enemyAABB = enemy.GetAABB();
	}
}

void Stage::ResolveExpEnemyCollision(ExpEnemy& enemy, cg2::AxisXYZ axis)
{
	cg2::AABB enemyAABB = enemy.GetAABB();
	cg2::Vector3 enemyPos = enemy.GetWorldPosition();
	const float kEpsilon = 0.01f;


	for (const MergedBlock& block : mergedBlocks_) {

		if (!cg2::IsCollision(enemyAABB, block.aabb)) {
			continue;
		}

		cg2::Vector3 overlap = {
			std::min(enemyAABB.max.x, block.aabb.max.x) - std::max(enemyAABB.min.x, block.aabb.min.x),
			std::min(enemyAABB.max.y, block.aabb.max.y) - std::max(enemyAABB.min.y, block.aabb.min.y),
			0.0f
		};

		if (axis == cg2::X) {
			enemyPos.x += (enemyAABB.min.x < block.aabb.min.x)
				? -(overlap.x + kEpsilon)
				: +(overlap.x + kEpsilon);
		} else if (axis == cg2::Y) {
			enemyPos.y += (enemyAABB.min.y < block.aabb.min.y)
				? -(overlap.y + kEpsilon)
				: +(overlap.y + kEpsilon);
		}

		enemy.SetWorldPosition(enemyPos);
		enemyAABB = enemy.GetAABB();
	}
}

void Stage::ResolveBulletsCollision(const std::vector<Bullet*>& bullets)
{
	for (Bullet* bullet : bullets) {
		if (!bullet || bullet->IsDead()) continue;

		cg2::Vector3 bulletPos = bullet->GetWorldPosition();
		float radius = bullet->GetRadius();
		cg2::Sphere bulletSphere{ bulletPos, radius };

		bool isCollided = false;
		float nearestDist = std::numeric_limits<float>::max();
		cg2::Vector3 nearestClosestPoint{};
		cg2::Vector3 nearestNormal{};

		for (const auto& block : mergedBlocks_) {

			if (!cg2::IsCollision(block.aabb, bulletSphere)) continue;

			cg2::Vector3 closestPoint{
				std::clamp(bulletSphere.center.x, block.aabb.min.x, block.aabb.max.x),
				std::clamp(bulletSphere.center.y, block.aabb.min.y, block.aabb.max.y),
				std::clamp(bulletSphere.center.z, block.aabb.min.z, block.aabb.max.z)
			};

			// 差分ベクトルを計算
			cg2::Vector3 diff = bulletSphere.center - closestPoint;
			diff.z = 0.0f; // 2D的な処理のためZを無視

			float dist = cg2::Length(diff);

			// ★修正ポイント：完全に埋まっている（中心がブロック内）場合
			if (dist < 0.0001f) {
				if (!bullet->UsesRunProjectileRules()) {
					// Preserve the arena's embedded-projectile behavior.
					bullet->Die();
					isCollided = false;
					break;
				}
				// A fast expedition shot can finish its step inside a block.
				// Use the nearest XY face instead of normalizing a zero vector,
				// allowing its first impact to split outside the surface.
				const float distances[4] = {bulletPos.x - block.aabb.min.x, block.aabb.max.x - bulletPos.x,
					bulletPos.y - block.aabb.min.y, block.aabb.max.y - bulletPos.y};
				int face = 0;
				for (int index = 1; index < 4; ++index) if (distances[index] < distances[face]) face = index;
				nearestClosestPoint = bulletPos;
				nearestNormal = {};
				if (face == 0) { nearestClosestPoint.x = block.aabb.min.x; nearestNormal.x = -1.0f; }
				if (face == 1) { nearestClosestPoint.x = block.aabb.max.x; nearestNormal.x = 1.0f; }
				if (face == 2) { nearestClosestPoint.y = block.aabb.min.y; nearestNormal.y = -1.0f; }
				if (face == 3) { nearestClosestPoint.y = block.aabb.max.y; nearestNormal.y = 1.0f; }
				isCollided = true;
				break;
			}

			// ここまで来たら「外側で接している」ので正規化できる
			cg2::Vector3 normal = cg2::Normalize(diff);

			if (dist < nearestDist) {
				nearestDist = dist;
				nearestClosestPoint = closestPoint;
				nearestNormal = normal;
				isCollided = true;
			}
		}

		// ループを抜けた後、埋まって死んだ弾は処理しない
		if (bullet->IsDead()) continue;

		if (isCollided) {
			bulletPos = nearestClosestPoint + nearestNormal * (radius + 0.01f);

			bullet->OnWallImpact(bulletPos, nearestNormal);
		}
	}
}

void Stage::ResolvePlayerCollisionSphere(Player& player)
{
	cg2::Sphere sphere = player.GetSphere();
	cg2::Vector3 pos = sphere.center;
	cg2::Vector3 vel = player.GetMove();

	for (const auto& line : blocks_) {
		for (const Block& block : line) {
			if (!block.isActive) continue;

			// ここで Sphere vs OBB
			cg2::CollisionResult hit =
				cg2::CheckSphereVsOBB(sphere, block.obb);

			if (!hit.hit) continue;

			// 押し戻し
			pos += hit.normal * hit.depth;

			// 法線方向の速度を消す（滑らない）
			float vn = cg2::Dot(vel, hit.normal);
			if (vn < 0.0f) {
				vel -= hit.normal * vn;
			}

			// 接地判定
			if (cg2::Dot(hit.normal, cg2::Vector3(0, 1, 0)) > 0.7f) {
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
	cg2::Sphere sphere = player.GetSphere();
	cg2::Vector3 pos = sphere.center;
	cg2::Vector3 vel = player.GetMove();

	for (const auto& line : blocks_) {
		for (const Block& block : line) {
			if (!block.isActive) continue;

			cg2::CollisionResult hit = cg2::CheckSphereVsOBB(sphere, block.obb);
			if (!hit.hit) continue;

			float upDot = cg2::Dot(hit.normal, cg2::Vector3(0, 1, 0));

			// ほぼ床 or 天井として扱える場合
			if (upDot > 0.7f || upDot < -0.7f) {

				// 押し戻し（Y成分だけ）
				pos.y += hit.normal.y * hit.depth;

				// Y方向の速度を止める
				if (vel.y * hit.normal.y < 0.0f) {
					vel.y = 0.0f;
				}

				// 接地判定（床のみ）


				sphere.center = pos;
			}
		}
	}

	player.SetWorldPosition(pos);
	player.SetVelocity(vel);
}
void Stage::ResolvePlayerCollisionSphereX(Player& player)
{
	cg2::Sphere sphere = player.GetSphere();
	cg2::Vector3 pos = sphere.center;
	cg2::Vector3 vel = player.GetMove();

	for (const auto& line : blocks_) {
		for (const Block& block : line) {
			if (!block.isActive) continue;

			cg2::CollisionResult hit = cg2::CheckSphereVsOBB(sphere, block.obb);
			if (!hit.hit) continue;

			// 横成分のみ使う
			cg2::Vector3 lateralNormal = hit.normal;
			lateralNormal.y = 0.0f;

			float lenSq = cg2::Dot(lateralNormal, lateralNormal);
			if (lenSq < 0.0001f) continue;

			lateralNormal = cg2::Normalize(lateralNormal);

			// 押し戻し（X方向）
			pos += lateralNormal * hit.depth;

			// 横速度を止める
			float vn = cg2::Dot(vel, lateralNormal);
			if (vn < 0.0f) {
				vel -= lateralNormal * vn;
			}

			sphere.center = pos;
		}
	}

	player.SetWorldPosition(pos);
	player.SetVelocity(vel);
}

bool Stage::IsCollisionWithAnyBlock(const cg2::Vector3& pos, float radius) {
	cg2::Sphere spawnSphere = { pos, radius };
	for (const auto& line : blocks_) {
		for (const Block& block : line) {
			if (!block.isActive) continue;
			// 既存の判定関数（CheckSphereVsOBB）を利用
			cg2::CollisionResult hit = cg2::CheckSphereVsOBB(spawnSphere, block.obb);
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

const std::vector<std::vector<Block>>& Stage::GetBlocks() const {
	return blocks_;
}
