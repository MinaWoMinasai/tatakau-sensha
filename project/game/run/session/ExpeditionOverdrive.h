#pragma once
#include "game/session/GameWorld.h"
#include <array>
#include <random>
#include <string>
#include <vector>

namespace myEngine {
struct ScreenEffectState;
}

namespace gameplay {
/// @brief 遠征の戦闘を長く派手にする担当。区画ごとの増援の波（追尾機・ワープ機を含む）、
/// 熱とオーバーヒート、熱を代償にする派手な強化、熱で画面を赤いネオンへ寄せる演出を持つ。
class ExpeditionOverdrive {
public:
    explicit ExpeditionOverdrive(GameWorld& world) : world_(world) {}

    /// @brief 熱のゲージと画面の縁の演出に使うスプライトを作る。
    void Initialize();

    /// @brief 戦闘区画の始まり。増援の波の数を決め、熱を0に戻す。
    /// @param spawnTypes 部屋に置かれた敵の種類（増援の混ぜ物に使う）。
    void BeginRoom(const std::vector<std::string>& spawnTypes, int combatStage, bool elite, bool tutorial, bool boss);

    /// @brief 毎フレームの演出（熱の赤み）を進める。遠征の更新の頭で呼ぶ。
    void UpdatePresentation(float dt);

    /// @brief 戦闘中だけ呼ぶ。増援の波・新しい敵の動き・熱を使う強化を進める。
    void UpdateCombat(float dt);

    /// @brief 自機が敵を倒したとき（誘爆・臨界駆動の衝撃波）。
    void OnEnemyDefeated(const myEngine::Vector3& position);

    /// @brief 増援の波をすべて出し終えたか（区画のクリア条件に使う）。
    bool WavesFinished() const
    {
        return wavesSpawned_ >= wavesTotal_;
    }

    /// @brief 目標の文字に足す、残りの増援の案内（無ければ空）。
    std::string WaveStatus() const;

    /// @brief 熱のゲージと、熱が高いときの赤いネオンの枠を描く（戦闘中のHUD）。
    void Draw();

    /// @brief 画面の後処理へ熱の赤みと、オーバーヒートの乱れを足す。
    void ApplyScreenEffect(myEngine::ScreenEffectState& state) const;

private:
    /// @brief 爆発・吸い寄せの輪の見た目を足す。
    void AddRing(const myEngine::Vector3& position, float radius, float life, const myEngine::Vector4& color);
    /// @brief 画面の揺れを足す（今の揺れより強いときだけ上書き）。
    void Shake(float power, float duration);
    /// @brief 強化に合わせた熱の設定を作り、変わっていれば自機へ入れる。
    void ApplyHeatConfig(bool force);
    /// @brief オーバーヒートした瞬間の演出と、熱核解放。
    void OnOverheat();
    /// @brief 次の増援の波を出す。
    void SpawnWave();
    /// @brief 自機から離れた、壁の無い出現位置を探す。見つからなければ偽。
    bool FindSpawnPosition(float minDistance, myEngine::Vector3& out);
    /// @brief 追尾機・ワープ機の動きと、出現のディゾルブを進める。
    void UpdateEnemyRoles(float dt);
    /// @brief 熱を使う強化（全方位斉射・重力崩壊・臨界駆動）を進める。
    void UpdateUpgrades(float dt);
    bool Has(tankrun::CardId card) const;
    /// @brief 敵が現れた場所に、ワープの光と輪を出す。
    void AnnounceArrival(const myEngine::Vector3& position);

    GameWorld& world_;
    std::mt19937 random_{0x5eedu};

    // 増援の波。
    int wavesTotal_ = 0;
    int wavesSpawned_ = 0;
    int stage_ = 1;
    bool elite_ = false;
    float roomTime_ = 0.0f;
    float sinceWave_ = 0.0f;
    std::vector<std::string> spawnTypes_;
    bool arrivalPending_ = false;

    // 熱と強化。
    PlayerHeat::Config appliedHeat_{};
    bool heatApplied_ = false;
    int lastOverheats_ = 0;
    uint32_t lastAttacks_ = 0;
    uint32_t lastDashes_ = 0;
    int barrageCounter_ = 0;
    float chainBudget_ = 12.0f;
    float selfHitCooldown_ = 0.0f;

    struct Implosion {
        myEngine::Vector3 position{};
        float timer = 0.0f;
    };

    std::vector<Implosion> implosions_;

    // 演出。
    float clock_ = 0.0f;
    float overheatFlash_ = 0.0f;
    float waveFlash_ = 0.0f;
    float heatRatio_ = 0.0f;
    bool overheated_ = false;
    bool combatVisible_ = false;

    // HUD。
    std::unique_ptr<myEngine::Sprite> heatTrack_, heatFill_;
    std::unique_ptr<myEngine::TextLabel> warningText_;
    std::string warningTextValue_;
    /// @brief 画面の縁の赤い光（上・下・左・右の各段）と、ネオンの枠線・角・走査線。
    std::vector<std::unique_ptr<myEngine::Sprite>> edgeGlow_;
    std::array<std::unique_ptr<myEngine::Sprite>, 4> frameLines_;
    std::array<std::unique_ptr<myEngine::Sprite>, 8> cornerMarks_;
    std::array<std::unique_ptr<myEngine::Sprite>, 3> scanLines_;
};
} // namespace gameplay
