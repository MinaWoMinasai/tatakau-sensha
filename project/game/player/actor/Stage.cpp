#include "Stage.h"

void Stage::Initialize() {

	mapChip_ = std::make_unique<MapChip>();
	mapChip_->LoadMapChipCsv("resources/map.csv");
	GenerateBlocks();
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

				block.worldTransform = InitWorldTransform();
				block.worldTransform.translate = mapChip_->GetMapChipPositionByIndex(x, y);
				block.object = std::make_unique<Object3d>();
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

	// ブロック統合処理 
	auto FixAABB = [](AABB& aabb) {
		if (aabb.min.x > aabb.max.x)
			std::swap(aabb.min.x, aabb.max.x);
		if (aabb.min.y > aabb.max.y)
			std::swap(aabb.min.y, aabb.max.y);
		if (aabb.min.z > aabb.max.z)
			std::swap(aabb.min.z, aabb.max.z);
		};

	mergedBlocks_.clear();
	for (uint32_t i = 0; i < numBlockVirtical; ++i) {
		bool merging = false;
		AABB mergedAABB{};
		MapChipType currentType{};

		for (uint32_t j = 0; j < numBlockHorizontal; ++j) {
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

void Stage::ResolvePlayerCollision(Player& player, AxisXYZ axis)
{

	AABB playerAABB = player.GetAABB();
	Vector3 playerPos = player.GetWorldPosition();
	Vector3 velocity = player.GetMove();

	const float kEpsilon = 0.01f;

	for (const MergedBlock& block : mergedBlocks_) {

		if (!IsCollision(playerAABB, block.aabb)) {
			continue;
		}

		// ダメージ床
		if (block.type == MapChipType::kDamageBlock) {
			//if (!enemy.IsDead()) {
			player.Damage();
			//}
		}

		Vector3 overlap = {
			std::min(playerAABB.max.x, block.aabb.max.x) - std::max(playerAABB.min.x, block.aabb.min.x),
			std::min(playerAABB.max.y, block.aabb.max.y) - std::max(playerAABB.min.y, block.aabb.min.y),
			0.0f
		};

		// --------------------
		// X方向衝突
		// --------------------
		if (axis == X) {
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
		if (axis == Y) {
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

void Stage::ResolvePlayerDroneCollision(PlayerDrone& playerDrone, AxisXYZ axis)
{

	AABB playerAABB = playerDrone.GetAABB();
	Vector3 playerPos = playerDrone.GetWorldPosition();
	Vector3 velocity = playerDrone.GetMove();

	const float kEpsilon = 0.01f;

	for (const MergedBlock& block : mergedBlocks_) {

		if (!IsCollision(playerAABB, block.aabb)) {
			continue;
		}

		// ダメージ床
		if (block.type == MapChipType::kDamageBlock) {
			//if (!enemy.IsDead()) {
			//	player.Damage();
			//}
		}

		Vector3 overlap = {
			std::min(playerAABB.max.x, block.aabb.max.x) - std::max(playerAABB.min.x, block.aabb.min.x),
			std::min(playerAABB.max.y, block.aabb.max.y) - std::max(playerAABB.min.y, block.aabb.min.y),
			0.0f
		};

		// --------------------
		// X方向衝突
		// --------------------
		if (axis == X) {
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
		if (axis == Y) {
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

void Stage::ResolveEnemyCollision(Enemy& enemy, AxisXYZ axis)
{
	AABB enemyAABB = enemy.GetAABB();
	Vector3 enemyPos = enemy.GetWorldPosition();
	const float kEpsilon = 0.01f;
	//bool hit = false;

	for (const MergedBlock& block : mergedBlocks_) {

		if (!IsCollision(enemyAABB, block.aabb)) {
			continue;
		}

		//hit = true;

		Vector3 overlap = {
			std::min(enemyAABB.max.x, block.aabb.max.x) - std::max(enemyAABB.min.x, block.aabb.min.x),
			std::min(enemyAABB.max.y, block.aabb.max.y) - std::max(enemyAABB.min.y, block.aabb.min.y),
			0.0f
		};

		if (axis == X) {
			enemyPos.x += (enemyAABB.min.x < block.aabb.min.x)
				? -(overlap.x + kEpsilon)
				: +(overlap.x + kEpsilon);
		} else if (axis == Y) {
			enemyPos.y += (enemyAABB.min.y < block.aabb.min.y)
				? -(overlap.y + kEpsilon)
				: +(overlap.y + kEpsilon);
		}

		enemy.SetWorldPosition(enemyPos);
		enemyAABB = enemy.GetAABB();
	}
}

void Stage::ResolveExpEnemyCollision(ExpEnemy& enemy, AxisXYZ axis)
{
	AABB enemyAABB = enemy.GetAABB();
	Vector3 enemyPos = enemy.GetWorldPosition();
	const float kEpsilon = 0.01f;
	//bool hit = false;

	for (const MergedBlock& block : mergedBlocks_) {

		if (!IsCollision(enemyAABB, block.aabb)) {
			continue;
		}

		//hit = true;

		Vector3 overlap = {
			std::min(enemyAABB.max.x, block.aabb.max.x) - std::max(enemyAABB.min.x, block.aabb.min.x),
			std::min(enemyAABB.max.y, block.aabb.max.y) - std::max(enemyAABB.min.y, block.aabb.min.y),
			0.0f
		};

		if (axis == X) {
			enemyPos.x += (enemyAABB.min.x < block.aabb.min.x)
				? -(overlap.x + kEpsilon)
				: +(overlap.x + kEpsilon);
		} else if (axis == Y) {
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

		Vector3 bulletPos = bullet->GetWorldPosition();
		float radius = bullet->GetRadius();
		Sphere bulletSphere{ bulletPos, radius };

		bool isCollided = false;
		float nearestDist = std::numeric_limits<float>::max();
		Vector3 nearestClosestPoint{};
		Vector3 nearestNormal{};

		for (const auto& block : mergedBlocks_) {

			if (!IsCollision(block.aabb, bulletSphere)) continue;

			Vector3 closestPoint{
				std::clamp(bulletSphere.center.x, block.aabb.min.x, block.aabb.max.x),
				std::clamp(bulletSphere.center.y, block.aabb.min.y, block.aabb.max.y),
				std::clamp(bulletSphere.center.z, block.aabb.min.z, block.aabb.max.z)
			};

			// 差分ベクトルを計算
			Vector3 diff = bulletSphere.center - closestPoint;
			diff.z = 0.0f; // 2D的な処理のためZを無視

			float dist = Length(diff);

			// ★修正ポイント：完全に埋まっている（中心がブロック内）場合
			if (dist < 0.0001f) {
				// 中心が埋まっている＝即死させるのが一番安全
				bullet->Die();

				// もう死んだので計算終了フラグを立ててループを抜ける
				isCollided = false;
				break;
			}

			// ここまで来たら「外側で接している」ので正規化できる
			Vector3 normal = Normalize(diff);

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

			Vector3 vel = bullet->GetMove();
			vel = vel - 2.0f * Dot(vel, nearestNormal) * nearestNormal;

			if (bullet->IsReflectable()) {

				bullet->SetVelocity(vel);
				bullet->SetWorldPosition(bulletPos);
			} else {
				// 反射しない弾は消滅
				bullet->SetWorldPosition(bulletPos);
				bullet->Die();
			}
		}
	}
}

void Stage::ResolvePlayerCollisionSphere(Player& player)
{
	Sphere sphere = player.GetSphere();
	Vector3 pos = sphere.center;
	Vector3 vel = player.GetMove();

	for (const auto& line : blocks_) {
		for (const Block& block : line) {
			if (!block.isActive) continue;

			// ここで Sphere vs OBB
			CollisionResult hit =
				CheckSphereVsOBB(sphere, block.obb);

			if (!hit.hit) continue;

			// 押し戻し
			pos += hit.normal * hit.depth;

			// 法線方向の速度を消す（滑らない）
			float vn = Dot(vel, hit.normal);
			if (vn < 0.0f) {
				vel -= hit.normal * vn;
			}

			// 接地判定
			if (Dot(hit.normal, Vector3(0, 1, 0)) > 0.7f) {
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
	Sphere sphere = player.GetSphere();
	Vector3 pos = sphere.center;
	Vector3 vel = player.GetMove();

	//player.SetOnGround(false);

	for (const auto& line : blocks_) {
		for (const Block& block : line) {
			if (!block.isActive) continue;

			CollisionResult hit = CheckSphereVsOBB(sphere, block.obb);
			if (!hit.hit) continue;

			float upDot = Dot(hit.normal, Vector3(0, 1, 0));

			// ほぼ床 or 天井として扱える場合
			if (upDot > 0.7f || upDot < -0.7f) {

				// 押し戻し（Y成分だけ）
				pos.y += hit.normal.y * hit.depth;

				// Y方向の速度を止める
				if (vel.y * hit.normal.y < 0.0f) {
					vel.y = 0.0f;
				}

				// 接地判定（床のみ）
				//if (upDot > 0.7f) {
				//	player.SetOnGround(true);
				//}

				sphere.center = pos;
			}
		}
	}

	player.SetWorldPosition(pos);
	player.SetVelocity(vel);
}
void Stage::ResolvePlayerCollisionSphereX(Player& player)
{
	Sphere sphere = player.GetSphere();
	Vector3 pos = sphere.center;
	Vector3 vel = player.GetMove();

	for (const auto& line : blocks_) {
		for (const Block& block : line) {
			if (!block.isActive) continue;

			CollisionResult hit = CheckSphereVsOBB(sphere, block.obb);
			if (!hit.hit) continue;

			// 横成分のみ使う
			Vector3 lateralNormal = hit.normal;
			lateralNormal.y = 0.0f;

			float lenSq = Dot(lateralNormal, lateralNormal);
			if (lenSq < 0.0001f) continue;

			lateralNormal = Normalize(lateralNormal);

			// 押し戻し（X方向）
			pos += lateralNormal * hit.depth;

			// 横速度を止める
			float vn = Dot(vel, lateralNormal);
			if (vn < 0.0f) {
				vel -= lateralNormal * vn;
			}

			sphere.center = pos;
		}
	}

	player.SetWorldPosition(pos);
	player.SetVelocity(vel);
}

bool Stage::IsCollisionWithAnyBlock(const Vector3& pos, float radius) {
	Sphere spawnSphere = { pos, radius };
	for (const auto& line : blocks_) {
		for (const Block& block : line) {
			if (!block.isActive) continue;
			// 既存の判定関数（CheckSphereVsOBB）を利用
			CollisionResult hit = CheckSphereVsOBB(spawnSphere, block.obb);
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