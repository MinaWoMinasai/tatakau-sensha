#pragma once
#include "game/session/GameplayTypes.h"

namespace gameplay {
/// @brief RunSessionStateに属する資源と実行状態を保持する。
struct RunSessionState {
    bool expeditionAuthoringHubOpen_ = false;

    nlohmann::json expeditionPostDraft_;

    nlohmann::json expeditionVisualDraft_;

    std::string expeditionAuthoringStatus_;

    nlohmann::json expeditionStyleBalanceDraft_;

    bool tankExpeditionBalanceEditorOpen_ = false;

    bool combatValidationEnabled_ = false, combatValidationRequested_ = false, combatValidationPhase2Injected_ = false;

    float combatValidationElapsed_ = 0;

    int combatValidationIndex_ = -1;

    CombatValidationProbe combatValidationProbe_;

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

    int expeditionServicePage_ = 0, expeditionBlueprint_ = 0;

    float expeditionMapScroll_ = 0;

    bool expeditionMapPreview_ = false;

    bool expeditionBuildChoice_ = false, expeditionBuildChosen_ = false;

    bool specialValidationEnabled_ = false;

    nlohmann::json specialValidation_;

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

    std::vector<ExpeditionHitSpark> expeditionHitSparks_;

    float expeditionHitSparkCooldown_ = 0;

    bool expeditionBossPhase2Seen_ = false;

    std::string expeditionLastFocus_;

    std::array<float, 3> expeditionCardFocus_{};

    std::vector<MapNodeVisual> expeditionMapVisuals_;

    std::vector<MapEdgeVisual> expeditionMapEdges_;

    std::vector<std::unique_ptr<cg2::Sprite>> expeditionMapGrid_;

    std::unique_ptr<cg2::TextLabel> expeditionMapTitle_, expeditionMapSubtitle_, expeditionMapInfo_, expeditionMapLegend_,
        expeditionMapHelp_;

    std::array<std::unique_ptr<cg2::TextLabel>, 3> expeditionBlueprintLabels_;

    std::array<std::unique_ptr<cg2::Sprite>, 3> expeditionBlueprintButtons_;

    std::vector<ExpeditionCreditOrb> expeditionCredits_;

    std::unique_ptr<cg2::Sprite> expeditionCreditIcon_, expeditionCreditPulse_, expeditionStaminaTrack_, expeditionStaminaFill_;

    std::unique_ptr<cg2::TextLabel> expeditionCreditText_;

    std::unique_ptr<NeonTextEffect> expeditionCompleteGlow_;

    std::array<std::unique_ptr<cg2::Sprite>, 4> expeditionSpotlight_;

    std::array<std::unique_ptr<cg2::Sprite>, 24> expeditionPointer_, expeditionPointerGlow_;

    std::unique_ptr<cg2::Sprite> expeditionPriceIcon_;

    size_t expeditionPointerCursor_ = 0;

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

    tankexp::TutorialValidationState tankExpeditionTutorialValidation_{};

    tankexp::ExpeditionTutorial tankExpeditionTutorial_{};

    bool tankExpeditionTutorialSaved_ = false;

    bool expeditionTutorialPreviouslyCompleted_ = false;

    bool tankExpeditionDetailsOpen_ = false;

    int tankExpeditionTutorialKills_ = 0;

    cg2::Vector3 tankExpeditionTutorialPrevious_{};

    std::unique_ptr<cg2::Sprite> tankExpeditionHpTrack_, tankExpeditionHpFill_;

    std::unique_ptr<cg2::Sprite> tankExpeditionExpTrack_, tankExpeditionExpFill_, tankExpeditionBuildPanel_;

    std::unique_ptr<cg2::TextLabel> tankExpeditionExpText_, tankExpeditionDetailsText_;

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

    bool prototypeRun_ = false;

    tankrun::RunDirector tankRun_{};

    int tankRunSelection_ = 0;

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

    std::vector<SpecialCombatFlash> specialCombatFlashes_;

    std::vector<BuildCombatFlash> buildCombatFlashes_;

    std::vector<SpecialProjectileVisual> specialProjectileVisuals_;

    float railChargeAudioAge_ = 0;
};
} // namespace gameplay
