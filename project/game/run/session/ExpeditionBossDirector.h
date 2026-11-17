#pragma once
#include "game/session/GameWorld.h"
#include "game/enemy/actor/NeonDepthCombat.h"
#include <array>
#include <random>
#include <vector>

namespace gameplay {
/// @brief 遠征のネオンボスの攻撃。見た目・視点・床コアの当たり判定は奥行きボスの仕組みのまま、
/// 攻撃だけを4つ（回転切り・雑魚召喚・ガトリング・追尾弾）に入れ替える。HPが半分を切ると第2形態で強くなる。
class ExpeditionBossDirector {
public:
    explicit ExpeditionBossDirector(GameWorld& world) : world_(world) {}

    /// @brief 部屋の始まり。ボス部屋なら攻撃を引き受け、それ以外なら手を離す。
    void Begin(bool bossRoom);
    /// @brief 戦闘中に毎フレーム呼ぶ。
    void Update(float dt);
    /// @brief 刃・予告の輪・照準線・追尾弾を、ネオンの線として積む。
    void QueueVisuals(const myEngine::Vector3& cameraRight, const myEngine::Vector3& cameraUp);
    /// @brief 見た目のポーズを、今の攻撃に合わせた奥行きボスの段階へ置き換える。置き換えたら真。
    bool OverrideVisual(neondepth::Snapshot& depth) const;

    bool IsActive() const
    {
        return active_;
    }

    bool IsPhaseTwo() const
    {
        return phaseTwo_;
    }

    /// @brief 自動確認用の数。
    struct Stats {
        int spins = 0, summons = 0, gatlings = 0, homings = 0;
        int spinHits = 0, spinContacts = 0, bullets = 0, orbs = 0, orbHits = 0, orbWalls = 0, summoned = 0, summonExpired = 0;
    };

    const Stats& GetStats() const
    {
        return stats_;
    }

    /// @brief 今の攻撃と段階の名前（自動確認の撮影に使う。例：spin_windup）。
    std::string DebugStateName() const;

private:
    enum class Attack {
        None,
        Spin,
        Summon,
        Gatling,
        Homing
    };
    enum class Step {
        Reposition,
        Windup,
        Attack,
        Recovery
    };

    struct Orb {
        myEngine::Vector3 position{};
        myEngine::Vector3 velocity{};
        float life = 0.0f;
        std::array<myEngine::Vector3, 6> trail{};
        float trailTimer = 0.0f;
    };

    struct Summoned {
        ExpEnemy* enemy = nullptr;
        float life = 0.0f;
    };

    /// @brief 床の格子（1マス1.0）。小さい弾用と、ボスの体用の2つを持つ。
    static constexpr int kGridMinX = 21, kGridMinY = 15, kGridW = 46, kGridH = 28;

    void BuildPassability();
    bool Passable(const std::vector<uint8_t>& grid, const myEngine::Vector3& position) const;
    /// @brief 目標までの距離の流れ場を作る（壁を回り込む向きを出すため）。
    void BuildFlow(const std::vector<uint8_t>& grid, const myEngine::Vector3& target, std::vector<int>& flow) const;
    /// @brief 流れ場に沿って目標へ向かう向き。直線で行けるなら目標へまっすぐ。
    myEngine::Vector3 FlowDirection(const std::vector<uint8_t>& grid, const std::vector<int>& flow, const myEngine::Vector3& from,
                                    const myEngine::Vector3& target) const;
    bool StraightClear(const std::vector<uint8_t>& grid, const myEngine::Vector3& from, const myEngine::Vector3& to) const;

    void ChooseAttack();
    void EnterStep(Step step, float duration);
    void MoveBoss(const myEngine::Vector3& direction, float speed, float dt);
    void UpdateSpin(float dt);
    void UpdateGatling(float dt);
    void UpdateHoming(float dt);
    void UpdateSummon();
    void UpdateOrbs(float dt);
    void UpdateSummoned(float dt);
    void FireOrb();
    void ClearAll(bool despawnSummons);
    void QueueFloorCircle(const myEngine::Vector3& center, float radius, float width, const myEngine::Vector4& color, int segments = 48);
    float BossFacing() const;

    GameWorld& world_;
    Stats stats_{};
    std::mt19937 random_{0xb055u};
    bool active_ = false;
    bool phaseTwo_ = false;
    Attack attack_ = Attack::None;
    Attack lastAttack_ = Attack::None;
    Step step_ = Step::Reposition;
    float timer_ = 0.0f;
    float duration_ = 0.0f;
    uint64_t serial_ = 0;
    float clock_ = 0.0f;

    // 位置取り。
    myEngine::Vector3 repositionTarget_{};

    // 回転切り。
    float bladeAngle_ = 0.0f;
    float bladeStart_ = 0.0f;
    float spinRevolutions_ = 1.0f;
    int spinHitRevolution_ = -1;
    float bladeLength_ = 7.0f;

    // ガトリング・追尾弾。
    float facing_ = 0.0f;
    float fireTimer_ = 0.0f;
    int orbsFired_ = 0;
    std::vector<Orb> orbs_;

    // 雑魚召喚。
    float summonCooldown_ = 0.0f;
    std::vector<myEngine::Vector3> summonPoints_;
    std::vector<Summoned> summoned_;

    // 床の格子と流れ場。
    std::vector<uint8_t> smallGrid_, bossGrid_;
    std::vector<int> playerFlow_, bossFlow_;
};
} // namespace gameplay
