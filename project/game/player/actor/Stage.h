#pragma once
#include <string>
#include "Player.h"
#include "PlayerDrone.h"
#include "Enemy.h"
#include "Bullet.h"
#include "MapChip.h"
#include "ExpEnemy.h"

/// @brief 同じマップ行で連続する同種ブロックの統合AABB、またはレベル障害物1個のAABBと種類を保持する。
struct MergedBlock {
    cg2::AABB aabb;
    MapChipType type;
};

/// @brief 地形1個の描画モデルを所有し、姿勢・AABB/OBB・有効状態・種類・元の位置を保持する。
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
    /// @brief resources/map.csvからマップチップを読み込み、描画モデルと衝突形状を生成する。
    void Initialize();
    /// @brief 45列・30行、各セル0～2のCSVを検証してからマップを差し替え、既存ブロックを消去して生成し直す。
    /// @return 読み込み・検証に成功した場合true。失敗時は既存の地形を保持する。
    /// @note フレーム間で呼ぶ。成功すると取得済みのブロック要素へのポインター・参照は無効になる。
    bool LoadRunMap(const std::string& csvPath);
    /// @brief 更新の互換用入口。現在は何もしない。
    void Update();
    /// @brief 現在の状態を描画する。描画先と対応するパイプラインの準備後に呼ぶ。
    void Draw();
    /// @brief カメラを中心とする矩形に1.5ブロック分の余白を加え、元の中心位置が範囲内の有効ブロックを描画する。
    /// @param halfWidth 表示矩形の横半幅（ワールド単位）。
    /// @param halfHeight 表示矩形の縦半幅（ワールド単位）。
    /// @note drawNormalBlocksがfalseなら通常ブロックを除く。形状全体との交差によるカリングではない。
    void DrawVisible(const cg2::Vector3& cameraPos, float halfWidth, float halfHeight, bool drawNormalBlocks = true);
    /// @brief 地形モデル・ブロック配列・統合AABBを消去する。マップチップデータは保持する。
    void ClearBlocksForPreview();
    /// @brief 対応するprefabの障害物モデルと衝突形状を追加する。未対応のprefabならfalseで何も追加しない。
    /// @note 衝突形状は位置とスケールから作る軸平行形状で、transformの回転は反映しない。
    bool AddLevelObstacle(const cg2::Transform& transform, const std::string& prefab);
    /// @brief レベル障害物として追加した行を消去し、残った地形の統合AABBを再構築する。
    void ClearLevelObstacles();
    /// @brief 自機のAABB衝突でDamageへ渡す、ダメージブロックの威力を設定する。
    void SetDamageBlockDamage(uint32_t damage)
    {
        damageBlockDamage_ = damage;
    }

    /// @brief マップチップの通常/ダメージブロックを、行y・列xのモデルと衝突形状へ生成する。
    /// @note 空白セルに既存の有効ブロックがあっても消去しない。地形の差し替えにはLoadRunMapを使う。
    void GenerateBlocks();
    /// @brief 行内で連続する同種の有効ブロックをX方向に統合し、衝突用AABB一覧を作り直す。
    /// @note レベル障害物は1個ずつ登録する。取得済みの統合要素への参照・ポインターは再取得する。
    void RebuildMergedBlocks();

    /// @brief 統合AABBとの接触で、指定したX/Y軸の位置を押し戻して同軸の速度を0にする。
    /// @note 各軸の移動後に呼ぶ。ダメージブロックではDamageも呼び、Y軸でブロックの上側へ押し戻す場合は接地を設定する。
    void ResolvePlayerCollision(Player& player, cg2::AxisXYZ axis);
    /// @brief 統合AABBとの接触で、指定したX/Y軸の位置を押し戻して同軸の速度を0にする。
    /// @note 各軸の移動後に呼ぶ。ダメージブロックでも、この経路はドローンのHPを減らさない。
    void ResolvePlayerDroneCollision(PlayerDrone& playerDrone, cg2::AxisXYZ axis);
    /// @brief ボスの移動後、統合AABBとの接触で指定したX/Y軸の位置だけを押し戻す。速度は変更しない。
    void ResolveEnemyCollision(Enemy& enemy, cg2::AxisXYZ axis);
    /// @brief 生存中の弾の現在位置の球と統合AABBを調べ、壁外の補正位置・XY法線をOnWallImpactへ渡す。
    /// @note nullptr・死亡済みの弾は除く。弾は処理完了まで有効に保つ。移動経路の連続判定は行わない。
    /// 成長弾ルールを使わない弾が壁内に埋まった場合は、壁通知の代わりに死亡させる。
    void ResolveBulletsCollision(const std::vector<Bullet*>& bullets);
    /// @brief 通常敵・資源の移動後、統合AABBとの接触で指定したX/Y軸の位置だけを押し戻す。速度・壁衝突回数は変更しない。
    void ResolveExpEnemyCollision(ExpEnemy& enemy, cg2::AxisXYZ axis);

    /// @brief ワールド座標posと半径radiusの球が、有効な個別ブロックのOBBに接触するかを返す。境界の接触も含む。
    bool IsCollisionWithAnyBlock(const cg2::Vector3& pos, float radius);

    /// @brief 個別ブロックのOBBに対して球を押し戻し、壁へ向かう法線速度を除いて上向き接触の接地を設定する。
    void ResolvePlayerCollisionSphere(Player& player);

    /// @brief 法線が主にY向きのOBB接触だけを処理し、Y位置と内向きY速度を補正する。接地フラグは設定しない。
    void ResolvePlayerCollisionSphereY(Player& player);

    /// @brief OBB接触法線からY成分を除いて球の位置・速度を補正する。XだけでなくZ成分も扱う。
    void ResolvePlayerCollisionSphereX(Player& player);

    /// @brief Stage所有の統合AABB一覧への借用参照を返す。
    /// @note 地形の消去・追加・再構築で内容が変わり、要素への参照・ポインター・イテレーターは無効になり得る。
    const std::vector<MergedBlock>& GetMergedBlocks() const;

    /// @brief Stage所有の個別ブロック配列への借用参照を返す。マップ部分は行y・列x、レベル障害物は後続の1要素行。
    /// @note 地形の消去・生成・障害物追加で要素への参照・ポインター・イテレーターは無効になり得る。
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
