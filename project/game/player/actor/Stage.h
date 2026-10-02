#pragma once
#include <string>
#include "Player.h"
#include "PlayerDrone.h"
#include "Enemy.h"
#include "Bullet.h"
#include "MapChip.h"
#include "ExpEnemy.h"

/// @brief 隣接する地形ブロックを統合した範囲を表す。
struct MergedBlock {
    cg2::AABB aabb;
    MapChipType type;
};

/// @brief 地形ブロックの配置・形状・衝突情報を表す。
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

/// @brief ステージの地形と障害物を保持し、移動と弾の衝突を解決する。
class Stage {
public:
    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize();
    // A validated 45 x 30 CSV replaces the map atomically before block generation.
    // Call between frames; returns false without replacing an invalid map.
    /// @brief 遠征マップを読み込む。
    bool LoadRunMap(const std::string& csvPath);
    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    void Update();
    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    void Draw();
    /// @brief 表示中を描画する。
    void DrawVisible(const cg2::Vector3& cameraPos, float halfWidth, float halfHeight, bool drawNormalBlocks = true);
    /// @brief 地形ブロック用のプレビューを消去する。
    void ClearBlocksForPreview();
    /// @brief レベルObstacleを追加する。
    bool AddLevelObstacle(const cg2::Transform& transform, const std::string& prefab);
    /// @brief レベルObstaclesを消去する。
    void ClearLevelObstacles();
    /// @brief ダメージ地形ブロックダメージを設定する。
    void SetDamageBlockDamage(uint32_t damage)
    {
        damageBlockDamage_ = damage;
    }

    /// @brief マップチップの生成
    void GenerateBlocks();
    /// @brief Merged地形ブロックを再構築する。
    void RebuildMergedBlocks();

    /// @brief 自機衝突を解決する。
    void ResolvePlayerCollision(Player& player, cg2::AxisXYZ axis);
    /// @brief 自機ドローン衝突を解決する。
    void ResolvePlayerDroneCollision(PlayerDrone& playerDrone, cg2::AxisXYZ axis);
    /// @brief 敵衝突を解決する。
    void ResolveEnemyCollision(Enemy& enemy, cg2::AxisXYZ axis);
    /// @brief 弾衝突を解決する。
    void ResolveBulletsCollision(const std::vector<Bullet*>& bullets);
    /// @brief 経験値敵衝突を解決する。
    void ResolveExpEnemyCollision(ExpEnemy& enemy, cg2::AxisXYZ axis);

    /// @brief 衝突を含むAny地形ブロックであるか判定する。
    bool IsCollisionWithAnyBlock(const cg2::Vector3& pos, float radius);

    /// @brief 自機衝突球を解決する。
    void ResolvePlayerCollisionSphere(Player& player);

    // Y軸（落下・接地）
    void ResolvePlayerCollisionSphereY(Player& player);

    // X軸（横移動・壁・斜面横成分）
    void ResolvePlayerCollisionSphereX(Player& player);

    /// @brief Merged地形ブロックを返す。
    const std::vector<MergedBlock>& GetMergedBlocks() const;

    /// @brief 地形ブロックを返す。
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
