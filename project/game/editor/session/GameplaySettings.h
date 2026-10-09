#pragma once
#include "game/session/GameWorld.h"

namespace gameplay {
/// @brief GameplaySettingsの処理を担当し、同じプレイの共有状態を借用する。
class GameplaySettings {
public:
    /// @brief 借用するワールドを設定する。
    explicit GameplaySettings(GameWorld& world) : world_(world) {}
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

    /// @brief 性能調整編集画面からのJsonを読み込む。
    void LoadBalanceEditorFromJson(const nlohmann::json& balanceJson);

    /// @brief 性能調整Jsonからの編集画面を組み立てる。
    nlohmann::json BuildBalanceJsonFromEditor() const;

    /// @brief 性能調整編集画面へのレベルファイルを保存する。
    bool SaveBalanceEditorToLevelFile(const std::string& filePath);

    /// @brief 性能調整AIHandoffを書き込む。
    bool WriteBalanceAIHandoff(const std::string& filePath) const;

private:
    GameWorld& world_;
};
} // namespace gameplay
