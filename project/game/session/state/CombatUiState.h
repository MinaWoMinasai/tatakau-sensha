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
