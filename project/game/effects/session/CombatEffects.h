#pragma once
#include "game/session/GameWorld.h"
#include <random>
#include <vector>

namespace gameplay {
/// @brief CombatEffectsの処理を担当し、同じプレイの共有状態を借用する。
class CombatEffects {
public:
    explicit CombatEffects(GameWorld& world) : world_(world) {}

    /// @brief 死亡後処理パルスを発動させる。
    void TriggerDeathPostPulse(const myEngine::Vector3& worldPosition, float strength);

    /// @brief 死亡後処理パルスを更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateDeathPostPulse(float deltaTime);

    /// @brief ネオン三角形粒子を更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateNeonTriangleParticles(float deltaTime);

    /// @brief ネオンの三角形のパーティクルを描画待ちに積む。
    void QueueNeonTriangleParticles(const myEngine::Vector3& cameraRight, const myEngine::Vector3& cameraUp,
                                    const myEngine::Vector3& cameraForward);

    /// @brief 倒れた敵の管の辺を、1本ずつ割れて飛ぶ線にする（計画書 p10 の20章。管の線で描くときだけ）。
    /// @note 衝突処理の途中から呼ばれる。粒子の入れ物へ足すだけにする。
    void SpawnBrokenTube(const ExpEnemy& enemy);

private:
    GameWorld& world_;
    /// @brief 倒れたばかりの敵の場所と種類の色。倒した瞬間の白さで、破片や輪が白くならないようにする。
    struct RecentKill {
        myEngine::Vector3 position{};
        myEngine::Vector4 color{};
        float age = 0.0f;
    };
    std::vector<RecentKill> recentKills_;
    /// @brief 近くで同じ頃に起きた撃破・爆発の数から、生まれた瞬間の強さの割り引きを求める（重なっても白くならないように）。
    float BirthShare(const myEngine::Vector3& position) const;
    /// @brief 割れた辺の速さ・向き・消える時刻の乱数（見た目だけ。遊びの乱数は使わない）。
    std::minstd_rand edgeRandom_{20251125u};
    /// @brief 破片と火花を割合で間引くときの端数（計画書 p10 の16章の M3）。
    float tubeSparkCarry_ = 0.0f;
    float tubeDebrisCarry_ = 0.0f;
};
} // namespace gameplay
