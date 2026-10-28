#pragma once
#include "game/session/GameWorld.h"
#include "game/run/SurvivorSpawnConfig.h"
#include "game/run/session/SurvivorPickups.h"
#include "game/ui/session/SurvivorChoiceHud.h"
#include <chrono>
#include <optional>
#include <random>
#include <sstream>

namespace gameplay {
/// @brief 育つ戦車の試作（RunMode::Survivor）の進行を担当し、同じプレイの共有状態を借用する。
/// @note 1区画の流れ（時間で増える湧き方と、3:00 で開く出口）、経験値の欠片・レベル、毎レベルの部品の3択を持つ。
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

    /// @brief レベルが上がって貯まった、まだ使っていない選択の数を返す。
    int GetPendingChoices() const
    {
        return pendingChoices_;
    }

    /// @brief 戦闘の時間に掛ける倍率を返す。3択を開いている間はスロー（既定0.25）、それ以外は1。
    /// @note CombatFramePipeline が、演出の減速（ジャスト回避など）を掛けた後の時間にさらに掛ける。
    float GetTimeScale() const
    {
        return choiceOpen_ ? config_.choiceSlowTimeScale : 1.0f;
    }

    /// @brief 形態の2択を開いているか。開いている間はゲームを止める（ArenaRunController::IsTankRunMenuOpen が真を返す）。
    bool IsFormChoiceOpen() const
    {
        return formChoiceOpen_;
    }

    /// @brief ゲームを止めているか。形態を選ぶ間と、調整パネルを「止める」にして開いている間は真。
    /// @note 止めている間も、スロー演出ではないので画面の色は残す（GameplayQueries・CombatFramePipeline）。
    bool IsPaused() const
    {
        return formChoiceOpen_ || (tuningOpen_ && tuningPause_);
    }

    /// @brief 結果画面の文字を作り直す。CombatFlow::UpdateResultText から呼ぶ。
    /// @param selection 0 なら「もう1回」、1 なら「タイトルへ」を選んでいる。
    void RefreshResult(int selection);

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
    /// @brief Tab・1〜3キー・クリックで、3択を開閉して選ぶ。押した瞬間で判定する。
    void UpdateChoiceInput();
    /// @brief 貯まった選択があれば3択を開き、札を引く。
    void OpenChoices();
    /// @brief 3択を閉じる（選択は貯まったまま）。
    void CloseChoices();
    /// @brief index 番目の札を選ぶ。まだ貯まっていれば次の3択を引き、無ければ閉じる。
    void ChooseOffer(size_t index);
    /// @brief 部品の Lv から自機の性能の倍率を作り直して渡す。
    void ApplyParts();
    /// @brief 次の3択を引いて画面に出す。宝箱の選択が貯まっていれば、軽い部品の出ない3択にする。
    void DrawNextOffers();
    /// @brief 衝角の3段：車体に触れた小さなシェイプを壊す。
    void UpdateSpikes();
    /// @brief 形態の2択を開く。部品の3択が開いていれば閉じる（選択は貯まったまま）。
    void OpenFormChoice();
    /// @brief 1・2キーかクリックで形態を選ぶ。選ぶまで閉じない。
    void UpdateFormChoiceInput();
    /// @brief index 番目の形態に変える。
    void ChooseForm(size_t index);
    /// @brief 欠片の引き寄せと拾う範囲を、設定から渡し直す。
    void ConfigurePickups();
    /// @brief F2 で調整パネルを開閉して描き、変えた値を自機と欠片に反映する。保存と再読込もここで行う。
    void UpdateTuningPanel();
    /// @brief 結果画面が出ているか（クリアか、倒れた演出の後）。
    bool IsResultVisible() const;
    /// @brief 取った部品を「名前 Lv」の並びにする。perLine 個ごとに改行する。
    std::string DescribeParts(size_t perLine) const;
    /// @brief 形態の名前を返す。まだ選んでいなければ基本砲。
    std::string FormName() const;
    /// @brief 実時間で5秒ごとに fps を記録する（遊んだ記録に入れる）。
    void SampleFps();
    /// @brief 結果画面に入ったときに1回、遊んだ記録を generated/tank_survivor/runs/ に書く（開発用の構成だけ）。
    void WriteRunLog();

    /// @brief MYENGINE_SURVIVOR_AUTOTEST=1：区画を最後まで遊び、時間ごとの敵の数と fps を記録して終了する。
    void UpdateSectorAutoTest(float dt);
    /// @brief MYENGINE_SURVIVOR_AUTOTEST=tour：部屋の四隅を回って画面を保存し、結果を書いて終了する。
    void UpdateTourAutoTest(float dt);
    /// @brief MYENGINE_SURVIVOR_AUTOTEST=perf：敵を約200体に保ち、散らばった場面と群がった場面の fps を perf.json に書いて終了する。
    void UpdatePerfAutoTest(float dt);
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
    survivor::PartLevels partLevels_{};
    std::vector<survivor::PartOffer> offers_;
    bool choiceOpen_ = false;
    int choicesTaken_ = 0;
    // 大五角の宝箱から得た、質の高い選択。pendingChoices_ にも数える。
    int pendingPremium_ = 0;
    int premiumTotal_ = 0;
    bool premiumGained_ = false;
    bool offersPremium_ = false;
    int spikeKills_ = 0;
    SurvivorChoiceHud choiceHud_;
    bool formChoiceOpen_ = false;
    bool formChosen_ = false;
    std::string formKey_ = "basic";
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

    // 調整パネル（F2）。
    bool tuningOpen_ = false;
    bool tuningPause_ = true;
    std::string tuningStatus_;

    // 結果画面。
    std::unique_ptr<myEngine::Sprite> resultPanel_;
    std::unique_ptr<myEngine::TextLabel> resultText_;
    std::unique_ptr<myEngine::TextLabel> resultMenu_;
    bool runLogWritten_ = false;

    // 遊んだ記録。時刻は区画の時間（秒）。
    std::vector<float> levelTimes_;
    // レベルが上がって選択が貯まった時刻。選ぶたびに古いものから取り出し、選ぶまでの待ちを測る。
    std::vector<float> pendingSince_;
    float choiceOpenSeconds_ = 0.0f;
    std::ostringstream choiceRecord_;
    float heatHighSeconds_ = 0.0f;
    std::chrono::steady_clock::time_point fpsClock_{};
    uint64_t fpsFrames_ = 0;
    std::vector<float> fpsSamples_;

    enum class AutoTest {
        None,
        Sector,
        Tour,
        Perf
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
    float autoChoiceTimer_ = 0.0f;
    bool autoChoiceCaptured_ = false;
    float autoMinTimeScale_ = 1.0f;
    size_t autoPick_ = 0;
    std::ostringstream autoChoiceLog_;
    // 熱とシールドの確認。
    float autoMaxHeat_ = 0.0f;
    uint32_t autoSampleShots_ = 0;
    int autoShieldStep_ = 0;
    std::ostringstream autoShieldLog_;
    // 形態の確認。MYENGINE_SURVIVOR_FORM=twin なら連装、それ以外は長砲身を選ぶ。
    size_t autoFormIndex_ = static_cast<size_t>(survivor::FormId::LongBarrel);
    float autoFormTimer_ = 0.0f;
    bool autoFormCaptured_ = false;
    float autoFormChosenAt_ = -1.0f;
    float autoFormCaptureAt_ = -1.0f;
    bool autoTourForm_ = false;
    // トゲの確認：0 未実施、2 壊れた、3 壊れなかった、-1 置けなかった。
    int autoSpikeProbe_ = 0;
    // 200体の fps の確認。場面ごとに1秒ずつの fps・敵の数・画面の中の数を足し込む。
    struct PerfPhase {
        int samples = 0;
        double fpsSum = 0.0;
        double fpsMin = 1.0e9;
        double aliveSum = 0.0;
        double onScreenSum = 0.0;
    };
    std::array<PerfPhase, 2> perfPhases_{};
    float perfNextSample_ = 1.0f;
    bool perfCrowded_ = false;
    std::ostringstream perfLog_;
};
} // namespace gameplay
