#pragma once
#include "game/session/GameWorld.h"
#include "game/run/SurvivorSave.h"
#include "game/run/SurvivorHeroSave.h"
#include "game/run/SurvivorSpawnConfig.h"
#include "game/run/session/SurvivorAudio.h"
#include "game/run/session/SurvivorLootField.h"
#include "game/run/session/SurvivorPickups.h"
#include "game/ui/session/SurvivorChoiceHud.h"
#include "game/ui/session/SurvivorLoadoutHud.h"
#include "game/ui/session/SurvivorDamageNumbers.h"
#include "game/ui/session/SurvivorItemScreen.h"
#include "game/ui/session/SurvivorWeaponHud.h"
#include <chrono>
#include <optional>
#include <random>
#include <sstream>
#include <unordered_map>

namespace gameplay {
/// @brief 育つ戦車の試作（RunMode::Survivor）の進行を担当し、同じプレイの共有状態を借用する。
/// @note 1区画の流れ（時間で増える湧き方と、3:00 で開く出口）、経験値の欠片・レベル、毎レベルの部品の3択を持つ。
/// 実装は役割ごとに分けてある：中心（SurvivorRunController.cpp）、湧き方・脅威・宿敵（SurvivorRunSpawns.cpp）、
/// 部品と選択（SurvivorRunLoadout.cpp）、結果画面と記録（SurvivorRunResult.cpp）、自動確認（SurvivorRunAutoTest.cpp）。
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

    /// @brief 戦闘の時間に掛ける倍率を返す。3択を止めずに開いている間（choices.pause が false）はスロー（既定0.25）、それ以外は1。
    /// @note CombatFramePipeline が、演出の減速（ジャスト回避など）を掛けた後の時間にさらに掛ける。
    float GetTimeScale() const
    {
        if (cineMode_ == CineMode::RivalDefeat) {
            return 0.3f;
        }
        return choiceOpen_ && !config_.choicePause ? config_.choiceSlowTimeScale : 1.0f;
    }

    /// @brief 選ぶ画面（形態・特性・レリックの2択と、止める設定の3択）を開いているか。
    /// 開いている間はゲームを止める（ArenaRunController::IsTankRunMenuOpen が真を返す）。
    bool IsFormChoiceOpen() const
    {
        return formChoiceOpen_ || traitChoiceOpen_ || relicChoiceOpen_ || IsChoicePausing();
    }

    /// @brief ゲームを止めているか。選ぶ画面の間・調整パネルを「止める」にして開いている間・宿敵の登場の間は真。
    /// @note 止めている間も、スロー演出ではないので画面の色は残す（GameplayQueries・CombatFramePipeline）。
    bool IsPaused() const
    {
        return IsFormChoiceOpen() || inventoryOpen_ || baseScreenOpen_ || restRoomOpen_ || (tuningOpen_ && tuningPause_) || cineMode_ == CineMode::RivalIntro ||
               cineMode_ == CineMode::Evolve;
    }

    /// @brief 部品の3択を開いていて、その間はゲームを止める設定か（遊んだ感想から、読む時間を取るため既定で止める）。
    bool IsChoicePausing() const
    {
        return choiceOpen_ && config_.choicePause;
    }

    /// @brief 演出のカメラ。active なら、CombatFramePipeline が追従カメラの代わりに使う。
    struct CameraDirective {
        bool active = false;
        myEngine::Vector3 focus{};    // 見る床の点
        float distanceScale = 1.0f;   // 通常の距離に対する倍率（小さいほど寄る）
        float tiltDegrees = 0.0f;     // 手前へ傾ける角度（0 で真上から）
    };
    /// @brief 今の演出のカメラ（なめらかにした値）を返す。
    CameraDirective GetCameraDirective() const;
    /// @brief 演出のカメラがあれば、追従カメラの後にカメラの位置と傾きを上書きする。CombatFramePipeline が毎フレーム呼ぶ。
    /// @note 傾けている間は、自機の照準を床の平面とのレイで求める。演出が終わると元の向きに戻す。
    void ApplyCinematicCamera();

    /// @brief 宿敵が倒れたときに CombatFlow から1回呼ばれる。結果画面へは移らず、欠片と宝箱を落として出口を開く。
    void OnRivalDefeated();

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
    /// @brief 脅威の段階を経過時間に合わせ、上がったら知らせる。包囲の輪の時刻もここで見る。
    void UpdateThreat();
    /// @brief 今の段階で、湧かせた敵のHPに掛ける倍率を返す。
    float EnemyHpScale() const;
    /// @brief 最後に湧かせた敵のHPを、今の段階の倍率で上げる。
    void ScaleLastSpawnHp();
    /// @brief 包囲の輪：自機を中心とした輪の上に三角を並べ、寄らせる。
    void SpawnRing(int count);
    /// @brief 自機へ寄ってくる敵（群れ・輪・重い四角・突進する五角）の数を返す。
    int CountChasers() const;
    /// @brief 予告の時刻が来た群れと輪を湧かせる。寄ってくる敵の上限を超える分は減らす。
    void ReleasePendingWaves();
    /// @brief 宿敵を、自機から最も遠い出口の候補の位置に出す。
    void SpawnRival();
    /// @brief 宿敵が戦っているか。
    bool IsRivalActive() const;
    /// @brief 撃破の演出を終えた宿敵を片付け、欠片と宝箱を落として出口を開く。
    void FinishRivalDefeat();
    /// @brief 演出の段階（登場・戦闘・撃破・自機の撃破）を進め、カメラの値をなめらかに目標へ寄せる。dt は基準時間。
    void UpdateCinematic(float dt);
    /// @brief 演出の上下の黒い帯と題字を描く。
    void DrawCinematic();
    /// @brief 敵どうしが重ならないように、近い組を押し離す。格子のマスで絞り、同じマスと隣のマスの組だけを調べる。
    void SeparateEnemies();
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
    /// @brief 区画の始めに、特性の2択を出す（選ぶまで止める）。
    void OpenTraitChoice();
    void UpdateTraitChoiceInput();
    void ChooseTrait(size_t index);
    /// @brief 大五角の宝箱から、まだ持っていないレリックの2択を出す（選ぶまで止める）。1つしか残っていなければ、もう1枚は質の高い選択。
    void OpenRelicChoice();
    void UpdateRelicChoiceInput();
    void ChooseRelic(size_t index);
    std::string DescribeRelic(survivor::RelicId id) const;
    /// @brief 持っているレリックの名前を「・」でつなぐ。無ければ「なし」。
    std::string RelicNames() const;
    /// @brief 今のレベルでの積載量を返す。
    int Capacity() const;
    /// @brief offerIndex 番目の札を積むために、外す部品を選ぶ画面を開く（3択は開いたまま）。
    void OpenRemoval(size_t offerIndex);
    /// @brief 外す画面の入力：数字キーかクリックで外し、Esc でやめる。
    void UpdateRemovalInput();
    /// @brief 部品を外して欠片を落とす。積載が足りたら、待っていた札を積む。
    void RemovePart(survivor::PartId id);
    /// @brief 部品を1つ外し、Lv に応じた欠片を落として記録する。incoming は代わりに積む部品（記録用）。
    void DiscardPart(survivor::PartId id, survivor::PartId incoming);
    /// @brief 部品の Lv と戦意の撃破数から、自機の撃ち方・守りの変化を作る。
    Player::SurvivorPartEffects MakePartEffects() const;
    /// @brief 戦意の撃破数が増えていれば、0.5秒ごとに自機の効果を差し替える。
    void RefreshMorale(float dt);
    /// @brief 緊急放出：一番重い部品を壊して大爆発を起こし、緊急放出も使い切る。使えたら真。
    bool UseEmergencyPurge();
    /// @brief Q の待ち時間を進め、Q キーで Q の部品を使う。
    void UpdateQ(float gameDt);
    /// @brief 積んでいる Q の部品を使う。使えたら真。
    bool UseQ();
    /// @brief 画面の下に出す Q の部品の状態（名前・待ち時間・威力）。Q の部品が無ければ空。
    std::string DescribeQ() const;
    /// @brief 欠片の引き寄せと拾う範囲を、設定から渡し直す。
    void ConfigurePickups();
    /// @brief F2 で調整パネルを開閉して描き、変えた値を自機と欠片に反映する。保存と再読込もここで行う。
    void UpdateTuningPanel();
    /// @brief 結果画面が出ているか（クリアか、倒れた演出の後）。
    bool IsResultVisible() const;
    /// @brief 取った部品を「名前 Lv」の並びにする。perLine 個ごとに改行する。
    std::string DescribeParts(size_t perLine) const;
    /// @brief 特性の札の説明文（設定の倍率から作る）。
    std::string DescribeTrait(survivor::TraitId id) const;
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
    // 敵の押し離しで毎回使う作業用の配列（確保をくり返さないために持っておく）。
    struct SeparationItem {
        ExpEnemy* enemy = nullptr;
        myEngine::Vector3 position{};
        myEngine::Vector3 push{};
        float radius = 0.0f;
        float mass = 1.0f;
        int64_t cell = 0;
    };
    std::vector<SeparationItem> separationItems_;
    std::vector<uint32_t> separationOrder_;
    // 押し離しにかかった時間（200体の fps の確認で記録する）。
    bool separationEnabled_ = true;
    double separationSeconds_ = 0.0;
    uint64_t separationCalls_ = 0;
    // 最小の案内（U3）。それぞれ初めて出てきたときに1回だけ、画面の下に短く出す。
    enum class Hint {
        Move,      // 始め：移動・射撃・ダッシュ
        Choice,    // 選択が貯まった：Tab で3択
        Load,      // 初めて3択を開いた：重さと積載
        Heat,      // 熱が7割を超えた
        Swarm,     // 群れの予告
        Ring,      // 包囲の輪の予告
        QSkill,    // Q の部品を積んだ
        Rival,     // 宿敵が現れた
        Reload,    // 弾倉が初めて空になった（武器の枠を使う遊び方）
        Count
    };
    /// @brief 1つの案内を出す秒数（実時間）。
    static constexpr float kHintSeconds = 5.0f;
    /// @brief 案内を出す順番に並べる。すでに出した案内は何もしない。
    void QueueHint(Hint hint);
    /// @brief 出している案内の時間を進め、終わったら次を出す。dt は基準時間。
    void UpdateHints(float dt);
    std::array<std::unique_ptr<myEngine::TextLabel>, static_cast<size_t>(Hint::Count)> hintTexts_;
    std::unique_ptr<myEngine::Sprite> hintPanel_;
    std::array<bool, static_cast<size_t>(Hint::Count)> hintQueued_{};
    std::vector<Hint> hintQueue_;
    int hintShowing_ = -1;
    float hintTimer_ = 0.0f;
    // 宿敵と自機の撃破と、形態の進化の演出。RivalIntro と Evolve はゲームを止め、RivalDefeat はスローにする。
    enum class CineMode {
        None,
        RivalIntro,
        RivalFight,
        RivalDefeat,
        PlayerDown,
        Evolve
    };
    // 進化の演出の後に戻る演出（宿敵との戦闘中なら RivalFight）と、光を出したか。
    CineMode cineReturnMode_ = CineMode::None;
    bool cineEvolveBurst_ = false;
    // 進化の題字（形態ごと。描画の途中で SetText しないよう、始めに全部作る）。
    std::array<std::unique_ptr<myEngine::TextLabel>, survivor::kFormCount> cineTitleForms_;
    CineMode cineMode_ = CineMode::None;
    float cineTime_ = 0.0f;
    myEngine::Vector3 cineAnchor_{};  // 撃破の演出で見る点（倒れた位置）
    int cineBursts_ = 0;
    // なめらかにしたカメラの値。
    myEngine::Vector3 cineFocus_{};
    float cineDistance_ = 1.0f;
    float cineTilt_ = 0.0f;
    float cineLetterbox_ = 0.0f;
    bool cineFocusReady_ = false;
    bool cameraTilted_ = false;
    myEngine::Vector3 cameraBaseRotation_{};
    std::unique_ptr<myEngine::Sprite> cineBarTop_;
    std::unique_ptr<myEngine::Sprite> cineBarBottom_;
    std::unique_ptr<myEngine::TextLabel> cineTitleIntro_;
    std::unique_ptr<myEngine::TextLabel> cineTitleDefeat_;
    // 脅威の段階と宿敵。
    int threatStage_ = 1;
    float nextRing_ = 0.0f;
    // 予告してから湧く群れと輪。at は湧く区画の時刻。輪の中心は湧く時刻の自機の位置なので center を使わない。
    struct PendingWave {
        float at = 0.0f;
        myEngine::Vector3 center{};
        int count = 0;
        bool ring = false;
    };
    std::vector<PendingWave> pendingWaves_;
    // 予告の「！」（画面の端）。群れの数だけ使う。
    std::array<std::unique_ptr<myEngine::TextLabel>, 4> warningTexts_;
    float nextHeavy_ = 0.0f;
    int heavySpawned_ = 0;
    int lungeSpawned_ = 0;
    int swarmsSpawned_ = 0;
    int wavesSkipped_ = 0;
    int ringsSpawned_ = 0;
    bool rivalSpawned_ = false;
    bool rivalDefeated_ = false;
    float rivalSpawnedAt_ = -1.0f;
    float rivalDefeatedAt_ = -1.0f;
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
    // 積載。積載を超える札を選ぶと、外す部品を選ぶ画面（removalOpen_）を開き、removalOffer_ 番目の札を待たせる。
    SurvivorLoadoutHud loadoutHud_;
    // 敵に当てたときのダメージの数字。
    SurvivorDamageNumbers damageNumbers_;
    /// @brief 設定からダメージの数字の見た目を渡す（始めと、調整パネルで変えたとき）。
    void ApplyDamageNumberConfig();
    bool removalOpen_ = false;
    size_t removalOffer_ = 0;
    int removals_ = 0;
    int maxLoad_ = 0;
    std::ostringstream removalRecord_;
    // 合体した回数と、最後に合体した区画の秒（-1 は合体していない）。
    int fusions_ = 0;
    float fusionTime_ = -1.0f;
    // 戦意：積んでいる間に倒した数と、自機に渡し済みの数。
    int moraleKills_ = 0;
    int moraleApplied_ = 0;
    float moraleRefresh_ = 0.0f;
    // 緊急放出で壊した部品（その区画の間、3択に出さない）。
    std::array<bool, survivor::kPartCount> destroyedParts_{};
    int purges_ = 0;
    // 臨界点：前のフレームまでのオーバーヒートの回数と、爆発した回数。
    int lastOverheats_ = 0;
    int criticalBlasts_ = 0;
    // Q の枠（1つ）。待ち時間は区画の時間で減る。
    float qCooldown_ = 0.0f;
    int qUses_ = 0;
    std::ostringstream qRecord_;
    std::unique_ptr<myEngine::TextLabel> qText_;
    bool formChoiceOpen_ = false;
    // 特性。区画の始めに2択を開き、選ぶまで止める。
    bool traitChoiceOpen_ = false;
    survivor::TraitId trait_ = survivor::TraitId::Standard;
    // レリック。宝箱で貯めた数・2択を開いているか・その札（Count は質の高い選択）・持っているもの・記録。
    int pendingRelics_ = 0;
    bool relicChoiceOpen_ = false;
    std::vector<survivor::RelicId> relicOffers_;
    std::array<bool, survivor::kRelicCount> relics_{};
    std::ostringstream relicRecord_;
    std::unique_ptr<myEngine::TextLabel> relicText_;
    // 次の形態の節目と、鍵付きの進化の鍵の進み具合（画面の下）。更新の中（RefreshEvolveText）だけで書き換える。
    std::unique_ptr<myEngine::TextLabel> evolveText_;
    void RefreshEvolveText();
    /// @brief 3択の札に足す一言（進化の鍵）。札ごとに1つ、無ければ空。
    std::vector<std::string> OfferNotes() const;
    // 形態の段（0：基本砲、1：Lv5 の形態、2：進化した形態）と、今の形態（基本砲は Count）・開いている選択の候補・たどった道。
    int formStage_ = 0;
    survivor::FormId formId_ = survivor::FormId::Count;
    std::vector<survivor::FormId> formOffers_;
    std::string formPath_ = "基本砲";
    // 形態を選んだ区画の秒（第1段・第2段の順）。
    std::vector<float> formTimes_;
    std::string formKey_ = "basic";
    /// @brief 形態を変える（選択の画面を閉じ、段を1つ進める）。
    void EvolveTo(survivor::FormId id);
    /// @brief 形態の数値を自機に入れる（砲塔・倍率・ため撃ち・弾の色）。id は固有の能力を借りる形態（基本砲は Count）。
    void ApplyFormToPlayer(const survivor::FormDefinition& form, survivor::FormId id);

    // 武器（カスタマイズシューティング、計画書 p3 の W0）。config_.weaponsEnabled のときだけ使う。
    /// @brief 武器の枠を使う遊び方か。真なら3択・レベル・形態の選択・特性・レリックを止める。
    bool WeaponMode() const
    {
        return config_.weaponsEnabled;
    }

    // 機体（計画書 p4 の K0）。SurvivorRunHero.cpp。武器の枠の遊び方の上で、枠を主武器1つにし、
    // ホイール・リロード・拠点の画面・落とす物（お金以外）を止め、右クリック・Shift・E・Q を機体の技にする。
    /// @brief 機体の遊び方か。
    bool HeroMode() const
    {
        return config_.weaponsEnabled && config_.heroes.enabled && hero_ >= 0;
    }
    /// @brief 出撃する機体の定義（HeroMode のときだけ呼ぶ）。ショップの技の強化とパークを効かせた後の値（RebuildHero）。
    const survivor::HeroDefinition& Hero() const
    {
        return heroDef_;
    }
    survivor::HeroDefinition heroDef_{};
    const survivor::AbilityDefinition& Ability(survivor::AbilitySlot slot) const
    {
        return Hero().abilities[static_cast<size_t>(slot)];
    }
    /// @brief 設定の start の機体を選び、主武器を1つ目の枠に入れて構える（弾は数えない）。見つからなければ機体を使わない。
    void StartHero();
    /// @brief 技の待ち時間・走る・Q のゲージを、区画の始め（ステージの始め）の状態にする。
    void ResetHeroState();
    /// @brief Shift・E・Q を押した瞬間で技を使う。右クリック（押している間の技）は UpdateHeroAbilities で見る。
    void UpdateHeroInput();
    /// @brief 待ち時間と効いている時間を進め、移動の速さと表示を渡す。dt は区画の時間。
    void UpdateHeroAbilities(float dt);
    /// @brief slot の技を使う。待ち時間中・空き・Q のゲージが足りないなら何もしない。使えたら真。
    bool UseAbility(survivor::AbilitySlot slot);
    /// @brief 与えたダメージ（と K2 からは回復した量）で Q のゲージを溜める。
    void AddUltimateCharge(float amount);
    /// @brief 右クリックの技がバリアで、バリアを張りたいか（自動確認ではボットの指示）。
    bool HeroBarrierWanted() const;
    /// @brief 操作の案内の一行（機体の技の名前から作る）。
    std::string HeroControlsText() const;
    /// @brief 画面の下の一行（技の名前と待ち時間、Q のゲージ）。
    std::string DescribeHeroAbilities() const;
    int hero_ = -1;
    // 技の枠ごとの、次に使えるまでの残りの秒と、使った回数・見た一番長い待ち時間（自動確認と記録）。
    std::array<float, survivor::kAbilitySlotCount> abilityCooldown_{};
    std::array<int, survivor::kAbilitySlotCount> abilityUses_{};
    std::array<float, survivor::kAbilitySlotCount> abilityCooldownSeen_{};
    // 走るの残りの秒と、走った秒の合計。
    float sprintTimer_ = 0.0f;
    float sprintSeconds_ = 0.0f;
    // Q のゲージ（0〜Hero().ultimateCharge）・初めて満タンになった区画の秒・満タンになった回数・満タンの間の脈の時間。
    float ultimateCharge_ = 0.0f;
    float ultimateFullAt_ = -1.0f;
    int ultimateFills_ = 0;
    float ultimatePulse_ = 0.0f;
    /// @brief 回復の場の輪と、弾幕の間の自機のまわりの輪を積む。
    void QueueHeroVisuals(myEngine::NeonGridRenderer& renderer, const myEngine::Vector3& cameraForward) const;
    /// @brief 弾幕：狙う範囲の中の近い敵を順に選んで、決まった間隔で撃つ。dt は区画の時間。
    void UpdateBarrage(float dt, const survivor::AbilityDefinition& ability);
    /// @brief 回復の場：中にいる間、最大HPの割合ずつ回復する。dt は区画の時間。
    void UpdateHealZone(float dt, const survivor::AbilityDefinition& ability);
    /// @brief kind の技を入れている枠の定義（無ければ nullptr）。
    const survivor::AbilityDefinition* FindAbility(survivor::AbilityKind kind) const;
    // 回復の場：置いた位置・残りの秒・端数の回復。弾幕：残りの秒・次の発射までの秒・狙う順番。
    myEngine::Vector3 healZonePosition_{};
    float healZoneTimer_ = 0.0f;
    float healZoneRemainder_ = 0.0f;
    float healZoneAge_ = 0.0f;
    float barrageTimer_ = 0.0f;
    float barrageShotTimer_ = 0.0f;
    size_t barrageTarget_ = 0;
    // 技の記録（自動確認と遊んだ記録）：ロケットの爆発と倒した数、回復した量と場の中にいた秒、弾幕の発射数と秒。
    uint64_t rocketBlasts_ = 0;
    uint64_t rocketKills_ = 0;
    uint64_t lastBulletBlasts_ = 0;
    uint64_t lastBulletBlastKills_ = 0;
    int healedByZone_ = 0;
    float healZoneInsideSeconds_ = 0.0f;
    int barrageShots_ = 0;
    float barrageSeconds_ = 0.0f;

    // モイラ型（計画書 p4 の K2）：吸い付く光線・吸収ゲージで回復・消える移動・跳ねる球・太い光線。
    /// @brief 主武器が技の部品（吸い付く光線）か。真なら主武器の弾は撃たない。
    bool HeroBeamPrimary() const
    {
        return HeroMode() && Hero().primaryAbility.kind == survivor::AbilityKind::DrainBeam;
    }

    /// @brief Q（アルティメット）が効いている間か。間は Q のゲージを溜めない。
    bool UltimateActive() const
    {
        return barrageTimer_ > 0.0f || lanceTimer_ > 0.0f;
    }

    /// @brief 主砲1発の威力に倍率を掛けた、機体の技の威力（毎秒の威力にも使う）。
    float HeroDamageUnit(float scale) const;
    /// @brief 機体の技で、敵（ExpEnemy か宿敵）に直接ダメージを与える。与えた量を返す（倒れていれば0）。
    uint32_t DealHeroDamage(Collider* target, bool rival, uint32_t amount, const myEngine::Vector3& from);
    /// @brief 吸収ゲージを amount 秒ぶん溜める（上限は自分を回復の duration）。
    void AddAbsorb(float amount);
    void UpdateDrainBeam(float dt, const survivor::AbilityDefinition& ability);
    void UpdateSelfHeal(float dt, const survivor::AbilityDefinition& ability);
    void UpdateBounceOrb(float dt, const survivor::AbilityDefinition& ability);
    void UpdateLance(float dt, const survivor::AbilityDefinition& ability);
    // 左クリック・右クリックを押しているか（押している間の技。UpdateHeroInput で見る）。
    bool primaryHeld_ = false;
    bool secondaryHeld_ = false;
    // 吸収ゲージ（秒）。吸い付く光線：吸い付いている敵の id（0 は無し、宿敵は ~0）、次に与えるまでの秒、光線の先の位置。
    float absorb_ = 0.0f;
    uint64_t beamTarget_ = 0;
    float beamTick_ = 0.0f;
    bool beamOn_ = false;
    myEngine::Vector3 beamEnd_{};
    float beamAge_ = 0.0f;
    // 自分を回復：端数の回復と、効いているか（表示）。
    float selfHealRemainder_ = 0.0f;
    bool selfHealOn_ = false;
    // 消える移動の残りの秒。
    float phaseTimer_ = 0.0f;
    // 跳ねる球：残りの秒・位置・速さ（ワールド単位/秒）・次に与えるまでの秒・最後に跳ねた敵の id。
    float orbTimer_ = 0.0f;
    myEngine::Vector3 orbPosition_{};
    myEngine::Vector3 orbVelocity_{};
    float orbTick_ = 0.0f;
    uint64_t orbLastBounce_ = 0;
    // 太い光線：残りの秒・次に与えるまでの秒・効いていた秒。
    float lanceTimer_ = 0.0f;
    float lanceTick_ = 0.0f;
    float lanceAge_ = 0.0f;
    // モイラ型の記録：光線・球・太い光線で与えた量、溜めた吸収ゲージ（秒）、自分を回復した量と押していた秒、球が跳ねた回数、
    // 太い光線が一度に当てた一番多い数、光線が吸い付いていた秒。
    uint64_t drainDamage_ = 0;
    uint64_t orbDamage_ = 0;
    uint64_t lanceDamage_ = 0;
    float absorbGained_ = 0.0f;
    int selfHealed_ = 0;
    float selfHealSeconds_ = 0.0f;
    int orbBounces_ = 0;
    int lanceMaxHits_ = 0;
    float beamLockedSeconds_ = 0.0f;
    // 自動確認のボットが、このフレームに押したことにする技の枠。
    std::array<bool, survivor::kAbilitySlotCount> autoHeroPress_{};
    // 自動確認のボットが、右クリック（押している間の技）を押し続けているか。
    bool autoSecondaryHold_ = false;
    bool autoOrbCaptured_ = false;
    bool autoLanceCaptured_ = false;
    bool autoBeamCaptured_ = false;
    bool autoPhaseCaptured_ = false;
    // 走っている間の画面を保存したか。走っている間とそうでない間に動いた距離と秒（平均の速さを出す。ワールド単位）。
    bool autoHeroCaptured_ = false;
    bool autoHealCaptured_ = false;
    bool autoBarrageCaptured_ = false;
    std::array<float, 2> autoMoveDistance_{};
    std::array<float, 2> autoMoveSeconds_{};
    myEngine::Vector3 autoLastPosition_{};
    /// @brief slot 番目の枠の武器を自機に入れる（すぐに替わる。切り替えの演出と待ちは StartSwap）。今の武器の弾は控えてから替える。
    void EquipWeapon(size_t slot);
    /// @brief ホイール・1〜3キー・R を見る。押した瞬間（ホイールは回した量）で判定する。
    void UpdateWeaponInput();
    /// @brief ホイールを notches 刻み回したときの切り替え（正で次の枠、負で前の枠。空きは飛ばす）。
    void HandleWeaponWheel(int notches);
    /// @brief slot 番目の枠への切り替えを始める。切り替え中・同じ枠・空きなら何もしない。リロード中なら、リロードはやめる。
    void StartSwap(size_t slot);
    /// @brief リロードを始める。弾倉が満タン・予備が無い・弾倉の無い武器・切り替え中なら何もしない。始めたら真。
    bool StartReload();
    /// @brief 切り替えとリロードの時間を進め、撃てない間を自機に渡し、ディゾルブの進みを決める。dt は区画の時間。
    void UpdateWeaponTimers(float dt);
    /// @brief 自機が撃って減らした弾を、今の枠の弾倉（弾倉の無い武器は予備）へ戻す。
    void SyncAmmoFromPlayer();
    /// @brief 今の枠の弾倉（弾倉の無い武器は予備）を、撃てる残りとして自機に渡す。
    void LoadAmmoToPlayer();
    /// @brief 右下の武器の輪に枠の武器を渡す（武器が変わったときだけ）。
    void RefreshWeaponSlots();
    /// @brief 持っている武器の名前を「・」でつなぐ。
    std::string WeaponNames() const;
    /// @brief 構えている枠の武器の値（レア度を効かせたもの）。空きなら nullptr。
    const survivor::WeaponDefinition* ActiveWeapon() const
    {
        return activeSlot_ < weaponSlots_.size() && weaponSlots_[activeSlot_] >= 0 ? &slotDefs_[activeSlot_] : nullptr;
    }
    /// @brief slot 番目の枠の武器の値（レア度を効かせたもの。weaponSlots_ が空きでない枠だけ）。
    const survivor::WeaponDefinition& SlotWeapon(size_t slot) const
    {
        return slotDefs_[slot];
    }
    /// @brief 枠の武器を runSlots_ から入れ直す（weaponSlots_ と、レア度を効かせた slotDefs_）。
    void SyncSlotWeapons();
    // 枠ごとの武器（config_.weapons の番号、-1 は空き）と、使っている枠・切り替えた回数。
    std::array<int, survivor::kWeaponSlots> weaponSlots_{-1, -1, -1};
    size_t activeSlot_ = 0;
    bool weaponEquipped_ = false;
    int weaponSwaps_ = 0;
    // 枠ごとの弾倉の中身と、弾の種類ごとの予備。
    std::array<int, survivor::kWeaponSlots> magazines_{};
    std::array<int, survivor::kAmmoTypeCount> reserves_{};
    // 枠ごとの熱（構えている枠は自機の熱。しまってある枠は撃っていないときの速さで冷えていく）。
    std::array<PlayerHeat::Snapshot, survivor::kWeaponSlots> weaponHeat_{};
    // 切り替え：行き先の枠（-1 は切り替えていない）・経った秒・かかる秒・半分で武器を入れたか。
    int swapTarget_ = -1;
    float swapTimer_ = 0.0f;
    float swapDuration_ = 0.0f;
    bool swapEquipped_ = false;
    // リロード：残りの秒（負ならしていない）・かかる秒・回数。
    float reloadTimer_ = -1.0f;
    float reloadDuration_ = 0.0f;
    int reloads_ = 0;
    SurvivorWeaponHud weaponHud_;

    // 落とす物と出撃中の袋（計画書 p3 の W3）。SurvivorRunLoot.cpp。
    /// @brief 倒した敵から、素材・弾・チップ・武器を確率で落とす（お金は欠片のまま）。衝突処理の途中から呼ばれるので、床の列に足すだけ。
    void DropLoot(const ExpEnemy& enemy);
    /// @brief 床の物を動かして触れた物を拾い、袋の文字を更新する。dt は区画の時間。
    void UpdateLoot(float dt);
    /// @brief 拾った物を、弾なら予備へ、ほかは袋へ入れる。
    void AddToBag(const survivor::LootItem& item);
    /// @brief 武器に付いているチップの一行（「チップ 1/2：弾頭（レア）」）。
    std::string DescribeChips(const survivor::BaseWeapon& owned, const survivor::WeaponDefinition& weapon) const;
    /// @brief 袋の中身の一行（お金・素材・チップ・武器）。
    std::string DescribeBag() const;
    /// @brief 床の物の名前（チップと武器はレア度つき）。
    std::string LootName(const survivor::LootItem& item) const;
    SurvivorLootField lootField_;
    survivor::LootBag bag_;
    // 欠片をお金に換えたときの端数。
    float moneyRemainder_ = 0.0f;
    // 種類ごと（弾・素材・チップ・武器の順）に落とした数と拾った数、種類ごとに拾った弾の数（自動確認と記録）。
    std::array<int, 4> lootDropped_{};
    std::array<int, 4> lootPicked_{};
    std::array<int, survivor::kAmmoTypeCount> ammoPicked_{};
    std::unique_ptr<myEngine::TextLabel> lootText_;
    std::string lootTextShown_;
    // 袋の一行を次に書き換えてよいまでの秒（機体の遊び方。お金が細かく増えるたびに文字の画像を作らない）。
    float lootTextWait_ = 0.0f;

    // E の持ち物の画面（W4）。開いている間は区画を止める（IsPaused）。戦闘中は見るだけ。
    /// @brief 持ち物の画面を開く（文字はここで1回だけ作る）。
    void OpenInventory();
    void CloseInventory();
    // アイコン中心の画面（X4）。SurvivorRunScreens.cpp。持ち物・拠点・休憩部屋で共通に使う。
    /// @brief マスの中身の種類（SurvivorItemScreen::Item::kind）。
    enum class ScreenItem {
        None,
        BaseWeapon,
        BagWeapon,
        BagChip,
        SlotChip,
        Material,
        Recipe,
        TreeNode,
        TreeBranch,
        ShopOffer
    };
    /// @brief 読み込んでおくアイコンの画像の場所。
    std::vector<std::string> ScreenIconPaths() const;
    SurvivorItemScreen::Item MakeChipItem(const survivor::LootBag::Chip& chip, ScreenItem kind, int id, const std::string& actions) const;
    /// @brief 武器の札を埋める。magazine が負なら、弾倉の残りと予備を出さない。removableFrom 番目以降のチップを外せる物にする。
    void FillWeaponCard(SurvivorItemScreen::WeaponCard& card, size_t slot, const survivor::BaseWeapon& owned, const survivor::WeaponDefinition& weapon, int magazine,
                        size_t removableFrom, const std::string& chipActions, const std::string& weaponActions) const;
    /// @brief 上の列（お金・素材・弾）とシールド・補助を埋めた画面の土台。
    SurvivorItemScreen::View BuildScreenBase(const std::array<int, survivor::kMaterialCount>& materials, int money, bool showAmmo) const;
    /// @brief 袋の武器・チップ（actions なら素材も）をマスへ並べる。actions なら動かせる。
    void AddBagToGrid(SurvivorItemScreen::View& view, bool actions) const;
    void RefreshInventoryScreen();
    /// @brief 画面で起きた事象（ドラッグ・クリック・右クリック・ボタン）を実行する。
    void HandleScreenEvent(const SurvivorItemScreen::Event& event);
    /// @brief 開いている画面のマウスとキーを見る（自動確認では何もしない）。
    void UpdateScreenInput();
    /// @brief マウスの状態を画面へ渡し、起きた事象を実行する（自動確認はこれを直接呼ぶ）。
    void DriveScreenPointer(const myEngine::Vector2& mouse, bool leftDown, bool rightTrigger, int wheel);
    /// @brief 休憩部屋で、マウスを合わせている物を捨てる。
    void DiscardHoveredItem();
    SurvivorItemScreen itemScreen_;
    // チップを効かせる前の弾の威力（画面の数字に、構えている武器のチップの分が乗らないように）。
    float baseBulletDamage_ = 1.0f;
    bool inventoryOpen_ = false;
    int inventoryOpens_ = 0;

    // 拠点（W5・G5）。SurvivorRunBase.cpp。持ち帰った物は save/ に1つの自動セーブとして残る。
    /// @brief セーブの場所。自動確認では generated の中の別のファイルを使い、遊んでいる人のセーブに触らない。
    std::string BasePath() const;
    /// @brief 拠点を読む。無ければ（自動確認とタイトルの見本では常に）新しく作る。
    void LoadBase();
    /// @brief 拠点を書く（タイトルの見本では書かない）。
    void SaveBase();
    /// @brief 拠点で装備している武器を枠に入れ、弾倉を満タン・予備を設定の数にして、最初の武器を構える。
    void StartLoadout();
    /// @brief 出撃前の拠点の画面を開く。閉じる（出撃する）までは区画を止める。
    void OpenBaseScreen();
    /// @brief 拠点の weaponIndex 番目の武器を slot 番目の枠へ入れる（ほかの枠に入っていれば入れ替える）。
    void EquipFromBase(int weaponIndex, size_t slot);
    /// @brief recipe 番目のレシピで、素材とお金を払ってコモンの武器を作り、拠点へ足す（W7）。足りなければ何もしない。
    bool CraftWeapon(size_t recipe);
    /// @brief 拠点の index 番目の武器を、派生の木の次の段（レア度を1つ上）へ進化させる（W7）。チップはそのまま。
    bool EvolveBaseWeapon(int index);
    /// @brief 拠点の index 番目の武器を、branch 番目の枝の武器へ変える（W10）。レア度とチップはそのまま。
    bool BranchBaseWeapon(int index, size_t branch);
    /// @brief 出撃する：枠を入れ直し、出撃の回数を足してセーブする。
    void StartSortie();
    /// @brief 拠点の画面の中身を作り直して画面に渡す。
    void RefreshBaseScreen();
    /// @brief 結果画面が出たときに1回だけ：クリアなら袋の中身を拠点へ足し、セーブする（倒れたら袋は失う）。
    void CommitRunToBase();
    survivor::BaseInventory base_;
    bool baseScreenOpen_ = false;

    // 機体の遊び方の拠点（計画書 p4 の K6）：機体を選ぶ・解放する。出撃の外に残るのは解放と記録だけ。SurvivorRunUnlock.cpp。
    std::string HeroSavePath() const;
    /// @brief 機体のセーブを読む（無ければ・自動確認では新しく）。
    void LoadHeroSaveFile();
    void SaveHeroSaveFile();
    /// @brief 拠点の札に並べる機体の番号（使える機体が先、まだの機体は後）。
    std::vector<int> HeroCardOrder() const;
    /// @brief 札の番号の機体を選ぶ。使える機体なら、その機体で出撃の準備をし直す。
    void SelectHeroCard(int card);
    /// @brief 選んでいる札の機体を解放する。候補で、解放ポイントが足りれば解放する。解放したら真。
    bool UnlockHeroCard();
    /// @brief 機体を選ぶ画面を作る（SurvivorRunScreens.cpp）。
    void RefreshHeroSelect();
    /// @brief 結果画面の最初に、スコアと初クリアのボーナスを解放ポイントに足して保存する（1回だけ）。
    void AwardHeroPoints();
    /// @brief 「次の機体まであと○○」の一行。
    std::string NextHeroText() const;
    survivor::HeroSave heroSave_{};
    std::string heroSaveError_;
    int heroCard_ = 0;
    bool heroStartFromEnv_ = false;
    bool heroAwarded_ = false;
    int heroAwardGained_ = 0;
    int heroAwardBonus_ = 0;
    // 自動確認：拠点の画面の段階と、解放の確認の結果。
    int autoHeroBaseState_ = 0;
    bool autoUnlockOk_ = false;
    int baseCursor_ = 0;
    // 右の欄（0 武器・1 作る・2 派生の木）。baseCursor_ は派生の木で見ている武器。作った数・進化させた数（自動確認と記録）。
    int basePage_ = 0;
    int crafted_ = 0;
    int evolved_ = 0;
    bool baseCommitted_ = false;
    bool baseBroughtHome_ = false;
    std::string baseError_;
    // 出撃中の枠の武器（レア度と付けているチップも持つ）。baseIndex は拠点の武器の番号（-1 は袋から付け替えた武器）。
    // baseChips は拠点から付いてきたチップの数（それより後ろが、この出撃で付けたチップ。外せるのはそちらだけ）。
    // 拠点へ書くのはクリアして持ち帰るときだけなので、倒れたら、袋から付けた武器とチップは失い、拠点の武器は元のまま。
    struct RunSlot {
        int baseIndex = -1;
        size_t baseChips = 0;
        survivor::BaseWeapon weapon;
    };
    std::array<RunSlot, survivor::kWeaponSlots> runSlots_{};
    // 枠ごとの、レア度を効かせた武器の値（SyncSlotWeapons で作る）。
    std::array<survivor::WeaponDefinition, survivor::kWeaponSlots> slotDefs_{};

    // 休憩部屋（W6・G6）。SurvivorRunRest.cpp。区画をクリアすると開き、袋の整理・売る・捨てる、袋の武器への付け替え、
    // チップの付け外しをして、「帰る」で結果画面へ（拠点へ持ち帰る）。「次へ進む」は3ステージの流れ（W8）で入る。開いている間は止まる。
    void OpenRestRoom();
    /// @brief 行と画面を作り直す（選んでいる行が見えるように一覧を動かす）。
    void RefreshRestRoom();
    /// @brief 休憩部屋を閉じて結果画面へ（結果画面で拠点へ持ち帰ってセーブする）。
    void LeaveRestRoom();
    /// @brief 袋の bagWeapon 番目の武器を slot 番目の枠へ。枠にあった武器は、袋から付けた物なら袋へ戻し（この出撃で付けたチップも袋へ）、
    /// 拠点の武器なら拠点に置いたまま（この出撃で付けたチップは袋へ）にする。
    bool RestEquip(int bagWeapon, size_t slot);
    /// @brief 袋の bagChip 番目のチップを slot 番目の枠の武器に付ける。チップ枠が空いていなければ何もしない。
    bool RestAttach(int bagChip, size_t slot);
    /// @brief slot 番目の枠の武器から、この出撃で付けた chip 番目のチップを外して袋へ戻す。
    bool RestDetach(size_t slot, size_t chip);
    /// @brief 袋の物を売る（素材は全部）。売った額を返す（売れなければ0）。
    int RestSell(ScreenItem kind, int index);
    /// @brief 袋の物を捨てる（素材は全部）。
    bool RestDiscard(ScreenItem kind, int index);
    /// @brief 枠を変えた後：枠の武器を入れ直し（変えた枠の弾倉は満タン）、構えている武器のチップを効かせ直す。
    void RefreshSlotsAfterChange(size_t changedSlot);
    // 3ステージの流れ（W8）。stage_ は今のステージ（0 ステージ1・1 ステージ2・2 ボスステージ）。
    /// @brief stage 番目のステージの設定（出口の秒・宿敵を出すか）を入れる。
    void ApplyStageSettings(int stage);
    /// @brief 休憩部屋の「次へ進む」：次のステージを、区画の時計・湧き方・出口・床の物を入れ直して始める。HPは戻す。
    void StartNextStage();
    bool IsBossStage() const
    {
        return WeaponMode() && stage_ == StageCount() - 1;
    }
    /// @brief ステージ（機体の遊び方では波）の数。機体の遊び方は波 count 回＋ボスの波（計画書 p4 の H1）。
    int StageCount() const
    {
        return HeroMode() ? config_.waves.count + 1 : survivor::SpawnConfig::kStageCount;
    }
    /// @brief 今のステージ（波）の、脅威の段階の上乗せと敵のHPの倍率。
    int StageThreatOffset() const;
    float StageEnemyHp() const;
    int stage_ = 0;
    // 機体の遊び方の波（計画書 p4 の K3）。SurvivorRunWaves.cpp。
    /// @brief 波の時間が終わった・ボスを倒した：落ちている欠片を拾ったことにし、敵と弾を片付け、波の間の画面（ボスの波なら結果画面）へ。
    void FinishWave();
    /// @brief 倒した敵の種類ごとのレベルポイント（H2）。
    int WavePoints(const ExpEnemy& enemy) const;
    /// @brief 波の間の修理（H8）：傷を全部と HP を治す。レベルポイントが足りなければ何もしない。治したら真。
    bool RepairHero();
    /// @brief 機体の遊び方の体力の決まり（H6・H7）を自機に入れる。
    void ApplyHeroHealthRules();
    // 波ごとの記録（自動確認と遊んだ記録）：稼いだポイント・通った被弾・傷・終わりの HP。
    struct WaveRecord {
        int wave = 0;
        int points = 0;
        uint64_t damage = 0;
        uint64_t capped = 0;
        int wound = 0;
        int hp = 0;
        int maxHp = 0;
        float seconds = 0.0f;
    };
    std::vector<WaveRecord> waveRecords_;
    int wavePoints_ = 0;
    // 出撃で稼いだレベルポイントの合計（使った分も含む。スコアに使う）。
    int pointsEarned_ = 0;
    uint64_t waveDamageStart_ = 0;
    uint64_t waveCappedStart_ = 0;
    int repairs_ = 0;
    int repairSpent_ = 0;

    // 波の間のショップ（計画書 p4 の K4）。SurvivorRunShop.cpp。
    /// @brief 並んでいる物1つ。upgrade なら index は強化の対象（0 主武器、1〜4 技の枠）、そうでなければ survivor::PerkId。
    struct ShopOffer {
        bool upgrade = false;
        int index = 0;
    };
    /// @brief 設定の機体に、技の強化の段とパークを効かせて Hero() を作り直し、主武器・最大HPに入れ直す。heal なら HP を満タンにする。
    void RebuildHero(bool heal);
    /// @brief 並ぶ物を引き直す（買えない物は並べない）。
    void RollShop();
    /// @brief 並んでいる offer 番目を買う。お金が足りなければ何もしない。買えたら真。
    bool BuyOffer(size_t offer);
    /// @brief 引き直す（値段は引くたびに上がる）。お金が足りなければ何もしない。引けたら真。
    bool RerollShop();
    int RerollPrice() const;
    int OfferPrice(const ShopOffer& offer) const;
    std::string OfferName(const ShopOffer& offer) const;
    std::string OfferText(const ShopOffer& offer) const;
    /// @brief 強化の対象の名前（主武器・右クリック…）と、技の部品。
    const survivor::AbilityDefinition& UpgradeAbility(int target) const;
    /// @brief 強化の段（0〜3）。
    int UpgradeTier(int target) const
    {
        return upgradeTiers_[static_cast<size_t>(target)];
    }
    bool HasPerk(survivor::PerkId perk) const
    {
        return HeroMode() && perks_[static_cast<size_t>(perk)];
    }
    float PerkValue(survivor::PerkId perk) const
    {
        return config_.shop.perkValues[static_cast<size_t>(perk)];
    }
    /// @brief 波の間のショップの画面を作る。
    void RefreshShop();
    /// @brief 機体の今の組み立て（左の3枚に技の段とパーク、左下に HP と Q）を入れた画面の土台。ショップと Tab の画面で使う。
    SurvivorItemScreen::View HeroBuildView() const;
    /// @brief 結果画面のスコアの内訳（進んだ波・稼いだポイント・ボス）と合計。
    struct HeroScore {
        int waves = 0;
        int points = 0;
        bool boss = false;
        int total = 0;
    };
    HeroScore ComputeHeroScore() const;
    std::array<int, survivor::kUpgradeTargets> upgradeTiers_{};
    std::array<bool, survivor::kPerkCount> perks_{};
    std::vector<ShopOffer> shopOffers_;
    int shopRerolls_ = 0;
    // ショップの記録：買った物（波:名前:値段）・引き直した回数・使ったポイント。
    std::vector<std::string> shopLog_;
    int shopRerollsTotal_ = 0;
    int shopSpent_ = 0;
    // パークの端数（吸い取る装甲の回復）と、主武器の威力の倍率（強化）・弾の爆発（強化の3段目）。
    float lifestealRemainder_ = 0.0f;
    float heroPrimaryDamage_ = 1.0f;
    float heroPrimaryBlast_ = 0.0f;
    // 吸収ビームの3段目：先から跳ねた2体目の位置（無ければ beamChainOn_ が偽）。跳ねる球の3段目：球の大きさの倍率。
    bool beamChainOn_ = false;
    myEngine::Vector3 beamChainEnd_{};
    float orbScale_ = 1.0f;

    // BGM と効果音。SurvivorRunAudio.cpp。毎フレームの回数の増え方から鳴らし、場面で曲を替える。
    struct AudioCounters {
        uint32_t shots = 0;
        size_t hits = 0;
        int kills = 0;
        uint64_t blasts = 0;
        int hp = 0;
        int reloads = 0;
        bool swapping = false;
        bool barrierOn = false;
        int blocked = 0;
        int breaks = 0;
        int ammoPicked = 0;
        int rarePicked = 0;
        int mines = 0;
        int warps = 0;
        int heals = 0;
        int sales = 0;
        int crafted = 0;
        int evolved = 0;
        int sorties = 0;
        bool cleared = false;
        bool bossSpawned = false;
        bool bossDefeated = false;
        std::array<int, survivor::kAbilitySlotCount> abilityUses{};
        int ultimateFills = 0;
        int barrageShots = 0;
        uint64_t rocketBlasts = 0;
    };
    AudioCounters ReadAudioCounters() const;
    /// @brief 回数の増え方から効果音を鳴らし、場面の曲を流す。dt は実時間（止めている間も曲を合わせる）。
    void UpdateAudio(float dt);
    SurvivorAudio audio_;
    AudioCounters audioCounters_{};
    bool audioCountersReady_ = false;
    /// @brief 設定ファイルへ書く形：ステージが動かす値（出口の秒・宿敵を出すか）を、読んだときの値へ戻したもの。
    survivor::SpawnConfig ConfigForSaving() const
    {
        survivor::SpawnConfig saving = config_;
        saving.exitOpenSeconds = fileExitOpenSeconds_;
        saving.rival.enabled = fileRivalEnabled_;
        return saving;
    }
    float fileExitOpenSeconds_ = 180.0f;
    bool fileRivalEnabled_ = true;

    // ステージ2から出る新しい敵（W9）。SurvivorRunElites.cpp。
    bool ElitesActive() const;
    /// @brief ステージの始めに、湧く時刻を入れ直す。
    void ResetEliteTimers();
    /// @brief 寄ってくる三角を、ばらばらな軌道にする。
    void MakeErratic(ExpEnemy& enemy);
    void SpawnElite(ExpEnemy::SurvivorRole role);
    void ExplodeMine(ExpEnemy& mine);
    /// @brief 新しい敵を湧かせ、役割ごとの行動（回復・ワープ・地雷）をさせる。dt は区画の時間。
    void UpdateElites(float dt);
    /// @brief 回復と爆発の広がる輪、地雷の範囲の輪を積む。
    void QueueEliteRings(myEngine::NeonGridRenderer& renderer, const myEngine::Vector3& cameraForward) const;
    struct EliteRing {
        myEngine::Vector3 position{};
        float age = 0.0f;
        float radius = 1.0f;
        myEngine::Vector4 color{};
    };
    // 回復する敵・ワープする敵・地雷をまく敵の、次に湧く時刻と、初めて出たことを知らせたか。
    std::array<float, 3> nextElite_{};
    std::array<bool, 3> eliteAnnounced_{};
    std::vector<EliteRing> eliteRings_;
    // 行動の回数（自動確認と記録）。
    struct EliteStats {
        int erratic = 0;
        std::array<int, 3> spawned{};
        int heals = 0;
        int healedHp = 0;
        int warps = 0;
        int minesLaid = 0;
        int mineExplosions = 0;
        int mineHits = 0;
    };
    EliteStats eliteStats_{};

    // バリアとスタミナ（X3）。SurvivorRunBarrier.cpp。
    /// @brief スタミナを増減し、wanted で張っている間は敵の弾と敵を止める。dt は区画の時間。
    void UpdateBarrier(float dt, bool wanted);
    /// @brief 位置が（margin の分だけ広げた）扇の中か。
    bool IsInsideBarrier(const myEngine::Vector3& position, float margin) const;
    /// @brief 張っている間、サッカーボールの網目の扇を積む。
    void QueueBarrier(myEngine::NeonGridRenderer& renderer, const myEngine::Vector3& cameraForward) const;
    /// @brief 網目の線の数（自動確認）。
    size_t BarrierNetSegmentCount() const;
    float barrierStamina_ = 0.0f;
    float barrierRegenWait_ = 0.0f;
    float barrierExhausted_ = 0.0f;
    float barrierFlash_ = 0.0f;
    float barrierAge_ = 0.0f;
    bool barrierOn_ = false;
    bool barrierAwaitRelease_ = false;
    // 張った秒・止めた弾・押し返した回数・尽きた回数（自動確認と記録）。
    float barrierSeconds_ = 0.0f;
    int barrierBlocked_ = 0;
    int barrierPushes_ = 0;
    int barrierBreaks_ = 0;
    bool restRoomOpen_ = false;
    // 休憩部屋でしたこと（自動確認と記録）。
    int restEquips_ = 0;
    int restAttaches_ = 0;
    int restDetaches_ = 0;
    int restSales_ = 0;
    int restSoldMoney_ = 0;
    int restDiscards_ = 0;
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
    // 体力の決まりの確認（機体の遊び方）を済ませたかと、その結果。
    bool autoHealthChecked_ = false;
    std::ostringstream autoHealthCheck_;
    // 形態の確認。MYENGINE_SURVIVOR_FORM=twin なら連装、それ以外は長砲身を選ぶ。
    size_t autoFormIndex_ = static_cast<size_t>(survivor::FormId::LongBarrel);
    float autoFormTimer_ = 0.0f;
    bool autoFormCaptured_ = false;
    // 進化（第2段）で選ぶ形態の名前（MYENGINE_SURVIVOR_FORM2）。空か候補に無ければ1枚目。
    std::string autoForm2Key_;
    float autoEvolvedAt_ = -1.0f;
    int autoEvolveCineStage_ = 0;
    int autoEvolveOffers_ = 0;
    float autoFormChosenAt_ = -1.0f;
    float autoFormCaptureAt_ = -1.0f;
    bool autoTourForm_ = false;
    // トゲの確認：0 未実施、2 壊れた、3 壊れなかった、-1 置けなかった。
    int autoSpikeProbe_ = 0;
    bool autoRingCaptured_ = false;
    bool autoRemovalCaptured_ = false;
    // 合体の札の画面を保存したか・合体の後の画面を保存する時刻・合体の前と後の積載。
    bool autoFusionCaptured_ = false;
    float autoFusionShotAt_ = -1.0f;
    std::array<int, 2> autoFusionLoad_{-1, -1};
    // 特性の2択を開いていた秒と、選ぶ札（MYENGINE_SURVIVOR_TRAIT=standard なら標準、ほかは冷却炉）。
    float autoTraitTimer_ = 0.0f;
    size_t autoTraitIndex_ = static_cast<size_t>(survivor::TraitId::CoolingFurnace);
    bool autoTraitCaptured_ = false;
    // レリックの2択を開いていた秒・画面を保存したか。
    float autoRelicTimer_ = 0.0f;
    bool autoRelicCaptured_ = false;
    std::ostringstream autoRelicLog_;
    // 撃破バーストの発動・再発動・子弾の数（区画を出る前の値）と、連鎖崩壊を積んでいた秒・その間の再発動の数。
    std::array<uint64_t, 3> autoBurstStats_{};
    float autoFusedSeconds_ = 0.0f;
    uint64_t autoRepeatsWhileFused_ = 0;
    // 連鎖放電が当たった数（区画を出る前の値）。
    uint64_t autoChainHits_ = 0;
    // 印の爆破と、壁で跳ね返った回数（区画を出る前の値）。
    uint64_t autoDetonations_ = 0;
    uint64_t autoWallBounces_ = 0;
    uint64_t autoPierces_ = 0;
    // Q の画面を保存する時刻。-1 はまだ使っていない、0 は保存済み。
    float autoQCaptureAt_ = -1.0f;
    std::string autoQCaptureName_;
    bool autoPurgeCaptured_ = false;
    // 部品を指定して始めたか（START_PARTS）。真なら、自動確認のボットは緊急放出を使わない。
    bool autoKeepParts_ = false;
    // Q の入れ替えの確認：0 未実施、2 入れ替わった、3 入れ替わらなかった、-1 行わない。
    int autoQSwapProbe_ = 0;
    bool autoSwarmWarningCaptured_ = false;
    bool autoRingWarningCaptured_ = false;
    int autoMaxChasers_ = 0;
    bool autoIntroCaptured_ = false;
    std::array<bool, static_cast<size_t>(Hint::Count)> autoHintCaptured_{};
    bool autoDefeatCaptured_ = false;
    bool autoDownCaptured_ = false;
    // 倒れる確認の時刻（MYENGINE_SURVIVOR_DEATH）。-1 はまだ読んでいない。
    float autoDeathAt_ = -1.0f;
    bool autoRivalCaptured_ = false;
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
    // 武器の確認：枠ごとに使った秒・撃った回数・砲門の数・威力・連射と、画面を保存したか。
    struct AutoWeaponStat {
        float seconds = 0.0f;
        uint32_t attacks = 0;
        size_t barrels = 0;
        float damageScale = 0.0f;
        // 1発のダメージの数字（倍率を掛けて丸めたもの）。
        uint32_t shotDamage = 0;
        float reloadScale = 0.0f;
        uint32_t railShots = 0;
        int overheats = 0;
        uint64_t blasts = 0;
        bool captured = false;
        float captureAt = -1.0f;
    };
    std::array<AutoWeaponStat, survivor::kWeaponSlots> autoWeaponStats_{};
    // 撃てないはずの間（切り替え・リロード・弾倉が空）に撃った回数と、その長さ。弾倉が空のまま続いた秒（予備がある間だけ数える。自動でリロードするので、ほぼ0）。
    uint32_t autoLockedAttacks_ = 0;
    float autoLockedSeconds_ = 0.0f;
    uint32_t autoEmptyAttacks_ = 0;
    float autoEmptySeconds_ = 0.0f;
    float autoEmptyTimer_ = 0.0f;
    // 前のフレームの終わりに撃てない間だったか（このフレームの射撃はその状態で行われる）。
    bool autoWasLocked_ = false;
    // 切り替えを始めてから撃てるようになるまでの実際の秒（枠ごとに最後の値）と、画面を保存したか。
    std::array<float, survivor::kWeaponSlots> autoSwapMeasured_{};
    float autoSwapStartedAt_ = -1.0f;
    bool autoSwapCaptured_ = false;
    bool autoReloadCaptured_ = false;
    // 持ち物の画面の確認：0 まだ・1 開いた・2 画面を頼んだ・3 閉じた。開いた時の区画の時計と、閉じる時に同じだったか。
    int autoInventoryState_ = 0;
    float autoInventoryTimer_ = 0.0f;
    float autoInventoryElapsed_ = 0.0f;
    bool autoInventoryFrozen_ = false;
    // 拠点の画面の確認：0 まだ・1 画面を頼んだ・2 出撃した。枠の入れ替えと戻しが合っていたか。
    // 自動確認でバリアを張りたい間か。止めた弾の画面を保存したか。スタミナの一番低い値。
    bool autoBarrierWanted_ = false;
    bool autoBarrierCaptured_ = false;
    std::array<bool, 5> autoEliteCaptured_{};
    // ワープする敵の画面を保存した時の、自機からの位置（x, y）とディゾルブの進み（z）。
    myEngine::Vector3 autoWarperOffset_{};
    float autoStaminaMin_ = 100.0f;
    int autoBaseState_ = 0;
    bool autoBaseEquipOk_ = false;
    // MYENGINE_SURVIVOR_FULL_RUN=1：3ステージを通す確認（ステージ1・2を短くし、強い持ち物で始め、ボスを倒して帰る）。
    bool autoFullRun_ = false;
    // 作る・進化を1回ずつ試して、払った分・レア度・威力が合っていたか。
    bool autoCraftOk_ = false;
    bool autoBranchOk_ = false;
    bool autoTuningCaptured_ = false;
    // 誘導の武器の弾の向きが、飛んでいる途中で変わった回数。
    std::unordered_map<uint64_t, myEngine::Vector3> autoHomingDirections_;
    int autoHomingTurns_ = 0;
    // 休憩部屋の確認：0 まだ・1 画面を頼んだ・2 操作して画面を頼んだ・3 帰った。チップを付けた武器を構えた時の部品の段の合計。
    int autoRestState_ = 0;
    int autoRestChipLevels_ = 0;
    uint32_t autoWeaponAttacks_ = 0;
    uint32_t autoWeaponRailShots_ = 0;
    int autoWeaponOverheats_ = 0;
    uint64_t autoWeaponBlasts_ = 0;
    bool perfCrowded_ = false;
    std::ostringstream perfLog_;
};
} // namespace gameplay
