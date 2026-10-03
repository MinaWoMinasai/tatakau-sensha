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

/// @brief 経験値敵・移動戦闘敵・遠征資源・召喚個体を所有し、生成、更新、死亡個体の削除を管理する。
class EnemyManager {
public:
    /// @brief ワールド座標の矩形出現領域と、生成間隔（秒）・現在の待ち時間・領域内の生存数上限を保持する。
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

    /// @brief 今後生成する個体が利用する自機・弾管理・ボスを借用する。既存個体や出現タイマーは消去しない。
    /// @note 各借用先は参照される間有効に保つ。通常のランダム生成はplayerが必要で、射撃にはbulletManagerが必要。
    void Initialize(Player* player, BulletManager* bulletManager, Enemy* boss);
    /// @brief 出現領域、召喚元の生存確認、各個体の更新・死亡実体の削除、召喚要求の消費を順に行う。
    /// @note 衝突・報酬通知が取得済みポインターを使っている最中には呼ばない。削除でその個体へのポインターが無効になる。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void Update(Stage& stage, float deltaTime);
    /// @brief drawBodyがtrueなら管理個体の通常モデル描画を呼ぶ。
    void Draw(bool drawBody = true);
    /// @brief 各個体のDrawBodyOnlyを呼ぶ。個体ごとのネオン描画方式による除外に従う。
    void DrawBodyOnly();
    /// @brief カメラ中心のXY矩形と各個体の半径が重なる範囲だけDrawBodyOnlyを呼ぶ。距離はワールド単位。
    void DrawBodyOnlyVisible(const cg2::Vector3& cameraPos, float halfWidth, float halfHeight);
    /// @brief 制作カタログまたは互換prefab名を解決し、指定位置に1体生成して所有する。
    /// @param hp 正なら初期HP・最大HPを上書きし、0以下なら種類/制作定義の値を使う。
    /// @return 登録した場合true。未対応prefabなら生成せずfalse。壁や自機との重なり・管理個体数の上限は判定しない。
    bool SpawnLevelEnemy(const cg2::Vector3& position, const std::string& prefab, int hp = -1);
    /// @brief 遠征の制作カタログをコピーして保持し、以後の生成時に使う。既存個体の設定は変えない。
    void SetExpeditionContent(const tankcontent::Catalog& catalog)
    {
        expeditionContent_ = catalog;
        useExpeditionContent_ = true;
    }
    /// @brief 指定位置へ資源を生成して所有し、その個体への借用ポインターを返す。HPは最低1。
    /// @note 死亡フラグが立っても実体削除までアドレスは有効。Updateによる削除またはClearで無効になる。
    ExpEnemy* SpawnRunResource(const cg2::Vector3& position, int hp, std::function<void(bool playerOwned)> onClaim);
    /// @brief 有効な出現領域をコピーして追加する。間隔は最低0.1秒、上限数とXY寸法は最低0にする。
    void AddLevelSpawnArea(const SpawnArea& spawnArea);
    /// @brief 出現領域と全所有個体を破棄し、ランダム出現タイマーを0へ戻す。
    /// @note 取得済みの個体ポインターは無効。カタログ、借用先、ランダム生成の有効設定、共有コールバックは保持する。
    void ClearLevelData();
    /// @brief ClearLevelDataの消去に加え、既定のランダム生成を停止する。
    /// @note 衝突・報酬通知などが取得済みポインターを使い終えた境界で呼ぶ。共有コールバックは保持する。
    void ClearRunActors();
    /// @brief 既定のランダム出現を切り替える。出現領域と召喚の処理はこの設定では停止しない。
    void SetDefaultRandomSpawnEnabled(bool enabled)
    {
        defaultRandomSpawnEnabled_ = enabled;
    }
    /// @brief ボスへの敵対共有設定を更新し、現在の全個体の衝突マスクも再設定する。
    void SetExpEnemyHostileToBoss(bool hostile);
    /// @brief 距離がmaxDistance未満の生存個体から最寄りを借用ポインターで返す。該当なしはnullptr。
    /// @param includeShooters falseではShooterだけでなく全IsCombatThreat対象を除く。資源は検索対象に含む。
    /// @note Updateでその個体を削除した場合、またはClear後は返却ポインターが無効になる。
    ExpEnemy* FindNearestEnemy(const cg2::Vector3& position, float maxDistance, bool includeShooters = true) const;
    /// @brief 距離がmaxDistance未満の生存資源から最寄りを借用ポインターで返す。該当なしはnullptr。
    ExpEnemy* FindNearestRunResource(const cg2::Vector3& position, float maxDistance) const;

    /// @brief 所有個体への借用ポインター一覧を値で返す。死亡済みで削除待ちの個体も含む。
    /// @note 一覧のコピーは所有権を持たない。個体のUpdate削除またはClear後は、その個体へのポインターが無効になる。
    std::vector<ExpEnemy*> GetEnemyPtrs() const;
    /// @brief 管理中の個体数を返す。資源・召喚個体・死亡済みの削除待ちも含む。
    size_t GetEnemyCount() const
    {
        return enemies_.size();
    }

private:
    /// @brief 既定マップ内で、壁と自機付近を避けた位置を最大10回試し、図形またはShooterを1体生成する。
    void Spawn(Stage& stage);
    /// @brief 有効な出現領域の待ち時間を進め、領域内の生存数と管理数の上限内で1体の生成を試す。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateLevelSpawnAreas(Stage& stage, float deltaTime);
    /// @brief 指揮官の召喚要求を消費し、所有者別の生存/累計枠と全体上限内で召喚個体を登録する。
    void UpdateSummonedUnits(Stage& stage);
    /// @brief 生存する召喚指揮官を失った個体へ解除を通知し、死亡フラグを立てる。ここでは実体を削除しない。
    void DismissOrphanedSummons();
    /// @brief XY矩形の境界を含めた領域内の生存個体数を返す。種類を限定せず、資源や召喚個体も含む。
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
