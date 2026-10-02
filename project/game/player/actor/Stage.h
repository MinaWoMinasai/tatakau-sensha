#pragma once
#include <string>
#include "Player.h"
#include "PlayerDrone.h"
#include "Enemy.h"
#include "Bullet.h"
#include "MapChip.h"
#include "ExpEnemy.h"

struct MergedBlock {
	cg2::AABB aabb;
	MapChipType type;
};

struct Block {
	cg2::Transform worldTransform;
	std::unique_ptr<cg2::Object3d> object;
	cg2::AABB aabb;
	cg2::OBB obb;
	bool isActive = false;
	bool isLevelObject = false;
	MapChipType type;
	cg2::Vector3 originalPos;
	float orbitAngle = 0.0f;
};

class Stage {
public:
	void Initialize();
	// A validated 45 x 30 CSV replaces the map atomically before block generation.
	// Call between frames; returns false without replacing an invalid map.
	bool LoadRunMap(const std::string& csvPath);
	void Update();
	void Draw();
	void DrawVisible(const cg2::Vector3& cameraPos, float halfWidth, float halfHeight, bool drawNormalBlocks = true);
	void ClearBlocksForPreview();
	bool AddLevelObstacle(const cg2::Transform& transform, const std::string& prefab);
	void ClearLevelObstacles();
	void SetDamageBlockDamage(uint32_t damage) { damageBlockDamage_ = damage; }

	/// <summary>
	/// マップチップの生成
	/// </summary>
	void GenerateBlocks();
	void RebuildMergedBlocks();

    void ResolvePlayerCollision(Player& player, cg2::AxisXYZ axis);
	void ResolvePlayerDroneCollision(PlayerDrone& playerDrone, cg2::AxisXYZ axis);
    void ResolveEnemyCollision(Enemy& enemy, cg2::AxisXYZ axis);
	void ResolveBulletsCollision(const std::vector<Bullet*>& bullets);
	void ResolveExpEnemyCollision(ExpEnemy& enemy, cg2::AxisXYZ axis);
	
	bool IsCollisionWithAnyBlock(const cg2::Vector3& pos, float radius);
	
	void ResolvePlayerCollisionSphere(Player& player);
	
	// Y軸（落下・接地）
	void ResolvePlayerCollisionSphereY(Player& player);

	// X軸（横移動・壁・斜面横成分）
	void ResolvePlayerCollisionSphereX(Player& player);

    const std::vector<MergedBlock>& GetMergedBlocks() const;

	const std::vector<std::vector<Block>>& GetBlocks() const;

private:

	// ブロック用のワールドトランスフォーム
	std::vector<std::vector<Block>> blocks_;

	// マップチップ
	std::unique_ptr<MapChip> mapChip_ = nullptr;

	std::vector<MergedBlock> mergedBlocks_;

	float dt_ = 0;
	uint32_t damageBlockDamage_ = 75;
};
