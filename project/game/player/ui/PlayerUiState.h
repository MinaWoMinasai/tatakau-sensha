#pragma once
#include "Player.h"

/// @brief 自機のHUD・進化画面・図鑑の表示資源と操作状態。戦闘状態は保持しない。
struct PlayerUiState {
    using UiProfileStats = Player::UiProfileStats;
    std::unique_ptr<cg2::Sprite> machineGunBtnSprite_ = nullptr;

    // ボタンの見た目
    cg2::Vector2 btnPos_ = {50.0f, 200.0f};

    // ボタンの位置（画面左下あたり）
    cg2::Vector2 btnSize_ = {100.0f, 50.0f};

    // ボタンのサイズ

    // 図鑑の並び順（表示したい順番に定義）
    std::vector<TankData> encyclopedia_;

    std::unique_ptr<cg2::Sprite> evolutionBackdropSprite_;

    std::unique_ptr<cg2::Sprite> evolutionPreviewPanelSprite_;

    std::unique_ptr<cg2::Sprite> evolutionStatsPanelSprite_;

    std::unique_ptr<cg2::Sprite> evolutionPreviewTankSprite_;

    std::unique_ptr<cg2::Sprite> evolutionShotSprite_;

    std::unique_ptr<cg2::Sprite> evolutionChangeButtonSprite_;

    std::unique_ptr<cg2::TextLabel> evolutionTitleLabel_;

    std::unique_ptr<cg2::TextLabel> evolutionHintLabel_;

    std::unique_ptr<cg2::TextLabel> evolutionPreviewNameLabel_;

    std::unique_ptr<cg2::TextLabel> evolutionRoleLabel_;

    std::unique_ptr<cg2::TextLabel> evolutionChangeButtonLabel_;

    std::array<std::unique_ptr<cg2::TextLabel>, 9> evolutionStatLabels_;

    static constexpr size_t kStaticEvolutionMaxCandidates = 4;

    static constexpr size_t kStaticEvolutionMaxNodes = kStaticEvolutionMaxCandidates + 1;

    static constexpr size_t kStaticEvolutionMaxPaths = kStaticEvolutionMaxCandidates + 1;

    /// @brief 進化選択画面の配置・色・文字・演出を指定する。
    struct EvolutionUiStyleConfig {
        bool enabled = true;
        bool radialLayout = false;
        cg2::Vector2 virtualResolution{1280.0f, 720.0f};
        float safeMargin = 48.0f;
        std::array<cg2::Vector2, 4> nodeAnchors{{{0.23f, 0.43f}, {0.67f, 0.22f}, {0.67f, 0.43f}, {0.67f, 0.64f}}};
        std::array<cg2::Vector2, 4> radialNodeAnchors{{{0.50f, 0.43f}, {0.33f, 0.20f}, {0.67f, 0.20f}, {0.50f, 0.70f}}};
        cg2::Vector2 branchPointAnchor{0.49f, 0.43f};
        cg2::Vector2 currentNodeSize{160.0f, 112.0f};
        cg2::Vector2 candidateNodeSize{160.0f, 112.0f};
        float normalScale = 1.0f;
        float hoverScale = 1.05f;
        float selectedScale = 1.08f;
        float nodeCornerCut = 12.0f;
        float nodeOutlineGlowWidth = 10.0f;
        float nodeOutlineWidth = 2.0f;
        float silhouetteScale = 1.0f;
        float circuitOuterGlowWidth = 18.0f;
        float circuitMiddleGlowWidth = 8.0f;
        float circuitCoreWidth = 2.5f;
        float circuitOpacity = 0.78f;
        float circuitOuterAlpha = 0.14f;
        float circuitMiddleAlpha = 0.34f;
        float circuitCoreAlpha = 0.90f;
        float backgroundDimOpacity = 0.88f;
        cg2::Vector2 detailPanelAnchor{0.50f, 0.88f};
        cg2::Vector2 detailPanelSize{1088.0f, 134.0f};
        cg2::Vector2 confirmButtonSize{186.0f, 44.0f};
        float titleFontSize = 26.0f;
        float classNameFontSize = 22.0f;
        float bodyFontSize = 16.0f;
        float buttonFontSize = 17.0f;
        std::string fontFamily = "Meiryo";
        std::string fontPath;
        int fontWeight = 400;
        NeonTextEffectStyle neonText{};
        cg2::Vector4 normalColor{0.12f, 0.34f, 0.42f, 0.82f};
        cg2::Vector4 availableColor{0.16f, 0.64f, 0.72f, 0.92f};
        cg2::Vector4 hoverColor{0.30f, 0.94f, 1.00f, 1.0f};
        cg2::Vector4 selectedColor{0.42f, 1.00f, 0.58f, 1.0f};
        cg2::Vector4 lockedColor{0.18f, 0.22f, 0.28f, 0.68f};
        cg2::Vector4 panelColor{0.025f, 0.055f, 0.080f, 0.94f};
        cg2::Vector4 titleTextColor{0.74f, 1.00f, 0.92f, 1.0f};
        cg2::Vector4 classTextColor{0.92f, 1.00f, 0.96f, 1.0f};
        cg2::Vector4 bodyTextColor{0.84f, 0.92f, 1.00f, 1.0f};
        cg2::Vector4 buttonTextColor{0.96f, 1.00f, 0.98f, 1.0f};
        cg2::Vector4 textOutlineColor{0.0f, 0.025f, 0.045f, 0.96f};
        float titleOutlineWidth = 0.9f;
        float classNameOutlineWidth = 1.0f;
        float bodyOutlineWidth = 0.35f;
        float buttonOutlineWidth = 0.5f;
        int fixedSelectedCandidate = 0;
    };

    EvolutionUiStyleConfig evolutionUiStyle_{};

    std::unique_ptr<cg2::Sprite> staticEvolutionBackdropSprite_;

    std::unique_ptr<TankButtonUiStyle> tankButtonUiStyle_;

    std::unique_ptr<cg2::ObjectPostEffect> staticEvolutionButtonBloomEffect_;

    std::unique_ptr<NeonTextEffect> staticEvolutionTextEffect_;

    std::array<std::unique_ptr<TankButtonUI>, kStaticEvolutionMaxNodes> staticEvolutionTankButtons_;

    std::unique_ptr<cg2::Sprite> staticEvolutionDetailPanelSprite_;

    std::unique_ptr<cg2::Sprite> staticEvolutionConfirmButtonSprite_;

    std::array<std::unique_ptr<cg2::Sprite>, 8> staticEvolutionConfirmOutlineSprites_;

    std::unique_ptr<cg2::Sprite> staticEvolutionBranchGlowSprite_;

    std::unique_ptr<cg2::Sprite> staticEvolutionBranchCoreSprite_;

    std::array<std::array<std::unique_ptr<cg2::Sprite>, 3>, kStaticEvolutionMaxNodes> staticEvolutionNodePanelSprites_;

    static constexpr size_t kStaticEvolutionNodeFrameSpriteCount = 24;

    std::array<std::array<std::unique_ptr<cg2::Sprite>, kStaticEvolutionNodeFrameSpriteCount>, kStaticEvolutionMaxNodes>
        staticEvolutionNodeFrameSprites_;

    static constexpr size_t kStaticEvolutionSilhouetteSpriteCount = 32;

    std::array<std::array<std::unique_ptr<cg2::Sprite>, kStaticEvolutionSilhouetteSpriteCount>, kStaticEvolutionMaxNodes>
        staticEvolutionSilhouetteSprites_;

    static constexpr size_t kStaticEvolutionCircuitSpriteCount = 27;

    std::array<std::unique_ptr<cg2::Sprite>, kStaticEvolutionCircuitSpriteCount> staticEvolutionCircuitSprites_;

    std::unique_ptr<cg2::TextLabel> staticEvolutionTitleLabel_;

    std::unique_ptr<cg2::TextLabel> staticEvolutionPrototypeLabel_;

    std::array<std::unique_ptr<cg2::TextLabel>, kStaticEvolutionMaxNodes> staticEvolutionNodeNameLabels_;

    std::array<std::unique_ptr<cg2::TextLabel>, kStaticEvolutionMaxNodes> staticEvolutionNodeRankLabels_;

    std::unique_ptr<cg2::TextLabel> staticEvolutionDetailClassLabel_;

    std::unique_ptr<cg2::TextLabel> staticEvolutionRoleLabel_;

    std::array<std::unique_ptr<cg2::TextLabel>, 3> staticEvolutionDeltaLabels_;

    std::unique_ptr<cg2::TextLabel> staticEvolutionAbilityLabel_;

    std::unique_ptr<cg2::TextLabel> staticEvolutionConfirmLabel_;

    std::unique_ptr<cg2::TextLabel> staticEvolutionPanelHintLabel_;

    std::array<cg2::Vector2, kStaticEvolutionMaxNodes> staticEvolutionNodeCentersVirtual_{};

    std::array<cg2::Vector2, kStaticEvolutionMaxNodes> staticEvolutionNodeDrawSizesVirtual_{};

    std::array<cg2::Vector2, kStaticEvolutionMaxNodes> staticEvolutionNodeHitSizesVirtual_{};

    std::array<std::array<cg2::Vector2, 4>, kStaticEvolutionMaxPaths> staticEvolutionCircuitControlPoints_{};

    std::array<int, kStaticEvolutionMaxPaths> staticEvolutionCircuitControlPointCounts_{};

    std::array<std::string, kStaticEvolutionMaxCandidates> staticEvolutionCandidateIds_{};

    size_t staticEvolutionCandidateCount_ = 0;

    int staticEvolutionHoveredNode_ = -1;

    bool staticEvolutionConfirmHovered_ = false;

    /// @brief 進化経路図の1ノードの機体IDと、縦方向の配置比率を表す。解放条件は機体設定で管理する。
    struct EvolutionCircuitNodeDefinition {
        std::string classId;
        float lane = 0.5f;
    };

    /// @brief 進化経路図のノード間の接続を表す。
    struct EvolutionCircuitEdgeDefinition {
        std::string from;
        std::string to;
    };

    static constexpr size_t kEvolutionCircuitMaxNodes = 12;

    static constexpr size_t kEvolutionCircuitMaxLineSprites = 108;

    std::vector<EvolutionCircuitNodeDefinition> evolutionCircuitNodes_;

    std::vector<EvolutionCircuitEdgeDefinition> evolutionCircuitEdges_;

    std::vector<std::string> evolutionHistory_;

    std::array<cg2::Vector2, kEvolutionCircuitMaxNodes> evolutionCircuitNodeCentersVirtual_{};

    std::array<std::unique_ptr<TankButtonUI>, kEvolutionCircuitMaxNodes> evolutionCircuitTankButtons_;

    std::unique_ptr<TankButtonUI> evolutionCircuitDetailPreview_;

    std::array<std::unique_ptr<cg2::Sprite>, kEvolutionCircuitMaxLineSprites> evolutionCircuitLineSprites_;

    std::unique_ptr<cg2::Sprite> evolutionCircuitBackdropSprite_;

    std::unique_ptr<cg2::Sprite> evolutionCircuitDetailPanelSprite_;

    std::unique_ptr<cg2::TextLabel> evolutionCircuitTitleLabel_;

    std::array<std::unique_ptr<cg2::TextLabel>, 4> evolutionCircuitRankLabels_;

    std::unique_ptr<cg2::TextLabel> evolutionCircuitDetailNameLabel_;

    std::unique_ptr<cg2::TextLabel> evolutionCircuitDetailMetaLabel_;

    std::unique_ptr<cg2::TextLabel> evolutionCircuitDetailRoleLabel_;

    std::array<std::unique_ptr<cg2::TextLabel>, 3> evolutionCircuitDetailStatLabels_;

    std::unique_ptr<cg2::TextLabel> evolutionCircuitHintLabel_;

    int evolutionCircuitSelectedNode_ = 0;

    int evolutionCircuitHoveredNode_ = -1;

    bool evolutionCircuitLoaded_ = false;

    std::string evolutionUiStyleStatus_;

    bool showEvolutionVirtualBounds_ = false;

    bool showEvolutionSafeArea_ = false;

    bool showEvolutionNodeBounds_ = false;

    bool showEvolutionMouseBounds_ = false;

    bool showEvolutionTextBounds_ = false;

    bool showEvolutionCenterLines_ = false;

    bool showEvolutionCircuitControlPoints_ = false;

    bool showEvolutionResolutionInfo_ = false;

    std::unique_ptr<cg2::Sprite> upgradeHudBackdropSprite_;

    std::unique_ptr<cg2::Sprite> upgradeHudExpBackSprite_;

    std::unique_ptr<cg2::Sprite> upgradeHudExpFillSprite_;

    std::unique_ptr<cg2::Sprite> upgradeHudLevelBackSprite_;

    std::unique_ptr<cg2::Sprite> upgradeHudLevelFillSprite_;

    std::unique_ptr<NeonProgressBar> upgradeHudLevelProgressBar_;

    std::unique_ptr<NeonProgressBar> upgradeHudExpProgressBar_;

    std::array<std::unique_ptr<NeonSegmentedBar>, 7> upgradeHudSegmentBars_;

    std::unique_ptr<cg2::ObjectPostEffect> upgradeHudBarBloomEffect_;

    NeonProgressBarStyle upgradeHudLevelProgressStyle_{};

    NeonProgressBarStyle upgradeHudExpProgressStyle_{};

    std::unique_ptr<cg2::TextLabel> upgradeHudTitleLabel_;

    std::unique_ptr<cg2::TextLabel> upgradeHudPointLabel_;

    std::unique_ptr<cg2::TextLabel> upgradeHudLevelLabel_;

    std::unique_ptr<cg2::TextLabel> upgradeHudLevelClassLabel_;

    std::unique_ptr<cg2::TextLabel> upgradeHudListLabel_;

    static constexpr size_t kUpgradeHudExpGlyphSlotCount = 32;

    std::array<std::unique_ptr<cg2::TextLabel>, kUpgradeHudExpGlyphSlotCount> upgradeHudExpGlyphLabels_;

    size_t upgradeHudExpGlyphCount_ = 0;

    static constexpr size_t kUpgradeHudLevelGlyphSlotCount = 2;

    std::array<std::unique_ptr<cg2::TextLabel>, kUpgradeHudLevelGlyphSlotCount> upgradeHudLevelGlyphLabels_;

    size_t upgradeHudLevelGlyphCount_ = 0;

    unsigned long long upgradeHudTextFontRevision_ = 0;

    bool upgradeHudTextPrepared_ = false;

    bool upgradeHudTextPreparedForSegmentedBars_ = false;

    std::array<std::unique_ptr<cg2::Sprite>, 7> upgradeHudButtonSprites_;

    std::array<std::unique_ptr<cg2::Sprite>, 7> upgradeHudPlusSprites_;

    std::array<std::unique_ptr<cg2::Sprite>, 7> upgradeHudMinusSprites_;

    std::array<std::unique_ptr<cg2::TextLabel>, 7> upgradeHudNameLabels_;

    std::array<std::unique_ptr<cg2::TextLabel>, 7> upgradeHudLevelLabels_;

    std::array<std::unique_ptr<cg2::TextLabel>, 7> upgradeHudMinusLabels_;

    std::array<std::unique_ptr<cg2::TextLabel>, 7> upgradeHudPlusLabels_;

    Microsoft::WRL::ComPtr<ID3D12Resource> upgradeHudBatchVertexResource_;

    D3D12_VERTEX_BUFFER_VIEW upgradeHudBatchVertexBufferView_{};

    cg2::TrailVertex* upgradeHudBatchVertexData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> upgradeHudBatchTransformResource_;

    cg2::Matrix4x4* upgradeHudBatchTransformData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> upgradeHudBatchMaterialResource_;

    cg2::Material* upgradeHudBatchMaterialData_ = nullptr;

    static constexpr uint32_t kUpgradeHudBatchMaxVertices = 256;

    std::array<float, 7> upgradeHudFlashTimers_{};

    std::array<float, 7> upgradeHudRefundFlashTimers_{};

    std::array<float, 7> upgradeHudMissFlashTimers_{};

    float upgradeHudListVisibility_ = 0.0f;

    float upgradeHudListAnimSpeed_ = 10.0f;

    float upgradeHudListSlideDistance_ = 260.0f;

    bool upgradeHudMouseCaptured_ = false;

    bool arenaUiEnabled_ = true;

    bool upgradeHudVisible_ = true;

    bool upgradeHudHideListWithoutPoints_ = true;

    bool upgradeHudDrawListPanels_ = true;

    bool upgradeHudDrawListText_ = true;

    bool upgradeHudDrawBottomBars_ = true;

    bool upgradeHudDrawBottomText_ = true;

    bool upgradeHudUseRectBatch_ = true;

    bool upgradeHudUseNeonProgressBars_ = true;

    bool upgradeHudRoundedProgressBars_ = true;

    bool upgradeHudUseSegmentedUpgradeBars_ = true;

    bool upgradeHudListTextBloomEnabled_ = false;

#if defined(USE_IMGUI) && !defined(NDEBUG)
    bool upgradeHudSegmentedBarBloomEnabled_ = true;
#endif

    cg2::Vector2 upgradeHudSegmentBarOffset_ = {0.0f, 0.0f};

    cg2::Vector2 upgradeHudSegmentBarSize_ = {230.0f, 22.0f};

    cg2::Vector2 upgradeHudPanelPos_ = {18.0f, 338.0f};

    cg2::Vector2 upgradeHudPanelSize_ = {340.0f, 260.0f};

    cg2::Vector2 upgradeHudRowStart_ = {30.0f, 384.0f};

    cg2::Vector2 upgradeHudButtonSize_ = {286.0f, 22.0f};

    cg2::Vector2 upgradeHudPlusSize_ = {32.0f, 18.0f};

    float upgradeHudRowGap_ = 29.0f;

    float upgradeHudNameX_ = 44.0f;

    float upgradeHudLevelX_ = 184.0f;

    float upgradeHudMinusX_ = 268.0f;

    float upgradeHudPlusX_ = 306.0f;

    float upgradeHudMinusLabelX_ = 277.0f;

    float upgradeHudPlusLabelX_ = 313.0f;

    float upgradeHudNameTextOffsetY_ = 3.0f;

    float upgradeHudLevelTextOffsetY_ = 3.0f;

    float upgradeHudMinusTextOffsetY_ = 1.0f;

    float upgradeHudPlusTextOffsetY_ = 1.0f;

    cg2::Vector2 upgradeHudTitlePos_ = {32.0f, 350.0f};

    cg2::Vector2 upgradeHudPointPos_ = {286.0f, 352.0f};

    cg2::Vector2 upgradeHudLevelBarPos_ = {415.0f, 656.0f};

    cg2::Vector2 upgradeHudLevelBarSize_ = {450.0f, 16.0f};

    cg2::Vector2 upgradeHudLevelTextPos_ = {565.0f, 653.0f};

    cg2::Vector2 upgradeHudExpBarPos_ = {390.0f, 680.0f};

    cg2::Vector2 upgradeHudExpBarSize_ = {500.0f, 20.0f};

    cg2::Vector2 upgradeHudExpTextPos_ = {560.0f, 677.0f};

    std::string upgradeHudConfigStatus_;

    UiProfileStats upgradeHudProfile_{};

    UiProfileStats evolutionUiProfile_{};

    int cachedUpgradeHudExp_ = -1;

    int cachedUpgradeHudNextExp_ = -1;

    int cachedUpgradeHudLevel_ = -1;

    int upgradeHudAnimatedLevel_ = -1;

    int cachedUpgradeHudSkillPoints_ = -1;

    int cachedUpgradeHudMaxEnhancePoint_ = -1;

    bool cachedUpgradeHudSegmentedBars_ = false;

    std::string cachedUpgradeHudClassName_;

    std::array<int, 7> cachedUpgradeHudLevels_{-1, -1, -1, -1, -1, -1, -1};

    bool cachedUpgradeHudListVisible_ = false;

    int editorSelectedClassIndex_ = 0;

    int codexSelectedClassIndex_ = 0;

    float codexPreviewTimer_ = 0.0f;

    float codexPreviewAimDeg_ = 0.0f;

    bool codexPreviewAutoMove_ = true;

    bool codexPreviewAutoFire_ = true;
};
