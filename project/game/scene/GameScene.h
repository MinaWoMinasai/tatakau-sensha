#pragma once
#include "game/weapon/CombatTypes.h"
#include "DeveloperTools.h"
#define NOMINMAX
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
#include "game/debug/NeonSkinnedPreview.h"
#include "game/debug/GameplayScenarioSession.h"
#endif
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <vector>
#include <unordered_map>
#include <d3d12.h>
#include <wrl.h>
#include "Audio.h"
#include "debugCamera.h"
#include "Dump.h"
#include "Easing.h"
#include "Resource.h"
#include "Sprite.h"
#include "TextLabel.h"
#include "WinApp.h"
#include "Object3d.h"
#include "Model.h"
#include "ModelManager.h"
#include "SrvManager.h"
#include "CollisionManager.h"
#include "MapChip.h"
#include "Fade.h"
#include "Stage.h"
#include "BulletManager.h"
#include "EnemyManager.h"
#include "IScene.h"
#include "ObjectPostEffect.h"
#include "RingManager.h"
#include "NeonGridRenderer.h"
#include "TrailManager.h"
#include "Skybox.h"
#include "game/level/LevelLoader.h"
#include "game/effects/ScreenEffectDirector.h"
#include "game/render/NeonProjectileRenderer.h"
#include "game/enemy/visual/NeonBossVisual.h"
#include "game/enemy/visual/BossVisualBridge.h"
#include "game/flow/CombatFlowController.h"
#include "game/ui/NeonTextEffect.h"
#include "game/ui/TankRewardCard.h"
#include "game/run/TankRunDirector.h"
#include "game/run/TankExpeditionDirector.h"
#include "game/run/TankExpeditionAudio.h"
#include "game/run/TankExpeditionTutorial.h"
#include "game/run/TankGuidedCombatTutorial.h"
#include "game/run/TankExpeditionMap.h"
#include "game/run/TankExpeditionTransition.h"
#include "game/run/TankExpeditionRooms.h"
#include "game/run/TankExpeditionContent.h"
#include "game/editor/ExpeditionRoomEditor.h"
#include "game/editor/ExpeditionMapEditor.h"
#include "game/editor/ExpeditionContentEditor.h"

// ゲームシーン
/// @brief 通常戦闘・ラン・遠征の進行と表示を接続し、自機・敵・地形の更新順と演出を管理する。
class GameScene : public IScene {

public:
    /// @brief コンストラクタ
    explicit GameScene(bool prototypeRun = false, bool expeditionRun = false);

    /// @brief デストラクタ
    ~GameScene();

    /// @brief 初期化
    void Initialize() override;

    /// @brief シーンの状態に応じて戦闘・演出・進行を更新する。戦闘の呼び出し順は実装内に記載する。
    void Update() override;

    /// @brief 描画
    void Draw() override;

    /// @brief 影を描画する。
    void DrawShadow() override;

    /// @brief 後処理演出3Dを描画する。
    void DrawPostEffect3D() override;
    /// @brief 後の後処理演出3Dを描画する。
    void DrawAfterPostEffect3D() override;

    /// @brief 描画
    void DrawSprite() override;

    /// @brief 終了済みであるか判定する。
    bool IsFinished() const override
    {
        return finished_;
    }

    /// @brief BallOBJ形式を返す。
    cg2::Object3d* GetBallObj()
    {
        return ballObj_.get();
    }

    /// @brief 最終差分時間を返す。
    float GetFinalDeltaTime() const override
    {
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
        if (developerBloomFreeze_ || neonBossDeveloperFreeze_) return 1.0f / 60.0f; // Freeze is a comparison, not the grayscale slow-motion effect.
#endif
        if (IsNeonShowcaseActive()) return 1.0f / 60.0f;
        return finalDeltaTime;
    }
    /// @brief 後処理ガウシアン強度を返す。
    float GetPostGaussianIntensity() const override
    {
        if (IsNeonShowcaseActive()) return 0.0f;
        return sceneFadeBlurIntensity_;
    }
    /// @brief 後処理演出パルスを返す。
    PostEffectPulse GetPostEffectPulse() const override
    {
        if (IsNeonShowcaseActive()) return {};
        return deathPostPulse_;
    }
    /// @brief 画面演出状態を返す。
    ScreenEffectState GetScreenEffectState() const override;
    bool IsNeonShowcaseActive() const {
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
        return neonSkinnedPreview_ && neonSkinnedPreview_->IsShowcaseActive();
#else
        return false;
#endif
    }
    DeveloperShowcaseState GetDeveloperShowcaseState() override;
    void RecordDeveloperPostParameters(const cg2::BloomParam& param) override;
    void RecordDeveloperFrame(cg2::DirectXCommon& dx) override;
    /// @brief 描画区間の計測値を設定する。
    void SetRenderProfile(const IScene::RenderProfile& profile) override;
    /// @brief タイトル背景のデモの進行と表示用の集計値を表す。
    struct TitleDemoStatus {
        int stage = 0;
        float stageSeconds = 0, totalSeconds = 0;
        uint32_t stagesVisitedMask = 0;
        int shots = 0, dashes = 0, kills = 0, rewards = 0, routes = 0, maxPlayerBullets = 0;
    };
    /// @brief タイトルデモを有効にする。
    void EnableTitleDemo()
    {
        titleDemo_ = true;
    }
    /// @brief タイトルデモであるか判定する。
    bool IsTitleDemo() const
    {
        return titleDemo_;
    }
    /// @brief タイトルデモ状態を返す。
    const TitleDemoStatus& GetTitleDemoStatus() const
    {
        return titleDemoStatus_;
    }
    /// @brief タイトルデモFadeを返す。
    float GetTitleDemoFade() const;
    /// @brief タイトルデモ計測を要求を予約する。
    void RequestTitleDemoCapture(const std::string& name);
    /// @brief タイトルデモ計測をコピーする。
    void CopyTitleDemoCapture();
    /// @brief タイトルデモ計測を予約分を処理する。
    void FlushTitleDemoCapture();
    /// @brief タイトルデモ強化ビルドを返す。
    nlohmann::json GetTitleDemoBuild() const;

    /// @brief 次のシーン名前を返す。
    std::string GetNextSceneName() const override;

private:
    /// @brief タイトルデモを初期化する。
    void InitializeTitleDemo();
    /// @brief タイトルデモを更新する。
    /// @param dt この処理で進める経過時間（秒）。
    void UpdateTitleDemo(float dt);
    /// @brief タイトルデモステージを初期状態へ戻す。
    void ResetTitleDemoStage(int stage);
    /// @brief タイトル背景デモの遷移結果を検証する。
    /// @param dt この処理で進める経過時間（秒）。
    void VerifyTitleDemoTransition(float dt);
    float titleDemoTransitionTimer_ = 0;
    bool titleDemoTransitionVerified_ = false;
    bool titleDemo_ = false;
    TitleDemoStatus titleDemoStatus_{};
    bool titleDemoPreviousDash_ = false;
    size_t titleDemoPreviousBulletCount_ = 0;
    float titleDemoRoomFade_ = 0;
    float titleDemoNavigationTimer_ = 0;
    cg2::Vector3 titleDemoMoveTarget_{};
    std::vector<cg2::Vector3> titleDemoPath_;
    /// @brief 戦車遠征部屋形状を現在の状態へ適用する。
    void ApplyTankExpeditionRoomGeometry();
    /// @brief 戦車遠征性能調整を初期化する。
    void InitializeTankExpeditionBalance();
    /// @brief 戦車遠征部屋性能調整を現在の状態へ適用する。
    void ApplyTankExpeditionRoomBalance();
    /// @brief 戦車遠征性能調整編集画面を更新する。
    void UpdateTankExpeditionBalanceEditor();
    /// @brief 戦車遠征性能調整編集画面を描画する。
    void DrawTankExpeditionBalanceEditor();
    /// @brief 遠征制作データHubを更新する。
    void UpdateExpeditionAuthoringHub();
    bool expeditionAuthoringHubOpen_ = false;
    nlohmann::json expeditionPostDraft_;
    nlohmann::json expeditionVisualDraft_;
    std::string expeditionAuthoringStatus_;
    nlohmann::json expeditionStyleBalanceDraft_;
    bool tankExpeditionBalanceEditorOpen_ = false;
    /// @brief 遠征マップを初期化する。
    void InitializeExpeditionMap();
    /// @brief 遠征マップを更新する。
    /// @param dt この処理で進める経過時間（秒）。
    void UpdateExpeditionMap(float dt);
    /// @brief 遠征制作データを更新する。
    void UpdateExpeditionAuthoring();
    /// @brief 遠征マップUIを最新の内容へ更新する。
    void RefreshExpeditionMapUi();
    /// @brief 遠征マップUIを描画する。
    void DrawExpeditionMapUi();
    /// @brief 遠征マップノードを開始する。
    void EnterExpeditionMapNode(const std::string& id);
    /// @brief 遠征マップノードを要求を予約する。
    void RequestExpeditionMapNode(const std::string& id);
    /// @brief 停止を伴う遠征遷移を開始し、反映時点で実行するactionを予約する。既に遷移中なら何もしない。
    void BeginExpeditionPresentation(int action, const std::string& title, const std::string& detail, const cg2::Vector4& color);
    /// @brief 遠征遷移と表示を進め、反映時点で予約済みactionを一度消費する。
    /// @param dt この処理で進める経過時間（秒）。
    void UpdateExpeditionPresentation(float dt);
    /// @brief 遠征画面演出を描画する。
    void DrawExpeditionPresentation();
    /// @brief 遠征修理・整備を選択する。
    void SelectExpeditionService(int option);
    /// @brief 生存中の非ボス戦を完了し、地図選択へ戻す。敵・弾の実体は次の部屋開始時まで保持する。
    void CompleteExpeditionMapCombat();
    /// @brief 制作部屋の地形を読み込み、旧戦闘を消去して自機・敵・資源を配置する。
    /// @return 有効な部屋の地形読み込みと配置処理を終えた場合true。敵1体ごとの生成成功は保証しない。
    /// @note 自機の部屋状態リセットは、遠征成長が有効で生存中の場合にだけ行われる。
    bool StartAuthoredExpeditionRoom();
    /// @brief 遠征修理・整備候補を最新の内容へ更新する。
    void RefreshExpeditionServiceOffers();
    /// @brief 遠征強化ビルドカードを初期化する。
    void InitializeExpeditionBuildCards();
    /// @brief 遠征強化ビルドカードを最新の内容へ更新する。
    void RefreshExpeditionBuildCards();
    /// @brief 遠征強化ビルドカードを更新する。
    /// @param dt この処理で進める経過時間（秒）。
    void UpdateExpeditionBuildCards(float dt);
    /// @brief 遠征強化ビルド外観を選択する。
    void SelectExpeditionBuildStyle(int index);
    /// @brief 遠征強化ビルドカード画面であるか判定する。
    bool IsExpeditionBuildCardScreen() const;
    /// @brief 遠征経験値を初期化する。
    void InitializeExpeditionExperience();
    /// @brief 遠征通貨を出現させる。
    void SpawnExpeditionCredits(const cg2::Vector3& position, int amount, bool flyImmediately = false);
    /// @brief 遠征通貨を更新する。
    /// @param dt この処理で進める経過時間（秒）。
    void UpdateExpeditionCredits(float dt, bool collectAll = false);
    /// @brief 遠征通貨を描画する。
    void DrawExpeditionCredits();
    /// @brief 遠征Vitalsを描画する。
    void DrawExpeditionVitals();
    /// @brief ガイド付き遠征を更新する。
    /// @param dt この処理で進める経過時間（秒）。
    void UpdateGuidedExpedition(float dt);
    /// @brief 遠征ガイドの現在の説明を確認済みにして次へ進む。
    void AcknowledgeGuidedExpedition();
    /// @brief ガイド付き遠征を描画する。
    void DrawGuidedExpedition();
    /// @brief ガイド付き遠征UIを最新の内容へ更新する。
    void RefreshGuidedExpeditionUi();
    /// @brief ガイド付き遠征Pausedであるか判定する。
    bool IsGuidedExpeditionPaused() const;
    /// @brief 遠征衝撃を後で処理するために予約する。
    void QueueExpeditionImpact(const cg2::Vector3& position, const cg2::Vector3& direction, bool finisher);
    /// @brief 遠征ポインターを描画する。
    void DrawExpeditionPointer(cg2::Vector2 target, bool right = true);
    /// @brief 通貨Iconを描画する。
    void DrawCurrencyIcon(cg2::Vector2 center, float size = 30);
    /// @brief Intro遠征修理・整備であるか判定する。
    bool IsIntroExpeditionService() const;
    /// @brief 遠征の修理や整備に必要な価格を計算する。
    int ExpeditionServicePrice(const std::string& id) const;
    /// @brief 遠征Blueprintを設定する。
    void SetExpeditionBlueprint(int index);
    /// @brief 遠征マップ検証を更新する。
    /// @param dt この処理で進める経過時間（秒）。
    void UpdateExpeditionMapValidation(float dt);
    /// @brief 戦闘検証Fixtureを初期化する。
    void InitializeCombatValidationFixture();
    /// @brief 戦闘検証を更新する。
    /// @param dt この処理で進める経過時間（秒）。
    bool UpdateCombatValidation(float dt);
    /// @brief 戦闘検証検証用を開始する。
    void BeginCombatValidationProbe(int index);
    /// @brief 戦闘検証検証用を終了する。
    void FinishCombatValidationProbe();
    /// @brief 戦闘検証Reportを書き込む。
    void WriteCombatValidationReport(bool completed);
    /// @brief 戦闘検証を記録する。
    void CaptureCombatValidation(const std::string& name);
    bool combatValidationEnabled_ = false, combatValidationRequested_ = false, combatValidationPhase2Injected_ = false;
    float combatValidationElapsed_ = 0;
    int combatValidationIndex_ = -1;
    /// @brief 戦闘検証時の開始条件と観測結果を保持する。
    struct CombatValidationProbe {
        std::string id;
        float age = 0, pathLength = 0, maxStep = 0, reloadSeconds = 0, sampleAge = 0;
        cg2::Vector3 previousPosition{};
        bool hasPrevious = false, previousReload = false, phase2 = false;
        unsigned shots = 0, dashes = 0, reloads = 0, phases = 0, patterns = 0;
        int previousAmmo = 0, reloadViolations = 0, wallIntersections = 0, tunnelingViolations = 0, playerBulletSamples = 0;
        size_t projectileSamples = 0, maxProjectiles = 0;
        nlohmann::json trace = nlohmann::json::array();
    } combatValidationProbe_;
    nlohmann::json combatValidationResults_ = nlohmann::json::array();
    std::vector<std::string> combatValidationErrors_, combatValidationCaptures_;
    bool expeditionMapEnabled_ = false;
    bool expeditionRoomEditorOpen_ = false, expeditionMapEditorOpen_ = false, expeditionContentEditorOpen_ = false;
    tankexp::MapDefinition expeditionMapDefinition_;
    tankexp::ExpeditionMapRun expeditionMapRun_;
    tankexp::RoomCatalog expeditionRooms_;
    tankcontent::Catalog expeditionContent_;
    tankexp::ExpeditionRoomEditor expeditionRoomEditor_;
    tankexp::MapEditor expeditionMapEditor_;
    tankcontent::ContentEditor expeditionContentEditor_;
    std::string expeditionMapSelection_, expeditionMapStatus_;
    std::vector<std::string> expeditionServiceOffers_;
    std::vector<std::string> expeditionIntroOffers_;
    uint32_t expeditionSeed_ = 0;
    std::unordered_map<std::string, int> expeditionPurchases_;
    // Retain purchased effects if their source is renamed/deleted while authoring.
    std::unordered_map<std::string, tankcontent::Upgrade> expeditionPurchasedModules_;
    /// @brief 遠征の強化効果の強度を表示・計算用にまとめる。
    std::array<float, tankrun::CardCount> ExpeditionEffectPowers() const;
    int expeditionServicePage_ = 0, expeditionBlueprint_ = 0;
    float expeditionMapScroll_ = 0;
    bool expeditionMapPreview_ = false;
    bool expeditionBuildChoice_ = false, expeditionBuildChosen_ = false;
    bool specialValidationEnabled_ = false;
    nlohmann::json specialValidation_;
    /// @brief 特殊検証Fixtureを初期化する。
    void InitializeSpecialValidationFixture();
    /// @brief 特殊検証を更新する。
    /// @param dt この処理で進める経過時間（秒）。
    bool UpdateSpecialValidation(float dt);
    tankbuild::Style expeditionBuildStyle_ = tankbuild::Style::Shooter;
    int expeditionPendingBuild_ = -1;
    std::array<std::unique_ptr<TankRewardCard>, 3> expeditionRewardCards_;
    tankexp::PresentationTransition expeditionTransition_;
    int expeditionTransitionAction_ = 0;
    int expeditionPendingService_ = -1;
    std::string expeditionPendingNode_;
    cg2::Vector4 expeditionTransitionColor_{0.3f, 0.9f, 1, 1};
    std::unique_ptr<cg2::Sprite> expeditionCurtain_, expeditionTransitionPanel_, expeditionTransitionRail_, expeditionTransitionProgress_;
    std::unique_ptr<cg2::TextLabel> expeditionTransitionTitle_, expeditionTransitionDetail_;
    float expeditionPresentationClock_ = 0, expeditionUiErrorAge_ = 0;
    /// @brief 遠征の命中火花の位置・色・残り時間を保持する。
    struct ExpeditionHitSpark {
        cg2::Vector3 position{}, direction{};
        float age = 0;
    };
    std::vector<ExpeditionHitSpark> expeditionHitSparks_;
    float expeditionHitSparkCooldown_ = 0;
    bool expeditionBossPhase2Seen_ = false;
    std::string expeditionLastFocus_;
    std::array<float, 3> expeditionCardFocus_{};
    /// @brief 遠征ルートの1ノードの描画資源と表示状態を保持する。
    struct MapNodeVisual {
        cg2::Vector2 center{};
        float focus = 0;
        std::unique_ptr<cg2::Sprite> halo, rim, fill;
        std::unique_ptr<cg2::TextLabel> icon, label, state;
    };
    /// @brief 遠征ルートの1接続の描画資源と表示状態を保持する。
    struct MapEdgeVisual {
        std::string from, to;
        std::unique_ptr<cg2::Sprite> glow, line, pulse;
    };
    std::vector<MapNodeVisual> expeditionMapVisuals_;
    std::vector<MapEdgeVisual> expeditionMapEdges_;
    std::vector<std::unique_ptr<cg2::Sprite>> expeditionMapGrid_;
    std::unique_ptr<cg2::TextLabel> expeditionMapTitle_, expeditionMapSubtitle_, expeditionMapInfo_, expeditionMapLegend_,
        expeditionMapHelp_;
    std::array<std::unique_ptr<cg2::TextLabel>, 3> expeditionBlueprintLabels_;
    std::array<std::unique_ptr<cg2::Sprite>, 3> expeditionBlueprintButtons_;
    /// @brief 遠征で回収する通貨の位置・移動・回収状態を保持する。
    struct ExpeditionCreditOrb {
        cg2::Vector3 position{}, velocity{};
        cg2::Vector2 launch{};
        float age = 0, flight = 0;
        int value = 0;
        bool flying = false;
        std::unique_ptr<cg2::Sprite> sprite;
    };
    std::vector<ExpeditionCreditOrb> expeditionCredits_;
    std::unique_ptr<cg2::Sprite> expeditionCreditIcon_, expeditionCreditPulse_, expeditionStaminaTrack_, expeditionStaminaFill_;
    std::unique_ptr<cg2::TextLabel> expeditionCreditText_;
    std::unique_ptr<NeonTextEffect> expeditionCompleteGlow_;
    std::array<std::unique_ptr<cg2::Sprite>, 4> expeditionSpotlight_;
    std::array<std::unique_ptr<cg2::Sprite>, 24> expeditionPointer_, expeditionPointerGlow_;
    std::unique_ptr<cg2::Sprite> expeditionPriceIcon_;
    size_t expeditionPointerCursor_ = 0;
    /// @brief 遠征Pointersを必要な状態を用意する。
    void EnsureExpeditionPointers(size_t count);
    std::unique_ptr<cg2::Sprite> expeditionContinueButton_, expeditionSkipButton_;
    std::unique_ptr<cg2::TextLabel> expeditionContinueText_, expeditionSkipText_;
    float expeditionCreditPulseAge_ = 0, expeditionImpactHold_ = 0;
    int expeditionCreditsCollected_ = 0;
    bool expeditionCollectAll_ = false, expeditionClearRewardQueued_ = false;
    bool expeditionGuideActive_ = false, expeditionGuideShooterSpawned_ = false;
    tankexp::GuidedCombatTutorial expeditionGuide_;
    int expeditionGuideLastKills_ = 0;
    uint32_t expeditionGuideDamageCount_ = 0;
    uint32_t expeditionGuideAttackCount_ = 0;
    float expeditionGuideAge_ = 0;
    bool expeditionMapAutoTest_ = false;
    float expeditionMapTestElapsed_ = 0, expeditionMapTestAge_ = 0;
    std::string expeditionMapTestState_;
    std::vector<std::string> expeditionMapTestVisited_;
    int expeditionMapTestPurchases_ = 0, expeditionMapTestHeals_ = 0, expeditionMapTestEvolutions_ = 0;
    nlohmann::json tankExpeditionBalance_;
    /// @brief 戦車遠征チュートリアルを更新する。
    /// @param dt この処理で進める経過時間（秒）。
    void UpdateTankExpeditionTutorial(float dt);
    /// @brief 遠征チュートリアルCompletionを保存する。
    void SaveExpeditionTutorialCompletion();
    /// @brief 戦車遠征チュートリアル検証を更新する。
    /// @param dt この処理で進める経過時間（秒）。
    void UpdateTankExpeditionTutorialValidation(float dt);
    tankexp::TutorialValidationState tankExpeditionTutorialValidation_{};
    /// @brief 戦車遠征チュートリアルを描画する。
    void DrawTankExpeditionTutorial();
    /// @brief 戦車遠征チュートリアルUIを最新の内容へ更新する。
    void RefreshTankExpeditionTutorialUi();
    tankexp::ExpeditionTutorial tankExpeditionTutorial_{};
    bool tankExpeditionTutorialSaved_ = false;
    bool expeditionTutorialPreviouslyCompleted_ = false;
    bool tankExpeditionDetailsOpen_ = false;
    int tankExpeditionTutorialKills_ = 0;
    cg2::Vector3 tankExpeditionTutorialPrevious_{};
    std::unique_ptr<cg2::Sprite> tankExpeditionHpTrack_, tankExpeditionHpFill_;
    std::unique_ptr<cg2::Sprite> tankExpeditionExpTrack_, tankExpeditionExpFill_, tankExpeditionBuildPanel_;
    std::unique_ptr<cg2::TextLabel> tankExpeditionExpText_, tankExpeditionDetailsText_;
    /// @brief 戦車遠征を初期化する。
    void InitializeTankExpedition();
    /// @brief 戦車遠征を更新する。
    /// @param dt この処理で進める経過時間（秒）。
    void UpdateTankExpedition(float dt);
    /// @brief 戦闘フェーズなら旧部屋の敵・弾・場の攻撃を消去し、新しい部屋を配置する。
    /// @note 地形の読み込み失敗は診断出力だけで、その後の初期化・配置は続ける。
    /// 自機の部屋状態リセットは、遠征成長が有効で生存中の場合にだけ行われる。
    void StartTankExpeditionRoom();
    /// @brief 生存中の非ボス戦を報酬/進路選択へ移す。マップ式では対応する完了処理へ委譲する。
    void FinishTankExpeditionRoom();
    /// @brief 戦車遠征選択肢を選択する。
    void SelectTankExpeditionOption(int index);
    /// @brief 戦車遠征UIを最新の内容へ更新する。
    void RefreshTankExpeditionUi();
    /// @brief 戦車遠征UIを描画する。
    void DrawTankExpeditionUi();
    /// @brief 戦車遠征音声を更新する。
    /// @param dt この処理で進める経過時間（秒）。
    void UpdateTankExpeditionAudio(float dt);
    /// @brief 遠征以外、または遠征でライバルが有効な場合true。死亡判定は別に必要。
    bool IsRunRivalActive() const
    {
        return !expeditionRun_ || tankExpeditionRivalActive_;
    }
    /// @brief 戦車遠征選択肢件数を返す。
    int GetTankExpeditionOptionCount() const;
    bool expeditionRun_ = false;
    tankexp::ExpeditionDirector tankExpedition_{};
    bool tankExpeditionRivalActive_ = false;
    bool tankExpeditionRoomPending_ = false;
    bool tankExpeditionResourceWon_ = false;
    bool tankExpeditionRewardOpen_ = false;
    bool tankExpeditionMaintenanceOpen_ = false;
    bool tankExpeditionResourceReleased_ = false;
    int tankExpeditionAutoMaintainedRoom_ = -1;
    int tankExpeditionValidationErrors_ = 0;
    bool tankExpeditionMusicEnabled_ = true;
    bool tankExpeditionEffectsEnabled_ = true;
    TankExpeditionAudio tankExpeditionAudio_;
    std::unordered_map<const ExpEnemy*, int> tankExpeditionEnemyHp_;
    std::unordered_map<const ExpEnemy*, bool> tankExpeditionEnemyWarning_;
    std::unordered_map<const ExpEnemy*, std::pair<uint64_t, uint64_t>> guardAudioCounts_;
    int tankExpeditionNodes_ = 0;
    int tankExpeditionSpawned_ = 0;
    int tankExpeditionAutoVariant_ = 0;
    int tankExpeditionCaptureIndex_ = 0;
    float tankExpeditionArrival_ = 0;
    std::vector<RunEvolutionChoice> tankExpeditionEvolutions_;
    std::unique_ptr<cg2::TextLabel> tankExpeditionMapText_;
    std::unique_ptr<cg2::TextLabel> tankExpeditionMaintenanceText_;
    std::unique_ptr<cg2::Sprite> tankExpeditionMaintenanceButton_;
    /// @brief 戦車遠征を初期化する。
    void InitializeTankRun();
    /// @brief 戦車遠征表示情報を初期化する。
    void InitializeTankRunVisuals();
    /// @brief 戦車遠征を更新する。
    /// @param dt この処理で進める経過時間（秒）。
    void UpdateTankRun(float dt);
    /// @brief 戦車遠征UIを描画する。
    void DrawTankRunUi();
    /// @brief 戦車遠征攻撃予告を後で処理するために予約する。
    void QueueTankRunTelegraph();
    /// @brief 戦車遠征カードを現在の状態へ適用する。
    void ApplyTankRunCards();
    /// @brief 戦車遠征リソースを更新する。
    /// @param dt この処理で進める経過時間（秒）。
    void UpdateTankRunResources(float dt);
    /// @brief 戦車遠征リソース取得の通知を受けて、このオブジェクトの状態を反映する。
    void OnTankRunResourceClaim(size_t index, bool playerOwned);
    /// @brief 戦車遠征敵Defeatedの通知を受けて、このオブジェクトの状態を反映する。
    void OnTankRunEnemyDefeated(const cg2::Vector3& position);
    /// @brief 戦車遠征選択肢を選択する。
    void SelectTankRunOption(int index);
    /// @brief 戦車遠征UIを最新の内容へ更新する。
    void RefreshTankRunUi();
    /// @brief 戦車遠征メニューOpenであるか判定する。
    bool IsTankRunMenuOpen() const;
    /// @brief 戦車遠征計測を要求を予約する。
    void RequestTankRunCapture(const std::string& name);
    /// @brief 戦車遠征計測をコピーする。
    void CopyTankRunCapture();
    /// @brief 戦車遠征計測を終了する。
    void FinishTankRunCapture();
    bool prototypeRun_ = false;
    tankrun::RunDirector tankRun_{};
    int tankRunSelection_ = 0;
    /// @brief 遠征中に配置する回収資源の位置と種類を表す。
    struct RunResource {
        cg2::Vector3 position{};
        float respawn = 0;
        bool active = false;
    };
    /// @brief 遠征の破裂演出の位置と進行状態を保持する。
    struct RunBurst {
        cg2::Vector3 position{};
        float age = 0;
        bool resource = false;
    };
    std::array<RunResource, 3> tankRunResources_{};
    std::vector<RunBurst> tankRunBursts_;
    int tankRunCombo_ = 0;
    int tankRunBestCombo_ = 0;
    float tankRunComboTime_ = 0;
    int tankRunLastBossLevel_ = 1;
    float tankRunMenuAge_ = 0.0f;
    float tankRunHudTimer_ = 0.0f;
    bool tankRunPaused_ = false;
    bool tankRunFinalStarted_ = false;
    bool tankRunAutoTest_ = false;
    float tankRunAutoTime_ = 0.0f;
    int tankRunAutoStep_ = 0;
    int tankRunAutoMenuIndex_ = 0;
    std::unique_ptr<cg2::Sprite> tankRunDimmer_;
    std::unique_ptr<cg2::Sprite> tankRunHudPanel_;
    std::unique_ptr<cg2::Sprite> tankRunBossTrack_;
    std::unique_ptr<cg2::Sprite> tankRunBossFill_;
    std::array<std::unique_ptr<cg2::Sprite>, 3> tankRunCards_;
    std::array<std::unique_ptr<cg2::TextLabel>, 3> tankRunCardTitles_;
    std::array<std::unique_ptr<cg2::TextLabel>, 3> tankRunCardBodies_;
    std::unique_ptr<cg2::TextLabel> tankRunHeading_;
    std::unique_ptr<cg2::TextLabel> tankRunDescription_;
    std::unique_ptr<cg2::TextLabel> tankRunFooter_;
    std::unique_ptr<cg2::TextLabel> tankRunHud_;
    std::unique_ptr<cg2::TextLabel> tankRunBuildText_;
    std::unique_ptr<cg2::TextLabel> tankRunObjectiveText_;
    std::unique_ptr<cg2::TextLabel> tankRunBossText_;
    Microsoft::WRL::ComPtr<ID3D12Resource> tankRunCaptureReadback_;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT tankRunCaptureLayout_{};
    std::string tankRunCapturePath_;
    bool tankRunCaptureCopied_ = false;
    /// @brief 対象を追従するHPバーの描画資源と表示状態を保持する。
    struct FollowHpBar {
        std::unique_ptr<cg2::Sprite> outline;
        std::unique_ptr<cg2::Sprite> background;
        std::unique_ptr<cg2::Sprite> fill;
    };
    /// @brief レベル配置オブジェクトの描画資源と姿勢を保持する。
    struct LevelVisualObject {
        std::string name;
        std::unique_ptr<cg2::Object3d> object;
    };
    /// @brief 現在のボス段階と発動済み条件を保持する。
    struct RuntimeBossPhase {
        LevelBossPhase phase;
        bool activated = false;
    };
    /// @brief 調整画面の選択・編集中の値・保存状態を保持する。
    struct BalanceEditorState {
        bool initialized = false;
        bool defaultRandomSpawnEnabled = true;
        int playerMaxHp = 10000;
        float playerReloadSpeed = 10.0f;
        float playerBulletDamage = 1.0f;
        float playerBulletSpeed = 0.3f;
        float playerMoveSpeed = 0.2f;
        float playerStaminaRecovery = 1.0f;
        float playerMaxStamina = 3.0f;
        int playerBodyDamage = 3;
        float playerHealthRegenUpgrade = 0.08f;
        float maxHpUpgradeAmount = 0.10f;
        float playerBodyDamageUpgrade = 0.10f;
        float playerBulletSpeedUpgrade = 0.08f;
        float playerBulletDamageUpgrade = 0.10f;
        float playerReloadUpgrade = 0.07f;
        float playerMoveSpeedUpgrade = 0.06f;
        float playerMinReloadSpeed = 3.0f;
        bool healToFull = false;
        int damageBlock = 90;
        int bossContact = 45;
        int expEnemyContact = 15;
        int shooterContact = 25;
        int shooterBullet = 18;
        float shooterDetectionRadius = 18.0f;
        float shooterTurnSpeed = 5.5f;
        float shooterFireInterval = 1.25f;
        float shooterBulletSpeed = 0.20f;
        float bossBulletSpeed = 0.4f;
        int bossMaxHp = 1050;
        int bossBulletCount = 2;
        float bossSpreadAngleDeg = 30.0f;
        float bossCooldown = 0.15f;
        int bossBulletDamage = 12;
        float bossBulletHp = 0.0f;
        float bossBulletPenetration = 0.0f;
        bool bossRandomSpread = true;
        int bossAttackPattern = 0;
        bool expEnemyHostileToBoss = false;
        int bossExpEnemyDamage = 12;
        int bossHealOnExpEnemyKill = 30;
        int bossKillsPerLevel = 3;
        int bossMaxHpGainPerLevel = 20;
        int bossDamageGainPerLevel = 2;
        bool bossLevelingModeEnabled = true;
        float bossLevelingEnterDistance = 24.0f;
        float bossLevelingExitDistance = 16.0f;
        float bossLevelingSearchRadius = 80.0f;
        float bossAimTurnHalfSeconds = 1.0f;
        std::string statusMessage;
    };
    enum class TutorialStep {
        Move,
        Shoot,
        Dash,
        Phase1Complete,
        Upgrade,
        EvolutionUnlocked,
        Evolution,
        TutorialComplete,
    };
    /// @brief チュートリアルの課題・表示・進行条件を指定する。
    struct TutorialConfig {
        bool enabled = true;
        float moveDistance = 3.0f;
        float stepCompleteDelay = 0.35f;
        float phase1CompleteDisplayDuration = 1.0f;
        float evolutionUnlockedDisplayDuration = 1.0f;
        float tutorialCompleteDisplayDuration = 1.5f;
    };

    /// @brief 追従HPBarsを初期化する。
    void InitializeFollowHpBars(size_t count);
    /// @brief 追従HPバー一括処理を初期化する。
    void InitializeFollowHpBarBatch();
    /// @brief 追従HPバーを描画する。
    void DrawFollowHpBar(const void* ownerKey, const cg2::Vector3& worldPos, int hp, int maxHp, float width, float yOffset);
    /// @brief 追従スタミナバーを描画する。
    void DrawFollowStaminaBar(const cg2::Vector3& worldPos, float stamina, float maxStamina, float width, float yOffset);
    /// @brief HPバー四角形を後で処理するために予約する。
    void QueueHpBarQuad(std::vector<cg2::VertexData>& vertices, const cg2::Vector2& center, const cg2::Vector2& size);
    /// @brief HPバーの一括描画で使うマテリアルのGPUバッファを保持する。
    struct HpBarMaterialBuffer {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        cg2::Material* data = nullptr;
    };

    /// @brief HPバー一括処理を描画する。
    void DrawHpBarBatch(uint32_t startVertex, uint32_t vertexCount, const std::string& textureFilePath,
                        const HpBarMaterialBuffer& material);
    /// @brief HPバーBatchesを描画する。
    void DrawHpBarBatches();
    /// @brief ワールド座標をカメラの画面座標へ変換する。
    cg2::Vector2 WorldToScreen(const cg2::Vector3& worldPos) const;
    /// @brief ネオン格子描画パスを描画する。
    void DrawNeonGridPass(bool includeStageBlockOutlines = true);
    /// @brief ステージ地形ブロックネオン描画パスを描画する。
    void DrawStageBlockNeonPass();
    /// @brief ステージ地形ブロックネオンOutlinesを後で処理するために予約する。
    void QueueStageBlockNeonOutlines();
    /// @brief 経験値敵ネオン塗りつぶしモデルを描画する。
    void DrawExpEnemyNeonFillModels();
    /// @brief 経験値敵ネオン深度線を描画する。
    void DrawExpEnemyNeonDepthLines();
    /// @brief 死亡後処理パルスを発動させる。
    void TriggerDeathPostPulse(const cg2::Vector3& worldPosition, float strength);
    /// @brief 死亡後処理パルスを更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateDeathPostPulse(float deltaTime);
    /// @brief 経験値敵ネオンShapesを後で処理するために予約する。
    void QueueExpEnemyNeonShapes(const cg2::Vector3& cameraRight, const cg2::Vector3& cameraUp, const cg2::Vector3& cameraForward);
    /// @brief アクターネオンBillboardsを後で処理するために予約する。
    void QueueActorNeonBillboards(const cg2::Vector3& cameraRight, const cg2::Vector3& cameraUp, bool drawBodies = true,
                                  bool drawBarrels = true);
    /// @brief アクターネオン機体塗りつぶし描画パスを描画する。
    void DrawActorNeonBodyFillPass();
    /// @brief 自機ネオンAfterimagesを更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdatePlayerNeonAfterimages(float deltaTime);
    struct PlayerMeleeSlash;
    /// @brief 発射イベントの壁までの表示を登録し、その時点の対象へレーザーダメージを適用する。
    /// @note 方向がほぼ0なら登録・ダメージ適用を行わない。残存表示から追加の命中判定は行わない。
    void SpawnPlayerLaser(const Player::LaserShotEvent& event);
    /// @brief レーザーの表示寿命だけを進め、期限切れの表示を消去する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdatePlayerLasers(float deltaTime);
    /// @brief 自機Lasersを後で処理するために予約する。
    void QueuePlayerLasers(const cg2::Vector3& cameraForward);
    /// @brief 発射イベントから地雷の位置・威力・待機時間・寿命を登録する。
    void SpawnPlayerMine(const Player::MineDropEvent& event);
    /// @brief 地雷の待機時間・寿命を進め、寿命切れまたは待機後の近接で起爆し、終了した爆発表示を消去する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdatePlayerMines(float deltaTime);
    /// @brief 添字の地雷の範囲ダメージを適用し、爆発表示を登録して地雷を削除する。
    /// @param index 現在の地雷配列の添字。範囲外なら何もしない。
    void DetonatePlayerMine(size_t index);
    /// @brief 自機Minesを後で処理するために予約する。
    void QueuePlayerMines(const cg2::Vector3& cameraRight, const cg2::Vector3& cameraUp, const cg2::Vector3& cameraForward);
    /// @brief 斬撃イベントから予備動作・振り・回復動作と命中履歴を持つ斬撃状態を登録する。
    /// @note 方向がほぼ0なら登録しない。この時点では命中ダメージを適用しない。
    void SpawnPlayerMeleeSlash(const Player::MeleeSlashEvent& event);
    /// @brief 自機に追従する斬撃の時間・命中・押し飛ばし・軌跡を処理し、期限切れの斬撃を消去する。
    /// @note 同じ斬撃の命中履歴にある対象へは再適用しない。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdatePlayerMeleeSlashes(float deltaTime);
    /// @brief 自機近接攻撃Slashesを後で処理するために予約する。
    void QueuePlayerMeleeSlashes();
    /// @brief 強化/特殊戦闘のイベントを消費して演出へ写し、既存演出の経過時間と特殊弾の表示情報を更新する。
    /// @note 攻撃結果は適用済み。表示上限でもイベントは消費し、ダメージを再適用しない。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateSpecialCombatPresentation(float deltaTime);
    /// @brief 特殊戦闘画面演出を後で処理するために予約する。
    void QueueSpecialCombatPresentation();
    /// @brief 特殊戦闘イベント・表示終点と、フラッシュ開始からの経過秒を保持する。
    struct SpecialCombatFlash {
        Player::SpecialCombatEvent event;
        float age = 0;
        cg2::Vector3 end{};
    };
    std::vector<SpecialCombatFlash> specialCombatFlashes_;
    /// @brief 射撃強化イベントと、フラッシュ開始からの経過秒を保持する。
    struct BuildCombatFlash {
        BulletManager::BuildEvent event;
        float age = 0;
    };
    std::vector<BuildCombatFlash> buildCombatFlashes_;
    /// @brief 特殊投射物の外観と表示状態を保持する。
    struct SpecialProjectileVisual {
        cg2::Vector3 position, direction;
        float radius;
        Bullet::SpecialKind kind;
    };
    std::vector<SpecialProjectileVisual> specialProjectileVisuals_;
    float railChargeAudioAge_ = 0;
    /// @brief 自機近接攻撃軌跡設定を作成して返す。
    cg2::TrailConfig MakePlayerMeleeTrailConfig(const PlayerMeleeSlash& slash, float alphaScale = 1.0f) const;
    /// @brief 振りの進行度を0～1へ制限して刃外側の軌跡の2端点を計算し、base/tipへ書き込む。
    void ComputePlayerMeleeBladeSection(const PlayerMeleeSlash& slash, float progress, cg2::Vector3& base, cg2::Vector3& tip) const;
    /// @brief ネオン三角形粒子を更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateNeonTriangleParticles(float deltaTime);
    /// @brief ネオン三角形粒子を後で処理するために予約する。
    void QueueNeonTriangleParticles(const cg2::Vector3& cameraRight, const cg2::Vector3& cameraUp, const cg2::Vector3& cameraForward);
    /// @brief 近距離カメラ2Dであるか判定する。
    bool IsNearCamera2D(const cg2::Vector3& worldPos, float halfWidth, float halfHeight, float margin = 0.0f) const;
    /// @brief 後処理設定Entriesを初期状態へ戻す。
    void ResetPostProfileEntries();
    /// @brief 後処理設定Entryを追加する。
    void AddPostProfileEntry(const char* name, float ms, bool active);
    /// @brief 後処理設定文字を更新する。
    void UpdatePostProfileText();
    /// @brief 性能BreakdownImGUIを描画する。
    void DrawPerformanceBreakdownImGui();
#if defined(USE_IMGUI) && !defined(NDEBUG)
    /// @brief 性能計測ImGUIを描画する。
    void DrawPerformanceCaptureImGui();
    /// @brief 性能計測を開始する。
    void StartPerformanceCapture();
    /// @brief 性能フレームを記録する。
    void CapturePerformanceFrame();
    /// @brief 性能計測ファイルを書き込む。
    bool WritePerformanceCaptureFiles();
#endif
    /// @brief 後処理設定Category有効であるか判定する。
    bool IsPostProfileCategoryEnabled(const char* category) const;
    /// @brief 後処理設定方式名前を返す。
    const char* GetPostProfileModeName() const;
    /// @brief ステージ後処理キャッシュUV差分を返す。
    cg2::Vector2 GetStagePostCacheUvOffset(const cg2::Vector3& currentCameraPos) const;
    /// @brief レベルファイルを読み込む。
    bool LoadLevelFile(LevelData& outLevel) const;
    /// @brief レベルデータを再読込または再装填する。
    void ReloadLevelData(bool resetSpawnPositions);
    /// @brief Appliedレベルデータを消去する。
    void ClearAppliedLevelData();
    /// @brief レベルデータを現在の状態へ適用する。
    void ApplyLevelData(const LevelData& levelData);
    /// @brief レベル性能調整を現在の状態へ適用する。
    void ApplyLevelBalance(const nlohmann::json& balanceJson);
    /// @brief ゲームシーン開発表示ImGUIを描画する。
    void DrawGameSceneDebugImGui();
    /// @brief 後処理演出パラメーターControlsを描画する。
    void DrawPostEffectParamControls(const char* labelPrefix, cg2::BloomParam& param);
    /// @brief ゲーム後処理演出設定を読み込む。
    bool LoadGamePostEffectConfig(const std::string& filePath = "resources/configs/gamePostEffects.json");
    /// @brief ゲーム後処理演出設定を保存する。
    bool SaveGamePostEffectConfig(const std::string& filePath = "resources/configs/gamePostEffects.json") const;
    /// @brief ゲーム後処理演出設定を組み立てる。
    nlohmann::json BuildGamePostEffectConfig() const;
    /// @brief ゲーム後処理演出設定を現在の状態へ適用する。
    void ApplyGamePostEffectConfig(const nlohmann::json& configJson);
    /// @brief ゲーム表示設定を読み込む。
    bool LoadGameVisualConfig(const std::string& filePath = "resources/configs/gameVisuals.json");
    /// @brief ゲーム表示設定を保存する。
    bool SaveGameVisualConfig(const std::string& filePath = "resources/configs/gameVisuals.json") const;
    /// @brief ゲーム表示設定を組み立てる。
    nlohmann::json BuildGameVisualConfig() const;
    /// @brief ゲーム表示設定を現在の状態へ適用する。
    void ApplyGameVisualConfig(const nlohmann::json& configJson);
    /// @brief ゲーム文字外観を現在の状態へ適用する。
    void ApplyGameTextAppearance();
    /// @brief ゲーム文字ブルームを描画する。
    void DrawGameTextBloom();
    /// @brief レベルAIDitor性能調整Labを描画する。
    void DrawLevelAIDitorBalanceLab(bool embedded = false);
    /// @brief 性能調整編集画面からのJsonを読み込む。
    void LoadBalanceEditorFromJson(const nlohmann::json& balanceJson);
    /// @brief 性能調整Jsonからの編集画面を組み立てる。
    nlohmann::json BuildBalanceJsonFromEditor() const;
    /// @brief 性能調整編集画面へのレベルファイルを保存する。
    bool SaveBalanceEditorToLevelFile(const std::string& filePath);
    /// @brief 性能調整AIHandoffを書き込む。
    bool WriteBalanceAIHandoff(const std::string& filePath) const;
    /// @brief レベルオブジェクトを現在の状態へ適用する。
    void ApplyLevelObject(const LevelObject& levelObject, bool allowBossSpawn);
    /// @brief レベル出現範囲を追加する。
    void AddLevelSpawnArea(const LevelSpawnArea& spawnArea);
    /// @brief レベル出現範囲からのオブジェクトを追加する。
    void AddLevelSpawnAreaFromObject(const LevelObject& levelObject);
    /// @brief HP比が開始閾値以下になった未発動ボス段階を配列順に適用し、追加配置を生成する。
    void UpdateLevelBossPhases();
    /// @brief ボス段階調整値を現在の状態へ適用する。
    void ApplyBossPhaseTuning(const LevelBossPhase& phase);
    /// @brief レベル演出Presetを現在の状態へ適用する。
    void ApplyLevelEffectPreset(const nlohmann::json& effectJson);
    /// @brief レベル編集画面プレビューを後で処理するために予約する。
    void QueueLevelEditorPreview();
    /// @brief レベルオブジェクトプレビューを後で処理するために予約する。
    void QueueLevelObjectPreview(const LevelObject& levelObject, const cg2::Vector4& color);
    /// @brief レベル出現範囲プレビューを後で処理するために予約する。
    void QueueLevelSpawnAreaPreview(const LevelSpawnArea& spawnArea, const cg2::Vector4& color);
    /// @brief レベル項目を追加する。
    bool AddLevelItem(const LevelObject& levelObject);
    /// @brief レベル項目を更新する。
    void UpdateLevelItems();
    /// @brief レベル項目を描画する。
    void DrawLevelItems();
    /// @brief 弾状態開発表示重ね表示を描画する。
    void DrawBulletStatusDebugOverlay();
    /// @brief 弾状態開発表示Tableを描画する。
    void DrawBulletStatusDebugTable();
    /// @brief 提出版UIを初期化する。
    void InitializeSubmissionUi();
    /// @brief チュートリアル設定を読み込む。
    bool LoadTutorialConfig(const std::string& filePath = "resources/configs/tutorial.json");
    /// @brief チュートリアルUIを初期化する。
    void InitializeTutorialUi();
    /// @brief チュートリアルを更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdateTutorial(float deltaTime);
    /// @brief チュートリアル段階を開始する。
    void EnterTutorialStep(TutorialStep step);
    /// @brief チュートリアル段階を完了にする。
    void CompleteTutorialStep();
    /// @brief チュートリアル強化報酬を付与する。
    void GrantTutorialUpgradeReward();
    /// @brief チュートリアル進化報酬を付与する。
    void GrantTutorialEvolutionReward();
    /// @brief チュートリアル文字を更新する。
    void UpdateTutorialText();
    /// @brief チュートリアルUIを描画する。
    void DrawTutorialUi();
    /// @brief 通常チュートリアルが有効で、敵AI・特殊戦闘・通常衝突を抑制する場合true。
    bool IsTutorialCombatSuppressed() const
    {
        return tutorialConfig_.enabled;
    }
    /// @brief 基準時間で撃破/死亡演出の終了を判定し、結果表示とその選択入力を処理する。
    /// @param baseDeltaTime 演出と進行に使う経過秒。戦闘の減速倍率を掛けない。
    void UpdateGameFlow(float baseDeltaTime);
    /// @brief 適用済みHPの差分・行動イベント・死亡状態を読み、演出と戦闘終了状態を反映する。
    /// @note 進化確定/取消の通知は消費し、遠征時は主攻撃通知も消費する。2つの引数は現在の実装では使わない。
    void UpdateGameplayEventEffects(float baseDeltaTime, bool justDodgeTriggered);
    /// @brief ボス撃破演出状態へ移り、次回の戦闘更新を止める。敵・弾はこの関数では消去しない。
    void BeginBossDefeatSequence();
    /// @brief 自機死亡を進行へ記録し、ゲームオーバー状態へ移って次回の戦闘更新を止める。
    void BeginGameOver();
    /// @brief 結果状態を開始する。
    void EnterResultState(bool stageClear);
    /// @brief リザルト画面の選択を確定し、対応する次画面へ進む。
    void ConfirmResultSelection();
    /// @brief 結果文字を更新する。
    void UpdateResultText();
    /// @brief イベントCalloutを設定する。
    void SetEventCallout(const std::string& text, float duration);
    /// @brief 自機機体設定監視を初期化する。
    void InitializePlayerClassConfigWatch();
    /// @brief 自機機体設定監視を更新する。
    /// @param deltaTime この処理で進める経過時間（秒）。
    void UpdatePlayerClassConfigWatch(float deltaTime);
    /// @brief 自機機体設定を再読込または再装填する。
    bool ReloadPlayerClassConfig(bool automatic);
    /// @brief ワールド座標を画面上の正規化座標へ変換する。
    cg2::Vector2 WorldToScreenUv(const cg2::Vector3& worldPos) const;

    /// @brief HPバーを表示するか判断するための距離と条件を保持する。
    struct HpBarVisibility {
        int lastHp = -1;
        int lastMaxHp = -1;
        float visibleTimer = 0.0f;
        float alpha = 0.0f;
    };

    /// @brief ポストエフェクトの1区間の所要時間を記録する。
    struct PostProfileEntry {
        const char* name = "";
        float ms = 0.0f;
        bool active = false;
    };

#if defined(USE_IMGUI) && !defined(NDEBUG)
    /// @brief 性能記録を開始・終了する条件を指定する。
    struct PerformanceCaptureConditions {
        std::string label;
        Player::UpgradeHudDebugSnapshot upgradeHud{};
        int postProfileMode = 0;
        std::string postProfileModeName;
        bool gridPostEnabled = false;
        bool stagePostEnabled = false;
        bool bulletTrailPostEnabled = false;
        bool playerPostEnabled = false;
        bool enemyPostEnabled = false;
        bool expEnemyPostEnabled = false;
        bool trailAutoFireEnabled = false;
        bool d3d12DebugLayerEnabled = false;
    };

    /// @brief 性能記録の1フレームの計測値とゲーム状態を保持する。
    struct PerformanceCaptureFrame {
        uint32_t frameIndex = 0;
        float fps = 0.0f;
        IScene::RenderProfile render{};
        std::array<PostProfileEntry, 16> postEntries{};
        size_t postEntryCount = 0;
        Player::UiProfileStats upgradeHud{};
        Player::UiProfileStats evolutionUi{};
        int playerLevel = 0;
        int skillPoints = 0;
        bool upgradeHudListVisible = false;
        Player::UpgradeHudDebugSnapshot upgradeHudAfterPlayerUpdate{};
        Player::UpgradeHudDebugSnapshot upgradeHudAfterCollision{};
        Player::UpgradeHudDebugSnapshot upgradeHudAtCapture{};
        size_t enemyCount = 0;
        size_t expEnemyCount = 0;
        size_t bulletCount = 0;
        size_t playerBulletCount = 0;
        size_t enemyBulletCount = 0;
        size_t hostileExpEnemyBulletCount = 0;
        size_t bulletTrailCount = 0;
        cg2::TrailManager::DrawStats trailDrawStats{};
        size_t playerLaserCount = 0;
        size_t playerMineCount = 0;
        size_t playerMeleeSlashCount = 0;
        size_t neonTriangleParticleCount = 0;
    };
#endif

    std::unique_ptr<cg2::DebugCamera> debugCamera;
    std::unique_ptr<cg2::Camera> camera;
    // Presentation owns no combat state; the Enemy snapshot is consumed after collision resolution.
    std::unique_ptr<NeonBossVisual> neonBossVisual_;
    BossVisualBridge bossVisualBridge_;
    bool neonBossVisualEnabled_ = true;
    void UpdateNeonBossVisual(float deltaTime);
    bool UseNeonBossVisual() const;
    void DrawNeonBossVisual();
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    void InitializeGameplayScenario();
    bool PrepareGameplayScenarioFrame();
    void RecordGameplayScenarioFrame();
    void RecordGameplayScenarioCapture(cg2::DirectXCommon& dx);
    gameplaytest::Snapshot MakeGameplayScenarioSnapshot() const;
    void QueueGameplayScenarioCapture(const std::string& name);
    NeonShowcaseCapture gameplayScenarioCapture_;
    gameplaytest::Snapshot gameplayScenarioCaptureSnapshot_;
    std::string gameplayScenarioCaptureName_;
    bool gameplayScenarioInitialized_ = false, gameplayScenarioCapturePending_ = false;
    bool gameplayScenarioEventApplied_ = false, gameplayScenarioRestartRequested_ = false;
    bool gameplayScenarioFinishDeferred_ = false;
    unsigned gameplayScenarioTransitionStep_ = 0;
    std::unique_ptr<NeonSkinnedPreview> neonSkinnedPreview_;
    bool selectNeonSkinnedPreviewTab_ = false;
    bool selectNeonBossTab_ = false;
    void DrawNeonBossDeveloperTools();
    void StartNeonBossDeveloperEncounter();
    void UpdateNeonBossDeveloperValidation();
    void RecordNeonBossDeveloperFrame(cg2::DirectXCommon& dx);
    nlohmann::json MakeNeonBossMetadata() const;
    void WriteNeonBossValidation(bool completed);
    NeonShowcaseCapture neonBossCapture_;
    bool neonBossAutoTest_ = false, neonBossDeveloperStartPending_ = false;
    bool neonBossDeveloperFreeze_ = false;
    int neonBossProbe_ = 0, neonBossCaptureStep_ = 0;
    int neonBossProfileStep_ = 0;
    float neonBossValidationAge_ = 0;
    std::string neonBossCaptureName_;
    nlohmann::json neonBossValidationCaptures_ = nlohmann::json::array();
    std::vector<std::string> neonBossValidationErrors_;
    unsigned neonBossDeadShots_ = 0, neonBossDeadDashCount_ = 0;
    cg2::Vector3 neonBossDeadPosition_{};
#endif

    std::unique_ptr<cg2::Object3d> enemyObject_;
    std::unique_ptr<cg2::Object3d> object3d3;

    std::unique_ptr<cg2::Object3d> playerObject_;

    std::unique_ptr<cg2::Object3d> ballObj_;
    std::unique_ptr<cg2::Object3d> ball_;

    std::unique_ptr<cg2::Object3d> groundObj_;

    // 入力
    cg2::Input* input_;

    // ワールドトランスフォーム
    cg2::Transform worldTransform_;

    // プレイヤー
    std::unique_ptr<Player> player_;

    // 敵
    std::unique_ptr<Enemy> enemy_;

    // 経験値敵
    std::unique_ptr<EnemyManager> enemyManager_;

    // ステージ
    std::unique_ptr<Stage> stage_;

    // 弾マネージャ
    std::unique_ptr<BulletManager> bulletManager_;

    // 衝突マネージャ
    std::unique_ptr<CollisionManager> collisionManager_;
    std::unique_ptr<cg2::RingManager> collisionDebugRingManager_;
    std::unique_ptr<cg2::NeonGridRenderer> neonGridRenderer_;
    std::unique_ptr<NeonProjectileRenderer> neonProjectileRenderer_;
    std::unique_ptr<cg2::TrailManager> playerMeleeTrailManager_;
    std::unique_ptr<cg2::Skybox> skybox_;
    std::unique_ptr<cg2::ObjectPostEffect> neonGridPostEffect_;
    std::unique_ptr<cg2::ObjectPostEffect> bulletTrailPostEffect_;
    std::unique_ptr<cg2::ObjectPostEffect> particlePostEffect_;
    std::unique_ptr<cg2::ObjectPostEffect> playerPostEffect_;
    std::unique_ptr<cg2::ObjectPostEffect> enemyPostEffect_;
    std::unique_ptr<cg2::ObjectPostEffect> expEnemyPostEffect_;
    std::unique_ptr<cg2::ObjectPostEffect> sharedObjectBloomPostEffect_;
    std::unique_ptr<cg2::ObjectPostEffect> stagePostEffect_;
    LevelData currentLevelData_;
    BalanceEditorState balanceEditor_;
    std::vector<LevelVisualObject> levelItems_;
    std::vector<RuntimeBossPhase> levelBossPhases_;

    // 終了フラグ
    bool finished_ = false;
    std::string nextSceneName_ = "TITLE";

    std::unique_ptr<Fade> fade_ = nullptr;
    Phase phase_ = Phase::kFadeIn;

    std::unique_ptr<cg2::Sprite> shotGide;
    std::unique_ptr<cg2::Sprite> wasdGide;
    std::unique_ptr<cg2::Sprite> dashGide;
    std::unique_ptr<cg2::Sprite> toTitleGide;
    std::unique_ptr<cg2::TextLabel> dashGuideText_;
    std::unique_ptr<cg2::TextLabel> moveGuideText_;
    std::unique_ptr<cg2::TextLabel> titleGuideText_;
    std::unique_ptr<cg2::TextLabel> controlGuideText_;
    std::unique_ptr<cg2::TextLabel> fpsText_;
    std::unique_ptr<cg2::TextLabel> postProfileText_;
    std::unique_ptr<cg2::TextLabel> flowBannerText_;
    std::unique_ptr<cg2::TextLabel> resultSummaryText_;
    std::unique_ptr<cg2::TextLabel> resultMenuText_;
    std::unique_ptr<cg2::TextLabel> eventCalloutText_;
    std::unique_ptr<cg2::Sprite> tutorialPanel_;
    std::unique_ptr<cg2::TextLabel> tutorialTitleText_;
    std::unique_ptr<cg2::TextLabel> tutorialInputText_;
    std::unique_ptr<cg2::TextLabel> tutorialDescriptionText_;
    std::unique_ptr<NeonTextEffect> gameTextNeonEffect_;
    int gameTextFontMode_ = 1;
    bool gameTextNeonEnabled_ = true;
    bool gameTextOutlineEnabled_ = false;
    cg2::Vector4 gameTextOutlineColor_{0.0f, 0.0f, 0.0f, 0.9f};
    float gameTextOutlineThickness_ = 1.0f;
    NeonTextEffectStyle gameTextNeonStyle_{};
    std::vector<FollowHpBar> followHpBars_;
    std::array<std::vector<cg2::VertexData>, 4> hpBarBackgroundVertices_;
    std::array<std::vector<cg2::VertexData>, 4> hpBarFillVertices_;
    std::vector<cg2::VertexData> staminaBarFillVertices_;
    std::array<std::vector<cg2::VertexData>, 4> hpBarOutlineVertices_;
    std::unordered_map<const void*, HpBarVisibility> hpBarVisibility_;
    std::array<HpBarMaterialBuffer, 13> hpBarMaterials_;
    Microsoft::WRL::ComPtr<ID3D12Resource> hpBarVertexResource_;
    D3D12_VERTEX_BUFFER_VIEW hpBarVertexBufferView_{};
    cg2::VertexData* hpBarVertexData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> hpBarTransformResource_;
    cg2::TransformationMatrix* hpBarTransformData_ = nullptr;
    size_t followHpBarIndex_ = 0;
    bool showFollowHpBars_ = true;
    bool showPlayerStaminaBar_ = true;
    bool showControlGuide_ = true;
    TutorialConfig tutorialConfig_{};
    TutorialStep tutorialStep_ = TutorialStep::Move;
    bool tutorialStepCompleting_ = false;
    bool tutorialUiVisible_ = false;
    float tutorialStepCompleteTimer_ = 0.0f;
    float tutorialPhase1CompleteTimer_ = 0.0f;
    float tutorialEvolutionUnlockedTimer_ = 0.0f;
    float tutorialCompleteTimer_ = 0.0f;
    float tutorialMoveDistance_ = 0.0f;
    cg2::Vector3 tutorialPreviousPlayerPosition_{};
    bool tutorialUpgradeRewardGranted_ = false;
    bool tutorialEvolutionRewardGranted_ = false;
    bool tutorialEvolutionUiWasOpen_ = false;
    bool tutorialCompleteExitReady_ = false;

    // カメラ合わせフラグ
    bool cameraFollow_ = true;

    cg2::Vector3 direction = {0.0f, -1.0f, 0.0f};
    float insensity = 1.0f;
    float shininess = 10.0f;

    float timeScale_ = 1.0f; // 1.0 が通常、0.2 なら 5倍スロー
    float finalDeltaTime = 1.0f / 60.0f;
    using GameFlowState = CombatFlowState;
    CombatFlowController combatFlow_;
    ScreenEffectDirector screenEffectDirector_{};
    float playTime_ = 0.0f;
    float eventCalloutTimer_ = 0.0f;
    std::filesystem::file_time_type playerClassConfigObservedWriteTime_{};
    std::filesystem::file_time_type playerClassConfigLoadedWriteTime_{};
    float playerClassConfigPollTimer_ = 0.0f;
    float playerClassConfigDebounceTimer_ = 0.0f;
    bool playerClassConfigHasObservedWriteTime_ = false;
    bool playerClassConfigHasLoadedWriteTime_ = false;
    bool playerClassConfigReloadPending_ = false;
    int resultSelection_ = 0;
    int justDodgeCount_ = 0;
    int damageTaken_ = 0;
    int defeatedEnemies_ = 0;
    int previousPlayerHp_ = -1;
    int previousBossHp_ = -1;
    bool previousDashing_ = false;
    bool bossEntryTriggered_ = false;
    bool bossDefeatHandled_ = false;
    bool playerDeathHandled_ = false;
    std::chrono::steady_clock::time_point fpsLastSampleTime_{};
    float fpsAccumulatedTime_ = 0.0f;
    int fpsFrameCount_ = 0;
    bool enablePlayerPostEffect_ = true;
    bool enableEnemyPostEffect_ = true;
    bool enableExpEnemyPostEffect_ = true;
    bool enableStagePostEffect_ = false;
#if CG2_DEVELOPER_TOOLS && !defined(NDEBUG)
    int developerBloomComparisonMode_ = -1;
    int developerToneMappingMode_ = -1;
    bool developerBloomFreeze_ = false;
    NeonShowcaseCapture developerGameCapture_;
    cg2::BloomParam developerCompositeParams_{};
    bool developerCompositeParamsAvailable_ = false;
    unsigned developerGameCaptureNumber_ = 0;
    std::string developerGameCaptureDirectory_;
    char developerGameCaptureLabel_[64] = "game_bloom";
    nlohmann::json MakeDeveloperGameCaptureMetadata(cg2::DirectXCommon& dx) const;
#endif
#ifdef USE_IMGUI
    bool showPostProfileOverlay_ = true;
#else
    bool showPostProfileOverlay_ = false;
#endif
    int postProfileMode_ = 0;
    std::array<PostProfileEntry, 16> postProfileEntries_;
    size_t postProfileEntryCount_ = 0;
    std::array<float, 16> postProfileAccumulatedMs_{};
    std::array<float, 16> postProfileAverageMs_{};
    int postProfileAccumulatedFrames_ = 0;
    IScene::RenderProfile renderProfile_{};
#if defined(USE_IMGUI) && !defined(NDEBUG)
    int performanceCaptureFrameCount_ = 30;
    std::array<char, 128> performanceCaptureLabel_{};
    bool performanceCaptureActive_ = false;
    bool performanceCaptureSkipCurrentFrame_ = false;
    PerformanceCaptureConditions performanceCaptureConditions_{};
    std::vector<PerformanceCaptureFrame> performanceCaptureFrames_;
    std::string performanceCaptureLastCsvPath_;
    std::string performanceCaptureStatus_;
    Player::UpgradeHudDebugSnapshot upgradeHudAfterPlayerUpdate_{};
    Player::UpgradeHudDebugSnapshot upgradeHudAfterCollision_{};
#endif
    bool stagePostCacheValid_ = false;
    cg2::Vector3 stagePostCacheCameraPos_{};
    float stagePostCacheRefreshPixels_ = 48.0f;
    float expEnemyPostVisibleHalfWidth_ = 20.0f;
    float expEnemyPostVisibleHalfHeight_ = 10.0f;
    bool slowMotionPostActive_ = false;
    bool keepPlayerColorDuringSlow_ = true;
    float slowPlayerChromAbAmount_ = 0.035f;
    float slowPlayerDistortionAmount_ = 0.018f;
    float slowPlayerGlitchAmount_ = 0.015f;
    float sceneFadeBlurTimer_ = 0.0f;
    float sceneFadeBlurDuration_ = 2.0f;
    float sceneFadeBlurIntensity_ = 0.0f;
    bool playerDeathShakeStarted_ = false;
    float cameraShakeTimer_ = 0.0f;
    float cameraShakeDuration_ = 0.65f;
    float cameraShakePower_ = 0.0f;
    bool showCollisionDebug_ = false;
    bool showCollisionDebugBullets_ = true;
    bool showBulletStatusDebugOverlay_ = false;
    bool showBulletStatusDebugTable_ = true;
    int bulletStatusDebugMaxLabels_ = 40;
    bool debugPlayerNoDamage_ = false;
    bool showGameDebugConsole_ = true;
    bool showParticleEditor_ = false;
    bool showPlayerClassEditor_ = false;
    bool showNeonGrid_ = true;
    bool showActorLocalGrid_ = true;
    bool showLevelAIDitorPreview_ = false;
    bool enableNeonGridPostEffect_ = true;
    bool enableBulletTrailPostEffect_ = true;
    bool enableParticlePostEffect_ = true;
    bool enableDeathPostPulse_ = true;
    PostEffectPulse deathPostPulse_{};
    float deathPostPulseTimer_ = 0.0f;
    float deathPostPulseDuration_ = 0.62f;
    float deathPostPulseScale_ = 1.0f;
    float deathPostBloomBoost_ = 0.65f;
    float deathPostChromAbAmount_ = 0.006f;
    float deathPostShockwaveStrength_ = 0.012f;
    float deathPostShockwaveWidth_ = 0.055f;
    float deathPostShockwaveMaxRadius_ = 0.72f;
    std::string postEffectConfigStatus_;
    std::string visualConfigStatus_;
    float worldGridSpacing_ = 2.0f;
    float worldGridLineWidth_ = 0.075f;
    cg2::Vector4 worldGridColor_ = {0.12f, 0.42f, 1.0f, 0.15f};
    float actorGridRadius_ = 5.4f;
    float actorGridSpacing_ = 1.0f;
    float actorGridLineWidth_ = 0.1f;
    float neonLineSoftEdgeRatio_ = 0.42f;
    float neonLineCoreIntensity_ = 1.35f;
    cg2::Vector4 playerGridColor_ = {0.50f, 1.0f, 0.35f, 1.0f};
    cg2::Vector4 enemyGridColor_ = {1.0f, 0.18f, 0.24f, 1.0f};
    cg2::Vector4 expEnemyGridColor_ = {1.0f, 0.32f, 0.58f, 1.0f};
    bool showStageBlockNeonOutlines_ = true;
    bool showStageNormalBlockBodies_ = true;
    float stageBlockNeonLineWidth_ = 0.10f;
    float stageBlockNeonDepthBias_ = 0.035f;
    cg2::Vector4 stageBlockNeonColor_ = {0.55f, 1.0f, 0.32f, 1.0f};
    bool showStageDamageBlockNeonOutlines_ = true;
    cg2::Vector4 stageDamageBlockNeonColor_ = {1.20f, 0.035f, 0.02f, 1.0f};
    float stageDamageBlockPulseSpeed_ = 5.0f;
    float stageDamageBlockPulseMin_ = 0.45f;
    float stageDamageBlockPulseMax_ = 1.35f;
    float stageDamageBlockPulseTime_ = 0.0f;
    bool cullActorLocalGrid_ = true;
    int maxExpEnemyLocalGrids_ = 18;
    bool showNeonTriangleDemo_ = true;
    int expEnemyNeonRenderMode_ = 0;
    float expEnemyNeonSquareSize_ = 1.8f;
    float expEnemyNeonTriangleRadius_ = 1.05f;
    float expEnemyNeonPentagonRadius_ = 1.05f;
    float expEnemyNeonShooterRadius_ = 0.95f;
    float expEnemyNeonLineWidth_ = 0.12f;
    int playerNeonRenderMode_ = 0;
    int bossNeonRenderMode_ = 0;
    float playerNeonBillboardRadius_ = 1.05f;
    float bossNeonBillboardRadius_ = 1.35f;
    float actorNeonBillboardLineWidth_ = 0.12f;
    float playerNeonEmission_ = 1.0f, bossNeonEmission_ = 1.0f;
    float bossNeonBarrelForwardOffset_ = 0.92f;
    float bossNeonBarrelSideOffset_ = 0.0f;
    float bossNeonBarrelLengthScale_ = 1.15f;
    float bossNeonBarrelWidthScale_ = 0.24f;
    float bossNeonBarrelAngleDeg_ = 0.0f;
    bool fillActorNeonBodies_ = true;
    cg2::Vector4 actorNeonBodyFillColor_ = {0.006f, 0.010f, 0.016f, 0.92f};
    float playerDashCurrentAlpha_ = 0.58f;
    float playerAfterimageAlpha_ = 0.42f;
    float playerAfterimageInterval_ = 0.045f;
    float playerAfterimageLifetime_ = 0.30f;
    float playerAfterimageSpawnTimer_ = 0.0f;
    bool showPlayerIdleMeleeSaber_ = true;
    bool enablePlayerMeleeRibbonTrail_ = false;
    float playerIdleSaberSideOffset_ = 0.72f;
    float playerIdleSaberForwardOffset_ = 0.18f;
    float playerIdleSaberLength_ = 1.18f;
    float playerIdleSaberAngleDeg_ = 58.0f;
    float playerIdleSaberHiltLength_ = 0.24f;
    float playerIdleSaberBladeWidth_ = 0.08f;
    float playerIdleSaberOuterWidthScale_ = 2.65f;
    float playerIdleSaberCoreWidthScale_ = 0.30f;
    float playerMeleeBladeOuterWidthScale_ = 2.35f;
    float playerMeleeBladeHaloWidthScale_ = 1.05f;
    float playerMeleeBladeCoreWidthScale_ = 0.26f;
    float playerMeleeTrailWidthScale_ = 1.0f;
    float playerMeleeTrailAlphaScale_ = 1.0f;
    float playerMeleeAfterimageAlphaScale_ = 1.0f;
    /// @brief 自機のネオン残像の姿勢・色・寿命を保持する。
    struct PlayerNeonAfterimage {
        cg2::Vector3 position{};
        cg2::Vector3 direction{0.0f, -1.0f, 0.0f};
        float life = 0.0f;
    };
    std::vector<PlayerNeonAfterimage> playerNeonAfterimages_;
    /// @brief 命中適用後のレーザーの端点・幅・色・残り表示秒を保持する。威力は保持しない。
    struct PlayerLaserBeam {
        cg2::Vector3 start{};
        cg2::Vector3 end{};
        float width = 0.18f;
        float life = 0.0f;
        float maxLife = 0.12f;
        cg2::Vector4 color{0.25f, 1.0f, 0.95f, 1.0f};
    };
    std::vector<PlayerLaserBeam> playerLaserBeams_;
    /// @brief 自機の地雷の位置・起爆条件・寿命を保持する。
    struct PlayerMine {
        cg2::Vector3 position{};
        float radius = 3.2f;
        float fuse = 0.45f;
        float life = 5.0f;
        float maxLife = 5.0f;
        uint32_t damage = 1;
        float rotation = 0.0f;
        cg2::Vector4 color{1.0f, 0.25f, 0.95f, 1.0f};
    };
    std::vector<PlayerMine> playerMines_;
    /// @brief 起爆後の爆発表示の位置・半径・色・残り表示秒を保持する。威力は保持しない。
    struct PlayerMineExplosion {
        cg2::Vector3 position{};
        float radius = 3.2f;
        float life = 0.28f;
        float maxLife = 0.28f;
        cg2::Vector4 color{1.0f, 0.25f, 0.95f, 1.0f};
    };
    std::vector<PlayerMineExplosion> playerMineExplosions_;
    /// @brief 自機の斬撃の形状・威力・有効時間を保持する。
    struct PlayerMeleeSlash {
        cg2::Vector3 origin{};
        cg2::Vector3 followAnchor{};
        cg2::Vector3 direction{1.0f, 0.0f, 0.0f};
        float range = 3.4f;
        float arcDeg = 105.0f;
        float startAngleDeg = -52.5f;
        float endAngleDeg = 52.5f;
        float bladeLengthScale = 1.0f;
        float hiltSideOffset = 0.0f;
        float windupAngleDeg = -100.0f;
        float returnAngleDeg = 58.0f;
        float width = 0.20f;
        float windupDuration = 0.08f;
        float swingDuration = 0.18f;
        float recoveryDuration = 0.10f;
        float elapsed = 0.0f;
        uint32_t damage = 1;
        bool hitApplied = false;
        float knockback = 0;
        bool finisher = false;
        // 同じ斬撃の重複命中照合に使う借用アドレス。所有せず、この履歴から対象のメンバーへアクセスしない。
        std::vector<const Collider*> hitTargets;
        float life = 0.18f;
        float maxLife = 0.18f;
        cg2::Vector4 color{0.55f, 1.25f, 1.0f, 1.0f};
        cg2::TrailInstance* trail = nullptr;
    };
    /// @brief 近接コンボの段数ごとの斬撃外観を指定する。
    struct MeleeComboVisualProfile {
        float startAngleDeg = -70.0f;
        float endAngleDeg = 45.0f;
        float durationScale = 1.0f;
        float bladeLengthScale = 1.0f;
        float bladeWidthScale = 1.0f;
        float hiltSideOffset = 0.0f;
        float windupAngleDeg = -100.0f;
        float returnAngleDeg = 58.0f;
        cg2::Vector4 colorScale{1.0f, 1.0f, 1.0f, 1.0f};
    };
    std::vector<PlayerMeleeSlash> playerMeleeSlashes_;
    std::vector<MeleeComboVisualProfile> playerMeleeComboVisuals_ = {
        {-75.0f, 45.0f, 0.92f, 0.96f, 0.90f, -0.04f, -105.0f, 58.0f, {0.90f, 1.05f, 1.10f, 1.0f}},
        {70.0f, -55.0f, 1.00f, 1.02f, 1.00f, 0.05f, 110.0f, 58.0f, {1.08f, 0.94f, 1.08f, 1.0f}},
        {-145.0f, 135.0f, 1.28f, 1.16f, 1.30f, 0.00f, -178.0f, 58.0f, {1.12f, 1.02f, 0.82f, 1.0f}}};
    /// @brief ネオン三角形演出の位置・速度・色・寿命を保持する。
    struct NeonTriangleParticle {
        cg2::Vector3 position{};
        cg2::Vector3 velocity{};
        cg2::Vector3 initialVelocity{};
        float radius = 0.35f;
        float rotation = 0.0f;
        float angularVelocity = 0.0f;
        float lineWidth = 0.055f;
        float life = 0.0f;
        float maxLife = 0.35f;
        float tiltRad = 0.0f;
        int trailCopies = 0;
        bool isBillboard = false;
        cg2::Vector4 color{1.0f, 0.4f, 1.0f, 1.0f};
    };
    std::vector<NeonTriangleParticle> neonTriangleParticles_;
    float neonParticleTriangleGlowWidthScale_ = 3.2f;
    float neonParticleTriangleCoreWidthScale_ = 0.28f;
    float neonParticleTriangleBrightness_ = 1.45f;
    float neonParticleTriangleTrailSpacing_ = 0.48f;
    float neonParticleTriangleBirthScale_ = 0.52f;
    int neonParticleTriangleTrailCopies_ = 3;
    int neonTriangleEffectMode_ = 0;
    cg2::Vector3 neonTriangleDemoCenter_ = {30.0f, 30.0f, 1.2f};
    float neonTriangleDemoRadius_ = 2.2f;
    float neonTriangleDemoLineWidth_ = 0.16f;
    float neonTriangleDemoRotateSpeed_ = 0.75f;
    float neonTriangleDemoRotation_ = 0.0f;
    cg2::Vector4 neonTriangleDemoColor_ = {0.15f, 0.95f, 1.0f, 1.0f};

    // Opt-in end-to-end experience validation; ordinary play never enters it.
    /// @brief 経験値検証を初期化する。
    void InitializeExperienceValidation();
    /// @brief 提出版UIを記録する。
    void RecordSubmissionUi(const std::string& screen);
    /// @brief 提出版検証を更新する。
    /// @param dt この処理で進める経過時間（秒）。
    bool UpdateSubmissionValidation(float dt);
    /// @brief 経験値検証を更新する。
    /// @param dt この処理で進める経過時間（秒）。
    bool UpdateExperienceValidation(float dt);
    /// @brief 経験値検証を記録する。
    void CaptureExperienceValidation(const std::string& name);
    /// @brief 経験値検証Reportを書き込む。
    void WriteExperienceValidationReport(bool completed);
    /// @brief 経験値近接攻撃検証用を開始する。
    void BeginExperienceMeleeProbe();
    int experienceValidationVariant_ = 0;
    int experienceValidationStyle_ = 2, experiencePreviewRarity_ = 0;
    bool experienceBuildPreserved_ = false;
    bool experienceEvolutionVerified_ = false;
    int experienceDroneSamples_ = 0;
    float experienceValidationElapsed_ = 0, experienceValidationStateAge_ = 0, experienceMeleeAge_ = 0;
    std::string experienceValidationState_;
    std::vector<std::string> experienceValidationCaptures_, experienceValidationErrors_;
    std::vector<std::string> experienceValidationIntroOffers_;
    bool experienceMeleeStarted_ = false, experienceMeleeDone_ = false;
    bool experienceCreditArrivalEligible_ = false, experienceCreditDelivered_ = false;
    int experienceEarlyFlightSamples_ = 0, experiencePrematureCredits_ = 0, experienceGroundOrbSamples_ = 0;
    int experienceIntroWallet_ = -1, experienceAfterIntroWallet_ = -1, experienceInitialKills_ = 0;
    int experienceForcedLaterClears_ = 0, experiencePlayerBulletSamples_ = 0;
    int experienceMeleeMinHp_ = 500, experienceMeleeSlashSamples_ = 0, experienceMeleeBulletSamples_ = 0;
    float experienceMeleeDisplacement_ = 0;
    cg2::Vector3 experienceMeleeTargetStart_{};
    unsigned experienceGuideStageMask_ = 0, experienceSuccessfulDashes_ = 0;
    nlohmann::json experienceIntroOfferDetails_ = nlohmann::json::array();
};
