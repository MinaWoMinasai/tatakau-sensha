#pragma once
#include "game/session/GameWorld.h"
#include "game/run/SurvivorSpawnConfig.h"
#include "game/run/session/SurvivorPickups.h"
#include <chrono>
#include <optional>
#include <random>
#include <sstream>

namespace gameplay {
/// @brief 育つ戦車の試作（RunMode::Survivor）の進行を担当し、同じプレイの共有状態を借用する。
/// @note 1区画の流れ（時間で増える湧き方と、3:00 で開く出口）と、経験値の欠片・レベルを持つ。部品は後のカードで足す。
class SurvivorRunController {
public:
    explicit SurvivorRunController(GameWorld& world) : world_(world) {}

    /// @brief 試作の見た目と自機の設定を整え、試作用の部屋と湧き方の設定を読み込み、敵のいないアリーナから始める。
    void Initialize();
    /// @brief 区画の時間を進め、敵を湧かせ、出口を開き、出口に入ったら区画をクリアにする。
    /// @param dt 基準時間（秒）。演出の減速を含まない。
    void Update(float dt);
    /// @brief 自機が敵を倒したときに呼ばれる。
    void OnEnemyDefeated(const myEngine::Vector3& position);
    /// @brief ネオンの格子のパスに、経験値の欠片と出口の輪を積む。
    void QueueWorldVisuals();
    /// @brief 経験値のゲージ・区画の時間・敵の数・出口の案内を描く。
    void DrawUi();

    /// @brief 今のレベル（1から）を返す。
    int GetLevel() const
    {
        return level_;
    }

    /// @brief レベルが上がって貯まった、まだ使っていない選択の数を返す（S7 で部品の3択に使う）。
    int GetPendingChoices() const
    {
        return pendingChoices_;
    }

private:
    /// @brief 生きている敵を種類ごとに数えた結果。
    struct AliveCounts {
        int shapes = 0;
        int tanks = 0;
        int bigPentagons = 0;
        int total = 0;
    };

    AliveCounts CountAlive() const;
    /// @brief 時間に応じて敵を湧かせる。1回の更新で湧かせる数は spawnBudget_ までにする。
    void UpdateSpawns(float dt);
    /// @brief 画面の外側の輪から、壁と柱を避けた位置を探す。見つからなければ空を返す。
    std::optional<myEngine::Vector3> FindSpawnPosition(float minDistance, float maxDistance);
    /// @brief 図形を1体湧かせる。triangleChase が真なら、自機へ寄ってくる三角にする。
    bool SpawnShape(const char* prefab, const myEngine::Vector3& position, bool triangleChase);
    /// @brief 出口を開く。自機から最も遠い候補の位置に置く。
    void OpenExit();
    /// @brief 区画の時間と敵の数の表示を更新する。
    void RefreshHud();
    /// @brief 拾った経験値を足し、必要な分が貯まるたびにレベルを上げる。
    void AddExperience(int amount);

    /// @brief MYENGINE_SURVIVOR_AUTOTEST=1：区画を最後まで遊び、時間ごとの敵の数と fps を記録して終了する。
    void UpdateSectorAutoTest(float dt);
    /// @brief MYENGINE_SURVIVOR_AUTOTEST=tour：部屋の四隅を回って画面を保存し、結果を書いて終了する。
    void UpdateTourAutoTest(float dt);
    /// @brief 自機の近くの点を画面へ写し、照準と同じ方法でゲームの平面へ戻したときのずれ（ワールド単位）を返す。
    float MeasureAimRoundTripError() const;
    /// @brief 画面の保存を要求する（描画で写し、次の更新で書き出す）。
    void RequestCapture(const std::string& name);

    GameWorld& world_;
    survivor::SpawnConfig config_;
    std::mt19937 random_{1};
    float elapsed_ = 0.0f;
    float spawnBudget_ = 0.0f;
    float nextSwarm_ = 0.0f;
    float nextTank_ = 0.0f;
    float nextBigPentagon_ = 0.0f;
    size_t nextTankType_ = 0;
    bool exitOpen_ = false;
    bool cleared_ = false;
    myEngine::Vector3 exitPosition_{};
    SurvivorPickups pickups_;
    int level_ = 1;
    int exp_ = 0;
    int pendingChoices_ = 0;
    int expDropped_ = 0;
    int expCollected_ = 0;
    std::unique_ptr<myEngine::Sprite> expTrack_;
    std::unique_ptr<myEngine::Sprite> expFill_;
    float hudRefresh_ = 0.0f;
    std::unique_ptr<myEngine::TextLabel> hudText_;
    std::unique_ptr<myEngine::TextLabel> exitText_;

    enum class AutoTest {
        None,
        Sector,
        Tour
    };
    AutoTest autoTest_ = AutoTest::None;
    float autoTestTime_ = 0.0f;
    float autoTestLegTime_ = 0.0f;
    size_t autoTestWaypoint_ = 0;
    bool autoTestSettling_ = false;
    bool autoTestCaptureQueued_ = false;
    float autoTestNextSample_ = 0.0f;
    float autoTestClearedTime_ = -1.0f;
    int autoTestNextCapture_ = 0;
    uint64_t drawnFrames_ = 0;
    uint64_t sampleFrames_ = 0;
    std::chrono::steady_clock::time_point sampleStart_{};
    int maxAliveAfterTwoMinutes_ = 0;
    std::ostringstream autoTestLog_;
};
} // namespace gameplay
