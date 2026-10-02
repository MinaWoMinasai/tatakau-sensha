#pragma once
#include <list>
#include <memory>
#include <string>
#include <vector>
#include "ExpEnemy.h"

class Player;
class Stage;
class BulletManager;
class Enemy;

/// @brief 遠征の敵の生成・更新・描画と削除を管理する。
class EnemyManager {
public:
    /// @brief 敵を出現させる領域と出現条件を指定する。
    struct SpawnArea {
        std::string name;
        std::string prefab;
        cg2::Vector3 center;
        cg2::Vector3 size;
        float spawnInterval = 2.0f;
        float timer = 0.0f;
        int maxAlive = 8;
        int hp = -1;
        bool enabled = true;
    };

    /// @brief 使用する資源と初期状態を用意する。呼び出し側で渡した利用先は、その利用期間中有効に保つ。
    void Initialize(Player* player, BulletManager* bulletManager, Enemy* boss);
    /// @brief 現在の状態を1回分進める。初期化後、描画に必要な状態を更新するために呼ぶ。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Update(Stage& stage, float deltaTime);
    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    void Draw(bool drawBody = true);
    /// @brief 機体専用を描画する。
    void DrawBodyOnly();
    /// @brief 機体専用表示中を描画する。
    void DrawBodyOnlyVisible(const cg2::Vector3& cameraPos, float halfWidth, float halfHeight);
    /// @brief レベル敵を出現させる。
    bool SpawnLevelEnemy(const cg2::Vector3& position, const std::string& prefab, int hp = -1);
    /// @brief 遠征Contentを設定する。
    void SetExpeditionContent(const tankcontent::Catalog& catalog)
    {
        expeditionContent_ = catalog;
        useExpeditionContent_ = true;
    }
    // The actor address is stable until the defeated actor is removed by Update.
    /// @brief 遠征リソースを出現させる。
    ExpEnemy* SpawnRunResource(const cg2::Vector3& position, int hp, std::function<void(bool playerOwned)> onClaim);
    /// @brief レベル出現範囲を追加する。
    void AddLevelSpawnArea(const SpawnArea& spawnArea);
    /// @brief レベルデータを消去する。
    void ClearLevelData();
    // Only between frames: existing actor pointers/collision lists are invalidated.
    /// @brief 遠征アクターを消去する。
    void ClearRunActors();
    /// @brief 既定値Random出現有効を設定する。
    void SetDefaultRandomSpawnEnabled(bool enabled)
    {
        defaultRandomSpawnEnabled_ = enabled;
    }
    /// @brief 経験値敵Hostileへのボスを設定する。
    void SetExpEnemyHostileToBoss(bool hostile);
    /// @brief 最寄り敵を検索する。
    ExpEnemy* FindNearestEnemy(const cg2::Vector3& position, float maxDistance, bool includeShooters = true) const;
    /// @brief 最寄り遠征リソースを検索する。
    ExpEnemy* FindNearestRunResource(const cg2::Vector3& position, float maxDistance) const;

    // 衝突判定のためにリストを公開
    std::vector<ExpEnemy*> GetEnemyPtrs() const;
    /// @brief 敵件数を返す。
    size_t GetEnemyCount() const
    {
        return enemies_.size();
    }

private:
    /// @brief 指定条件で敵などの対象を出現させる。
    void Spawn(Stage& stage);
    /// @brief レベル出現Areasを更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateLevelSpawnAreas(Stage& stage, float deltaTime);
    /// @brief 召喚済み単位を更新する。
    void UpdateSummonedUnits(Stage& stage);
    /// @brief OrphanedSummonsを解除する。
    void DismissOrphanedSummons();
    /// @brief 指定範囲内の敵の数を返す。
    int CountEnemiesInArea(const SpawnArea& spawnArea) const;

    std::vector<std::unique_ptr<ExpEnemy>> enemies_;
    tankcontent::Catalog expeditionContent_;
    bool useExpeditionContent_ = false;
    std::vector<SpawnArea> spawnAreas_;
    Player* player_ = nullptr;
    BulletManager* bulletManager_ = nullptr;
    Enemy* boss_ = nullptr;

    float spawnTimer_ = 0.0f;
    bool defaultRandomSpawnEnabled_ = true;
    const float kSpawnInterval = 1.2f; // 短い間隔で図形を散らす
    const int kMaxEnemies = 45;        // 最大数
};
