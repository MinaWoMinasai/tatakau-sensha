#pragma once
#include "game/player/ui/PlayerUiState.h"

/// @brief PlayerHudの表示と入力を担当し、必要な自機の状態を借用する。
class PlayerHud {
public:
    /// @brief 自機と画面状態を接続する。
    PlayerHud(Player& player, PlayerUiState& ui) : player_(player), ui_(ui) {}
    using UiProfileStats = Player::UiProfileStats;
#if defined(USE_IMGUI) && !defined(NDEBUG)
    using UpgradeHudDebugSnapshot = Player::UpgradeHudDebugSnapshot;
#endif
    /// @brief 強化HUDのゲージの発光を、シーンのポストエフェクト後に描画する。
    void DrawUpgradeHudAfterPostEffects();

    /// @brief ゲーム中のHUDで発光対象にする文字ラベルの借用ポインターをlabelsの末尾へ追加する。
    /// @note 進化画面中は追加しない。所有権は移らず、UIの再構築やPlayerの破棄をまたいで保持しない。
    void AppendGameplayNeonTextLabels(std::vector<cg2::TextLabel*>& labels) const;

    /// @brief 強化HUD文字テクスチャを利用前に準備する。
    void PrepareUpgradeHudTextTextures();

    /// @brief 強化HUDの表示設定を編集する制作画面を表示する。USE_IMGUIが有効な構成で使う。
    void DrawUpgradeHudDebugImGui();

    /// @brief 強化HUDの処理時間と描画件数への読み取り専用参照を返す。
    const UiProfileStats& GetUpgradeHudProfileStats() const;

#if defined(USE_IMGUI) && !defined(NDEBUG)
    /// @brief 強化HUD開発表示状態の写しを返す。
    UpgradeHudDebugSnapshot GetUpgradeHudDebugSnapshot() const;
#endif

    /// @brief 強化HUDを初期化する。
    void InitializeUpgradeHud();

    /// @brief 経験値表示の文字ラベルを更新し、描画用テクスチャを準備して配置する。
    /// @param text 1バイトずつ分けて表示する文字列。呼び出し側では数字やEXPなどのASCII文字を渡す。
    void UpdateUpgradeHudExpGlyphs(const std::string& text, const cg2::TextStyle& style);

    /// @brief 強化HUDの経験値表示の各文字を配置する。
    void PositionUpgradeHudExpGlyphs();

    /// @brief レベルの数字と機体名の文字ラベルを更新し、配置する。
    void UpdateUpgradeHudLevelLabels(int level, const std::string& className, const cg2::TextStyle& style);

    /// @brief 強化HUDのレベル表示の各文字を配置する。
    void PositionUpgradeHudLevelLabels();

    /// @brief レベル・経験値ゲージへ、現在の端の丸み設定を含むスタイルを反映する。
    void ApplyUpgradeHudProgressBarStyles();

    /// @brief 強化HUDを更新する。
    /// @param uiDeltaTime UI用の経過秒。戦闘の時間倍率と分けて渡す。
    void UpdateUpgradeHud(float uiDeltaTime);

    /// @brief 強化HUDを描画する。
    void DrawUpgradeHud();

    /// @brief 強化段数バーの位置・サイズ・段数を設定し、点灯段数を0に戻す。
    void PrepareUpgradeHudSegmentBars();

    /// @brief 強化HUDの矩形をまとめて描くためのGPUバッファと描画用変換を準備する。
    void InitializeUpgradeHudBatch();

    /// @brief 強化HUDの矩形群を頂点へまとめ、1つの描画呼び出しで描く。
    void DrawUpgradeHudRectBatch(bool showUpgradeList, float expRatio, float levelRatio, float listAlpha, float listOffsetX);

    /// @brief 指定した画面座標の矩形を、2三角形分の6頂点としてverticesの末尾へ追加する。
    void QueueUpgradeHudRect(std::vector<cg2::TrailVertex>& vertices, const cg2::Vector2& pos, const cg2::Vector2& size,
                             const cg2::Vector4& color) const;

    /// @brief 保存しているHUD配置をスプライト・文字・ゲージへ反映する。
    void ApplyUpgradeHudLayout();

    /// @brief 強化HUD設定を読み込む。
    bool LoadUpgradeHudConfig(const std::string& path = "resources/configs/playerUpgradeHud.json");

    /// @brief 強化HUD設定を保存する。
    /// @return 保存先を開き、JSONの書き込みを行えばtrue。書き込み後のストリームエラーは検査しない。
    bool SaveUpgradeHudConfig(const std::string& path = "resources/configs/playerUpgradeHud.json") const;

private:
    Player& player_;
    PlayerUiState& ui_;
};
