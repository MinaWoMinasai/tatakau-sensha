#pragma once
#include "game/session/GameplayTypes.h"

namespace gameplay {
/// @brief CombatUiStateに属する資源と実行状態を保持する。
struct CombatUiState {
    // 終了フラグ
    bool finished_ = false;

    std::string nextSceneName_ = "TITLE";

    std::unique_ptr<Fade> fade_ = nullptr;

    Phase phase_ = Phase::kFadeIn;

    std::unique_ptr<myEngine::Sprite> shotGide;

    std::unique_ptr<myEngine::Sprite> wasdGide;

    std::unique_ptr<myEngine::Sprite> dashGide;

    std::unique_ptr<myEngine::Sprite> toTitleGide;

    std::unique_ptr<myEngine::TextLabel> dashGuideText_;

    std::unique_ptr<myEngine::TextLabel> moveGuideText_;

    std::unique_ptr<myEngine::TextLabel> titleGuideText_;

    std::unique_ptr<myEngine::TextLabel> controlGuideText_;

    std::unique_ptr<myEngine::TextLabel> fpsText_;

    std::unique_ptr<myEngine::TextLabel> postProfileText_;

    std::unique_ptr<myEngine::TextLabel> flowBannerText_;

    // Bloom comparison caption (presentation helper).
    std::unique_ptr<myEngine::TextLabel> bloomViewText_;

    // Final boss cinematic: letterbox bars and a title card (entrance / defeat).
    std::unique_ptr<myEngine::Sprite> cinematicBarTop_;
    std::unique_ptr<myEngine::Sprite> cinematicBarBottom_;
    std::unique_ptr<myEngine::TextLabel> cinematicTitle_;
    std::unique_ptr<myEngine::TextLabel> cinematicSubtitle_;
    int cinematicTitleMode_ = 0;

    std::unique_ptr<myEngine::TextLabel> resultSummaryText_;

    std::unique_ptr<myEngine::TextLabel> resultMenuText_;

    std::unique_ptr<myEngine::TextLabel> eventCalloutText_;

    std::unique_ptr<myEngine::Sprite> tutorialPanel_;

    std::unique_ptr<myEngine::TextLabel> tutorialTitleText_;

    std::unique_ptr<myEngine::TextLabel> tutorialInputText_;

    std::unique_ptr<myEngine::TextLabel> tutorialDescriptionText_;

    std::unique_ptr<NeonTextEffect> gameTextNeonEffect_;

    int gameTextFontMode_ = 1;

    bool gameTextNeonEnabled_ = true;

    bool gameTextOutlineEnabled_ = false;

    myEngine::Vector4 gameTextOutlineColor_{0.0f, 0.0f, 0.0f, 0.9f};

    float gameTextOutlineThickness_ = 1.0f;

    NeonTextEffectStyle gameTextNeonStyle_{};

    std::vector<FollowHpBar> followHpBars_;

    std::array<std::vector<myEngine::VertexData>, 4> hpBarBackgroundVertices_;

    std::array<std::vector<myEngine::VertexData>, 4> hpBarFillVertices_;

    std::vector<myEngine::VertexData> staminaBarFillVertices_;

    std::array<std::vector<myEngine::VertexData>, 4> hpBarOutlineVertices_;

    std::unordered_map<const void*, HpBarVisibility> hpBarVisibility_;

    std::array<HpBarMaterialBuffer, 13> hpBarMaterials_;

    Microsoft::WRL::ComPtr<ID3D12Resource> hpBarVertexResource_;

    D3D12_VERTEX_BUFFER_VIEW hpBarVertexBufferView_{};

    myEngine::VertexData* hpBarVertexData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> hpBarTransformResource_;

    myEngine::TransformationMatrixWithShadow* hpBarTransformData_ = nullptr;

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

    myEngine::Vector3 tutorialPreviousPlayerPosition_{};

    bool tutorialUpgradeRewardGranted_ = false;

    bool tutorialEvolutionRewardGranted_ = false;

    bool tutorialEvolutionUiWasOpen_ = false;

    bool tutorialCompleteExitReady_ = false;

    // カメラ合わせフラグ
    bool cameraFollow_ = true;

    myEngine::Vector3 direction = {0.0f, -1.0f, 0.0f};

    float insensity = 1.0f;

    float shininess = 10.0f;

    float timeScale_ = 1.0f;

    // 1.0 が通常、0.2 なら 5倍スロー
    float finalDeltaTime = 1.0f / 60.0f;

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
};
} // namespace gameplay
