#pragma once
#include "Player.h"
#include "PlayerDrone.h"
#include "Enemy.h"
#include "Bullet.h"
#include "MapChip.h"

struct MergedBlock {
	AABB aabb;
	MapChipType type;
};

struct Block {
	Transform worldTransform;
	std::unique_ptr<Object3d> object;
	AABB aabb;
	OBB obb;
	bool isActive = false;
	MapChipType type;
	Vector3 originalPos;
	float orbitAngle = 0.0f;
};

class Stage {
public:
    void Initialize();
    void Update();
	void Draw();

	/// <summary>
	/// マップチップの生成
	/// </summary>
	void GenerateBlocks();

    void ResolvePlayerCollision(Player& player, AxisXYZ axis);
	void ResolvePlayerDroneCollision(PlayerDrone& playerDrone, AxisXYZ axis);
    void ResolveEnemyCollision(Enemy& enemy, AxisXYZ axis);
	void ResolveBulletsCollision(const std::vector<Bullet*>& bullets);

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
};
